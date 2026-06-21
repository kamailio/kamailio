package proxy

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// ShutdownResult summarizes the outcome of a coordinated shutdown.
type ShutdownResult struct {
	ListenersStopped int
	Errors           []string
	Duration         time.Duration
	TimedOut         bool
}

// ShutdownProxy stops all resources attached to a ProxyCore and any
// HealthServer / network listeners registered with it. It is safe to
// call on a nil or already-stopped ProxyCore.
func ShutdownProxy(ctx context.Context, core *ProxyCore, health *HealthServer, extraListeners ...interface{ Shutdown(context.Context) error }) ShutdownResult {
	result := ShutdownResult{}
	if core == nil && health == nil && len(extraListeners) == 0 {
		return result
	}
	if ctx == nil {
		ctx = context.Background()
	}
	start := time.Now()

	// Step 1: stop accepting new requests
	if core != nil {
		if err := core.Shutdown(ctx); err != nil {
			result.Errors = append(result.Errors, fmt.Sprintf("core shutdown: %v", err))
		}
	}

	// Step 2: stop the health server (if any)
	if health != nil {
		if err := health.Shutdown(ctx); err != nil {
			result.Errors = append(result.Errors, fmt.Sprintf("health server: %v", err))
		}
	}

	// Step 3: stop additional listeners (transport level)
	var wg sync.WaitGroup
	mu := sync.Mutex{}
	for _, l := range extraListeners {
		if l == nil {
			continue
		}
		wg.Add(1)
		go func(l interface{ Shutdown(context.Context) error }) {
			defer wg.Done()
			if err := l.Shutdown(ctx); err != nil {
				mu.Lock()
				result.Errors = append(result.Errors, fmt.Sprintf("listener: %v", err))
				mu.Unlock()
				return
			}
			mu.Lock()
			result.ListenersStopped++
			mu.Unlock()
		}(l)
	}

	// Wait for all listeners, respecting the outer context.
	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()

	select {
	case <-done:
	case <-ctx.Done():
		result.TimedOut = true
		result.Errors = append(result.Errors, fmt.Sprintf("shutdown context: %v", ctx.Err()))
	}

	result.Duration = time.Since(start)
	return result
}

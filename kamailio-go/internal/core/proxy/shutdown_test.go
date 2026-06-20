package proxy

import (
	"context"
	"errors"
	"testing"
	"time"
)

type fakeShutdowner struct {
	delay time.Duration
	err   error
	calls int
}

func (f *fakeShutdowner) Shutdown(ctx context.Context) error {
	f.calls++
	if f.delay > 0 {
		select {
		case <-time.After(f.delay):
		case <-ctx.Done():
			return ctx.Err()
		}
	}
	return f.err
}

func TestShutdownProxy_Basic(t *testing.T) {
	core := NewProxyCore(nil)
	fs := &fakeShutdowner{}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	result := ShutdownProxy(ctx, core, nil, fs)
	if result.ListenersStopped != 1 {
		t.Errorf("listeners stopped = %d, want 1", result.ListenersStopped)
	}
	if len(result.Errors) > 0 {
		t.Errorf("unexpected errors: %v", result.Errors)
	}
	if result.TimedOut {
		t.Error("should not timeout")
	}
	if fs.calls != 1 {
		t.Errorf("shutdown calls = %d, want 1", fs.calls)
	}
}

func TestShutdownProxy_Timeout(t *testing.T) {
	fs := &fakeShutdowner{delay: 100 * time.Millisecond}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Millisecond)
	defer cancel()

	result := ShutdownProxy(ctx, nil, nil, fs)
	if !result.TimedOut {
		t.Error("expected timed out")
	}
}

func TestShutdownProxy_ErrorFromListener(t *testing.T) {
	fs := &fakeShutdowner{err: errors.New("boom")}
	result := ShutdownProxy(context.Background(), nil, nil, fs)
	if len(result.Errors) == 0 {
		t.Error("expected errors from listener")
	}
	if result.ListenersStopped != 0 {
		t.Errorf("listeners stopped = %d, want 0", result.ListenersStopped)
	}
}

func TestShutdownProxy_NilSafe(t *testing.T) {
	result := ShutdownProxy(context.Background(), nil, nil)
	if len(result.Errors) > 0 {
		t.Errorf("unexpected errors: %v", result.Errors)
	}
}

func TestProxyCore_DrainAndIsDraining(t *testing.T) {
	core := NewProxyCore(nil)
	if core.IsDraining() {
		t.Error("should not be draining initially")
	}

	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()
	if err := core.Drain(ctx); err != nil {
		t.Errorf("drain: %v", err)
	}
	// After Drain returns, the flag is reset
	if core.IsDraining() {
		t.Error("should not be draining after Drain returns")
	}
}

func TestProxyCore_NilSafe(t *testing.T) {
	var core *ProxyCore
	_ = core.Drain(context.Background())
	_ = core.Shutdown(context.Background())
	if core.IsDraining() {
		t.Error("nil core should not report draining")
	}
}

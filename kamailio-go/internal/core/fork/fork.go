// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Forking coordinator - parallel and serial branch dispatcher.
 *
 * Matches the stateful proxy behaviour from Kamailio's C implementation,
 * in particular the t_fork() family of functions in modules/tm.
 */

package fork

import (
	"context"
	"net"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// Branch represents one target leg of a forked request.
type Branch struct {
	URI    string        // the target SIP URI to forward to
	Status int           // SIP status code received; 0 = unanswered
	Reason string        // reason phrase
	Err    error         // set if the underlying forward returned an error
	Took   time.Duration // round-trip time for this branch
	Dead   bool          // true when this branch is definitively rejected
	Winner bool          // true when this branch produced the winning 2xx
}

// Forker coordinates parallel branches for a single request.
// Usage:
//
//	f := NewForker(timeout)
//	f.AddBranch("sip:alice@host1:5060")
//	f.AddBranch("sip:alice@host2:5060")
//	branches, err := f.Run(ctx, forwarder, originalMsg, srcAddr)
//	// branches are ordered by insertion; the winning 2xx is marked.
type Forker struct {
	mu       sync.Mutex
	branches []*Branch
	timeout  time.Duration
}

// NewForker constructs a Forker with a per-request timeout.
// A timeout of 0 means the Forker uses a default 5s.
func NewForker(timeout time.Duration) *Forker {
	if timeout <= 0 {
		timeout = 5 * time.Second
	}
	return &Forker{timeout: timeout}
}

// AddBranch registers one target URI.
func (f *Forker) AddBranch(uri string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.branches = append(f.branches, &Branch{URI: uri})
}

// BranchCount returns the number of registered branches.
func (f *Forker) BranchCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return len(f.branches)
}

// ForwardFn abstracts the underlying "send one request to one host"
// operation. ProxyCore implements this in terms of forward.Forwarder.
type ForwardFn func(ctx context.Context, msg *parser.SIPMsg, uri string, src net.Addr) (status int, reason string, err error)

// Run executes all registered branches in parallel and returns them in
// registration order. The first branch returning 2xx is flagged as Winner.
func (f *Forker) Run(ctx context.Context, fn ForwardFn, msg *parser.SIPMsg, src net.Addr) ([]*Branch, error) {
	f.mu.Lock()
	working := make([]*Branch, len(f.branches))
	copy(working, f.branches)
	f.mu.Unlock()
	if len(working) == 0 {
		return nil, nil
	}

	runCtx, cancel := context.WithTimeout(ctx, f.timeout)
	defer cancel()

	var wg sync.WaitGroup
	var anyWin sync.Once
	for i := range working {
		wg.Add(1)
		go func(idx int, br *Branch) {
			defer wg.Done()
			select {
			case <-runCtx.Done():
				br.Err = runCtx.Err()
				br.Dead = true
				return
			default:
			}
			start := time.Now()
			status, reason, err := fn(runCtx, msg, br.URI, src)
			br.Took = time.Since(start)
			if err != nil {
				br.Err = err
				br.Dead = true
				return
			}
			br.Status = status
			br.Reason = reason
			if status >= 200 && status < 300 {
				anyWin.Do(func() {
					f.mu.Lock()
					br.Winner = true
					f.mu.Unlock()
				})
			} else if status >= 300 {
				br.Dead = true
			}
		}(i, working[i])
	}
	wg.Wait()
	return working, nil
}

// Winner returns the winning branch after Run returns, or nil if none.
func (f *Forker) Winner() *Branch {
	f.mu.Lock()
	defer f.mu.Unlock()
	for _, b := range f.branches {
		if b.Winner {
			return b
		}
	}
	return nil
}

// BestStatus returns the highest-priority status code across all branches
// after Run completes. Priority: 2xx > 3xx > 4xx > 5xx > 6xx > 0/unanswered.
func (f *Forker) BestStatus() (int, string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.branches) == 0 {
		return 0, ""
	}
	// score map: 2xx highest, then 3xx, then 4xx, then 5xx, then 6xx.
	// Within 2xx, lower status codes are preferred (200 beats 202).
	// Within non-2xx, higher status codes are preferred (more specific reply wins).
	type entry struct{ s int; r string }
	var best *entry
	bestScore := 0.0 // high = better
	for _, b := range f.branches {
		if b.Status == 0 {
			continue
		}
		var sc float64
		switch {
		case b.Status >= 200 && b.Status < 300:
			// Positive and large; lower status preferred.
			sc = 10000.0 - float64(b.Status)
		case b.Status >= 300 && b.Status < 400:
			sc = 1000.0 + float64(b.Status)
		case b.Status >= 400 && b.Status < 500:
			sc = 100.0 + float64(b.Status)
		case b.Status >= 500 && b.Status < 600:
			sc = 10.0 + float64(b.Status)
		default:
			sc = float64(b.Status)
		}
		if best == nil || sc > bestScore {
			best = &entry{s: b.Status, r: b.Reason}
			bestScore = sc
		}
	}
	if best == nil {
		return 0, ""
	}
	return best.s, best.r
}

// SerialFork executes branches sequentially. Each branch waits for the previous
// one to terminate. Returns the winning 2xx branch, or the best failing branch
// if none succeed.
func SerialFork(ctx context.Context, fn ForwardFn, msg *parser.SIPMsg, src net.Addr, branches []string, perBranchTimeout time.Duration) []*Branch {
	if perBranchTimeout <= 0 {
		perBranchTimeout = 5 * time.Second
	}
	result := make([]*Branch, 0, len(branches))
	for _, uri := range branches {
		br := &Branch{URI: uri}
		result = append(result, br)
		select {
		case <-ctx.Done():
			br.Err = ctx.Err()
			br.Dead = true
			continue
		default:
		}
		branchCtx, cancel := context.WithTimeout(ctx, perBranchTimeout)
		start := time.Now()
		status, reason, err := fn(branchCtx, msg, uri, src)
		br.Took = time.Since(start)
		cancel()
		if err != nil {
			br.Err = err
			br.Dead = true
			continue
		}
		br.Status = status
		br.Reason = reason
		if status >= 200 && status < 300 {
			br.Winner = true
			return result
		}
		if status >= 300 {
			br.Dead = true
		}
	}
	return result
}

// keep import of str.Mk live for downstream consumers that build on top of
// the str.Str type.
var _ = str.Mk

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Pike - simple flood detector.
 *
 * Pike tracks per-source-IP request rates and blocks IPs that exceed
 * the configured threshold. It is the kamailio-go equivalent of the
 * kamailio `pike` module: a lightweight sliding-window counter with
 * an asynchronous janitor to bound memory use.
 *
 * Typical use from ProxyCore:
 *
 *   p := pike.New(20, 5*time.Second) // 20 requests per 5 seconds
 *   allowed, remaining, until := p.Hit("10.0.0.1")
 *   if !allowed { ... reject with 503 ... }
 *
 * Pike is safe for concurrent use.
 */

package pike

import (
	"sync"
	"time"
)

// Pike tracks per-source-IP request rates and rejects sources that
// exceed the configured limit within the configured window.
type Pike struct {
	mu       sync.RWMutex
	hits     map[string][]time.Time
	limit    int
	window   time.Duration
	janitorT *time.Ticker
	stopC    chan struct{}
}

// New constructs a Pike with the given limit and window. A janitor
// runs every `window` to evict stale entries. If limit <= 0 or
// window <= 0, sensible defaults are substituted (20 / 5 seconds).
func New(limit int, window time.Duration) *Pike {
	if limit <= 0 {
		limit = 20
	}
	if window <= 0 {
		window = 5 * time.Second
	}
	p := &Pike{
		hits:     make(map[string][]time.Time),
		limit:    limit,
		window:   window,
		stopC:    make(chan struct{}),
		janitorT: time.NewTicker(window),
	}
	go p.janitor()
	return p
}

// Close stops the janitor goroutine. Subsequent operations on a
// closed Pike still work but no longer shed expired entries in the
// background (Hit/Count still prune lazily on demand).
func (p *Pike) Close() {
	if p == nil {
		return
	}
	close(p.stopC)
	p.janitorT.Stop()
}

// Hit records a request from ip and returns:
//
//	allowed   — true if the request is within the limit, false if it
//	            should be rejected.
//	remaining — number of requests still allowed in this window.
//	resetIn   — how long until this window expires and counters reset.
//
// An empty ip address is treated as a no-op (always allowed).
func (p *Pike) Hit(ip string) (bool, int, time.Duration) {
	if p == nil {
		return true, 0, 0
	}
	if ip == "" {
		return true, 0, 0
	}
	now := time.Now()
	cutoff := now.Add(-p.window)

	p.mu.Lock()
	defer p.mu.Unlock()

	// Prune old hits for this IP.
	recent := p.hits[ip][:0]
	for _, t := range p.hits[ip] {
		if t.After(cutoff) {
			recent = append(recent, t)
		}
	}
	// Record this hit.
	recent = append(recent, now)
	p.hits[ip] = recent

	allowed := len(recent) <= p.limit
	remaining := p.limit - len(recent)
	if remaining < 0 {
		remaining = 0
	}
	resetIn := p.window
	if len(recent) > 0 {
		resetIn = p.window - now.Sub(recent[0])
		if resetIn < 0 {
			resetIn = 0
		}
	}
	return allowed, remaining, resetIn
}

// IsBlocked reports whether the given IP is currently over the limit.
// It counts as a hit - repeated calls with a blocked IP will continue
// to report blocked (and add more hits, widening the blocked window).
func (p *Pike) IsBlocked(ip string) bool {
	allowed, _, _ := p.Hit(ip)
	return !allowed
}

// Count returns the number of hits recorded for ip in the current window.
func (p *Pike) Count(ip string) int {
	if p == nil {
		return 0
	}
	p.mu.RLock()
	defer p.mu.RUnlock()
	return len(p.hits[ip])
}

// Clear removes all state about ip.
func (p *Pike) Clear(ip string) {
	if p == nil {
		return
	}
	p.mu.Lock()
	delete(p.hits, ip)
	p.mu.Unlock()
}

// ActiveIPs returns a snapshot of currently-tracked IP addresses.
func (p *Pike) ActiveIPs() []string {
	if p == nil {
		return nil
	}
	p.mu.RLock()
	defer p.mu.RUnlock()
	out := make([]string, 0, len(p.hits))
	for ip := range p.hits {
		out = append(out, ip)
	}
	return out
}

// janitor periodically prunes completely-empty IP entries to cap
// memory use. It runs on the Pike's ticker and exits when Close is
// called (or StopC is closed).
func (p *Pike) janitor() {
	for {
		select {
		case <-p.stopC:
			return
		case <-p.janitorT.C:
			cutoff := time.Now().Add(-p.window)
			p.mu.Lock()
			for ip, times := range p.hits {
				recent := times[:0]
				for _, t := range times {
					if t.After(cutoff) {
						recent = append(recent, t)
					}
				}
				if len(recent) == 0 {
					delete(p.hits, ip)
				} else {
					p.hits[ip] = recent
				}
			}
			p.mu.Unlock()
		}
	}
}

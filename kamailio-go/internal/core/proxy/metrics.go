// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Proxy metrics - lightweight counters and latency accumulators.
 *
 * Exposes:
 *   - Total requests / replies / error counts
 *   - Per-method counters
 *   - Per-method latency accumulators (used for averages on snapshot)
 *
 * The goal is to provide enough information to drive health checks and
 * monitoring dashboards without pulling in an external metrics library.
 */

package proxy

import (
	"sync"
	"sync/atomic"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// Metrics accumulates counters and latency data. It is safe for
// concurrent use - readers should call Snapshot().
type Metrics struct {
	requests      atomic.Uint64
	replies       atomic.Uint64
	errors        atomic.Uint64
	methodCounts  sync.Map // map[parser.RequestMethod]*uint64
	methodLatency sync.Map // map[parser.RequestMethod]*time.Duration
	startedAt     time.Time
}

func newMetrics() *Metrics {
	return &Metrics{startedAt: time.Now()}
}

// incRequest increments the incoming request counter.
func (m *Metrics) incRequest() { m.requests.Add(1) }

// incReply increments the incoming reply counter.
func (m *Metrics) incReply() { m.replies.Add(1) }

// incError increments the error counter with the given status code.
// The status code is currently recorded but not indexed separately.
func (m *Metrics) incError(code int) {
	m.errors.Add(1)
	_ = code
}

// recordLatency records that a request of the given method completed in
// duration d. Counts are kept per method for average latency calculations.
func (m *Metrics) recordLatency(method parser.RequestMethod, d time.Duration) {
	raw, _ := m.methodCounts.LoadOrStore(method, new(uint64))
	if counter, ok := raw.(*uint64); ok {
		atomic.AddUint64(counter, 1)
	}

	raw2, _ := m.methodLatency.LoadOrStore(method, new(time.Duration))
	if total, ok := raw2.(*time.Duration); ok {
		// Accumulate durations atomically by treating the value as an
		// int64. We use compare-and-swap to guarantee no lost updates.
		for {
			old := atomic.LoadInt64((*int64)(total))
			newVal := old + int64(d)
			if atomic.CompareAndSwapInt64((*int64)(total), old, newVal) {
				break
			}
		}
	}
}

// --------------------------------------------------------------------
// Snapshot
// --------------------------------------------------------------------

// Snapshot is a stable, point-in-time copy of the metrics.
type Snapshot struct {
	Requests      uint64
	Replies       uint64
	Errors        uint64
	Uptime        time.Duration
	ByMethod      map[string]uint64
	AvgByMethod   map[string]time.Duration
}

// Snapshot returns a stable view of the current metrics values.
func (m *Metrics) Snapshot() *Snapshot {
	snap := &Snapshot{
		Requests:    m.requests.Load(),
		Replies:     m.replies.Load(),
		Errors:      m.errors.Load(),
		Uptime:      time.Since(m.startedAt),
		ByMethod:    make(map[string]uint64),
		AvgByMethod: make(map[string]time.Duration),
	}

	m.methodCounts.Range(func(key, value interface{}) bool {
		method, ok := key.(parser.RequestMethod)
		if !ok {
			return true
		}
		countPtr, ok := value.(*uint64)
		if !ok {
			return true
		}
		count := atomic.LoadUint64(countPtr)

		name := parser.MethodName(method)
		snap.ByMethod[name] = count

		if lat, ok := m.methodLatency.Load(method); ok {
			if total, ok := lat.(*time.Duration); ok && count > 0 {
				d := time.Duration(atomic.LoadInt64((*int64)(total)))
				snap.AvgByMethod[name] = d / time.Duration(count)
			}
		}
		return true
	})

	return snap
}

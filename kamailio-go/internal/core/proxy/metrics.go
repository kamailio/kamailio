// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Proxy metrics - lightweight counters and latency accumulators.
 *
 * Exposes:
 *   - Total requests / replies / error counts
 *   - Per-method counters and per-method response/error buckets
 *   - Per-status-class buckets (1xx / 2xx / 3xx / 4xx / 5xx / 6xx)
 *   - Average response latency across all methods
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

// MethodStats tracks counters for a single SIP method.
type MethodStats struct {
	Requests  uint64
	Responses uint64
	Success   uint64
	Error     uint64
	Redirect  uint64
}

// Metrics accumulates counters and latency data. It is safe for
// concurrent use - readers should call Snapshot().
type Metrics struct {
	mu             sync.RWMutex
	startedAt      time.Time
	totalRequests  atomic.Uint64
	totalResponses atomic.Uint64
	totalErrors    atomic.Uint64
	totalSuccess   atomic.Uint64
	latencySumNs   atomic.Uint64
	latencySamples atomic.Uint64

	byMethod map[string]*MethodStats
	byStatus map[int]uint64 // keyed by status / 100 (i.e. 1..6)
}

func newMetrics() *Metrics {
	return &Metrics{
		startedAt: time.Now(),
		byMethod:  make(map[string]*MethodStats),
		byStatus:  make(map[int]uint64),
	}
}

// incRequest increments the incoming request counter and the per-method
// request counter.
func (m *Metrics) incRequest() { m.countRequest(parser.MethodUndefined) }

// countRequest increments the counters for the given SIP method.
func (m *Metrics) countRequest(method parser.RequestMethod) {
	m.totalRequests.Add(1)

	name := parser.MethodName(method)
	m.mu.Lock()
	stat, ok := m.byMethod[name]
	if !ok {
		stat = &MethodStats{}
		m.byMethod[name] = stat
	}
	stat.Requests++
	m.mu.Unlock()
}

// incReply increments the incoming reply counter. It is kept for
// backward compatibility and is equivalent to countResponse with a
// zero status.
func (m *Metrics) incReply() { m.totalResponses.Add(1) }

// incError increments the error counter with the given status code.
// The status code is used to classify the reply by bucket in addition
// to incrementing the global error counter.
func (m *Metrics) incError(code int) {
	m.totalErrors.Add(1)
	if code <= 0 {
		return
	}
	m.mu.Lock()
	m.byStatus[code/100]++
	m.mu.Unlock()
}

// countResponse increments the reply counters for the given method and
// classifies the reply status into the 1xx/2xx/3xx/4xx/5xx/6xx
// histogram and per-method success/error/redirect counters.
func (m *Metrics) countResponse(method parser.RequestMethod, status int) {
	if status <= 0 {
		m.totalResponses.Add(1)
		return
	}
	m.totalResponses.Add(1)

	bucket := status / 100
	switch bucket {
	case 2:
		m.totalSuccess.Add(1)
	case 4, 5, 6:
		m.totalErrors.Add(1)
	}
	m.mu.Lock()
	m.byStatus[bucket]++
	name := parser.MethodName(method)
	stat, ok := m.byMethod[name]
	if !ok {
		stat = &MethodStats{}
		m.byMethod[name] = stat
	}
	stat.Responses++
	switch bucket {
	case 2:
		stat.Success++
	case 3:
		stat.Redirect++
	case 4, 5, 6:
		stat.Error++
	}
	m.mu.Unlock()
}

// recordLatency records that a request completed in duration d.
// Latencies are accumulated as nanoseconds so the overall average can
// be computed on demand.
func (m *Metrics) recordLatency(d time.Duration) {
	if d <= 0 {
		return
	}
	m.latencySumNs.Add(uint64(d.Nanoseconds()))
	m.latencySamples.Add(1)
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
	ByMethodStats map[string]*MethodStats

	// Status-class histogram.
	Response1xx uint64
	Response2xx uint64
	Response3xx uint64
	Response4xx uint64
	Response5xx uint64
	Response6xx uint64

	// Aggregate summary.
	TotalRequests  uint64
	TotalResponses uint64
	TotalErrors    uint64
	TotalSuccess   uint64
	AvgLatencyMs   float64
}

// Snapshot returns a stable view of the current metrics values.
func (m *Metrics) Snapshot() *Snapshot {
	sumNs := m.latencySumNs.Load()
	samples := m.latencySamples.Load()
	var avgMs float64
	if samples > 0 {
		avgMs = float64(sumNs) / float64(samples) / 1e6
	}

	m.mu.RLock()
	byMethod := make(map[string]*MethodStats, len(m.byMethod))
	byReq := make(map[string]uint64, len(m.byMethod))
	byAvg := make(map[string]time.Duration, len(m.byMethod))
	for k, v := range m.byMethod {
		byMethod[k] = &MethodStats{
			Requests:  v.Requests,
			Responses: v.Responses,
			Success:   v.Success,
			Error:     v.Error,
			Redirect:  v.Redirect,
		}
		byReq[k] = v.Requests
	}
	r1xx := m.byStatus[1]
	r2xx := m.byStatus[2]
	r3xx := m.byStatus[3]
	r4xx := m.byStatus[4]
	r5xx := m.byStatus[5]
	r6xx := m.byStatus[6]
	m.mu.RUnlock()

	return &Snapshot{
		Requests:      m.totalRequests.Load(),
		Replies:       m.totalResponses.Load(),
		Errors:        m.totalErrors.Load(),
		Uptime:        time.Since(m.startedAt),
		ByMethod:      byReq,
		AvgByMethod:   byAvg,
		ByMethodStats: byMethod,
		Response1xx:   r1xx,
		Response2xx:   r2xx,
		Response3xx:   r3xx,
		Response4xx:   r4xx,
		Response5xx:   r5xx,
		Response6xx:   r6xx,
		TotalRequests: m.totalRequests.Load(),
		TotalResponses: m.totalResponses.Load(),
		TotalErrors:   m.totalErrors.Load(),
		TotalSuccess:  m.totalSuccess.Load(),
		AvgLatencyMs:  avgMs,
	}
}

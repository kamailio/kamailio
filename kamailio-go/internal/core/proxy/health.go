// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Health / readiness / status HTTP server.
 *
 * Exposes lightweight endpoints decoupled from the SIP signalling
 * path:
 *   - GET /healthz  -> liveness probe (always 200 while the process
 *                      is running)
 *   - GET /ready    -> readiness probe (200 once the proxy core is
 *                      wired up and serving)
 *   - GET /status   -> detailed JSON snapshot of configuration,
 *                      metrics, and registered listeners
 *
 * The endpoints intentionally do not touch SIP socket state, so a
 * stuck signalling path cannot block monitoring traffic.
 */

package proxy

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"sync"
	"time"
)

// HealthStatus enumerates the possible values returned by health endpoints.
type HealthStatus string

const (
	HealthStatusHealthy  HealthStatus = "healthy"
	HealthStatusDegraded HealthStatus = "degraded"
	HealthStatusUnhealthy HealthStatus = "unhealthy"
)

// HealthResponse is the JSON payload returned by /healthz and /ready.
type HealthResponse struct {
	Status    HealthStatus `json:"status"`
	Timestamp string       `json:"timestamp"`
	Uptime    string       `json:"uptime"`
}

// StatusResponse is the detailed JSON payload returned by /status.
type StatusResponse struct {
	Status    HealthStatus       `json:"status"`
	Timestamp string             `json:"timestamp"`
	Uptime    string             `json:"uptime"`
	Config    *StatusConfigInfo  `json:"config"`
	Metrics   *StatusMetricsInfo `json:"metrics"`
	Listeners []string           `json:"listeners"`
}

// StatusConfigInfo captures static configuration highlights.
type StatusConfigInfo struct {
	Realm               string `json:"realm"`
	AuthRequired        bool   `json:"auth_required"`
	NATDetectionEnabled bool   `json:"nat_detection_enabled"`
	MediaProxyEnabled   bool   `json:"media_proxy_enabled"`
	PresenceEnabled     bool   `json:"presence_enabled"`
	RecordRouteEnabled  bool   `json:"record_route_enabled"`
}

// StatusMetricsInfo captures dynamic metrics.
type StatusMetricsInfo struct {
	Requests     uint64             `json:"requests"`
	Replies      uint64             `json:"replies"`
	Errors       uint64             `json:"errors"`
	ByMethod     map[string]uint64  `json:"by_method,omitempty"`
	AvgLatencyMs map[string]float64 `json:"avg_latency_ms,omitempty"`
}

// HealthServer exposes HTTP endpoints for monitoring.
type HealthServer struct {
	mu       sync.RWMutex
	cfg      *ProxyConfig
	core     *ProxyCore
	server   *http.Server
	listener net.Listener
	started  time.Time
	running  bool
}

// NewHealthServer creates a health server for a proxy core.
func NewHealthServer(cfg *ProxyConfig, core *ProxyCore) *HealthServer {
	if cfg == nil {
		cfg = &ProxyConfig{}
	}
	return &HealthServer{
		cfg:     cfg,
		core:    core,
		started: time.Now(),
	}
}

// ListenAndServe starts the HTTP listener on the given address.
// It blocks until the server is shut down; call in a goroutine for
// background execution.
func (h *HealthServer) ListenAndServe(addr string) error {
	h.mu.Lock()
	if h.running {
		h.mu.Unlock()
		return fmt.Errorf("health server already running")
	}
	h.running = true
	h.mu.Unlock()

	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", h.handleHealth)
	mux.HandleFunc("/ready", h.handleReady)
	mux.HandleFunc("/status", h.handleStatus)
	mux.HandleFunc("/metrics", h.handleStatus) // alias for /status
	mux.HandleFunc("/", h.handleHealth)

	ln, err := net.Listen("tcp", addr)
	if err != nil {
		h.mu.Lock()
		h.running = false
		h.mu.Unlock()
		return err
	}

	h.mu.Lock()
	h.listener = ln
	h.mu.Unlock()

	h.server = &http.Server{
		Addr:         addr,
		Handler:      mux,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 5 * time.Second,
	}
	return h.server.Serve(ln)
}

// Addr returns the listener's address or the configured address.
func (h *HealthServer) Addr() string {
	h.mu.RLock()
	defer h.mu.RUnlock()
	if h.listener != nil {
		return h.listener.Addr().String()
	}
	return ""
}

// Shutdown gracefully shuts down the HTTP listener.
func (h *HealthServer) Shutdown(ctx context.Context) error {
	h.mu.Lock()
	if !h.running {
		h.mu.Unlock()
		return nil
	}
	h.running = false
	server := h.server
	h.mu.Unlock()

	if server == nil {
		return nil
	}
	return server.Shutdown(ctx)
}

func (h *HealthServer) handleHealth(w http.ResponseWriter, r *http.Request) {
	resp := &HealthResponse{
		Status:    HealthStatusHealthy,
		Timestamp: time.Now().UTC().Format(time.RFC3339),
		Uptime:    time.Since(h.started).Round(time.Second).String(),
	}
	writeJSON(w, http.StatusOK, resp)
}

func (h *HealthServer) handleReady(w http.ResponseWriter, r *http.Request) {
	// A simple readiness criterion: the server is running and has a core.
	h.mu.RLock()
	ready := h.running && h.core != nil
	h.mu.RUnlock()

	if !ready {
		writeJSON(w, http.StatusServiceUnavailable, &HealthResponse{
			Status:    HealthStatusUnhealthy,
			Timestamp: time.Now().UTC().Format(time.RFC3339),
			Uptime:    time.Since(h.started).Round(time.Second).String(),
		})
		return
	}
	writeJSON(w, http.StatusOK, &HealthResponse{
		Status:    HealthStatusHealthy,
		Timestamp: time.Now().UTC().Format(time.RFC3339),
		Uptime:    time.Since(h.started).Round(time.Second).String(),
	})
}

func (h *HealthServer) handleStatus(w http.ResponseWriter, r *http.Request) {
	var byMethod map[string]uint64
	var avgLatencyMs map[string]float64
	requests := uint64(0)
	replies := uint64(0)
	errors := uint64(0)

	if h.core != nil {
		metrics := h.core.Metrics()
		if metrics != nil {
			snap := metrics.Snapshot()
			requests = snap.Requests
			replies = snap.Replies
			errors = snap.Errors
			byMethod = snap.ByMethod
			// Convert avg latency to ms
			if len(snap.AvgByMethod) > 0 {
				avgLatencyMs = make(map[string]float64, len(snap.AvgByMethod))
				for m, d := range snap.AvgByMethod {
					avgLatencyMs[m] = float64(d) / float64(time.Millisecond)
				}
			}
		}
	}

	cfg := h.cfg
	if cfg == nil {
		cfg = &ProxyConfig{}
	}

	resp := &StatusResponse{
		Status:    HealthStatusHealthy,
		Timestamp: time.Now().UTC().Format(time.RFC3339),
		Uptime:    time.Since(h.started).Round(time.Second).String(),
		Config: &StatusConfigInfo{
			Realm:               cfg.Realm,
			AuthRequired:        cfg.AuthRequired,
			NATDetectionEnabled: cfg.NATDetectionEnabled,
			MediaProxyEnabled:   cfg.MediaProxyEnabled,
			PresenceEnabled:     cfg.PresenceEnabled,
			RecordRouteEnabled:  cfg.RecordRouteEnabled,
		},
		Metrics: &StatusMetricsInfo{
			Requests:     requests,
			Replies:      replies,
			Errors:       errors,
			ByMethod:     byMethod,
			AvgLatencyMs: avgLatencyMs,
		},
		Listeners: []string{},
	}

	if h.core != nil {
		for _, l := range h.core.listenerAddrs() {
			if l != nil {
				resp.Listeners = append(resp.Listeners, l.AddrString())
			}
		}
	}

	writeJSON(w, http.StatusOK, resp)
}

func writeJSON(w http.ResponseWriter, status int, v interface{}) {
	b, err := json.Marshal(v)
	if err != nil {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprintf(w, `{"error":"%s"}`, err.Error())
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_, _ = w.Write(b)
}

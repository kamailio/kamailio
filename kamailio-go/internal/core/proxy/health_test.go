// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for the health / readiness / status HTTP server.
 */

package proxy

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"testing"
	"time"
)

func TestHealthServer_StartAndShutdown(t *testing.T) {
	cfg := &ProxyConfig{Realm: "test.local"}
	core := NewProxyCore(cfg)
	hs := NewHealthServer(cfg, core)

	// Start on a random port
	addr := "127.0.0.1:0"
	errCh := make(chan error, 1)
	go func() {
		errCh <- hs.ListenAndServe(addr)
	}()

	// Wait a brief moment for listener to come up
	time.Sleep(50 * time.Millisecond)

	// Now grab the actual address
	actual := hs.Addr()
	if actual == "" {
		t.Fatal("expected non-empty listener address")
	}

	// Test /healthz
	resp, err := http.Get("http://" + actual + "/healthz")
	if err != nil {
		t.Fatalf("http get: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Errorf("healthz status = %d, want %d", resp.StatusCode, http.StatusOK)
	}
	var hr HealthResponse
	body, _ := io.ReadAll(resp.Body)
	if err := json.Unmarshal(body, &hr); err != nil {
		t.Fatalf("parse health response: %v", err)
	}
	if hr.Status != HealthStatusHealthy {
		t.Errorf("status = %q, want %q", hr.Status, HealthStatusHealthy)
	}
	if hr.Timestamp == "" {
		t.Error("expected non-empty timestamp")
	}

	// Test /ready
	resp2, err := http.Get("http://" + actual + "/ready")
	if err != nil {
		t.Fatalf("http get ready: %v", err)
	}
	resp2.Body.Close()
	if resp2.StatusCode != http.StatusOK {
		t.Errorf("ready status = %d", resp2.StatusCode)
	}

	// Test /status
	resp3, err := http.Get("http://" + actual + "/status")
	if err != nil {
		t.Fatalf("http get status: %v", err)
	}
	defer resp3.Body.Close()
	var sr StatusResponse
	body3, _ := io.ReadAll(resp3.Body)
	if err := json.Unmarshal(body3, &sr); err != nil {
		t.Fatalf("parse status response: %v", err)
	}
	if sr.Config == nil || sr.Config.Realm != "test.local" {
		t.Errorf("realm = %q, want test.local", sr.Config.Realm)
	}
	if sr.Metrics == nil {
		t.Error("expected metrics in status response")
	}

	// Shutdown
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if err := hs.Shutdown(ctx); err != nil {
		t.Errorf("shutdown: %v", err)
	}

	select {
	case err := <-errCh:
		if err != nil && err != http.ErrServerClosed {
			t.Errorf("server exit: %v", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("server did not stop in time")
	}
}

func TestHealthServer_NilCoreStillResponds(t *testing.T) {
	hs := NewHealthServer(nil, nil)
	addr := "127.0.0.1:0"
	errCh := make(chan error, 1)
	go func() {
		errCh <- hs.ListenAndServe(addr)
	}()
	time.Sleep(50 * time.Millisecond)

	actual := hs.Addr()
	resp, err := http.Get("http://" + actual + "/healthz")
	if err != nil {
		t.Fatalf("http get: %v", err)
	}
	resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Errorf("status = %d", resp.StatusCode)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	hs.Shutdown(ctx)
	<-errCh
}

func TestHealthServer_StartTwice(t *testing.T) {
	hs := NewHealthServer(nil, NewProxyCore(nil))
	addr := "127.0.0.1:0"
	go hs.ListenAndServe(addr)
	time.Sleep(50 * time.Millisecond)

	// Second start should fail
	err := hs.ListenAndServe(addr)
	if err == nil {
		t.Error("expected error when starting twice")
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	hs.Shutdown(ctx)
}

func TestHealthServer_StatusAfterRequests(t *testing.T) {
	cfg := &ProxyConfig{Realm: "counter-test"}
	core := NewProxyCore(cfg)
	hs := NewHealthServer(cfg, core)

	addr := "127.0.0.1:0"
	go hs.ListenAndServe(addr)
	time.Sleep(50 * time.Millisecond)
	actual := hs.Addr()

	// Fake 10 requests being counted
	for i := 0; i < 10; i++ {
		core.metrics.incRequest()
	}

	resp, err := http.Get("http://" + actual + "/status")
	if err != nil {
		t.Fatalf("http get: %v", err)
	}
	defer resp.Body.Close()
	var sr StatusResponse
	body, _ := io.ReadAll(resp.Body)
	if err := json.Unmarshal(body, &sr); err != nil {
		t.Fatalf("parse: %v", err)
	}
	if sr.Metrics.Requests != 10 {
		t.Errorf("requests = %d, want 10", sr.Metrics.Requests)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	hs.Shutdown(ctx)
}

// ensure the proxy core listener adapter implements listenerAddr
func TestListenerAddr_ImplementsInterface(t *testing.T) {
	var _ listenerAddr = (*UDPListenerAdapter)(nil)
	var _ listenerAddr = (*TCPListenerAdapter)(nil)
}

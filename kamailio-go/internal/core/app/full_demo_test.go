// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Smoke-tests for the full-stack launcher. Covers:
 *   - NewFullStack / Close round-trip
 *   - default realm and subsystem wiring
 *   - optional RPC endpoint binding to a loopback port
 */

package app

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"testing"
	"time"
)

func TestFullStack_NewAndClose(t *testing.T) {
	fs, err := NewFullStack(FullStackConfig{})
	if err != nil {
		t.Fatalf("NewFullStack: %v", err)
	}
	if fs == nil {
		t.Fatalf("nil FullStack")
	}
	if fs.Proxy == nil {
		t.Fatalf("ProxyCore not wired")
	}
	if fs.Pike == nil {
		t.Fatalf("Pike not wired")
	}
	if fs.HTables == nil {
		t.Fatalf("HTables not wired")
	}
	if fs.Msilo == nil {
		t.Fatalf("Msilo not wired")
	}
	// proxy setters should have been applied.
	if fs.Proxy.HTables() == nil {
		t.Fatalf("ProxyCore.HTables() is nil")
	}
	if fs.Proxy.Msilo() == nil {
		t.Fatalf("ProxyCore.Msilo() is nil")
	}
	if err := fs.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
}

func TestFullStack_RPCEndpoint(t *testing.T) {
	// Let the kernel allocate a free port.
	fs, err := NewFullStack(FullStackConfig{
		Realm:       "test.local",
		RPCEndpoint: "127.0.0.1:0",
	})
	if err != nil {
		t.Fatalf("NewFullStack: %v", err)
	}
	defer fs.Close()

	// Give the listener a moment to bind.
	var addr string
	for attempt := 0; attempt < 50; attempt++ {
		if a := fs.RPC.ListenerAddr(); a != "" {
			addr = a
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	if addr == "" {
		t.Fatalf("RPC endpoint never bound; got addr=%q", addr)
	}

	body, err := json.Marshal(map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      1,
		"method":  "kamailio.status",
	})
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	url := fmt.Sprintf("http://%s/rpc", addr)
	resp, err := http.Post(url, "application/json", bytes.NewReader(body))
	if err != nil {
		t.Fatalf("POST: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("status: got %d", resp.StatusCode)
	}
	out, _ := io.ReadAll(resp.Body)
	var decoded map[string]interface{}
	if err := json.Unmarshal(out, &decoded); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if fmt.Sprint(decoded["jsonrpc"]) != "2.0" {
		t.Fatalf("jsonrpc: %v", decoded["jsonrpc"])
	}
	if _, ok := decoded["result"]; !ok {
		t.Fatalf("missing result: %v", decoded)
	}
}

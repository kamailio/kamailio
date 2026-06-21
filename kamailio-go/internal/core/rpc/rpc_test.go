// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for the JSON-RPC HTTP endpoint.
 *
 * Each test wires a subset of subsystems into a Server and issues
 * HTTP calls against it via httptest.NewServer so the kernel
 * allocates a free port — nothing leaks between tests.
 */

package rpc

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/htable"
	"github.com/kamailio/kamailio-go/internal/core/msilo"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/pike"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
)

// rpcBody encodes a JSON-RPC 2.0 request.
func rpcBody(method string, params ...interface{}) io.Reader {
	body := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      1,
		"method":  method,
	}
	if len(params) > 0 {
		body["params"] = params
	}
	buf, _ := json.Marshal(body)
	return bytes.NewReader(buf)
}

// rpcCall performs a POST call to the test server's /rpc endpoint and
// returns the raw response body alongside the HTTP status.
func rpcCall(ts *httptest.Server, method string, params ...interface{}) (int, []byte) {
	resp, err := http.Post(ts.URL+"/rpc", "application/json", rpcBody(method, params...))
	if err != nil {
		return 0, nil
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	return resp.StatusCode, body
}

// mustParseRPC decodes a JSON-RPC 2.0 response from body.
func mustParseRPC(t *testing.T, body []byte) (map[string]interface{}, interface{}) {
	t.Helper()
	var out map[string]interface{}
	if err := json.Unmarshal(body, &out); err != nil {
		t.Fatalf("json unmarshal: %v", err)
	}
	if v, ok := out["jsonrpc"]; !ok || fmt.Sprint(v) != "2.0" {
		t.Fatalf("expected jsonrpc=2.0, got %v", v)
	}
	return out, out["result"]
}

// mustParseRPCError returns the error object of a JSON-RPC response.
func mustParseRPCError(t *testing.T, body []byte) map[string]interface{} {
	t.Helper()
	var out map[string]interface{}
	if err := json.Unmarshal(body, &out); err != nil {
		t.Fatalf("json unmarshal: %v", err)
	}
	e, ok := out["error"].(map[string]interface{})
	if !ok {
		t.Fatalf("expected error object, got %T", out["error"])
	}
	return e
}

// ------------------------------------------------------------------
// Test 1: /rpc → kamailio.ping
// ------------------------------------------------------------------

func TestRPC_Ping(t *testing.T) {
	s := New(nil, nil, nil, nil)
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	status, body := rpcCall(ts, "kamailio.ping")
	if status != http.StatusOK {
		t.Fatalf("status: got %d want %d", status, http.StatusOK)
	}
	_, result := mustParseRPC(t, body)
	res, ok := result.(map[string]interface{})
	if !ok {
		t.Fatalf("result is not map: %T", result)
	}
	if pong, _ := res["pong"].(bool); !pong {
		t.Fatalf("pong: got %v want true", pong)
	}
	if at, _ := res["at"].(string); at == "" {
		t.Fatalf("at is empty")
	}
}

// ------------------------------------------------------------------
// Test 2: pike status + clear
// ------------------------------------------------------------------

func TestRPC_PikeStatusAndClear(t *testing.T) {
	pk := pike.New(3, 5*time.Second)
	// Generate some hits so ActiveIPs returns a non-empty list.
	for i := 0; i < 2; i++ {
		pk.Hit("10.0.0.1")
		pk.Hit("10.0.0.2")
	}
	defer pk.Close()

	s := New(nil, pk, nil, nil)
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	status, body := rpcCall(ts, "kamailio.pike.status")
	if status != http.StatusOK {
		t.Fatalf("status: got %d", status)
	}
	_, result := mustParseRPC(t, body)
	res := result.(map[string]interface{})
	count, _ := res["count"].(float64)
	if int(count) < 2 {
		t.Fatalf("active count: got %v want >=2", count)
	}

	// Clear one IP, then the rest.
	_, body = rpcCall(ts, "kamailio.pike.clear", "10.0.0.1")
	_, result = mustParseRPC(t, body)
	cres := result.(map[string]interface{})
	if cres["cleared"] != "10.0.0.1" {
		t.Fatalf("cleared: got %v", cres["cleared"])
	}
	_, body = rpcCall(ts, "kamailio.pike.clear")
	_, result = mustParseRPC(t, body)
	cres = result.(map[string]interface{})
	if cres["cleared"] != "all" {
		t.Fatalf("cleared all: got %v", cres["cleared"])
	}
	if len(pk.ActiveIPs()) != 0 {
		t.Fatalf("after clear, pike still has %d IPs", len(pk.ActiveIPs()))
	}
}

// ------------------------------------------------------------------
// Test 3: htable set / get round-trip
// ------------------------------------------------------------------

func TestRPC_HTableSetGet(t *testing.T) {
	hm := htable.NewManager()
	s := New(nil, nil, hm, nil)
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	// set a key
	status, body := rpcCall(ts, "kamailio.htable.set", "subscribers", "alice", "active")
	if status != http.StatusOK {
		t.Fatalf("set status: got %d", status)
	}
	_, result := mustParseRPC(t, body)
	setRes := result.(map[string]interface{})
	if setRes["value"] != "active" {
		t.Fatalf("set value: got %v", setRes["value"])
	}

	// get the key back
	_, body = rpcCall(ts, "kamailio.htable.get", "subscribers", "alice")
	_, result = mustParseRPC(t, body)
	getRes := result.(map[string]interface{})
	if !getRes["found"].(bool) {
		t.Fatalf("found: got false want true")
	}
	if getRes["value"] != "active" {
		t.Fatalf("value mismatch: got %v", getRes["value"])
	}

	// missing key
	_, body = rpcCall(ts, "kamailio.htable.get", "subscribers", "bob")
	_, result = mustParseRPC(t, body)
	getRes = result.(map[string]interface{})
	if getRes["found"].(bool) {
		t.Fatalf("found: got true want false for missing key")
	}

	// list tables
	_, body = rpcCall(ts, "kamailio.htable.list")
	_, result = mustParseRPC(t, body)
	listRes := result.(map[string]interface{})
	tables, _ := listRes["tables"].([]interface{})
	found := false
	for _, n := range tables {
		if n == "subscribers" {
			found = true
		}
	}
	if !found {
		t.Fatalf("list tables missing subscribers, got %v", tables)
	}
}

// ------------------------------------------------------------------
// Test 4: msilo queued counts
// ------------------------------------------------------------------

func buildMessageFor(t *testing.T, user string) *parser.SIPMsg {
	t.Helper()
	raw := "MESSAGE sip:" + user + "@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-" + user + "\r\n" +
		"To: <sip:" + user + "@example.com>\r\n" +
		"From: <sip:bot@example.com>;tag=bot-" + user + "\r\n" +
		"Call-ID: rpc-" + user + "\r\n" +
		"CSeq: 1 MESSAGE\r\n" +
		"Content-Length: 7\r\n" +
		"\r\n" +
		"hello\r\n"
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	return msg
}

func TestRPC_MsiloQueued(t *testing.T) {
	ms := msilo.New(nil, "msilo")
	if _, err := ms.Store(buildMessageFor(t, "alice")); err != nil {
		t.Fatalf("store alice: %v", err)
	}
	if _, err := ms.Store(buildMessageFor(t, "alice")); err != nil {
		t.Fatalf("store alice 2: %v", err)
	}
	if _, err := ms.Store(buildMessageFor(t, "bob")); err != nil {
		t.Fatalf("store bob: %v", err)
	}

	s := New(nil, nil, nil, ms)
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	_, body := rpcCall(ts, "kamailio.msilo.queued")
	_, result := mustParseRPC(t, body)
	res := result.(map[string]interface{})
	if int(res["total"].(float64)) != 3 {
		t.Fatalf("total queued: got %v want 3", res["total"])
	}

	_, body = rpcCall(ts, "kamailio.msilo.queued", "alice")
	_, result = mustParseRPC(t, body)
	res = result.(map[string]interface{})
	if int(res["queued"].(float64)) != 2 {
		t.Fatalf("alice queued: got %v want 2", res["queued"])
	}

	_, body = rpcCall(ts, "kamailio.msilo.queued", "bob")
	_, result = mustParseRPC(t, body)
	res = result.(map[string]interface{})
	if int(res["queued"].(float64)) != 1 {
		t.Fatalf("bob queued: got %v want 1", res["queued"])
	}

	_, body = rpcCall(ts, "kamailio.msilo.queued", "carol")
	_, result = mustParseRPC(t, body)
	res = result.(map[string]interface{})
	if int(res["queued"].(float64)) != 0 {
		t.Fatalf("carol queued: got %v want 0", res["queued"])
	}
}

// ------------------------------------------------------------------
// Test 5: unknown method returns JSON-RPC error structure
// ------------------------------------------------------------------

func TestRPC_MethodNotFound(t *testing.T) {
	s := New(nil, nil, nil, nil)
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	status, body := rpcCall(ts, "kamailio.nonexistent")
	if status != http.StatusOK {
		// The error is still conveyed inside a 200 response body —
		// that is the JSON-RPC 2.0 convention.
		t.Fatalf("status: got %d want 200 (JSON-RPC errors are in-body)", status)
	}
	errObj := mustParseRPCError(t, body)
	code, _ := errObj["code"].(float64)
	if int(code) != ErrMethod {
		t.Fatalf("error code: got %d want %d", int(code), ErrMethod)
	}
	msg, _ := errObj["message"].(string)
	if msg == "" {
		t.Fatalf("error message empty")
	}
}

// ------------------------------------------------------------------
// Test 6: kamailio.stats returns proxy metrics snapshot
// ------------------------------------------------------------------

func TestRPC_Stats(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "test.local"})
	s := NewExtended(ServerConfig{Core: pc})
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	status, body := rpcCall(ts, "kamailio.stats")
	if status != http.StatusOK {
		t.Fatalf("status: got %d", status)
	}
	_, result := mustParseRPC(t, body)
	res, ok := result.(map[string]interface{})
	if !ok {
		t.Fatalf("result is not map: %T", result)
	}
	if ok, _ := res["ok"].(bool); !ok {
		t.Fatalf("ok: got %v want true", res["ok"])
	}
	metrics, ok := res["metrics"].(map[string]interface{})
	if !ok {
		t.Fatalf("metrics missing or wrong type: %T", res["metrics"])
	}
	if _, hasTotal := metrics["TotalRequests"]; !hasTotal {
		t.Fatalf("metrics.TotalRequests missing: got %v", metrics)
	}
	if _, has := metrics["Uptime"]; !has {
		t.Fatalf("metrics.Uptime missing: got %v", metrics)
	}
}

// ------------------------------------------------------------------
// Test 7: kamailio.dialog.list lists dialogs added to the dialog manager
// ------------------------------------------------------------------

func TestRPC_DialogList(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "test.local"})
	dm := dialog.NewManager()
	pc.SetDialogs(dm)

	// Use the manager's Add method to manually create a couple of tracked
	// dialogs (short dialogs) for listing.
	d1, err := dialog.CreateUASDialog(buildInviteMsg(t, "alice", "bob", "call-1"), "")
	if err != nil {
		t.Fatalf("create dialog 1: %v", err)
	}
	if err := dm.Add(d1); err != nil {
		t.Fatalf("add dialog 1: %v", err)
	}
	d2, err := dialog.CreateUASDialog(buildInviteMsg(t, "alice", "bob", "call-2"), "")
	if err != nil {
		t.Fatalf("create dialog 2: %v", err)
	}
	if err := dm.Add(d2); err != nil {
		t.Fatalf("add dialog 2: %v", err)
	}

	s := NewExtended(ServerConfig{Core: pc, Dialogs: dm})
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	status, body := rpcCall(ts, "kamailio.dialog.list")
	if status != http.StatusOK {
		t.Fatalf("status: got %d", status)
	}
	_, result := mustParseRPC(t, body)
	res := result.(map[string]interface{})
	count, _ := res["count"].(float64)
	if int(count) != 2 {
		t.Fatalf("count: got %v want 2", count)
	}
	list, _ := res["dialogs"].([]interface{})
	if len(list) != 2 {
		t.Fatalf("dialogs length: got %d want 2", len(list))
	}
	// Ensure each entry has call_id and state fields.
	for i, entry := range list {
		m, ok := entry.(map[string]interface{})
		if !ok {
			t.Fatalf("dialogs[%d] not map: %T", i, entry)
		}
		if _, has := m["call_id"]; !has {
			t.Fatalf("dialogs[%d] missing call_id: %v", i, m)
		}
		if _, has := m["state"]; !has {
			t.Fatalf("dialogs[%d] missing state: %v", i, m)
		}
	}
}

func buildInviteMsg(t *testing.T, from, to, callID string) *parser.SIPMsg {
	t.Helper()
	raw := "INVITE sip:" + to + "@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-" + callID + "\r\n" +
		"From: <sip:" + from + "@example.com>;tag=tag-" + from + "\r\n" +
		"To: <sip:" + to + "@example.com>\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:" + from + "@10.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	return msg
}

// ------------------------------------------------------------------
// Test 8: kamailio.script.reload parses + installs a script
// ------------------------------------------------------------------

func TestRPC_ScriptReload(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "test.local"})
	s := NewExtended(ServerConfig{Core: pc})
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	scriptText := "request_route { xlog(\"reloaded\"); }"
	_, body := rpcCall(ts, "kamailio.script.reload", scriptText)
	_, result := mustParseRPC(t, body)
	res := result.(map[string]interface{})
	if ok, _ := res["ok"].(bool); !ok {
		t.Fatalf("ok: got false want true, result=%v", res)
	}
	if errMsg, _ := res["error"].(string); errMsg != "" {
		t.Fatalf("error: got %q want empty", errMsg)
	}

	// Error path: invalid script text
	_, body = rpcCall(ts, "kamailio.script.reload", "this is not a valid script")
	_, result = mustParseRPC(t, body)
	res = result.(map[string]interface{})
	if ok, _ := res["ok"].(bool); ok {
		t.Fatalf("ok: got true want false for malformed script")
	}
}

// ------------------------------------------------------------------
// Test 9: kamailio.shutdown returns ok
// ------------------------------------------------------------------

func TestRPC_Shutdown(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "test.local"})
	s := NewExtended(ServerConfig{Core: pc})
	ts := httptest.NewServer(s.handler)
	defer ts.Close()

	_, body := rpcCall(ts, "kamailio.shutdown")
	_, result := mustParseRPC(t, body)
	res := result.(map[string]interface{})
	if ok, _ := res["ok"].(bool); !ok {
		t.Fatalf("ok: got false want true, result=%v", res)
	}
}

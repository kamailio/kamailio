package app

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"testing"
	"time"
)

// TestBootstrap_NoFlags_Defaults verifies NewBootstrap works with an
// empty options struct and the resulting Bootstrap shuts down cleanly.
func TestBootstrap_NoFlags_Defaults(t *testing.T) {
	boot, err := NewBootstrap(BootstrapOptions{})
	if err != nil {
		t.Fatalf("NewBootstrap failed: %v", err)
	}
	if boot == nil {
		t.Fatal("NewBootstrap returned nil Bootstrap")
	}
	if boot.ProxyCore == nil {
		t.Error("expected ProxyCore to be non-nil")
	}
	if boot.Script != nil {
		t.Error("expected nil Script when no script file provided")
	}
	if boot.RPCServer != nil {
		t.Error("expected nil RPCServer when no rpc-addr provided")
	}
	boot.Shutdown()
}

// TestBootstrap_ScriptFileLoads writes a tiny routing script to a
// temporary file, asks NewBootstrap to load it and verifies the parsed
// script ends up in Bootstrap.Script.
func TestBootstrap_ScriptFileLoads(t *testing.T) {
	tmp, err := os.CreateTemp("", "kamailio-*.cfg")
	if err != nil {
		t.Fatalf("CreateTemp: %v", err)
	}
	defer os.Remove(tmp.Name())

	content := `request_route { sl_send_reply("200", "OK"); }`
	if _, err := tmp.WriteString(content); err != nil {
		t.Fatalf("WriteString: %v", err)
	}
	tmp.Close()

	boot, err := NewBootstrap(BootstrapOptions{ScriptFile: tmp.Name()})
	if err != nil {
		t.Fatalf("NewBootstrap failed: %v", err)
	}
	defer boot.Shutdown()
	if boot.Script == nil {
		t.Fatalf("expected parsed Script, got nil (file=%q)", tmp.Name())
	}
}

// TestBootstrap_RPCServerStarts configures an RPC endpoint on a random
// local port, waits briefly for the listener to come up and then hits
// it with a kamailio.ping request to confirm the JSON-RPC path works.
func TestBootstrap_RPCServerStarts(t *testing.T) {
	boot, err := NewBootstrap(BootstrapOptions{RPCAddr: "127.0.0.1:0"})
	if err != nil {
		t.Fatalf("NewBootstrap failed: %v", err)
	}
	defer boot.Shutdown()

	if boot.RPCServer == nil {
		t.Fatal("expected non-nil RPCServer")
	}

	// Wait up to ~1 second for the listener to report an address.
	var addr string
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		addr = boot.RPCServer.Addr()
		if addr != "" {
			break
		}
		time.Sleep(20 * time.Millisecond)
	}
	if addr == "" {
		t.Fatal("RPC server did not report a listening address")
	}

	url := fmt.Sprintf("http://%s/rpc", addr)
	body := bytes.NewBufferString(`{"jsonrpc":"2.0","method":"kamailio.ping","id":1}`)

	resp, err := http.Post(url, "application/json", body)
	if err != nil {
		t.Fatalf("http.Post: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		b, _ := io.ReadAll(resp.Body)
		t.Fatalf("unexpected status %d: %s", resp.StatusCode, string(b))
	}

	var out map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
		t.Fatalf("decode json response: %v", err)
	}
	if out["jsonrpc"] != "2.0" {
		t.Errorf("expected jsonrpc version 2.0, got %v", out["jsonrpc"])
	}
	if _, ok := out["result"]; !ok {
		t.Errorf("expected result field in response, got %v", out)
	}
}

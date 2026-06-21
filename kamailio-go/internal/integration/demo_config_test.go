// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go — Phase 37 demo config integration tests.
 *
 * Verifies that configs/demo-full.cfg (a single-file demonstration
 * of every feature from Phase 27 through Phase 35) parses cleanly
 * through the script engine and actually routes requests when
 * executed.
 */

package integration

import (
	"os"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/script"
)

const demoConfigPath = "../../configs/demo-full.cfg"

// loadDemoConfig reads the demo-full.cfg file off disk.
func loadDemoConfig(t *testing.T) string {
	t.Helper()
	data, err := os.ReadFile(demoConfigPath)
	if err != nil {
		t.Fatalf("failed to read demo config %s: %v", demoConfigPath, err)
	}
	if len(data) == 0 {
		t.Fatalf("demo config %s is empty", demoConfigPath)
	}
	return string(data)
}

// buildDemoInviteMsg constructs a minimal INVITE for script execution.
// Kept separate from the forward_phase6_test.go helper to avoid
// signature / symbol clashes across the integration package.
func buildDemoInviteMsg(t *testing.T) *parser.SIPMsg {
	t.Helper()
	raw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKdemo1\r\n" +
			"From: <sip:alice@example.com>;tag=demo-alice\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: demo-invite-1@example.com\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n\r\n",
	)
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("failed to parse INVITE fixture: %v", err)
	}
	return msg
}

// TestDemoConfig_Parse loads configs/demo-full.cfg and asserts it
// parses without error, producing a non-nil Script with at least
// the named routes referenced by the request_route classifier.
func TestDemoConfig_Parse(t *testing.T) {
	src := loadDemoConfig(t)
	sc, err := script.Parse(src)
	if err != nil {
		t.Fatalf("script.Parse(demo-full.cfg) returned error: %v", err)
	}
	if sc == nil {
		t.Fatalf("script.Parse(demo-full.cfg) returned nil Script")
	}
	if len(sc.Root) == 0 {
		t.Errorf("expected non-empty request_route block, got 0 actions")
	}

	requiredRoutes := []string{"RELAY", "REGISTRAR", "MSILO", "CDR_CLOSE"}
	for _, name := range requiredRoutes {
		if _, ok := sc.Routes[name]; !ok {
			t.Errorf("demo config missing expected named route %q", name)
		}
	}
}

// TestDemoConfig_Exec feeds an INVITE through the parsed demo config
// and asserts the RELAY branch actually produced side effects that
// the proxy core would interpret as "forward this request".
func TestDemoConfig_Exec(t *testing.T) {
	src := loadDemoConfig(t)
	sc, err := script.Parse(src)
	if err != nil {
		t.Fatalf("script.Parse(demo-full.cfg) returned error: %v", err)
	}
	if sc == nil {
		t.Fatalf("script.Parse(demo-full.cfg) returned nil Script")
	}

	msg := buildDemoInviteMsg(t)
	ctx := script.NewExecContext(msg, nil, "example.com")
	if err := sc.Execute(ctx); err != nil {
		t.Fatalf("sc.Execute returned error: %v", err)
	}

	if ctx.DstURI == "" && ctx.Reply == nil {
		t.Fatalf("INVITE execution produced neither DstURI nor Reply — " +
			"the RELAY route did not run")
	}
	if ctx.DstURI == "" {
		t.Fatalf("expected $du to be set by route[RELAY], got empty DstURI")
	}
	if want := "sip:relay.example.com:5060"; ctx.DstURI != want {
		t.Errorf("expected DstURI = %q, got %q", want, ctx.DstURI)
	}
	if len(ctx.Branches) == 0 {
		t.Errorf("expected at least one parallel fork branch from route[RELAY]")
	}
}

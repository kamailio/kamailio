package integration

import (
	"fmt"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/app"
	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/presence"
)

func TestMultiModule_AuthChallenge(t *testing.T) {
	cfg := config.DefaultConfig()
	engine := app.NewEngine(cfg)
	engine.Start()
	defer engine.Stop()

	// Message without authorization must trigger a challenge.
	registerMsg, _ := parser.ParseMsg([]byte(
		"REGISTER sip:example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKauth1\r\n" +
			"From: <sip:alice@example.com>;tag=alice-challenge\r\n" +
			"To: <sip:alice@example.com>\r\n" +
			"Call-ID: auth-test@example.com\r\n" +
			"CSeq: 1 REGISTER\r\n" +
			"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
			"Content-Length: 0\r\n\r\n",
	))

	code, body := engine.ProcessRegister(registerMsg, "10.0.0.1:5060")
	if code != 401 {
		t.Errorf("expected 401 challenge, got %d", code)
	}
	if body == "" {
		t.Error("expected non-empty WWW-Authenticate body")
	}

	// Second registration WITH auth should succeed. The Engine treats any
	// Authorization header as valid; the actual digest verification is
	// delegated to the auth package tests.
	authorizedMsg, _ := parser.ParseMsg([]byte(
		"REGISTER sip:example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKauth2\r\n" +
			"From: <sip:alice@example.com>;tag=alice-challenge\r\n" +
			"To: <sip:alice@example.com>\r\n" +
			"Call-ID: auth-test-2@example.com\r\n" +
			"CSeq: 2 REGISTER\r\n" +
			"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
			"Authorization: Digest username=\"alice\", realm=\"test\", nonce=\"abc\", uri=\"sip:example.com\", response=\"xyz\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	))

	code2, _ := engine.ProcessRegister(authorizedMsg, "10.0.0.1:5060")
	if code2 != 200 {
		t.Errorf("expected 200 OK for authorized REGISTER, got %d", code2)
	}

	stats := engine.Stats()
	if stats.Requests < 2 {
		t.Errorf("requests = %d, want >=2", stats.Requests)
	}
}

func TestMultiModule_DialogTracking(t *testing.T) {
	cfg := config.DefaultConfig()
	engine := app.NewEngine(cfg)
	engine.Start()
	defer engine.Stop()

	// Alice calls Bob.
	invite, _ := parser.ParseMsg([]byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKdlg1\r\n" +
			"From: <sip:alice@example.com>;tag=alice-dlg1\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: dialog-track-1@example.com\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
			"Proxy-Authorization: Digest username=\"alice\", realm=\"t\", nonce=\"n\", uri=\"sip:bob@example.com\", response=\"r\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	))
	engine.ProcessInvite(invite, "10.0.0.1:5060")

	// Alice calls Charlie (second dialog).
	invite2, _ := parser.ParseMsg([]byte(
		"INVITE sip:charlie@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKdlg2\r\n" +
			"From: <sip:alice@example.com>;tag=alice-dlg2\r\n" +
			"To: <sip:charlie@example.com>\r\n" +
			"Call-ID: dialog-track-2@example.com\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
			"Proxy-Authorization: Digest username=\"alice\", realm=\"t\", nonce=\"n\", uri=\"sip:charlie@example.com\", response=\"r\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	))
	engine.ProcessInvite(invite2, "10.0.0.1:5060")

	stats := engine.Stats()
	if stats.Dialogs < 2 {
		t.Errorf("dialogs = %d, want >=2 (two concurrent dialogs)", stats.Dialogs)
	}

	// Hang up first call.
	bye, _ := parser.ParseMsg([]byte(
		"BYE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKbye1\r\n" +
			"From: <sip:alice@example.com>;tag=alice-dlg1\r\n" +
			"To: <sip:bob@example.com>;tag=remote-bob\r\n" +
			"Call-ID: dialog-track-1@example.com\r\n" +
			"CSeq: 2 BYE\r\n" +
			"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
			"Content-Length: 0\r\n\r\n",
	))
	code, _ := engine.ProcessBye(bye, "10.0.0.1:5060")
	if code != 200 {
		t.Errorf("BYE code = %d, want 200", code)
	}
}

func TestMultiModule_PresenceStateCycle(t *testing.T) {
	cfg := config.DefaultConfig()
	engine := app.NewEngine(cfg)
	engine.Start()
	defer engine.Stop()

	// Test state transitions: open -> busy -> away -> closed.
	uri := "sip:alice@example.com"
	contact := "sip:alice@10.0.0.1:5060"

	states := []presence.PresenceState{
		presence.PresenceStateOpen,
		presence.PresenceStateBusy,
		presence.PresenceStateAway,
		presence.PresenceStateClosed,
	}
	notes := []string{"Available", "On call", "Away", "Offline"}

	for i, state := range states {
		// Re-use the same entity tag so each Publish replaces the prior
		// record for that entity. Using a fresh entity per iteration would
		// leave multiple records at the same priority (the server picks the
		// highest-priority record, not the most recent), which makes the
		// final state non-deterministic.
		engine.Presence().Publish(uri, state, notes[i], contact, "entity-cycle", 3600*time.Second)
		if engine.Presence().GetState(uri) != state {
			t.Errorf("iteration %d: got state %v, want %v", i, engine.Presence().GetState(uri), state)
		}

		doc := engine.Presence().GetStateDocument(uri)
		if doc == "" {
			t.Errorf("iteration %d: empty presence document", i)
		}
	}
}

func TestMultiModule_StatsReporting(t *testing.T) {
	bm := app.NewBootManager()
	bm.LoadConfig("")
	engine, err := bm.Boot()
	if err != nil {
		t.Fatalf("Boot: %v", err)
	}

	// Send a mix of REGISTER/INVITE/SUBSCRIBE to exercise counters.
	for i := 0; i < 5; i++ {
		regMsg, _ := parser.ParseMsg([]byte(
			"REGISTER sip:example.com SIP/2.0\r\n" +
				"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKstats" + fmt.Sprintf("%d", i) + "\r\n" +
				"From: <sip:user" + fmt.Sprintf("%d", i) + "@example.com>;tag=s" + fmt.Sprintf("%d", i) + "\r\n" +
				"To: <sip:user" + fmt.Sprintf("%d", i) + "@example.com>\r\n" +
				"Call-ID: statscall" + fmt.Sprintf("%d", i) + "@example.com\r\n" +
				"CSeq: 1 REGISTER\r\n" +
				"Contact: <sip:user" + fmt.Sprintf("%d", i) + "@10.0.0.1:5060>\r\n" +
				"Authorization: Digest username=\"u\", realm=\"t\", nonce=\"n\", uri=\"sip:example.com\", response=\"r\"\r\n" +
				"Content-Length: 0\r\n\r\n",
		))
		engine.ProcessRegister(regMsg, "10.0.0.1:5060")
	}

	// Presence publishes.
	engine.Presence().Publish("sip:alice@example.com", presence.PresenceStateOpen,
		"Hi", "sip:alice@10.0.0.1:5060", "e1", 3600*time.Second)
	engine.Presence().Publish("sip:bob@example.com", presence.PresenceStateAway,
		"AFK", "sip:bob@10.0.0.2:5060", "e2", 3600*time.Second)

	// Subscribe to one.
	engine.Presence().Subscribe("sip:alice@example.com", "sip:bob@example.com", "presence", 3600*time.Second)
	engine.Presence().Subscribe("sip:alice@example.com", "sip:charlie@example.com", "presence", 3600*time.Second)

	stats := engine.Stats()

	// Verify all modules produced counts.
	if stats.Requests < 5 {
		t.Errorf("requests = %d, want >=5", stats.Requests)
	}
	if stats.Contacts < 2 {
		t.Errorf("contacts = %d, want >=2", stats.Contacts)
	}
	if stats.Presentities < 2 {
		t.Errorf("presentities = %d, want >=2", stats.Presentities)
	}
	if stats.Subscriptions < 2 {
		t.Errorf("subscriptions = %d, want >=2", stats.Subscriptions)
	}
	if stats.State != "running" {
		t.Errorf("state = %q", stats.State)
	}

	// Shutdown and verify final state.
	bm.Shutdown()
	stats = engine.Stats()
	if stats.State != "stopped" {
		t.Errorf("final state = %q", stats.State)
	}
}

func TestMultiModule_InvalidEventRejected(t *testing.T) {
	cfg := config.DefaultConfig()
	engine := app.NewEngine(cfg)
	engine.Start()
	defer engine.Stop()

	subMsg, _ := parser.ParseMsg([]byte(
		"SUBSCRIBE sip:alice@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.2:5060;branch=z9hG4bKbad1\r\n" +
			"From: <sip:bob@example.com>;tag=bob-bad\r\n" +
			"To: <sip:alice@example.com>\r\n" +
			"Call-ID: bad-event@example.com\r\n" +
			"CSeq: 1 SUBSCRIBE\r\n" +
			"Event: unsupported-event\r\n" +
			"Contact: <sip:bob@10.0.0.2:5060>\r\n" +
			"Authorization: Digest username=\"bob\", realm=\"t\", nonce=\"n\", uri=\"sip:alice@example.com\", response=\"r\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	))

	code, _, _ := engine.ProcessSubscribe(subMsg, "10.0.0.2:5060")
	// The event package is not registered so it returns 489.
	if code != 489 {
		t.Errorf("expected 489 Bad Event for unsupported event package, got %d", code)
	}
}

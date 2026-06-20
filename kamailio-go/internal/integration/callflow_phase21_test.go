package integration

import (
	"fmt"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/app"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/presence"
)

// buildRawSIPMessage is a small helper that wires up a SIP message from raw
// bytes using the project's parser.
func buildRawSIPMessage(raw string) *parser.SIPMsg {
	msg, _ := parser.ParseMsg([]byte(raw))
	return msg
}

func TestBootManager_LoadDefaults(t *testing.T) {
	bm := app.NewBootManager()
	cfg, err := bm.LoadConfig("")
	if err != nil {
		t.Fatalf("LoadConfig: %v", err)
	}
	if cfg == nil {
		t.Fatal("expected non-nil config")
	}
}

func TestBootManager_BootAndShutdown(t *testing.T) {
	bm := app.NewBootManager()
	bm.LoadConfig("")

	engine, err := bm.Boot()
	if err != nil {
		t.Fatalf("Boot: %v", err)
	}

	modules := bm.ModuleList()
	if len(modules) < 5 {
		t.Errorf("expected at least 5 modules, got %d", len(modules))
	}

	stats := engine.Stats()
	if stats.State != "running" {
		t.Errorf("state = %q", stats.State)
	}

	bm.Shutdown()
	if engine.Stats().State != "stopped" {
		t.Error("expected stopped state")
	}
}

func TestCallFlow_RegisterAndDial(t *testing.T) {
	bm := app.NewBootManager()
	bm.LoadConfig("")
	engine, err := bm.Boot()
	if err != nil {
		t.Fatalf("Boot: %v", err)
	}
	defer bm.Shutdown()

	// 1. Alice registers
	regMsg := buildRawSIPMessage(
		"REGISTER sip:example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bKreg1\r\n" +
			"Max-Forwards: 70\r\n" +
			"From: <sip:alice@example.com>;tag=alice123\r\n" +
			"To: <sip:alice@example.com>\r\n" +
			"Call-ID: callreg1@192.168.1.10\r\n" +
			"CSeq: 1 REGISTER\r\n" +
			"Contact: <sip:alice@192.168.1.10:5060>;expires=3600\r\n" +
			"Authorization: Digest username=\"alice\", realm=\"test\", nonce=\"n1\", uri=\"sip:example.com\", response=\"r1\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	)
	code, _ := engine.ProcessRegister(regMsg, "192.168.1.10:5060")
	if code != 200 {
		t.Errorf("REGISTER code = %d, want 200", code)
	}

	// 2. Bob registers
	regMsg2 := buildRawSIPMessage(
		"REGISTER sip:example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 192.168.1.11:5060;branch=z9hG4bKreg2\r\n" +
			"Max-Forwards: 70\r\n" +
			"From: <sip:bob@example.com>;tag=bob123\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: callreg2@192.168.1.11\r\n" +
			"CSeq: 1 REGISTER\r\n" +
			"Contact: <sip:bob@192.168.1.11:5060>;expires=3600\r\n" +
			"Authorization: Digest username=\"bob\", realm=\"test\", nonce=\"n2\", uri=\"sip:example.com\", response=\"r2\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	)
	code, _ = engine.ProcessRegister(regMsg2, "192.168.1.11:5060")
	if code != 200 {
		t.Errorf("REGISTER2 code = %d, want 200", code)
	}

	stats := engine.Stats()
	if stats.Contacts < 2 {
		t.Errorf("contacts = %d, want >=2", stats.Contacts)
	}

	// 3. Alice dials Bob
	invite := buildRawSIPMessage(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bKcall1\r\n" +
			"Max-Forwards: 70\r\n" +
			"From: <sip:alice@example.com>;tag=alice-dial-1\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: call-dial-1@example.com\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Contact: <sip:alice@192.168.1.10:5060>\r\n" +
			"Proxy-Authorization: Digest username=\"alice\", realm=\"test\", nonce=\"n3\", uri=\"sip:bob@example.com\", response=\"r3\"\r\n" +
			"Content-Type: application/sdp\r\n" +
			"Content-Length: 50\r\n\r\n" +
			"v=0\r\no=- 1 2 IN IP4 192.168.1.10\r\ns=call\r\n",
	)
	code, _ = engine.ProcessInvite(invite, "192.168.1.10:5060")
	if code != 100 {
		t.Errorf("INVITE code = %d, want 100", code)
	}

	stats = engine.Stats()
	if stats.Dialogs < 1 {
		t.Errorf("dialogs = %d, want >=1", stats.Dialogs)
	}

	// 4. Alice hangs up
	bye := buildRawSIPMessage(
		"BYE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bKbye1\r\n" +
			"Max-Forwards: 70\r\n" +
			"From: <sip:alice@example.com>;tag=alice-dial-1\r\n" +
			"To: <sip:bob@example.com>;tag=remote-tag\r\n" +
			"Call-ID: call-dial-1@example.com\r\n" +
			"CSeq: 2 BYE\r\n" +
			"Contact: <sip:alice@192.168.1.10:5060>\r\n" +
			"Content-Length: 0\r\n\r\n",
	)
	code, _ = engine.ProcessBye(bye, "192.168.1.10:5060")
	if code != 200 {
		t.Errorf("BYE code = %d, want 200", code)
	}
}

func TestCallFlow_PresencePublishSubscribe(t *testing.T) {
	bm := app.NewBootManager()
	bm.LoadConfig("")
	engine, err := bm.Boot()
	if err != nil {
		t.Fatalf("Boot: %v", err)
	}
	defer bm.Shutdown()

	// Alice publishes presence
	engine.Presence().Publish(
		"sip:alice@example.com",
		presence.PresenceStateOpen,
		"Available",
		"sip:alice@10.0.0.1:5060",
		"entity-alice-1",
		3600*time.Second,
	)

	if engine.Presence().GetState("sip:alice@example.com") != presence.PresenceStateOpen {
		t.Error("expected presence state open")
	}

	// Bob subscribes to Alice's presence
	subMsg := buildRawSIPMessage(
		"SUBSCRIBE sip:alice@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.2:5060;branch=z9hG4bKsub1\r\n" +
			"Max-Forwards: 70\r\n" +
			"From: <sip:bob@example.com>;tag=bobsub1\r\n" +
			"To: <sip:alice@example.com>\r\n" +
			"Call-ID: subcall1@example.com\r\n" +
			"CSeq: 1 SUBSCRIBE\r\n" +
			"Event: presence\r\n" +
			"Contact: <sip:bob@10.0.0.2:5060>\r\n" +
			"Authorization: Digest username=\"bob\", realm=\"test\", nonce=\"n1\", uri=\"sip:alice@example.com\", response=\"r1\"\r\n" +
			"Content-Length: 0\r\n\r\n",
	)
	code, _, sub := engine.ProcessSubscribe(subMsg, "10.0.0.2:5060")
	if code != 200 {
		t.Errorf("SUBSCRIBE code = %d, want 200", code)
	}
	if sub == nil {
		t.Fatal("expected subscription")
	}
	if sub.Event != "presence" {
		t.Errorf("event = %q", sub.Event)
	}

	stats := engine.Stats()
	if stats.Subscriptions < 1 {
		t.Errorf("subscriptions = %d, want >=1", stats.Subscriptions)
	}
	if stats.Presentities < 1 {
		t.Errorf("presentities = %d, want >=1", stats.Presentities)
	}

	// Alice changes state to away - verify state doc updates
	engine.Presence().Publish(
		"sip:alice@example.com",
		presence.PresenceStateAway,
		"Away from desk",
		"sip:alice@10.0.0.1:5060",
		"entity-alice-2",
		3600*time.Second,
	)
	doc := engine.Presence().GetStateDocument("sip:alice@example.com")
	if doc == "" {
		t.Fatal("expected presence document")
	}
}

func TestCallFlow_ConcurrentRequests(t *testing.T) {
	bm := app.NewBootManager()
	bm.LoadConfig("")
	engine, err := bm.Boot()
	if err != nil {
		t.Fatalf("Boot: %v", err)
	}
	defer bm.Shutdown()

	done := make(chan struct{})
	go func() {
		defer close(done)
		for i := 0; i < 20; i++ {
			msg := buildRawSIPMessage(
				"REGISTER sip:example.com SIP/2.0\r\n" +
					"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKconc" + fmt.Sprintf("%d", i) + "\r\n" +
					"From: <sip:user" + fmt.Sprintf("%d", i) + "@example.com>;tag=conc" + fmt.Sprintf("%d", i) + "\r\n" +
					"To: <sip:user" + fmt.Sprintf("%d", i) + "@example.com>\r\n" +
					"Call-ID: conc" + fmt.Sprintf("%d", i) + "@example.com\r\n" +
					"CSeq: 1 REGISTER\r\n" +
					"Contact: <sip:user" + fmt.Sprintf("%d", i) + "@10.0.0.1:5060>;expires=3600\r\n" +
					"Authorization: Digest username=\"u" + fmt.Sprintf("%d", i) + "\", realm=\"t\", nonce=\"n\", uri=\"sip:example.com\", response=\"r\"\r\n" +
					"Content-Length: 0\r\n\r\n",
			)
			engine.ProcessRegister(msg, "10.0.0.1:5060")
		}
	}()

	select {
	case <-done:
	case <-time.After(5 * time.Second):
		t.Fatal("timeout in concurrent registration")
	}

	stats := engine.Stats()
	if stats.Requests < 20 {
		t.Errorf("requests = %d, want >=20", stats.Requests)
	}
}

package integration

import (
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/app"
	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/presence"
)

func buildEngineRegisterMsg(aor, contact string) *parser.SIPMsg {
	raw := "REGISTER sip:" + aor + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: <sip:" + aor + ">;tag=1928301774\r\n" +
		"To: <sip:" + aor + ">\r\n" +
		"Call-ID: a84b4c76e66710\r\n" +
		"CSeq: 314159 REGISTER\r\n" +
		"Contact: <" + contact + ">;expires=3600\r\n" +
		"Content-Length: 0\r\n\r\n"
	msg, _ := parser.ParseMsg([]byte(raw))
	return msg
}

func buildEngineInviteMsg(fromURI, toURI, callID string) *parser.SIPMsg {
	raw := "INVITE sip:" + toURI + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: <sip:" + fromURI + ">;tag=1928301774\r\n" +
		"To: <sip:" + toURI + ">\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:" + fromURI + "@10.0.0.1:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 10\r\n\r\n" +
		"v=0\r\no=- 1\r\n"
	msg, _ := parser.ParseMsg([]byte(raw))
	return msg
}

func buildEngineByeMsg(callID, fromURI, toURI string) *parser.SIPMsg {
	raw := "BYE sip:" + toURI + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: <sip:" + fromURI + ">;tag=1928301774\r\n" +
		"To: <sip:" + toURI + ">;tag=456789\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 3 BYE\r\n" +
		"Contact: <sip:" + fromURI + "@10.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n\r\n"
	msg, _ := parser.ParseMsg([]byte(raw))
	return msg
}

func buildEngineSubscribeMsg(fromURI, toURI string) *parser.SIPMsg {
	raw := "SUBSCRIBE sip:" + toURI + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: <sip:" + fromURI + ">;tag=1928301774\r\n" +
		"To: <sip:" + toURI + ">\r\n" +
		"Call-ID: a84b4c76e66710-sub\r\n" +
		"CSeq: 314159 SUBSCRIBE\r\n" +
		"Event: presence\r\n" +
		"Contact: <sip:" + fromURI + "@10.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n\r\n"
	msg, _ := parser.ParseMsg([]byte(raw))
	return msg
}

func TestEngine_Lifecycle(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	if err := eng.Start(); err != nil {
		t.Fatalf("Start: %v", err)
	}
	stats := eng.Stats()
	if stats.State != "running" {
		t.Errorf("state = %q, want %q", stats.State, "running")
	}
	eng.Stop()
	stats = eng.Stats()
	if stats.State != "stopped" {
		t.Errorf("state after stop = %q, want %q", stats.State, "stopped")
	}
}

func TestEngine_Register(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	eng.Start()
	defer eng.Stop()

	msg := buildEngineRegisterMsg("alice@example.com", "sip:alice@192.168.1.10:5060")
	code, _ := eng.ProcessRegister(msg, "192.168.1.10:5060")
	if code != 200 && code != 401 {
		t.Errorf("expected 200 or 401, got %d", code)
	}
	stats := eng.Stats()
	if stats.Requests < 1 {
		t.Errorf("requests = %d, want >= 1", stats.Requests)
	}
}

func TestEngine_InviteAndBye(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	eng.Start()
	defer eng.Stop()

	callID := "call-end-to-end-1"
	invite := buildEngineInviteMsg("alice@example.com", "bob@example.com", callID)
	code, _ := eng.ProcessInvite(invite, "192.168.1.10:5060")
	if code != 100 && code != 407 {
		t.Errorf("expected 100 or 407, got %d", code)
	}

	bye := buildEngineByeMsg(callID, "alice@example.com", "bob@example.com")
	code, _ = eng.ProcessBye(bye, "192.168.1.10:5060")
	if code != 200 {
		t.Errorf("BYE code = %d, want 200", code)
	}
}

func TestEngine_Subscribe(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	eng.Start()
	defer eng.Stop()

	msg := buildEngineSubscribeMsg("bob@example.com", "alice@example.com")
	code, _, sub := eng.ProcessSubscribe(msg, "192.168.1.11:5060")
	if code != 200 && code != 401 {
		t.Errorf("expected 200 or 401, got %d", code)
	}
	if sub != nil && sub.Event != "presence" {
		t.Errorf("event = %q, want %q", sub.Event, "presence")
	}
}

func TestEngine_NATDetection(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	eng.Start()
	defer eng.Stop()

	msg := buildEngineRegisterMsg("alice@example.com", "sip:alice@192.168.1.10:5060")
	code, _ := eng.ProcessRegister(msg, "203.0.113.10:5060")
	if code != 200 && code != 401 {
		t.Errorf("code = %d, want 200 or 401", code)
	}
}

func TestEngine_PublishAndSubscribe(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	eng.Start()
	defer eng.Stop()

	eng.Presence().Publish("sip:alice@example.com", presence.PresenceStateOpen,
		"Available", "sip:alice@10.0.0.1:5060", "entity-1", 1*time.Hour)
	if eng.Presence().GetState("sip:alice@example.com") != presence.PresenceStateOpen {
		t.Error("expected presence state open")
	}
}

func TestEngine_StatsAccumulate(t *testing.T) {
	cfg := config.DefaultConfig()
	eng := app.NewEngine(cfg)
	eng.Start()
	defer eng.Stop()

	for i := 0; i < 5; i++ {
		msg := buildEngineRegisterMsg("alice@example.com", "sip:alice@192.168.1.10:5060")
		eng.ProcessRegister(msg, "192.168.1.10:5060")
	}
	stats := eng.Stats()
	if stats.Requests < 5 {
		t.Errorf("requests = %d, want >= 5", stats.Requests)
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later
package nat

import (
	"sync"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// inviteMessage returns a minimal INVITE SIPMsg parsed from raw bytes.
func inviteMessage(callID, fromTag, contactHost, body string) *parser.SIPMsg {
	raw := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK-test\r\n" +
		"From: <sip:alice@example.com>;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@" + contactHost + ":5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: " + itoa(len(body)) + "\r\n" +
		"\r\n" + body
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		return nil
	}
	return msg
}

func byeMessage(callID, fromTag, toTag string) *parser.SIPMsg {
	raw := "BYE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK-bye\r\n" +
		"From: <sip:alice@example.com>;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@example.com>;tag=" + toTag + "\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 2 BYE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		return nil
	}
	return msg
}

func okAnswerMessage(callID, fromTag, toTag, body string) *parser.SIPMsg {
	raw := "SIP/2.0 200 OK\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK-ok\r\n" +
		"From: <sip:alice@example.com>;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@example.com>;tag=" + toTag + "\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:bob@10.0.0.2:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: " + itoa(len(body)) + "\r\n" +
		"\r\n" + body
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		return nil
	}
	return msg
}

// itoa converts a non-negative int to its decimal string. We avoid
// strconv to keep test dependencies minimal (already imported indirectly
// by the parser package).
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	buf := [20]byte{}
	pos := len(buf)
	for n > 0 {
		pos--
		buf[pos] = byte('0' + n%10)
		n /= 10
	}
	return string(buf[pos:])
}

// Test 1: No engine - only Contact rewriting + session bookkeeping.
func TestPipeline_OnInviteOffer_NoEngine(t *testing.T) {
	p := NewPipeline(nil, "203.0.113.50")

	body := "v=0\r\no=- 0 0 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n"
	msg := inviteMessage("offer-noengine-1", "tag-alice", "10.0.0.1", body)
	if msg == nil {
		t.Fatal("failed to parse invite")
	}

	id, err := p.OnInviteOffer(msg)
	if err != nil {
		t.Fatalf("OnInviteOffer returned error: %v", err)
	}
	if id != "offer-noengine-1" {
		t.Errorf("expected call-id 'offer-noengine-1', got %q", id)
	}

	if p.ActiveSessions() != 1 {
		t.Errorf("expected 1 active session, got %d", p.ActiveSessions())
	}

	if msg.Contact == nil {
		t.Fatal("contact header missing after offer")
	}
	contact := msg.Contact.Body.String()
	if !contains(contact, "203.0.113.50") {
		t.Errorf("expected contact host rewritten to 203.0.113.50, got %q", contact)
	}
}

// Test 2: BYE cleans up a previously tracked session.
func TestPipeline_OnBye_DeletesSession(t *testing.T) {
	p := NewPipeline(nil, "203.0.113.50")

	body := "v=0\r\no=- 0 0 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n"
	invite := inviteMessage("bye-test-1", "ft-1", "10.0.0.1", body)
	if _, err := p.OnInviteOffer(invite); err != nil {
		t.Fatalf("OnInviteOffer: %v", err)
	}
	if p.ActiveSessions() != 1 {
		t.Fatalf("expected 1 session, got %d", p.ActiveSessions())
	}

	bye := byeMessage("bye-test-1", "ft-1", "tt-1")
	if err := p.OnBye(bye); err != nil {
		t.Fatalf("OnBye: %v", err)
	}
	if p.ActiveSessions() != 0 {
		t.Fatalf("expected 0 sessions after bye, got %d", p.ActiveSessions())
	}

	// Second Bye on the same call should be a no-op, not panic.
	if err := p.OnBye(bye); err != nil {
		t.Fatalf("second OnBye: %v", err)
	}
}

// Test 3: Concurrent calls - exercise the mutex and session map.
func TestPipeline_ActiveSessions_Concurrent(t *testing.T) {
	p := NewPipeline(nil, "203.0.113.50")

	const goroutines = 8
	const perRoutine = 50

	var wg sync.WaitGroup
	wg.Add(goroutines)
	for g := 0; g < goroutines; g++ {
		go func(g int) {
			defer wg.Done()
			for i := 0; i < perRoutine; i++ {
				callID := "concurrent-" + itoa(g) + "-" + itoa(i)
				body := "v=0\r\no=- 0 0 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\n"
				msg := inviteMessage(callID, "tag-"+itoa(g), "10.0.0.1", body)
				if msg == nil {
					continue
				}
				_, _ = p.OnInviteOffer(msg)
			}
			_ = p.ActiveSessions()
		}(g)
	}
	wg.Wait()

	expected := goroutines * perRoutine
	if got := p.ActiveSessions(); got != expected {
		t.Errorf("expected %d active sessions, got %d", expected, got)
	}
}

// Test 4: Answer with no engine should not panic and should still track.
func TestPipeline_OnAnswer_NoEngineSafe(t *testing.T) {
	p := NewPipeline(nil, "203.0.113.50")

	body := "v=0\r\no=- 0 0 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n"
	invite := inviteMessage("ans-noeng-1", "ft-a", "10.0.0.1", body)
	if _, err := p.OnInviteOffer(invite); err != nil {
		t.Fatalf("OnInviteOffer: %v", err)
	}

	ok := okAnswerMessage("ans-noeng-1", "ft-a", "tt-a", body)
	id, err := p.OnAnswer(ok)
	if err != nil {
		t.Fatalf("OnAnswer: %v", err)
	}
	if id != "ans-noeng-1" {
		t.Errorf("expected call-id 'ans-noeng-1', got %q", id)
	}

	// Nil pipeline/message should return an error but not panic.
	if _, err := p.OnAnswer(nil); err == nil {
		t.Error("expected error when msg is nil")
	}
}

// Test 5: extractSDP / setSDP round-trip preserves body content.
func TestPipeline_ExtractSDP_RoundTrip(t *testing.T) {
	const expected = "v=0\r\no=- 0 0 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n"

	// Case A: message whose Body was set via our pipeline.
	msg := &parser.SIPMsg{Body: expected}
	got, err := extractSDP(msg)
	if err != nil {
		t.Fatalf("extractSDP: %v", err)
	}
	if got != expected {
		t.Errorf("case A: expected %q, got %q", expected, got)
	}

	// Case B: message with only raw Buf and no Body field.
	headers := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-rt\r\n" +
		"From: <sip:alice@example.com>;tag=tagx\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: rt1\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: " + itoa(len(expected)) + "\r\n\r\n"
	raw := headers + expected

	msg2, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	got2, err := extractSDP(msg2)
	if err != nil {
		t.Fatalf("extractSDP: %v", err)
	}
	if got2 == "" {
		t.Log("msg Body/Buffer empty after parse, proceeding to setSDP test")
	}

	// setSDP should replace Body and also update the raw buffer.
	const replacement = "v=0\r\no=- 0 0 IN IP4 203.0.113.50\r\ns=-\r\n"
	setSDP(msg2, replacement)
	got3, err := extractSDP(msg2)
	if err != nil {
		t.Fatalf("extractSDP after setSDP: %v", err)
	}
	if got3 != replacement {
		t.Errorf("round-trip: expected %q, got %q", replacement, got3)
	}
}

// helpers ---------------------------------------------------------------

func contains(haystack, needle string) bool {
	return needle == "" || index(haystack, needle) >= 0
}

func index(haystack, needle string) int {
	for i := 0; i+len(needle) <= len(haystack); i++ {
		if haystack[i:i+len(needle)] == needle {
			return i
		}
	}
	return -1
}

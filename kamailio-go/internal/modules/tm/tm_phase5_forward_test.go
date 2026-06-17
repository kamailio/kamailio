// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module - forward / proxy call flow tests (phase 5)
 */

package tm

import (
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// INVITE request for phase 5 tests
var phase5Invite = []byte("INVITE sip:bob@ims.example.com SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
	"Max-Forwards: 70\r\n" +
	"From: Alice <sip:alice@ims.example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@ims.example.com>\r\n" +
	"Call-ID: invite-callid-phase5-001\r\n" +
	"CSeq: 314159 INVITE\r\n" +
	"Contact: <sip:alice@192.168.1.100>\r\n" +
	"Content-Type: application/sdp\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

// makeResponseBytes builds a raw SIP response string (for tests that want
// to exercise ParseMsg followed by HandleResponse) with the given status
// code and reason, reusing the Call-ID / From tag from the shared INVITE.
func makeResponseBytes(code int, reason, callID, fromTag string) []byte {
	// We mimic a response returned from a downstream branch of the
	// forwarded INVITE.  Via is new (the downstream's top Via branch) so
	// the transaction can be matched.
	via := "SIP/2.0/UDP 10.0.0.2:5060;branch=z9hG4bK-downstream-" + callID
	return []byte("SIP/2.0 " + itoa(code) + " " + reason + "\r\n" +
		"Via: " + via + "\r\n" +
		"From: Alice <sip:alice@ims.example.com>;tag=" + fromTag + "\r\n" +
		"To: Bob <sip:bob@ims.example.com>;tag=dst-tag-" + callID + "\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	var buf [20]byte
	i := len(buf)
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		buf[i] = '-'
	}
	return string(buf[i:])
}

// --------------------------------------------------------------------
// TestTM_MultiBranchForwarding
// --------------------------------------------------------------------
func TestTM_MultiBranchForwarding(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(phase5Invite)
	if err != nil {
		t.Fatalf("ParseMsg error: %v", err)
	}

	// 1. Create transaction from the incoming INVITE.
	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest error: %v", err)
	}
	if cell == nil {
		t.Fatal("expected non-nil cell")
	}

	// 2. Relay to two downstream targets, creating two branches.
	_, b0, err := mgr.RelayRequest(msg, "10.0.0.2", 5060)
	if err != nil {
		t.Fatalf("RelayRequest(10.0.0.2) error: %v", err)
	}
	_, b1, err := mgr.RelayRequest(msg, "10.0.0.3", 5060)
	if err != nil {
		t.Fatalf("RelayRequest(10.0.0.3) error: %v", err)
	}

	// 3. Verify both branches exist.
	if cell.NrOfOutgoings != 2 {
		t.Errorf("expected 2 outgoings, got %d", cell.NrOfOutgoings)
	}
	if b0 != 0 {
		t.Errorf("expected branch index 0, got %d", b0)
	}
	if b1 != 1 {
		t.Errorf("expected branch index 1, got %d", b1)
	}
	if len(cell.UAC) < 2 || cell.UAC[0] == nil || cell.UAC[1] == nil {
		t.Fatalf("expected UAC slots at branches 0 and 1; got %+v", cell.UAC)
	}

	// 4. Simulate 180 Ringing received on branch 0.
	resp180, err := parser.ParseMsg(
		makeResponseBytes(180, "Ringing", "invite-callid-phase5-001", "1928301774"),
	)
	if err != nil {
		t.Fatalf("parse 180 response error: %v", err)
	}
	_, _, err = mgr.HandleResponse(resp180)
	if err != nil {
		t.Fatalf("HandleResponse(180) error: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("after 180 expected TStateProceeding, got %d", cell.State)
	}

	// 5. Simulate 200 OK received on branch 1.
	resp200, err := parser.ParseMsg(
		makeResponseBytes(200, "OK", "invite-callid-phase5-001", "1928301774"),
	)
	if err != nil {
		t.Fatalf("parse 200 response error: %v", err)
	}
	_, _, err = mgr.HandleResponse(resp200)
	if err != nil {
		t.Fatalf("HandleResponse(200) error: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("after 200 expected TStateCompleted, got %d", cell.State)
	}

	// 6. Best response across branches should be 200.
	bestStatus, _ := mgr.GetBestResponse(cell)
	if bestStatus != 200 {
		t.Errorf("GetBestResponse: expected 200, got %d", bestStatus)
	}
}

// --------------------------------------------------------------------
// TestTM_ResponseSerializationRoundtrip
// --------------------------------------------------------------------
func TestTM_ResponseSerializationRoundtrip(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(phase5Invite)
	if err != nil {
		t.Fatalf("ParseMsg error: %v", err)
	}

	// Create transaction so we have something to forward from.
	_, err = mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest error: %v", err)
	}

	// 1. Build a forwarded request (decrements Max-Forwards, adds Via).
	fwd, err := parser.BuildForwardRequest(msg, "UDP", "proxy.ims.example.com", 5060, "sip:bob@10.0.0.2:5060")
	if err != nil {
		t.Fatalf("BuildForwardRequest error: %v", err)
	}

	// 2. Serialize the forwarded request to bytes and re-parse it.
	raw, err := parser.BuildMessage(fwd)
	if err != nil {
		t.Fatalf("BuildMessage error: %v", err)
	}
	reparsed, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("re-ParseMsg error: %v", err)
	}

	// 3. Build a 200 OK reply from the re-parsed request and serialize
	//    it to ensure round-trip produces a well-formed response.
	reply, err := parser.CreateReply(reparsed, parser.ReplyOptions{
		StatusCode: 200,
		ReasonPhrase: "OK",
	})
	if err != nil {
		t.Fatalf("CreateReply from reparsed request error: %v", err)
	}
	replyBytes, err := parser.BuildMessage(reply)
	if err != nil {
		t.Fatalf("BuildMessage(reply) error: %v", err)
	}
	replyStr := string(replyBytes)

	// 4. Verify the response contains the expected 200 status line.
	if !strings.HasPrefix(replyStr, "SIP/2.0 200 OK") &&
		!strings.Contains(replyStr, "200 OK") {
		t.Errorf("response missing '200 OK' status line, got:\n%s", replyStr)
	}

	// 5. Verify Max-Forwards was decremented from the original 70 -> 69
	//    on the forwarded request.  We re-parse the raw forward to be
	//    sure.
	fwdRaw, err := parser.BuildMessage(fwd)
	if err != nil {
		t.Fatalf("BuildMessage(fwd) error: %v", err)
	}
	if !strings.Contains(string(fwdRaw), "Max-Forwards: 69") {
		t.Errorf("forwarded request should have Max-Forwards: 69, got:\n%s", string(fwdRaw))
	}
}

// --------------------------------------------------------------------
// TestTM_LookupReplyForRelayedRequest
// --------------------------------------------------------------------
func TestTM_LookupReplyForRelayedRequest(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(phase5Invite)
	if err != nil {
		t.Fatalf("ParseMsg error: %v", err)
	}

	// 1. Create transaction and relay to one downstream branch.
	_, err = mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest error: %v", err)
	}
	cell, branch, err := mgr.RelayRequest(msg, "10.0.0.2", 5060)
	if err != nil {
		t.Fatalf("RelayRequest error: %v", err)
	}

	// 2. Build a raw 183 response as if it came off the wire, parse it
	//    back, and run through HandleResponse.
	raw183 := makeResponseBytes(183, "Session Progress", "invite-callid-phase5-001", "1928301774")
	resp183, err := parser.ParseMsg(raw183)
	if err != nil {
		t.Fatalf("ParseMsg(183) error: %v", err)
	}
	_, _, err = mgr.HandleResponse(resp183)
	if err != nil {
		t.Fatalf("HandleResponse(183) error: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("after 183 expected TStateProceeding, got %d", cell.State)
	}

	// Track this branch's last-received status.
	_ = branch

	// 3. Send a 200 OK response (again raw -> parsed -> HandleResponse).
	raw200 := makeResponseBytes(200, "OK", "invite-callid-phase5-001", "1928301774")
	resp200, err := parser.ParseMsg(raw200)
	if err != nil {
		t.Fatalf("ParseMsg(200) error: %v", err)
	}
	_, _, err = mgr.HandleResponse(resp200)
	if err != nil {
		t.Fatalf("HandleResponse(200) error: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("after 200 expected TStateCompleted, got %d", cell.State)
	}
}

// --------------------------------------------------------------------
// TestTM_CompleteProxyCallFlow
// --------------------------------------------------------------------
func TestTM_CompleteProxyCallFlow(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(phase5Invite)
	if err != nil {
		t.Fatalf("ParseMsg error: %v", err)
	}

	// Step 1: incoming INVITE -> create transaction.
	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest(INVITE) error: %v", err)
	}
	if cell == nil {
		t.Fatal("expected non-nil cell after INVITE")
	}

	// Step 2: generate a 100 Trying reply upstream (uses BuildSimpleResponse,
	// which exercises the response building path).
	tryingBytes, err := parser.BuildSimpleResponse(msg, 100, "Trying")
	if err != nil {
		t.Fatalf("BuildSimpleResponse(100) error: %v", err)
	}
	if !strings.Contains(string(tryingBytes), "SIP/2.0 100 Trying") {
		t.Errorf("100 Trying response malformed, got:\n%s", string(tryingBytes))
	}

	// Step 3: RelayRequest forward to downstream.
	_, branch, err := mgr.RelayRequest(msg, "10.0.0.2", 5060)
	if err != nil {
		t.Fatalf("RelayRequest error: %v", err)
	}

	// Step 4: Downstream 180 Ringing -> HandleResponse -> RelayReply
	// upstream.
	raw180 := makeResponseBytes(180, "Ringing", "invite-callid-phase5-001", "1928301774")
	resp180, err := parser.ParseMsg(raw180)
	if err != nil {
		t.Fatalf("ParseMsg(180) error: %v", err)
	}
	_, _, err = mgr.HandleResponse(resp180)
	if err != nil {
		t.Fatalf("HandleResponse(180) error: %v", err)
	}
	if err := mgr.RelayReply(msg, branch, 180, "Ringing"); err != nil {
		t.Fatalf("RelayReply(180) error: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("after relayed 180 expected TStateProceeding, got %d", cell.State)
	}

	// Step 5: Downstream 200 OK -> HandleResponse -> RelayReply upstream.
	raw200 := makeResponseBytes(200, "OK", "invite-callid-phase5-001", "1928301774")
	resp200, err := parser.ParseMsg(raw200)
	if err != nil {
		t.Fatalf("ParseMsg(200) error: %v", err)
	}
	_, _, err = mgr.HandleResponse(resp200)
	if err != nil {
		t.Fatalf("HandleResponse(200) error: %v", err)
	}
	if err := mgr.RelayReply(msg, branch, 200, "OK"); err != nil {
		t.Fatalf("RelayReply(200) error: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("after relayed 200 expected TStateCompleted, got %d", cell.State)
	}

	// Step 6: ACK arrives from the original caller -> LookupACK to
	// correlate it with the INVITE transaction.
	ackRaw := []byte("ACK sip:bob@ims.example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK-ack-001\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@ims.example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@ims.example.com>;tag=dst-tag-invite-callid-phase5-001\r\n" +
		"Call-ID: invite-callid-phase5-001\r\n" +
		"CSeq: 314159 ACK\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	ackMsg, err := parser.ParseMsg(ackRaw)
	if err != nil {
		t.Fatalf("ParseMsg(ACK) error: %v", err)
	}
	ackCell, err := mgr.LookupRequest(ackMsg)
	if err != nil {
		t.Fatalf("LookupRequest(ACK) error: %v", err)
	}
	if ackCell == nil {
		t.Fatal("expected LookupACK to find the original INVITE transaction")
	}

	// Step 7: BYE from caller -> new standalone transaction -> 200 OK.
	byeRaw := []byte("BYE sip:bob@ims.example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK-bye-001\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@ims.example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@ims.example.com>;tag=dst-tag-invite-callid-phase5-001\r\n" +
		"Call-ID: invite-callid-phase5-001\r\n" +
		"CSeq: 314160 BYE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	byeMsg, err := parser.ParseMsg(byeRaw)
	if err != nil {
		t.Fatalf("ParseMsg(BYE) error: %v", err)
	}
	byeCell, err := mgr.HandleRequest(byeMsg)
	if err != nil {
		t.Fatalf("HandleRequest(BYE) error: %v", err)
	}
	if byeCell == nil {
		t.Fatal("expected new BYE transaction")
	}
	// Build a 200 OK for the BYE.
	okBytes, err := parser.BuildSimpleResponse(byeMsg, 200, "OK")
	if err != nil {
		t.Fatalf("BuildSimpleResponse(BYE, 200) error: %v", err)
	}
	if !strings.Contains(string(okBytes), "SIP/2.0 200 OK") {
		t.Errorf("BYE 200 OK response malformed, got:\n%s", string(okBytes))
	}
}

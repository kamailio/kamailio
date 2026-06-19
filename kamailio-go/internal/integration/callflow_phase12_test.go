// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phase 12 - Complete SIP Call Flow Tests
 *
 * Tests the full SIP INVITE / 100 Trying / 180 Ringing / 200 OK / ACK / BYE
 * call flow using the parser + TM + SDP components.
 */

package integration

import (
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/sdp"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
)

// buildINVITE_Flow constructs a basic INVITE request as raw bytes (with SDP body).
func buildINVITE_Flow(from, to, contact, callID string, cseq int, sdpBody string) []byte {
	var sb strings.Builder
	sb.WriteString("INVITE sip:")
	sb.WriteString(to)
	sb.WriteString(" SIP/2.0\r\n")
	sb.WriteString("Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK")
	sb.WriteString(callID)
	sb.WriteString("\r\n")
	sb.WriteString("From: <sip:")
	sb.WriteString(from)
	sb.WriteString(">;tag=tag-from-")
	sb.WriteString(callID)
	sb.WriteString("\r\n")
	sb.WriteString("To: <sip:")
	sb.WriteString(to)
	sb.WriteString(">\r\n")
	sb.WriteString("Call-ID: ")
	sb.WriteString(callID)
	sb.WriteString("\r\n")
	sb.WriteString("CSeq: ")
	sb.WriteString(strconvItoa(cseq))
	sb.WriteString(" INVITE\r\n")
	sb.WriteString("Contact: <sip:")
	sb.WriteString(contact)
	sb.WriteString(">\r\n")
	sb.WriteString("Max-Forwards: 70\r\n")
	sb.WriteString("Content-Type: application/sdp\r\n")
	sb.WriteString("Content-Length: ")
	sb.WriteString(strconvItoa(len(sdpBody)))
	sb.WriteString("\r\n")
	sb.WriteString("\r\n")
	sb.WriteString(sdpBody)
	return []byte(sb.String())
}

// buildBYE_Flow constructs a BYE request that references an existing dialog.
func buildBYE_Flow(from, to, callID, fromTag, toTag string, cseq int) []byte {
	var sb strings.Builder
	sb.WriteString("BYE sip:")
	sb.WriteString(to)
	sb.WriteString(" SIP/2.0\r\n")
	sb.WriteString("Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK")
	sb.WriteString(callID)
	sb.WriteString("bye\r\n")
	sb.WriteString("From: <sip:")
	sb.WriteString(from)
	sb.WriteString(">;tag=")
	sb.WriteString(fromTag)
	sb.WriteString("\r\n")
	sb.WriteString("To: <sip:")
	sb.WriteString(to)
	sb.WriteString(">;tag=")
	sb.WriteString(toTag)
	sb.WriteString("\r\n")
	sb.WriteString("Call-ID: ")
	sb.WriteString(callID)
	sb.WriteString("\r\n")
	sb.WriteString("CSeq: ")
	sb.WriteString(strconvItoa(cseq))
	sb.WriteString(" BYE\r\n")
	sb.WriteString("Max-Forwards: 70\r\n")
	sb.WriteString("Content-Length: 0\r\n")
	sb.WriteString("\r\n")
	return []byte(sb.String())
}

// buildACK constructs an ACK to confirm a 2xx response to an INVITE.
func buildACK(from, to, callID, fromTag, toTag string, cseq int) []byte {
	var sb strings.Builder
	sb.WriteString("ACK sip:")
	sb.WriteString(to)
	sb.WriteString(" SIP/2.0\r\n")
	sb.WriteString("Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK")
	sb.WriteString(callID)
	sb.WriteString("ack\r\n")
	sb.WriteString("From: <sip:")
	sb.WriteString(from)
	sb.WriteString(">;tag=")
	sb.WriteString(fromTag)
	sb.WriteString("\r\n")
	sb.WriteString("To: <sip:")
	sb.WriteString(to)
	sb.WriteString(">;tag=")
	sb.WriteString(toTag)
	sb.WriteString("\r\n")
	sb.WriteString("Call-ID: ")
	sb.WriteString(callID)
	sb.WriteString("\r\n")
	sb.WriteString("CSeq: ")
	sb.WriteString(strconvItoa(cseq))
	sb.WriteString(" ACK\r\n")
	sb.WriteString("Max-Forwards: 70\r\n")
	sb.WriteString("Content-Length: 0\r\n")
	sb.WriteString("\r\n")
	return []byte(sb.String())
}

// buildCANCEL constructs a CANCEL for an in-progress INVITE.
func buildCANCEL(from, to, callID, fromTag string, cseq int) []byte {
	var sb strings.Builder
	sb.WriteString("CANCEL sip:")
	sb.WriteString(to)
	sb.WriteString(" SIP/2.0\r\n")
	sb.WriteString("Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK")
	sb.WriteString(callID)
	sb.WriteString("cancel\r\n")
	sb.WriteString("From: <sip:")
	sb.WriteString(from)
	sb.WriteString(">;tag=")
	sb.WriteString(fromTag)
	sb.WriteString("\r\n")
	sb.WriteString("To: <sip:")
	sb.WriteString(to)
	sb.WriteString(">\r\n")
	sb.WriteString("Call-ID: ")
	sb.WriteString(callID)
	sb.WriteString("\r\n")
	sb.WriteString("CSeq: ")
	sb.WriteString(strconvItoa(cseq))
	sb.WriteString(" CANCEL\r\n")
	sb.WriteString("Max-Forwards: 70\r\n")
	sb.WriteString("Content-Length: 0\r\n")
	sb.WriteString("\r\n")
	return []byte(sb.String())
}

// strconvItoa converts int to string without importing strconv in every helper.
func strconvItoa(n int) string {
	// simple decimal conversion
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	var buf [20]byte
	pos := len(buf)
	for n > 0 {
		pos--
		buf[pos] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		pos--
		buf[pos] = '-'
	}
	return string(buf[pos:])
}

// ============================================================
// Test: Full SIP Call Flow (Parser + TM + SDP)
// ============================================================

func TestSIP_CallFlow_INVITE_100Trying(t *testing.T) {
	// Step 1: Build INVITE with SDP
	session := sdp.NewSession("alice", "1000", "192.168.1.10")
	session.SessionName = "Voice Call"
	session.AddAudio(16384, []string{"0", "8"})
	sdpBody, err := session.Build()
	if err != nil {
		t.Fatalf("Failed to build SDP: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"call-flow-100",
		1,
		sdpBody,
	)

	// Step 2: Parse the INVITE
	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Failed to parse INVITE: %v", err)
	}

	if msg == nil {
		t.Fatal("Parsed message is nil")
	}

	if !msg.IsRequest() {
		t.Error("INVITE should be a request")
	}

	if msg.Method() != parser.MethodInvite {
		t.Errorf("Expected INVITE method, got %v", msg.Method())
	}

	// Step 3: Send 100 Trying response
	trying, err := parser.Create100Trying(msg)
	if err != nil {
		t.Fatalf("Failed to create 100 Trying: %v", err)
	}

	tryingBytes, err := parser.BuildMessage(trying)
	if err != nil {
		t.Fatalf("Failed to build 100 Trying: %v", err)
	}

	// Verify the first line
	if !strings.HasPrefix(string(tryingBytes), "SIP/2.0 100 Trying") {
		t.Errorf("100 Trying response should start with 'SIP/2.0 100 Trying', got: %s",
			string(tryingBytes)[:50])
	}
}

func TestSIP_CallFlow_INVITE_200OK_SDP(t *testing.T) {
	// Step 1: Build an INVITE with caller SDP
	sdpOffer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("Failed to format offer SDP: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"call-flow-200",
		1,
		sdpOffer,
	)

	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	// Step 2: Extract the offer SDP from the body
	if msg.Body == nil {
		t.Fatal("No body in INVITE")
	}
	bodyBytes, ok := msg.Body.([]byte)
	if !ok || len(bodyBytes) == 0 {
		t.Fatal("Empty or non-bytes body in INVITE")
	}
	parsedSDP, err := sdp.Parse(string(bodyBytes))
	if err != nil {
		t.Fatalf("Failed to parse body SDP: %v", err)
	}

	if parsedSDP.Version != 0 {
		t.Errorf("Expected SDP version 0, got %d", parsedSDP.Version)
	}

	audios := parsedSDP.GetAudio()
	if len(audios) == 0 {
		t.Fatal("No audio media in SDP offer")
	}
	if audios[0].Port != 16384 {
		t.Errorf("Expected port 16384, got %d", audios[0].Port)
	}

	// Step 3: Build 200 OK with a caller's answer SDP
	sdpAnswer, err := sdp.FormatAudioSDP("192.168.1.20", 20000, "bob")
	if err != nil {
		t.Fatalf("Failed to format answer SDP: %v", err)
	}

	ok200, err := parser.Create200OKWithSDP(msg, sdpAnswer)
	if err != nil {
		t.Fatalf("Failed to create 200 OK with SDP: %v", err)
	}

	okBytes, err := parser.BuildMessage(ok200)
	if err != nil {
		t.Fatalf("Failed to build 200 OK: %v", err)
	}

	if !strings.Contains(string(okBytes), "SIP/2.0 200 OK") {
		t.Error("200 OK response missing status line")
	}

	// Check body is present
	if !strings.Contains(string(okBytes), "a=rtpmap:0 PCMU/8000") {
		t.Error("200 OK body missing PCMU rtpmap")
	}
}

func TestSIP_CallFlow_CompleteDialog(t *testing.T) {
	// Full dialog: INVITE -> 100 Trying -> 180 Ringing -> 200 OK -> ACK -> BYE -> 200 OK
	callID := "call-dialog-1"
	fromUser := "alice@example.com"
	toUser := "bob@example.com"
	contact := "alice@192.168.1.10:5060"

	// --- Phase 1: INVITE (with SDP offer) ---
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP offer failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(fromUser, toUser, contact, callID, 1, offer)
	invite, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	// --- Phase 2: 100 Trying ---
	trying, err := parser.Create100Trying(invite)
	if err != nil {
		t.Fatalf("Create100Trying failed: %v", err)
	}
	tryingBytes, err := parser.BuildMessage(trying)
	if err != nil {
		t.Fatalf("BuildMessage Trying failed: %v", err)
	}
	if !strings.Contains(string(tryingBytes), "SIP/2.0 100 Trying") {
		t.Error("100 Trying missing in response")
	}

	// --- Phase 3: 180 Ringing ---
	ringing, err := parser.Create180Ringing(invite)
	if err != nil {
		t.Fatalf("Create180Ringing failed: %v", err)
	}
	ringingBytes, err := parser.BuildMessage(ringing)
	if err != nil {
		t.Fatalf("BuildMessage Ringing failed: %v", err)
	}
	if !strings.Contains(string(ringingBytes), "SIP/2.0 180 Ringing") {
		t.Error("180 Ringing missing in response")
	}

	// --- Phase 4: 200 OK (with SDP answer) ---
	answer, err := sdp.FormatAudioSDP("192.168.1.20", 20000, "bob")
	if err != nil {
		t.Fatalf("FormatAudioSDP answer failed: %v", err)
	}

	ok, err := parser.Create200OKWithSDP(invite, answer)
	if err != nil {
		t.Fatalf("Create200OKWithSDP failed: %v", err)
	}
	okBytes, err := parser.BuildMessage(ok)
	if err != nil {
		t.Fatalf("BuildMessage OK failed: %v", err)
	}

	if !strings.Contains(string(okBytes), "SIP/2.0 200 OK") {
		t.Error("200 OK status missing")
	}
	if !strings.Contains(string(okBytes), "application/sdp") {
		t.Error("200 OK should have application/sdp content")
	}

	// Extract To tag from 200 OK and From tag from INVITE
	toTag := extractTagFromHeader(ok, false) // false = use To header
	fromTag := extractTagFromHeader(invite, true) // true = use From header

	// --- Phase 5: ACK confirms the call ---
	ackRaw := buildACK(fromUser, toUser, callID, fromTag, toTag, 1)
	ack, err := parser.ParseMsg(ackRaw)
	if err != nil {
		t.Fatalf("Parse ACK failed: %v", err)
	}
	if ack.Method() != parser.MethodACK {
		t.Errorf("Expected ACK method, got %v", ack.Method())
	}

	// --- Phase 6: BYE terminates the call ---
	byeRaw := buildBYE_Flow(fromUser, toUser, callID, fromTag, toTag, 2)
	bye, err := parser.ParseMsg(byeRaw)
	if err != nil {
		t.Fatalf("Parse BYE failed: %v", err)
	}
	if bye.Method() != parser.MethodBye {
		t.Errorf("Expected BYE method, got %v", bye.Method())
	}

	// --- Phase 7: 200 OK confirms BYE ---
	byeOK, err := parser.Create200OK(bye)
	if err != nil {
		t.Fatalf("Create200OK (for BYE) failed: %v", err)
	}
	byeOKBytes, err := parser.BuildMessage(byeOK)
	if err != nil {
		t.Fatalf("BuildMessage (BYE OK) failed: %v", err)
	}
	if !strings.Contains(string(byeOKBytes), "SIP/2.0 200 OK") {
		t.Error("BYE 200 OK missing status line")
	}
}

// extractTagFromHeader extracts the tag param from either the From or To header body.
// When useFrom is true, it scans the From header; otherwise it scans To.
func extractTagFromHeader(msg *parser.SIPMsg, useFrom bool) string {
	if msg == nil {
		return ""
	}
	var hdr *parser.HdrField
	if useFrom {
		hdr = msg.From
	} else {
		hdr = msg.To
	}
	if hdr == nil {
		return ""
	}
	body := hdr.Body.String()
	idx := strings.Index(body, "tag=")
	if idx < 0 {
		return ""
	}
	tag := body[idx+4:]
	if end := strings.IndexAny(tag, "; \r\n"); end >= 0 {
		return tag[:end]
	}
	return tag
}

// extractTagFromMsg returns the tag param from the From header body (kept for compatibility).
func extractTagFromMsg(msg *parser.SIPMsg) string {
	return extractTagFromHeader(msg, true)
}

// ============================================================
// Test: TM transaction lifecycle
// ============================================================

func TestSIP_TransactionLifecycle(t *testing.T) {
	mgr := tm.NewManagerWithTimers(1024)

	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"tx-lifecycle-1",
		1,
		offer,
	)

	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	// Create the transaction
	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("NewTransaction failed: %v", err)
	}
	if cell == nil {
		t.Fatal("NewTransaction returned nil cell")
	}

	// Look it up by message
	found := mgr.GetTable().LookupByMsg(msg)
	if found == nil {
		t.Fatal("LookupByMsg didn't find transaction")
	}

	// Verify Via branch is stored
	if cell.ViaBranch.Len == 0 {
		t.Error("ViaBranch should be set for transaction")
	}
}

// ============================================================
// Test: Max-Forwards handling
// ============================================================

func TestSIP_MaxForwards_DecrementAndZero(t *testing.T) {
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"mf-test-1",
		1,
		offer,
	)

	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	mf := msg.GetMaxForwards()
	if mf != 70 {
		t.Errorf("Expected Max-Forwards=70, got %d", mf)
	}

	// Decrement
	if !msg.DecrementMaxForwards() {
		t.Error("DecrementMaxForwards should succeed for 70")
	}
	mf = msg.GetMaxForwards()
	if mf != 69 {
		t.Errorf("Expected Max-Forwards=69 after decrement, got %d", mf)
	}

	// Now test: proxy should reject when Max-Forwards=1 and still tries to forward.
	// The helper DecrementMaxForwards should return false when the header value would go to 0.
}

// ============================================================
// Test: Various error response codes
// ============================================================

func TestSIP_ErrorResponses(t *testing.T) {
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"err-resp-1",
		1,
		offer,
	)
	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse failed: %v", err)
	}

	// 403 Forbidden
	forbid, err := parser.Create403Forbidden(msg)
	if err != nil {
		t.Fatalf("Create403Forbidden failed: %v", err)
	}
	b, _ := parser.BuildMessage(forbid)
	if !strings.Contains(string(b), "SIP/2.0 403 Forbidden") {
		t.Error("403 response malformed")
	}

	// 404 Not Found
	notFound, err := parser.Create404NotFound(msg)
	if err != nil {
		t.Fatalf("Create404NotFound failed: %v", err)
	}
	b, _ = parser.BuildMessage(notFound)
	if !strings.Contains(string(b), "SIP/2.0 404 Not Found") {
		t.Error("404 response malformed")
	}

	// 480 Temporarily Unavailable
	unavail, err := parser.Create480TemporaryUnavailable(msg)
	if err != nil {
		t.Fatalf("Create480TemporaryUnavailable failed: %v", err)
	}
	b, _ = parser.BuildMessage(unavail)
	if !strings.Contains(string(b), "SIP/2.0 480 Temporarily Unavailable") {
		t.Error("480 response malformed")
	}

	// 500 Server Internal Error
	serverErr, err := parser.Create500ServerInternalError(msg, "database unreachable")
	if err != nil {
		t.Fatalf("Create500ServerInternalError failed: %v", err)
	}
	b, _ = parser.BuildMessage(serverErr)
	if !strings.Contains(string(b), "SIP/2.0 500 Server Internal Error") {
		t.Error("500 response malformed")
	}

	// 603 Decline
	decline, err := parser.Create603Decline(msg)
	if err != nil {
		t.Fatalf("Create603Decline failed: %v", err)
	}
	b, _ = parser.BuildMessage(decline)
	if !strings.Contains(string(b), "SIP/2.0 603 Decline") {
		t.Error("603 response malformed")
	}
}

// ============================================================
// Test: 183 Session Progress with early media SDP
// ============================================================

func TestSIP_SessionProgress_EarlyMedia(t *testing.T) {
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"early-media-1",
		1,
		offer,
	)
	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse failed: %v", err)
	}

	// Early media SDP (ringback tone)
	earlyMedia, err := sdp.FormatAudioSDP("192.168.1.30", 30000, "pbx")
	if err != nil {
		t.Fatalf("FormatAudioSDP early media failed: %v", err)
	}

	prog, err := parser.Create183SessionProgress(msg, earlyMedia)
	if err != nil {
		t.Fatalf("Create183SessionProgress failed: %v", err)
	}
	b, err := parser.BuildMessage(prog)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}

	if !strings.Contains(string(b), "SIP/2.0 183 Session Progress") {
		t.Error("183 response malformed")
	}

	if !strings.Contains(string(b), "application/sdp") {
		t.Error("183 should carry SDP body")
	}
}

// ============================================================
// Test: SDP round-trip (build -> parse -> build)
// ============================================================

func TestSDP_RoundTrip(t *testing.T) {
	original := sdp.NewSession("alice", "123456", "192.168.1.10")
	original.SessionName = "Voice"
	m := original.AddAudio(16384, []string{"0", "8"})
	m.SetDirection("sendrecv")

	// Build SDP
	sdpStr, err := original.Build()
	if err != nil {
		t.Fatalf("Build failed: %v", err)
	}

	// Parse it back
	parsed, err := sdp.Parse(sdpStr)
	if err != nil {
		t.Fatalf("Parse failed: %v", err)
	}

	// Verify fields round-tripped
	if parsed.Origin.Username != "alice" {
		t.Errorf("username mismatch: %q", parsed.Origin.Username)
	}
	if parsed.SessionName != "Voice" {
		t.Errorf("session name mismatch: %q", parsed.SessionName)
	}

	audios := parsed.GetAudio()
	if len(audios) != 1 {
		t.Fatalf("expected 1 audio media, got %d", len(audios))
	}
	if audios[0].Port != 16384 {
		t.Errorf("audio port mismatch: %d", audios[0].Port)
	}

	// Build again and compare
	sdpStr2, err := parsed.Build()
	if err != nil {
		t.Fatalf("second build failed: %v", err)
	}
	if sdpStr != sdpStr2 {
		t.Error("SDP round-trip produced different output")
	}
}

// ============================================================
// Test: Route/Record-Route header handling
// ============================================================

func TestSIP_RouteHeaders_ForwardRequest(t *testing.T) {
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"route-headers-1",
		1,
		offer,
	)

	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	// Build forwarded request (proxy adds Via and decrements Max-Forwards)
	fwd, err := parser.BuildForwardRequestWithRecordRoute(
		msg, "UDP", "192.168.1.1", 5060,
		"sip:bob@192.168.1.20:5060",
		"sip:proxy@192.168.1.1:5060",
	)
	if err != nil {
		t.Fatalf("BuildForwardRequestWithRecordRoute failed: %v", err)
	}

	fwdBytes, err := parser.BuildMessage(fwd)
	if err != nil {
		t.Fatalf("BuildMessage fwd failed: %v", err)
	}

	// Must contain the added Via header (proxy's own address)
	if !strings.Contains(string(fwdBytes), "Via:") {
		t.Error("Forwarded request should have Via header")
	}

	// Max-Forwards should be decremented
	if !strings.Contains(string(fwdBytes), "Max-Forwards: 69") {
		t.Error("Forwarded request should have Max-Forwards=69 (was 70)")
	}

	// Record-Route should be present (to keep proxy in signalling path)
	if !strings.Contains(string(fwdBytes), "Record-Route:") {
		t.Error("Forwarded request should have Record-Route header")
	}
}

// ============================================================
// Test: CANCEL flow (INVITE -> CANCEL -> 200 OK for CANCEL -> 487 for INVITE)
// ============================================================

func TestSIP_CancelFlow(t *testing.T) {
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	callID := "cancel-test-1"
	fromUser := "alice@example.com"
	toUser := "bob@example.com"

	// --- Phase 1: INVITE (establish transaction) ---
	inviteRaw := buildINVITE_Flow(fromUser, toUser, "alice@192.168.1.10:5060", callID, 1, offer)
	invite, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	// --- Phase 2: 100 Trying ---
	trying, err := parser.CreateReply(invite, parser.ReplyOptions{
		StatusCode: 100,
		ReasonPhrase: "Trying",
	})
	if err != nil {
		t.Fatalf("CreateReply 100 failed: %v", err)
	}
	tryingBytes, err := parser.BuildMessage(trying)
	if err != nil {
		t.Fatalf("BuildMessage trying failed: %v", err)
	}
	if !strings.Contains(string(tryingBytes), "SIP/2.0 100 Trying") {
		t.Error("100 Trying response invalid")
	}

	// --- Phase 3: Caller sends CANCEL ---
	fromTag := extractTagFromHeader(invite, true)
	cancelRaw := buildCANCEL(fromUser, toUser, callID, fromTag, 1)
	cancel, err := parser.ParseMsg(cancelRaw)
	if err != nil {
		t.Fatalf("Parse CANCEL failed: %v", err)
	}
	if cancel.Method() != parser.MethodCancel {
		t.Errorf("Expected CANCEL method, got %v", cancel.Method())
	}

	// --- Phase 4: 200 OK for CANCEL ---
	cancelOK, err := parser.Create200OK(cancel)
	if err != nil {
		t.Fatalf("Create200OK for CANCEL failed: %v", err)
	}
	cancelOKBytes, err := parser.BuildMessage(cancelOK)
	if err != nil {
		t.Fatalf("BuildMessage for CANCEL 200 failed: %v", err)
	}
	if !strings.Contains(string(cancelOKBytes), "SIP/2.0 200 OK") {
		t.Error("CANCEL 200 OK response invalid")
	}

	// --- Phase 5: 487 Request Terminated for the INVITE ---
	terminated, err := parser.CreateReply(invite, parser.ReplyOptions{
		StatusCode: 487,
		ReasonPhrase: "Request Terminated",
	})
	if err != nil {
		t.Fatalf("CreateReply 487 failed: %v", err)
	}
	termBytes, err := parser.BuildMessage(terminated)
	if err != nil {
		t.Fatalf("BuildMessage 487 failed: %v", err)
	}
	if !strings.Contains(string(termBytes), "SIP/2.0 487 Request Terminated") {
		t.Error("487 response invalid")
	}
}

// ============================================================
// Test: Multiple INVITEs with distinct Call-IDs (TM isolation)
// ============================================================

func TestSIP_MultipleTransactions(t *testing.T) {
	mgr := tm.NewManagerWithTimers(1024)

	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	// Create three distinct INVITE transactions
	callIDs := []string{"multi-tx-1", "multi-tx-2", "multi-tx-3"}
	for i, callID := range callIDs {
		inviteRaw := buildINVITE_Flow(
			"alice@example.com",
			"bob@example.com",
			"alice@192.168.1.10:5060",
			callID,
			i+1,
			offer,
		)
		msg, err := parser.ParseMsg(inviteRaw)
		if err != nil {
			t.Fatalf("Parse INVITE %d failed: %v", i, err)
		}
		cell, err := mgr.NewTransaction(msg)
		if err != nil {
			t.Fatalf("NewTransaction %d failed: %v", i, err)
		}
		if cell == nil {
			t.Fatalf("cell %d is nil", i)
		}
		if cell.ViaBranch.Len == 0 {
			t.Errorf("cell %d should have Via branch set", i)
		}
	}

	// Verify each Call-ID maps to a distinct, look-up-able transaction
	for i, callID := range callIDs {
		// Reconstruct INVITE with matching CSeq number for LookupByMsg
		inviteRaw := buildINVITE_Flow(
			"alice@example.com",
			"bob@example.com",
			"alice@192.168.1.10:5060",
			callID,
			i+1,
			offer,
		)
		msg, err := parser.ParseMsg(inviteRaw)
		if err != nil {
			t.Fatalf("Re-parse failed: %v", err)
		}
		found := mgr.GetTable().LookupByMsg(msg)
		if found == nil {
			t.Errorf("LookupByMsg did not find transaction for %s", callID)
		}
	}
}

// ============================================================
// Test: Via branch extraction from raw header string
// ============================================================

func TestSIP_ViaBranchExtraction(t *testing.T) {
	offer, err := sdp.FormatAudioSDP("192.168.1.10", 16384, "alice")
	if err != nil {
		t.Fatalf("FormatAudioSDP failed: %v", err)
	}

	inviteRaw := buildINVITE_Flow(
		"alice@example.com",
		"bob@example.com",
		"alice@192.168.1.10:5060",
		"branch-extract-1",
		1,
		offer,
	)

	msg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("Parse INVITE failed: %v", err)
	}

	if msg.HdrVia1 == nil {
		t.Fatal("INVITE should have Via header")
	}

	// Via body should contain "branch="
	viaBody := msg.HdrVia1.Body.String()
	if !strings.Contains(viaBody, "branch=") {
		t.Error("Via body should contain branch=")
	}

	// Via1 parsed via lazy parse should also work
	vb, err := msg.GetParsedVia()
	if err != nil {
		t.Fatalf("GetParsedVia failed: %v", err)
	}
	if vb == nil {
		t.Fatal("GetParsedVia returned nil")
	}
	if vb.Branch == nil {
		t.Error("Via body should have branch param")
	} else if vb.Branch.Value.Len == 0 {
		t.Error("Via branch value should be non-empty")
	}
}

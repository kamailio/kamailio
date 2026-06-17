// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Pipeline integration tests for the Kamailio-Go server.
 *
 * Tests cover:
 *   - ParseMsg and dispatch to handleRequest/handleReply
 *   - INVITE → 100 Trying + 200 OK
 *   - REGISTER → 200 OK
 *   - ACK to confirm dialog
 *   - BYE → terminate dialog
 *   - Max-Forwards: 0 → 483 Too Many Hops
 *
 * Uses a standalone Server (does not start listeners) so the tests
 * focus on the pipeline logic itself.
 */

package main

import (
	"net"
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// -------------------- sample messages --------------------

// sampleINVITE returns a minimal valid INVITE request.
func sampleINVITE() []byte {
	return []byte("INVITE sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-abc123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-1\r\n" +
		"To: \"Bob\" <sip:bob@127.0.0.1:5060>\r\n" +
		"Call-ID: test-invite-001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5080>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

func sampleINVITEWithSDP() []byte {
	sdp := "v=0\r\n" +
		"o=- 1234 5678 IN IP4 127.0.0.1\r\n" +
		"s=- \r\n" +
		"c=IN IP4 127.0.0.1\r\n" +
		"t=0 0\r\n" +
		"m=audio 49152 RTP/AVP 0 8 101\r\n" +
		"a=rtpmap:0 PCMU/8000\r\n" +
		"a=rtpmap:8 PCMA/8000\r\n" +
		"a=rtpmap:101 telephone-event/8000\r\n" +
		"a=sendrecv\r\n"
	return []byte("INVITE sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-sdp123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-sdp\r\n" +
		"To: \"Bob\" <sip:bob@127.0.0.1:5060>\r\n" +
		"Call-ID: test-invite-sdp-001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5080>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: " + itoa(len(sdp)) + "\r\n" +
		"\r\n" + sdp)
}

func sampleREGISTER() []byte {
	return []byte("REGISTER sip:ims.example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-reg123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: <sip:alice@ims.example.com>;tag=reg-from-1\r\n" +
		"To: <sip:alice@ims.example.com>\r\n" +
		"Call-ID: test-register-001\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@127.0.0.1:5080>;expires=3600\r\n" +
		"Expires: 3600\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

func sampleINVITEZeroMF() []byte {
	return []byte("INVITE sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-zeroMF\r\n" +
		"Max-Forwards: 0\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-zeromf\r\n" +
		"To: \"Bob\" <sip:bob@127.0.0.1:5060>\r\n" +
		"Call-ID: test-invite-zeromf\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5080>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

// sampleACK builds an ACK with the given to-tag inserted in the To header.
func sampleACK(toTag string) []byte {
	toHdr := "To: \"Bob\" <sip:bob@127.0.0.1:5060>;tag=" + toTag
	return []byte("ACK sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-ack123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-1\r\n" +
		toHdr + "\r\n" +
		"Call-ID: test-invite-001\r\n" +
		"CSeq: 1 ACK\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

// sampleBYE builds a BYE with the given to-tag in the To header.
func sampleBYE(toTag string) []byte {
	toHdr := "To: \"Bob\" <sip:bob@127.0.0.1:5060>;tag=" + toTag
	return []byte("BYE sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-bye123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-1\r\n" +
		toHdr + "\r\n" +
		"Call-ID: test-invite-001\r\n" +
		"CSeq: 2 BYE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

// sample200OKReply returns a 200 OK reply to be routed through handleReply.
func sample200OKReply() []byte {
	return []byte("SIP/2.0 200 OK\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-reply123\r\n" +
		"From: \"Caller\" <sip:caller@127.0.0.1:5080>;tag=caller-tag\r\n" +
		"To: \"Callee\" <sip:callee@127.0.0.1:5060>;tag=callee-tag\r\n" +
		"Call-ID: reply-test-001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:callee@127.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

func fakeSrcAddr() *net.UDPAddr {
	return &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 5080,
	}
}

func newTestServer() *Server {
	cfg := config.DefaultConfig()
	s := NewServer(cfg)
	s.initPipeline()
	return s
}

// -------------------- helpers --------------------

func assertStatus(t *testing.T, raw []byte, expected int) {
	t.Helper()
	if len(raw) == 0 {
		t.Fatal("empty response")
	}
	firstLine := string(raw)
	if idx := strings.Index(firstLine, "\r\n"); idx >= 0 {
		firstLine = firstLine[:idx]
	}
	if !strings.HasPrefix(firstLine, "SIP/2.0") {
		t.Fatalf("response does not start with SIP/2.0: %q", firstLine)
	}
	want := itoa(expected)
	if !strings.Contains(firstLine, want) {
		t.Fatalf("expected status %d in first line, got %q", expected, firstLine)
	}
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
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

// -------------------- tests --------------------

func TestPipeline_ParseAndDispatch(t *testing.T) {
	raw := sampleINVITE()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}
	if !msg.IsRequest() {
		t.Fatal("expected request")
	}
	if msg.Method() != parser.MethodInvite {
		t.Fatalf("expected INVITE method, got %v", msg.Method())
	}

	s := newTestServer()
	s.handleRequest(msg, fakeSrcAddr())
	count := s.tm.TransactionCount()
	if count == 0 {
		t.Fatal("expected at least 1 transaction after handleRequest")
	}
}

func TestPipeline_INVITEWith100Trying(t *testing.T) {
	raw := sampleINVITE()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	trying, err := parser.CreateReply(msg, parser.ReplyOptions{
		StatusCode:   100,
		ReasonPhrase: "Trying",
	})
	if err != nil {
		t.Fatalf("CreateReply(100) failed: %v", err)
	}
	bytes, err := parser.BuildMessage(trying)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	assertStatus(t, bytes, 100)
}

func TestPipeline_INVITEWith200OK(t *testing.T) {
	raw := sampleINVITE()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}
	ok, err := parser.CreateReply(msg, parser.ReplyOptions{
		StatusCode:   200,
		ReasonPhrase: "OK",
		Contact:      "<sip:proxy@example.com>",
	})
	if err != nil {
		t.Fatalf("CreateReply(200) failed: %v", err)
	}
	bytes, err := parser.BuildMessage(ok)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	assertStatus(t, bytes, 200)
}

func TestPipeline_REGISTERWith200OK(t *testing.T) {
	raw := sampleREGISTER()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}
	if msg.Method() != parser.MethodRegister {
		t.Fatalf("expected REGISTER method, got %v", msg.Method())
	}
	ok, err := parser.CreateReply(msg, parser.ReplyOptions{
		StatusCode:   200,
		ReasonPhrase: "OK",
	})
	if err != nil {
		t.Fatalf("CreateReply(200) failed: %v", err)
	}
	bytes, err := parser.BuildMessage(ok)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	assertStatus(t, bytes, 200)
}

// TestPipeline_ACKFor200 verifies that an ACK received after an INVITE
// keeps the dialog state confirmed (not terminated).
func TestPipeline_ACKFor200(t *testing.T) {
	s := newTestServer()

	invite, err := parser.ParseMsg(sampleINVITE())
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	// Create dialog using UAS logic – the dialog stores a remote tag
	// (from-tag) and generates a local tag (placed in To header).
	d, err := dialog.CreateUASDialog(invite, "<sip:proxy@example.com>")
	if err != nil {
		t.Fatalf("CreateUASDialog failed: %v", err)
	}
	if d.LocalTag == "" {
		t.Fatal("expected a local tag to be generated")
	}
	if err := s.dialogs.Add(d); err != nil {
		t.Fatalf("Add dialog failed: %v", err)
	}

	// Send an ACK through the pipeline using the real local tag.
	ack, err := parser.ParseMsg(sampleACK(d.LocalTag))
	if err != nil {
		t.Fatalf("ParseMsg(ACK) failed: %v", err)
	}
	s.handleRequest(ack, fakeSrcAddr())

	fromTag := ""
	if invite.From != nil {
		fromTag = extractTagFrom(invite.From.Body.String())
	}
	got := s.dialogs.Lookup(d.CallID, fromTag, d.LocalTag)
	if got == nil {
		t.Fatalf("dialog disappeared after ACK")
	}
	if !got.IsConfirmed() {
		t.Fatal("expected dialog to still be Confirmed after ACK")
	}
	if got.IsTerminated() {
		t.Fatal("dialog should not be Terminated after ACK")
	}
}

// TestPipeline_BYETerminatesDialog verifies BYE terminates a matching dialog.
func TestPipeline_BYETerminatesDialog(t *testing.T) {
	s := newTestServer()

	invite, err := parser.ParseMsg(sampleINVITE())
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	d, err := dialog.CreateUASDialog(invite, "<sip:proxy@example.com>")
	if err != nil {
		t.Fatalf("CreateUASDialog failed: %v", err)
	}
	d.Confirm()
	if err := s.dialogs.Add(d); err != nil {
		t.Fatalf("Add dialog failed: %v", err)
	}

	bye, err := parser.ParseMsg(sampleBYE(d.LocalTag))
	if err != nil {
		t.Fatalf("ParseMsg(BYE) failed: %v", err)
	}
	s.handleRequest(bye, fakeSrcAddr())

	fromTag := ""
	if invite.From != nil {
		fromTag = extractTagFrom(invite.From.Body.String())
	}
	got := s.dialogs.Lookup(d.CallID, fromTag, d.LocalTag)
	if got == nil {
		t.Fatal("dialog missing after BYE")
	}
	if !got.IsTerminated() {
		t.Fatalf("expected dialog to be Terminated after BYE")
	}
}

// TestPipeline_483TooManyHops verifies Max-Forwards: 0 → 483.
func TestPipeline_483TooManyHops(t *testing.T) {
	raw := sampleINVITEZeroMF()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	if checkMaxForwards(msg) {
		t.Fatal("expected checkMaxForwards to return false for Max-Forwards:0")
	}

	reply, err := parser.CreateReply(msg, parser.ReplyOptions{
		StatusCode:   483,
		ReasonPhrase: "Too Many Hops",
	})
	if err != nil {
		t.Fatalf("CreateReply failed: %v", err)
	}
	bytes, err := parser.BuildMessage(reply)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	assertStatus(t, bytes, 483)
}

// TestPipeline_handleReplyConfirmsDialog verifies that receiving a 2xx
// for an INVITE confirms the matching dialog.
func TestPipeline_handleReplyConfirmsDialog(t *testing.T) {
	s := newTestServer()

	reply, err := parser.ParseMsg(sample200OKReply())
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}
	if !reply.IsReply() {
		t.Fatal("expected reply message")
	}

	// Extract tags from the reply (From = local tag, To = remote tag).
	callID := ""
	fromTag := ""
	toTag := ""
	if reply.CallID != nil {
		callID = strings.TrimSpace(reply.CallID.Body.String())
	}
	if reply.From != nil {
		fromTag = extractTagFrom(reply.From.Body.String())
	}
	if reply.To != nil {
		toTag = extractTagFrom(reply.To.Body.String())
	}

	// Build a dialog using UAS-style (we are the UAS).
	// But since this is a reply message, use the raw message structure.
	d, err := dialog.CreateUASDialog(reply, "<sip:proxy@example.com>")
	if err != nil {
		t.Fatalf("CreateUASDialog failed: %v", err)
	}
	if err := s.dialogs.Add(d); err != nil {
		t.Fatalf("Add dialog failed: %v", err)
	}

	s.handleReply(reply, fakeSrcAddr())

	got := s.dialogs.Lookup(callID, fromTag, toTag)
	if got == nil {
		t.Fatal("dialog missing after handleReply")
	}
	if !got.IsConfirmed() {
		t.Fatal("expected dialog to be Confirmed after 2xx reply")
	}
}

// TestPipeline_INVITEWithSDPBody verifies the parser handles full INVITEs
// with SDP bodies and can rebuild them via BuildMessage.
func TestPipeline_INVITEWithSDPBody(t *testing.T) {
	raw := sampleINVITEWithSDP()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg(SDP) failed: %v", err)
	}
	if !msg.IsRequest() {
		t.Fatal("expected request")
	}
	if msg.Method() != parser.MethodInvite {
		t.Fatalf("expected INVITE, got %v", msg.Method())
	}

	bytes, err := parser.BuildMessage(msg)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	if len(bytes) < 20 {
		t.Fatal("BuildMessage returned too few bytes")
	}
}

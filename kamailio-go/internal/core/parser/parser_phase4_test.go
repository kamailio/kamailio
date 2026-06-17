// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - unit tests for Phase 4:
 *   - BuildMessage (SIPMsg -> []byte)
 *   - CreateReply (request -> response)
 *   - BuildForwardRequest (request -> forwarded request)
 *   - End-to-end: INVITE -> 100 Trying -> 180 Ringing -> 200 OK (with SDP) -> ACK
 */

package parser

import (
	"bytes"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ============================================================
// Test helpers
// ============================================================

// makeSimpleINVITE returns a parsed SIPMsg for an INVITE.
func makeSimpleINVITE() *SIPMsg {
	raw := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.1:5060;branch=z9hG4bK776a\r\n" +
		"From: <sip:alice@example.com>;tag=abc123\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: callid-12345@192.168.1.1\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@192.168.1.1:5060;transport=udp>\r\n" +
		"Max-Forwards: 70\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 137\r\n" +
		"\r\n" +
		"v=0\r\n" +
		"o=alice 123456 789012 IN IP4 192.168.1.1\r\n" +
		"s=Test\r\n" +
		"c=IN IP4 192.168.1.1\r\n" +
		"t=0 0\r\n" +
		"m=audio 49170 RTP/AVP 0 8\r\n" +
		"a=rtpmap:0 PCMU/8000\r\n" +
		"a=rtpmap:8 PCMA/8000\r\n"
	msg, err := ParseMsg([]byte(raw))
	if err != nil {
		panic(err)
	}
	// Overwrite body to match (parse_uri parser may not preserve the body as-is,
	// so we explicitly set it for predictable round-tripping in tests)
	msg.Body = []byte("v=0\r\n" +
		"o=alice 123456 789012 IN IP4 192.168.1.1\r\n" +
		"s=Test\r\n" +
		"c=IN IP4 192.168.1.1\r\n" +
		"t=0 0\r\n" +
		"m=audio 49170 RTP/AVP 0 8\r\n" +
		"a=rtpmap:0 PCMU/8000\r\n" +
		"a=rtpmap:8 PCMA/8000\r\n")
	return msg
}

// ============================================================
// BuildMessage tests
// ============================================================

func TestBuildMessage_RequestLine(t *testing.T) {
	msg := makeSimpleINVITE()
	out, err := BuildMessage(msg)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	// First line should be INVITE sip:bob@example.com SIP/2.0
	// Allow trailing \r\n
	if !bytes.HasPrefix(out, []byte("INVITE sip:bob@example.com SIP/2.0\r\n")) {
		t.Fatalf("unexpected first line: %q", string(firstNL(out)))
	}
}

func TestBuildMessage_HeaderCount(t *testing.T) {
	msg := makeSimpleINVITE()
	out, err := BuildMessage(msg)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	if bytes.Contains(out, []byte("Via:")) {
		// Good - Via header present
	} else {
		t.Fatalf("no Via header found in: %q", string(out[:200]))
	}
	if !bytes.Contains(out, []byte("Content-Length:")) {
		t.Fatalf("no Content-Length header found")
	}
}

func TestBuildMessage_ContentLengthMatches(t *testing.T) {
	msg := makeSimpleINVITE()
	out, err := BuildMessage(msg)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}
	// Content-Length should match body size
	body, _ := extractBody(out)
	if cl := expectedContentLength(out); cl != len(body) {
		t.Fatalf("Content-Length mismatch: header=%d actual body=%d", cl, len(body))
	}
}

func TestBuildMessage_Reply(t *testing.T) {
	invite := makeSimpleINVITE()
	reply, err := Create200OK(invite)
	if err != nil {
		t.Fatalf("Create200OK failed: %v", err)
	}
	out, err := BuildMessage(reply)
	if err != nil {
		t.Fatalf("BuildMessage(reply) failed: %v", err)
	}
	if !bytes.HasPrefix(out, []byte("SIP/2.0 200 OK\r\n")) {
		t.Fatalf("unexpected reply line: %q", string(firstNL(out)))
	}
}

// ============================================================
// CreateReply tests
// ============================================================

func TestCreateReply_100Trying(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create100Trying(req)
	if err != nil {
		t.Fatalf("Create100Trying failed: %v", err)
	}
	if reply.StatusCode() != 100 {
		t.Fatalf("expected status 100, got %d", reply.StatusCode())
	}
	// 100 Trying should NOT have a to-tag
	for _, h := range reply.Headers {
		if h.Type == HdrTo {
			toBody := h.Body.String()
			if containsTag(toBody) {
				t.Fatalf("100 Trying should not have To tag, but got: %q", toBody)
			}
		}
	}
}

func TestCreateReply_180Ringing(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create180Ringing(req)
	if err != nil {
		t.Fatalf("Create180Ringing failed: %v", err)
	}
	if reply.StatusCode() != 180 {
		t.Fatalf("expected status 180, got %d", reply.StatusCode())
	}
	// Must have a to-tag
	if !replyHasToTag(reply) {
		t.Fatalf("180 Ringing should have a To tag")
	}
}

func TestCreateReply_200OK(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create200OK(req)
	if err != nil {
		t.Fatalf("Create200OK failed: %v", err)
	}
	if reply.StatusCode() != 200 {
		t.Fatalf("expected status 200, got %d", reply.StatusCode())
	}
	// Must have a to-tag
	if !replyHasToTag(reply) {
		t.Fatalf("200 OK should have a To tag")
	}
	// Call-ID must match request
	if !replyHasCallID(reply, "callid-12345@192.168.1.1") {
		t.Fatalf("Call-ID mismatch")
	}
	// CSeq must match request
	if !replyHasCSeq(reply, 1, "INVITE") {
		t.Fatalf("CSeq mismatch")
	}
}

func TestCreateReply_4xx(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create404NotFound(req)
	if err != nil {
		t.Fatalf("Create404NotFound failed: %v", err)
	}
	if reply.StatusCode() != 404 {
		t.Fatalf("expected 404, got %d", reply.StatusCode())
	}

	reply, err = Create403Forbidden(req)
	if err != nil {
		t.Fatalf("Create403Forbidden failed: %v", err)
	}
	if reply.StatusCode() != 403 {
		t.Fatalf("expected 403, got %d", reply.StatusCode())
	}

	reply, err = Create480TemporaryUnavailable(req)
	if err != nil {
		t.Fatalf("Create480TemporaryUnavailable failed: %v", err)
	}
	if reply.StatusCode() != 480 {
		t.Fatalf("expected 480, got %d", reply.StatusCode())
	}

	reply, err = Create486Busy(req)
	if err != nil {
		t.Fatalf("Create486Busy failed: %v", err)
	}
	if reply.StatusCode() != 486 {
		t.Fatalf("expected 486, got %d", reply.StatusCode())
	}
}

func TestCreateReply_500ServerError(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create500ServerInternalError(req, "panic")
	if err != nil {
		t.Fatalf("Create500ServerInternalError failed: %v", err)
	}
	if reply.StatusCode() != 500 {
		t.Fatalf("expected 500, got %d", reply.StatusCode())
	}
}

func TestCreateReply_603Decline(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create603Decline(req)
	if err != nil {
		t.Fatalf("Create603Decline failed: %v", err)
	}
	if reply.StatusCode() != 603 {
		t.Fatalf("expected 603, got %d", reply.StatusCode())
	}
}

func TestCreateReply_401Auth(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create401Unauthorized(req,
		`Digest realm="example.com", nonce="abcdef123456", algorithm=MD5, qop="auth"`)
	if err != nil {
		t.Fatalf("Create401Unauthorized failed: %v", err)
	}
	if reply.StatusCode() != 401 {
		t.Fatalf("expected 401, got %d", reply.StatusCode())
	}
	// Ensure WWW-Authenticate header is present
	if !replyHasHeader(reply, "www-authenticate") {
		t.Fatalf("401 reply should contain WWW-Authenticate header")
	}
}

func TestCreateReply_407ProxyAuth(t *testing.T) {
	req := makeSimpleINVITE()
	reply, err := Create407ProxyAuthRequired(req,
		`Digest realm="proxy.example.com", nonce="123456", algorithm=MD5, qop="auth"`)
	if err != nil {
		t.Fatalf("Create407ProxyAuthRequired failed: %v", err)
	}
	if reply.StatusCode() != 407 {
		t.Fatalf("expected 407, got %d", reply.StatusCode())
	}
	if !replyHasHeader(reply, "proxy-authenticate") {
		t.Fatalf("407 reply should contain Proxy-Authenticate header")
	}
}

func TestCreateReply_SDPBody(t *testing.T) {
	req := makeSimpleINVITE()
	sdp := "v=0\r\no=bob 1234 5678 IN IP4 192.168.1.2\r\ns=call\r\nc=IN IP4 192.168.1.2\r\nt=0 0\r\nm=audio 49170 RTP/AVP 0 8\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\n"
	reply, err := Create200OKWithSDP(req, sdp)
	if err != nil {
		t.Fatalf("Create200OKWithSDP failed: %v", err)
	}
	if reply.StatusCode() != 200 {
		t.Fatalf("expected 200, got %d", reply.StatusCode())
	}
	// Verify content type is application/sdp
	if !replyHasHeader(reply, "content-type") {
		t.Fatalf("200 reply with SDP should contain Content-Type header")
	}
	// Verify Content-Length matches body
	if bodyBytes, ok := reply.Body.([]byte); ok {
		if len(bodyBytes) != len(sdp) {
			t.Fatalf("body length mismatch: %d vs %d", len(bodyBytes), len(sdp))
		}
	}
}

func TestCreateReply_InvalidStatusCode(t *testing.T) {
	req := makeSimpleINVITE()
	_, err := CreateReply(req, ReplyOptions{StatusCode: 99})
	if err == nil {
		t.Fatalf("expected error for invalid status code")
	}
}

// ============================================================
// Forward request tests
// ============================================================

func TestBuildForwardRequest_ViaPrepend(t *testing.T) {
	req := makeSimpleINVITE()
	fwd, err := BuildForwardRequest(req, "UDP", "10.0.0.1", 5060, "sip:next@10.0.0.1:5060")
	if err != nil {
		t.Fatalf("BuildForwardRequest failed: %v", err)
	}
	// First header should be Via (the proxy's Via)
	if len(fwd.Headers) == 0 {
		t.Fatalf("no headers on forwarded request")
	}
	if fwd.Headers[0].Type != HdrVia {
		t.Fatalf("expected Via at position 0, got type %d", fwd.Headers[0].Type)
	}
	if !containsString(fwd.Headers[0].Body.String(), "SIP/2.0/UDP 10.0.0.1:5060") {
		t.Fatalf("unexpected Via body: %q", fwd.Headers[0].Body.String())
	}
	if !containsString(fwd.Headers[0].Body.String(), "z9hG4bK") {
		t.Fatalf("expected branch prefix z9hG4bK in Via: %q", fwd.Headers[0].Body.String())
	}
}

func TestBuildForwardRequest_MaxForwards(t *testing.T) {
	req := makeSimpleINVITE()
	originalMF := 70
	_ = originalMF
	fwd, err := BuildForwardRequest(req, "UDP", "10.0.0.1", 5060, "sip:next@10.0.0.1:5060")
	if err != nil {
		t.Fatalf("BuildForwardRequest failed: %v", err)
	}
	// Max-Forwards should now be 69
	for _, h := range fwd.Headers {
		if h.Type == HdrMaxForwards {
			if h.Body.String() != "69" {
				t.Fatalf("expected Max-Forwards=69, got %q", h.Body.String())
			}
			return
		}
	}
	t.Fatalf("no Max-Forwards header on forwarded request")
}

func TestBuildForwardRequest_RURIReplaced(t *testing.T) {
	req := makeSimpleINVITE()
	fwd, err := BuildForwardRequest(req, "UDP", "10.0.0.1", 5060, "sip:next@10.0.0.1:5060")
	if err != nil {
		t.Fatalf("BuildForwardRequest failed: %v", err)
	}
	if fwd.FirstLine == nil || fwd.FirstLine.Req == nil {
		t.Fatalf("missing request line")
	}
	if fwd.FirstLine.Req.URI.String() != "sip:next@10.0.0.1:5060" {
		t.Fatalf("expected R-URI=sip:next@10.0.0.1:5060, got %q",
			fwd.FirstLine.Req.URI.String())
	}
}

func TestBuildForwardRequest_WithRecordRoute(t *testing.T) {
	req := makeSimpleINVITE()
	fwd, err := BuildForwardRequestWithRecordRoute(req,
		"UDP", "10.0.0.1", 5060, "sip:next@10.0.0.1:5060",
		"sip:proxy@10.0.0.1:5060")
	if err != nil {
		t.Fatalf("BuildForwardRequestWithRecordRoute failed: %v", err)
	}
	// Should have a Record-Route header near the top (index 1)
	if len(fwd.Headers) < 2 {
		t.Fatalf("expected at least 2 headers")
	}
	if fwd.Headers[1].Type != HdrRecordRoute {
		t.Fatalf("expected Record-Route at index 1, got type %d", fwd.Headers[1].Type)
	}
	if !containsString(fwd.Headers[1].Body.String(), "lr") {
		t.Fatalf("expected loose-route in Record-Route: %q", fwd.Headers[1].Body.String())
	}
}

func TestBuildForwardedRequest_Serialized(t *testing.T) {
	req := makeSimpleINVITE()
	out, err := BuildForwardedRequest(req, "UDP", "10.0.0.1", 5060,
		"sip:next@10.0.0.1:5060", "sip:proxy@10.0.0.1:5060")
	if err != nil {
		t.Fatalf("BuildForwardedRequest failed: %v", err)
	}
	// Out should be a valid SIP message starting with INVITE ...
	if !bytes.HasPrefix(out, []byte("INVITE")) {
		t.Fatalf("unexpected prefix: %q", string(firstNL(out)))
	}
	// Should contain SIP/2.0
	if !bytes.Contains(out, []byte("SIP/2.0")) {
		t.Fatalf("no SIP/2.0 in: %q", string(out[:150]))
	}
}

func TestBuildSimpleResponse(t *testing.T) {
	req := makeSimpleINVITE()
	out, err := BuildSimpleResponse(req, 404, "Not Found")
	if err != nil {
		t.Fatalf("BuildSimpleResponse failed: %v", err)
	}
	if !bytes.HasPrefix(out, []byte("SIP/2.0 404 Not Found\r\n")) {
		t.Fatalf("unexpected reply: %q", string(firstNL(out)))
	}
}

// ============================================================
// End-to-end INVITE call flow
// ============================================================

// TestEndToEndCallFlow simulates a basic proxy call flow:
//   - UAC -> Proxy: INVITE
//   - Proxy -> UAC : 100 Trying
//   - Proxy -> UAC: 180 Ringing
//   - Proxy -> UAC: 200 OK (with SDP)
//   - UAC -> Proxy: ACK (we just verify that ACK parsing works)
func TestEndToEndCallFlow(t *testing.T) {
	invite := makeSimpleINVITE()

	// 1. 100 Trying
	trying, err := Create100Trying(invite)
	if err != nil {
		t.Fatalf("Create100Trying: %v", err)
	}
	if trying.StatusCode() != 100 {
		t.Fatalf("expected 100, got %d", trying.StatusCode())
	}
	// Verify Via headers are preserved
	found := false
	for _, h := range trying.Headers {
		if h.Type == HdrVia {
			found = true
			break
		}
	}
	if !found {
		t.Fatalf("100 Trying should preserve Via headers")
	}

	// 2. 180 Ringing
	ringing, err := Create180Ringing(invite)
	if err != nil {
		t.Fatalf("Create180Ringing: %v", err)
	}
	if ringing.StatusCode() != 180 {
		t.Fatalf("expected 180, got %d", ringing.StatusCode())
	}

	// 3. 200 OK with SDP
	sdp := "v=0\r\no=bob 123456 789012 IN IP4 192.168.1.2\r\n" +
		"s=Test\r\nc=IN IP4 192.168.1.2\r\nt=0 0\r\n" +
		"m=audio 49170 RTP/AVP 0 8\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\n"
	twoHundred, err := Create200OKWithSDP(invite, sdp)
	if err != nil {
		t.Fatalf("Create200OKWithSDP: %v", err)
	}
	if twoHundred.StatusCode() != 200 {
		t.Fatalf("expected 200, got %d", twoHundred.StatusCode())
	}

	// 4. 200 OK round-trip through BuildMessage
	bytesMsg, err := BuildMessage(twoHundred)
	if err != nil {
		t.Fatalf("BuildMessage(200OK): %v", err)
	}
	// Re-parse the resulting 200 OK to verify round-trip validity
	reparsed, err := ParseMsg(bytesMsg)
	if err != nil {
		t.Fatalf("ParseMsg on 200 OK bytes failed: %v (bytes=%q)", err, string(bytesMsg[:200]))
	}
	if reparsed.StatusCode() != 200 {
		t.Fatalf("re-parsed status mismatch")
	}
}

// TestParseSessionTimers_CallFlow tests Session-Expires being honoured in the call
func TestCallFlowWithSessionTimers(t *testing.T) {
	inviteRaw := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.1:5060;branch=z9hG4bKabc\r\n" +
		"From: <sip:alice@example.com>;tag=abc123\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: session-timer-call-1\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Session-Expires: 1800;refresher=uas\r\n" +
		"Min-SE: 90\r\n" +
		"Max-Forwards: 70\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"
	msg, err := ParseMsg([]byte(inviteRaw))
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	// Find Session-Expires and parse
	var seBody *SessionExpiresBody
	var minSEBody *MinSEBody
	for _, h := range msg.Headers {
		if h.Type == HdrSessionExpires {
			seBody, err = ParseSessionExpires(h)
			if err != nil {
				t.Fatalf("ParseSessionExpires failed: %v", err)
			}
		}
		if h.Type == HdrMinSE {
			minSEBody, err = ParseMinSE(h)
			if err != nil {
				t.Fatalf("ParseMinSE failed: %v", err)
			}
		}
	}
	if seBody == nil {
		t.Fatalf("no Session-Expires parsed")
	}
	if seBody.Seconds != 1800 {
		t.Fatalf("expected Session-Expires=1800, got %d", seBody.Seconds)
	}
	if !seBody.IsUASRefresher() {
		t.Fatalf("expected refresher=UAS")
	}
	if minSEBody == nil {
		t.Fatalf("no Min-SE parsed")
	}
	if minSEBody.Seconds != 90 {
		t.Fatalf("expected Min-SE=90, got %d", minSEBody.Seconds)
	}
}

// ============================================================
// Utility helpers
// ============================================================

func firstNL(b []byte) []byte {
	for i, c := range b {
		if c == '\r' || c == '\n' {
			return b[:i]
		}
	}
	return b
}

func containsTag(s string) bool {
	return bytes.Contains([]byte(s), []byte(";tag="))
}

func replyHasToTag(reply *SIPMsg) bool {
	for _, h := range reply.Headers {
		if h.Type == HdrTo {
			return containsTag(h.Body.String())
		}
	}
	return false
}

func replyHasCallID(reply *SIPMsg, expected string) bool {
	for _, h := range reply.Headers {
		if h.Type == HdrCallID {
			return h.Body.String() == expected
		}
	}
	return false
}

func replyHasCSeq(reply *SIPMsg, num int, method string) bool {
	for _, h := range reply.Headers {
		if h.Type == HdrCSeq {
			body := h.Body.String()
			return containsString(body, method) &&
				containsString(body, str.Mk(strconvInt(num)).String())
		}
	}
	return false
}

func replyHasHeader(reply *SIPMsg, nameLower string) bool {
	for _, h := range reply.Headers {
		if toLowerStr(h.Name.String()) == nameLower {
			return true
		}
	}
	return false
}

func containsString(haystack, needle string) bool {
	return bytes.Contains([]byte(haystack), []byte(needle))
}

func toLowerStr(s string) string {
	b := []byte(s)
	for i := range b {
		if b[i] >= 'A' && b[i] <= 'Z' {
			b[i] += 'a' - 'A'
		}
	}
	return string(b)
}

func strconvInt(n int) string {
	// Use str package's Mk to avoid import cycle - but simpler: use a local helper
	// returning decimal representation.
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	var digits []byte
	for n > 0 {
		digits = append(digits, byte('0'+n%10))
		n /= 10
	}
	for i, j := 0, len(digits)-1; i < j; i, j = i+1, j-1 {
		digits[i], digits[j] = digits[j], digits[i]
	}
	if neg {
		return "-" + string(digits)
	}
	return string(digits)
}

func extractBody(raw []byte) (body []byte, ok bool) {
	sep := []byte("\r\n\r\n")
	idx := bytes.Index(raw, sep)
	if idx == -1 {
		return nil, false
	}
	return raw[idx+4:], true
}

func expectedContentLength(raw []byte) int {
	clKey := []byte("content-length:")
	for _, line := range bytes.Split(raw, []byte("\r\n")) {
		// Strip case-sensitively
		if len(line) > len(clKey) &&
			string(bytes.ToLower(line[:len(clKey)])) == string(clKey) {
			// Parse trailing digits
			val := bytes.TrimSpace(line[len(clKey):])
			n := 0
			for _, c := range val {
				if c < '0' || c > '9' {
					return -1
				}
				n = n*10 + int(c-'0')
			}
			return n
		}
	}
	return -1
}

// Ensure _ usage of str import to avoid no-import issues if tests get trimmed.
var _ = str.Mk

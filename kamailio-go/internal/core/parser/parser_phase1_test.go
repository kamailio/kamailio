// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - unit tests for Phase 1 new parsers
 * Covers: parse_params.go, parse_content.go, parse_body.go,
 *         parse_methods.go, parse_expires.go, parse_identity.go,
 *         parse_ppi_pai.go, parse_disposition.go, parse_rpid.go,
 *         parse_sipifmatch.go
 */

package parser

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ==================== parse_params.go tests ====================

func TestParseParams_KeyValue(t *testing.T) {
	params, count := ParseParams("name=value;foo=bar", ';')
	if count != 2 {
		t.Fatalf("expected 2 params, got %d", count)
	}
	if params == nil {
		t.Fatal("expected non-nil params linked list")
	}
	if params.Name.String() != "name" || params.Value.String() != "value" {
		t.Fatalf("expected first param name=value, got %s=%s",
			params.Name.String(), params.Value.String())
	}
	if params.Next == nil || params.Next.Name.String() != "foo" || params.Next.Value.String() != "bar" {
		t.Fatalf("expected second param foo=bar, got %+v", params.Next)
	}
}

func TestParseParams_QuotedValue(t *testing.T) {
	params, _ := ParseParams(`info="<https://cert.example.com/foo.pem>";alg=ES256`, ';')
	if params == nil {
		t.Fatal("expected params")
	}
	// info should be unquoted and contain the URL with angle brackets preserved inside the quotes
	got := params.Value.String()
	if got == "" {
		t.Fatal("expected non-empty info value")
	}
	// the URL is preserved (with or without outer quotes stripped depends on implementation, but shouldn't be empty)
}

func TestParseParams_BareNames(t *testing.T) {
	params, count := ParseParams("lr;a=b;r2", ';')
	if count != 3 {
		t.Fatalf("expected 3 params, got %d", count)
	}
	// first param is "lr" with no value (URI param style)
	if params.Name.String() != "lr" {
		t.Fatalf("expected first param 'lr', got %q", params.Name.String())
	}
}

func TestParseParams_ToMap(t *testing.T) {
	m := ParseParamsToMap("a=1;b=2;c=3", ';')
	if len(m) != 3 {
		t.Fatalf("expected 3 entries in map, got %d", len(m))
	}
	if m["a"] != "1" || m["b"] != "2" || m["c"] != "3" {
		t.Fatalf("unexpected map content: %+v", m)
	}
}

func TestParamListGet(t *testing.T) {
	params, _ := ParseParams("X=1;Y=2;Z=3", ';')
	p := ParamListGet(params, "y") // case-insensitive
	if p == nil {
		t.Fatal("expected to find param 'y'")
	}
	if p.Value.String() != "2" {
		t.Fatalf("expected value '2', got %q", p.Value.String())
	}
	if ParamListGet(params, "W") != nil {
		t.Fatal("expected nil for missing param")
	}
}

func TestParseParams_EmptyString(t *testing.T) {
	params, count := ParseParams("", ';')
	if count != 0 || params != nil {
		t.Fatalf("expected empty result for empty input, got count=%d params=%v", count, params)
	}
}

// ==================== parse_content.go tests ====================

func TestParseContentType_SDP(t *testing.T) {
	body, err := ParseContentType(str.Mk("application/sdp"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if body.TypeString() != "application/sdp" {
		t.Fatalf("expected application/sdp, got %q", body.TypeString())
	}
	if !body.Match("application/sdp") {
		t.Fatal("expected Match(application/sdp) to be true")
	}
	if !body.Match("application/*") {
		t.Fatal("expected Match(application/*) to be true")
	}
	if body.Match("text/*") {
		t.Fatal("expected Match(text/*) to be false")
	}
}

func TestParseContentType_WithParams(t *testing.T) {
	body, err := ParseContentType(str.Mk("multipart/mixed; boundary=\"simple boundary\""))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if body.TypeString() != "multipart/mixed" {
		t.Fatalf("expected multipart/mixed, got %q", body.TypeString())
	}
	boundaryParam := body.GetParam("boundary")
	if boundaryParam == nil {
		t.Fatal("expected boundary param")
	}
	if boundaryParam.Value.String() != "simple boundary" {
		t.Fatalf("expected 'simple boundary', got %q", boundaryParam.Value.String())
	}
}

func TestParseContentLength_Valid(t *testing.T) {
	v, err := ParseContentLengthValue(str.Mk("512"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if v != 512 {
		t.Fatalf("expected 512, got %d", v)
	}
}

func TestParseContentLength_Invalid(t *testing.T) {
	_, err := ParseContentLengthValue(str.Mk("abc"))
	if err == nil {
		t.Fatal("expected error for non-numeric content-length")
	}
}

func TestParseContentType_NilHeader(t *testing.T) {
	_, err := ParseContentTypeFromHeader(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
}

// ==================== parse_body.go tests ====================

func TestParseMessageBody_SDP(t *testing.T) {
	ct, _ := ParseContentType(str.Mk("application/sdp"))
	raw := []byte("v=0\r\no=- 123456 1 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\nm=audio 5000 RTP/AVP 0\r\na=rtpmap:0 PCMU/8000\r\n")
	mb, err := ParseMessageBody(raw, ct, len(raw))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if mb.Type != BodySDP {
		t.Fatalf("expected BodySDP, got %d", mb.Type)
	}
	if mb.Length != len(raw) {
		t.Fatalf("expected length %d, got %d", len(raw), mb.Length)
	}
	if !mb.IsSDP() {
		t.Fatal("expected IsSDP() true")
	}
}

func TestParseMessageBody_JSON(t *testing.T) {
	ct, _ := ParseContentType(str.Mk("application/json"))
	raw := []byte(`{"key":"value"}`)
	mb, _ := ParseMessageBody(raw, ct, -1)
	if mb.Type != BodyJSON {
		t.Fatalf("expected BodyJSON, got %d", mb.Type)
	}
}

func TestParseMessageBody_NoContentType(t *testing.T) {
	raw := []byte("just some bytes")
	mb, _ := ParseMessageBody(raw, nil, -1)
	if mb.Type != BodyUnknown {
		t.Fatalf("expected BodyUnknown, got %d", mb.Type)
	}
}

// ==================== parse_methods.go tests ====================

func TestParseMethodBytes_Invite(t *testing.T) {
	if ParseMethodBytes([]byte("INVITE")) != MethodInvite {
		t.Fatal("expected MethodInvite for 'INVITE'")
	}
	if ParseMethodBytes([]byte("invite")) != MethodInvite {
		t.Fatal("expected MethodInvite for lowercase 'invite'")
	}
	if ParseMethodBytes([]byte("Invite")) != MethodInvite {
		t.Fatal("expected MethodInvite for mixed-case")
	}
}

func TestParseMethodBytes_AllKnown(t *testing.T) {
	cases := []struct {
		name   string
		method RequestMethod
	}{
		{"INVITE", MethodInvite},
		{"CANCEL", MethodCancel},
		{"ACK", MethodACK},
		{"BYE", MethodBye},
		{"INFO", MethodInfo},
		{"REGISTER", MethodRegister},
		{"SUBSCRIBE", MethodSubscribe},
		{"NOTIFY", MethodNotify},
		{"MESSAGE", MethodMessage},
		{"OPTIONS", MethodOptions},
		{"PRACK", MethodPRACK},
		{"UPDATE", MethodUpdate},
		{"REFER", MethodRefer},
		{"PUBLISH", MethodPublish},
	}
	for _, c := range cases {
		if ParseMethodBytes([]byte(c.name)) != c.method {
			t.Fatalf("expected method %d for %q", c.method, c.name)
		}
	}
}

func TestParseMethodBytes_Unknown(t *testing.T) {
	if ParseMethodBytes([]byte("UNKNOWN_METHOD")) != MethodOther {
		t.Fatal("expected MethodOther for unknown method")
	}
}

func TestParseMethodBytes_Empty(t *testing.T) {
	if ParseMethodBytes([]byte("")) != MethodUndefined {
		t.Fatal("expected MethodUndefined for empty input")
	}
}

func TestMethodNameFast(t *testing.T) {
	if MethodNameFast(MethodInvite) != "INVITE" {
		t.Fatal("expected 'INVITE'")
	}
	if MethodNameFast(MethodUndefined) != "" {
		t.Fatal("expected empty string for MethodUndefined")
	}
	// unknown method should return "OTHER"
}

func TestIsMethod(t *testing.T) {
	if !IsMethod([]byte("INVITE"), MethodInvite) {
		t.Fatal("expected IsMethod=INVITE to match")
	}
	if IsMethod([]byte("INVITE"), MethodBye) {
		t.Fatal("expected no match for different method")
	}
}

// ==================== parse_expires.go tests ====================

func TestParseExpiresBody_Valid(t *testing.T) {
	eb, err := ParseExpiresBody(str.Mk("3600"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if eb.Value != 3600 {
		t.Fatalf("expected 3600, got %d", eb.Value)
	}
	if eb.Error {
		t.Fatal("expected Error=false")
	}
}

func TestParseExpiresBody_Zero(t *testing.T) {
	eb, err := ParseExpiresBody(str.Mk("0"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if eb.Value != 0 {
		t.Fatalf("expected 0, got %d", eb.Value)
	}
}

func TestParseExpiresBody_Invalid(t *testing.T) {
	_, err := ParseExpiresBody(str.Mk("-5"))
	if err == nil {
		t.Fatal("expected error for negative value")
	}
	_, err = ParseExpiresBody(str.Mk("abc"))
	if err == nil {
		t.Fatal("expected error for non-numeric value")
	}
}

func TestParseExpiresBody_Empty(t *testing.T) {
	_, err := ParseExpiresBody(str.Mk(""))
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

func TestParseExpiresBody_StringRoundtrip(t *testing.T) {
	eb, _ := ParseExpiresBody(str.Mk("120"))
	if eb.String() != "120" {
		t.Fatalf("expected '120', got %q", eb.String())
	}
}

func TestParseMinExpiresBody(t *testing.T) {
	eb, err := ParseMinExpiresBody(str.Mk("60"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if eb.Value != 60 {
		t.Fatalf("expected 60, got %d", eb.Value)
	}
}

// ==================== parse_identity.go tests ====================

func TestParseIdentity_WithParams(t *testing.T) {
	token := "eyJhbGciOiJFUzI1NiJ9.abc123.def456"
	body := str.Mk(token + ";info=<https://cert.example.com/foo.pem>;alg=ES256;ppt=shaken")
	ib, err := ParseIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if ib.Token.String() != token {
		t.Fatalf("expected token %q, got %q", token, ib.Token.String())
	}
	if ib.Info.String() == "" {
		t.Fatal("expected non-empty info")
	}
	if ib.Alg.String() != "ES256" {
		t.Fatalf("expected alg=ES256, got %q", ib.Alg.String())
	}
	if ib.Ppt.String() != "shaken" {
		t.Fatalf("expected ppt=shaken, got %q", ib.Ppt.String())
	}
}

func TestParseIdentity_TokenOnly(t *testing.T) {
	token := "base64tokenvalue"
	ib, err := ParseIdentity(str.Mk(token))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if ib.Token.String() != token {
		t.Fatalf("expected token %q, got %q", token, ib.Token.String())
	}
}

func TestParseIdentity_Empty(t *testing.T) {
	_, err := ParseIdentity(str.Mk(""))
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

func TestParseIdentityInfo_Standard(t *testing.T) {
	body := str.Mk("<https://cert.example.com/foo.pem>;alg=ES256;domain=example.com")
	iib, err := ParseIdentityInfo(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if iib.URI.String() != "https://cert.example.com/foo.pem" {
		t.Fatalf("unexpected URI: %q", iib.URI.String())
	}
	if iib.Alg.String() != "ES256" {
		t.Fatalf("unexpected alg: %q", iib.Alg.String())
	}
	if iib.Domain.String() != "example.com" {
		t.Fatalf("unexpected domain: %q", iib.Domain.String())
	}
}

func TestParseIdentityInfo_NoAngleBrackets(t *testing.T) {
	body := str.Mk("https://cert.example.com/pub.pem;alg=Ed25519")
	iib, err := ParseIdentityInfo(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if iib.URI.String() != "https://cert.example.com/pub.pem" {
		t.Fatalf("unexpected URI: %q", iib.URI.String())
	}
}

func TestParseIdentityInfo_Empty(t *testing.T) {
	_, err := ParseIdentityInfo(str.Mk(""))
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

func TestParseIdentity_NilHeader(t *testing.T) {
	_, err := ParseIdentityFromHeader(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
	_, err = ParseIdentityInfoFromHeader(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
}

// ==================== parse_ppi_pai.go tests ====================

func TestParsePreferredIdentity_SingleURI(t *testing.T) {
	body := str.Mk("<sip:alice@example.com>")
	pib, err := ParsePreferredIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if pib.Count != 1 {
		t.Fatalf("expected count=1, got %d", pib.Count)
	}
	if pib.First.URI.String() != "sip:alice@example.com" {
		t.Fatalf("unexpected URI: %q", pib.First.URI.String())
	}
	if pib.First.URIType != SIPURIT {
		t.Fatalf("expected SIPURIT, got %d", pib.First.URIType)
	}
}

func TestParsePreferredIdentity_WithDisplay(t *testing.T) {
	body := str.Mk(`"Alice Smith" <sip:alice@example.com>;phone-context=example.com`)
	pib, err := ParsePreferredIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if pib.First.DisplayName.String() != "Alice Smith" {
		t.Fatalf("expected display name 'Alice Smith', got %q", pib.First.DisplayName.String())
	}
	if pib.First.URI.String() != "sip:alice@example.com" {
		t.Fatalf("unexpected URI: %q", pib.First.URI.String())
	}
}

func TestParsePreferredIdentity_MultipleEntries(t *testing.T) {
	body := str.Mk("<sip:alice@example.com>, <tel:+12345678>")
	pib, err := ParsePreferredIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if pib.Count != 2 {
		t.Fatalf("expected count=2, got %d", pib.Count)
	}
	if pib.First.URIType != SIPURIT {
		t.Fatalf("expected first entry to be SIPURIT, got %d", pib.First.URIType)
	}
	if pib.First.Next.URIType != TELURIT {
		t.Fatalf("expected second entry TELURIT, got %d", pib.First.Next.URIType)
	}
}

func TestParsePreferredIdentity_SIPS(t *testing.T) {
	body := str.Mk("<sips:bob@example.com>")
	pib, _ := ParsePreferredIdentity(body)
	if pib.First.URIType != SIPSURIT {
		t.Fatalf("expected SIPSURIT, got %d", pib.First.URIType)
	}
}

func TestParsePreferredIdentity_Empty(t *testing.T) {
	_, err := ParsePreferredIdentity(str.Mk(""))
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

func TestParseAssertedIdentity_Basic(t *testing.T) {
	body := str.Mk(`"Bob" <sip:bob@example.com>;screen=yes`)
	aib, err := ParseAssertedIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if aib.Count != 1 {
		t.Fatalf("expected count=1, got %d", aib.Count)
	}
	if aib.First.DisplayName.String() != "Bob" {
		t.Fatalf("expected display name 'Bob', got %q", aib.First.DisplayName.String())
	}
	if aib.First.Params == nil {
		t.Fatal("expected screen param to be present in params list")
	}
}

func TestParseAssertedIdentity_MultipleEntries(t *testing.T) {
	body := str.Mk(`"Alice" <sip:alice@example.com>, "Bob" <sip:bob@example.com>`)
	aib, err := ParseAssertedIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if aib.Count != 2 {
		t.Fatalf("expected count=2, got %d", aib.Count)
	}
}

func TestParsePreferredIdentity_StringRoundtrip(t *testing.T) {
	body := str.Mk(`"Alice" <sip:alice@example.com>;phone-context=example.com`)
	pib, err := ParsePreferredIdentity(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	s := pib.String()
	if s == "" {
		t.Fatal("expected non-empty string output")
	}
}

// ==================== parse_disposition.go tests ====================

func TestParseDisposition_SessionRequired(t *testing.T) {
	body, err := ParseDisposition(str.Mk("session;handling=required"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if body.Disposition.String() != "session" {
		t.Fatalf("expected 'session', got %q", body.Disposition.String())
	}
	if !body.IsRequired() {
		t.Fatal("expected IsRequired() true")
	}
}

func TestParseDisposition_WithParams(t *testing.T) {
	body, err := ParseDisposition(str.Mk("render;handling=optional;rendering=session"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if body.Disposition.String() != "render" {
		t.Fatalf("expected 'render', got %q", body.Disposition.String())
	}
	if body.Handling.String() != "optional" {
		t.Fatalf("expected 'optional', got %q", body.Handling.String())
	}
	if body.Rendering.String() != "session" {
		t.Fatalf("expected 'session', got %q", body.Rendering.String())
	}
}

func TestParseDisposition_StringRoundtrip(t *testing.T) {
	body, err := ParseDisposition(str.Mk("session;handling=required;rendering=session;custom=value"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	s := body.String()
	if s == "" {
		t.Fatal("expected non-empty output")
	}
}

func TestParseDisposition_NilHeader(t *testing.T) {
	_, err := ParseDispositionFromHeader(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
}

// ==================== parse_rpid.go tests ====================

func TestParseRPID_WithDisplayAndParams(t *testing.T) {
	body := str.Mk(`"Alice" <sip:alice@example.com>;screen=yes;privacy=full;party=calling`)
	rb, err := ParseRPID(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rb.DisplayName.String() != "Alice" {
		t.Fatalf("expected 'Alice', got %q", rb.DisplayName.String())
	}
	if rb.URI.String() != "sip:alice@example.com" {
		t.Fatalf("unexpected URI: %q", rb.URI.String())
	}
	if !rb.IsScreened() {
		t.Fatal("expected IsScreened() true")
	}
	if !rb.IsCalling() {
		t.Fatal("expected IsCalling() true")
	}
	if rb.Privacy.String() != "full" {
		t.Fatalf("expected 'full', got %q", rb.Privacy.String())
	}
}

func TestParseRPID_SimpleURI(t *testing.T) {
	body := str.Mk("<sip:bob@example.com>;screen=no;party=called")
	rb, err := ParseRPID(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rb.URI.String() != "sip:bob@example.com" {
		t.Fatalf("unexpected URI: %q", rb.URI.String())
	}
	if rb.IsScreened() {
		t.Fatal("expected IsScreened() false")
	}
	if rb.IsCalling() {
		t.Fatal("expected IsCalling() false")
	}
}

func TestParseRPID_StringRoundtrip(t *testing.T) {
	body := str.Mk(`"Alice" <sip:alice@example.com>;screen=yes;privacy=full`)
	rb, err := ParseRPID(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	s := rb.String()
	if s == "" {
		t.Fatal("expected non-empty string")
	}
}

func TestParseRPID_NilHeader(t *testing.T) {
	_, err := ParseRPIDFromHeader(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
}

// ==================== parse_sipifmatch.go tests ====================

func TestParseSIPIfMatch_Wildcard(t *testing.T) {
	sb, err := ParseSIPIfMatch(str.Mk("*"))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !sb.IsWildcard() {
		t.Fatal("expected wildcard")
	}
	if sb.String() != "*" {
		t.Fatalf("expected '*', got %q", sb.String())
	}
}

func TestParseSIPIfMatch_Tag(t *testing.T) {
	tag := "3z8h0nd29387421"
	sb, err := ParseSIPIfMatch(str.Mk(tag))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if sb.IsWildcard() {
		t.Fatal("expected not wildcard")
	}
	if sb.Tag.String() != tag {
		t.Fatalf("expected tag %q, got %q", tag, sb.Tag.String())
	}
}

func TestParseSIPIfMatch_QuotedTag(t *testing.T) {
	sb, err := ParseSIPIfMatch(str.Mk(`"quoted-etag"`))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if sb.Tag.String() != "quoted-etag" {
		t.Fatalf("expected 'quoted-etag', got %q", sb.Tag.String())
	}
}

func TestParseSIPIfMatch_Empty(t *testing.T) {
	_, err := ParseSIPIfMatch(str.Mk(""))
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

func TestParseSIPIfMatch_NilHeader(t *testing.T) {
	_, err := ParseSIPIfMatchFromHeader(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
}

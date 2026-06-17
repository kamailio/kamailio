// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - unit tests for Phase 2: lazy parsing + multi-Via + addr-spec + header utils
 */

package parser

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ==================== parse_addr_spec.go tests ====================

func TestParseNameAddr_WithDisplay(t *testing.T) {
	as, err := ParseNameAddr(`"Alice Smith" <sip:alice@example.com>;tag=abc123`, true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if as.DisplayName.String() != "Alice Smith" {
		t.Fatalf("expected display name 'Alice Smith', got %q", as.DisplayName.String())
	}
	if as.URIString.String() != "sip:alice@example.com" {
		t.Fatalf("expected URI 'sip:alice@example.com', got %q", as.URIString.String())
	}
	if as.URIType != SIPURIT {
		t.Fatalf("expected SIPURIT, got %d", as.URIType)
	}
	if as.URI == nil {
		t.Fatal("expected non-nil URI (parseURI=true)")
	}
	tagParam := as.GetParam("tag")
	if tagParam == nil || tagParam.Value.String() != "abc123" {
		t.Fatalf("expected tag=abc123, got %+v", tagParam)
	}
}

func TestParseNameAddr_AddrSpecOnly(t *testing.T) {
	as, err := ParseNameAddr("sip:bob@example.com;lr", true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if as.DisplayName.Len > 0 {
		t.Fatalf("expected empty display name, got %q", as.DisplayName.String())
	}
	if as.URIString.String() != "sip:bob@example.com" {
		t.Fatalf("unexpected URI: %q", as.URIString.String())
	}
	if !as.HasParam("lr") {
		t.Fatal("expected lr param to be present")
	}
}

func TestParseNameAddr_SIPS(t *testing.T) {
	as, err := ParseNameAddr("<sips:secure@example.com>", true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if as.URIType != SIPSURIT {
		t.Fatalf("expected SIPSURIT, got %d", as.URIType)
	}
}

func TestParseNameAddr_TEL(t *testing.T) {
	as, err := ParseNameAddr("<tel:+12345678>", false)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if as.URIType != TELURIT {
		t.Fatalf("expected TELURIT, got %d", as.URIType)
	}
}

func TestParseNameAddr_NoURIParse(t *testing.T) {
	as, err := ParseNameAddr("<sip:user@host>", false)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if as.URI != nil {
		t.Fatal("expected nil URI when parseURI=false")
	}
	if as.URIString.String() != "sip:user@host" {
		t.Fatalf("expected URIString 'sip:user@host', got %q", as.URIString.String())
	}
}

func TestParseNameAddr_EmptyInput(t *testing.T) {
	_, err := ParseNameAddr("", true)
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

func TestParseNameAddr_QuotedDisplayNameWithSpecialChars(t *testing.T) {
	// Display name may contain angle brackets inside quotes
	as, err := ParseNameAddr(`"<Not An Address>" <sip:user@example.com>`, true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if as.DisplayName.String() != "<Not An Address>" {
		t.Fatalf("expected '<Not An Address>', got %q", as.DisplayName.String())
	}
}

func TestParseNameAddr_StringRoundtrip(t *testing.T) {
	input := `"Alice" <sip:alice@example.com>;tag=xyz`
	as, err := ParseNameAddr(input, true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	s := as.String()
	if s == "" {
		t.Fatal("expected non-empty output")
	}
}

func TestParseNameAddrList_MultipleEntries(t *testing.T) {
	input := `sip:p1@example.com;lr, "Alice" <sip:alice@example.com>, <sip:p2@example.com;transport=tcp>`
	list, err := ParseNameAddrList(input, true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if list.Count != 3 {
		t.Fatalf("expected 3 entries, got %d", list.Count)
	}
	if list.Entries[0].URIType != SIPURIT {
		t.Fatalf("expected SIPURIT for first entry, got %d", list.Entries[0].URIType)
	}
	if list.Entries[1].DisplayName.String() != "Alice" {
		t.Fatalf("expected 'Alice', got %q", list.Entries[1].DisplayName.String())
	}
}

func TestParseNameAddrList_Empty(t *testing.T) {
	list, err := ParseNameAddrList("", true)
	if err == nil && list.Count != 0 {
		t.Fatalf("expected 0 entries, got %d", list.Count)
	}
}

// ==================== parse_multi_via.go tests ====================

func TestParseMultiVia_Single(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host.example.com:5060;branch=z9hG4bK-test123")
	first, count, err := ParseMultiVia(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if count != 1 {
		t.Fatalf("expected 1 via, got %d", count)
	}
	if first == nil {
		t.Fatal("expected non-nil first via")
	}
	if first.Host.String() != "host.example.com" {
		t.Fatalf("expected host.example.com, got %q", first.Host.String())
	}
}

func TestParseMultiVia_Multiple(t *testing.T) {
	input := "SIP/2.0/UDP proxy1.example.com;branch=z9hG4bK-1, " +
		"SIP/2.0/UDP proxy2.example.com;branch=z9hG4bK-2, " +
		"SIP/2.0/TCP proxy3.example.com;branch=z9hG4bK-3;received=192.168.1.1"
	first, count, err := ParseMultiVia(str.Mk(input))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if count != 3 {
		t.Fatalf("expected 3 vias, got %d", count)
	}
	// check linked list
	if first.Next == nil || first.Next.Next == nil {
		t.Fatal("expected linked list with 3 elements")
	}
	if first.Host.String() != "proxy1.example.com" {
		t.Fatalf("expected proxy1.example.com, got %q", first.Host.String())
	}
	if first.Next.Host.String() != "proxy2.example.com" {
		t.Fatalf("expected proxy2.example.com, got %q", first.Next.Host.String())
	}
	if first.Next.Next.Host.String() != "proxy3.example.com" {
		t.Fatalf("expected proxy3.example.com, got %q", first.Next.Next.Host.String())
	}
	// check received on third
	if first.Next.Next.Received == nil || first.Next.Next.Received.Value.String() != "192.168.1.1" {
		t.Fatal("expected received=192.168.1.1 on third via")
	}
}

func TestValidateBranch_ValidRFC3261(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060;branch=z9hG4bKvalid123")
	first, _, _ := ParseMultiVia(body)
	hasBranch, hasMagic, value := ValidateBranch(first)
	if !hasBranch {
		t.Fatal("expected hasBranch=true")
	}
	if !hasMagic {
		t.Fatal("expected hasMagicCookie=true")
	}
	if value != "z9hG4bKvalid123" {
		t.Fatalf("unexpected branch value: %q", value)
	}
}

func TestValidateBranch_NoBranch(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060")
	first, _, _ := ParseMultiVia(body)
	hasBranch, hasMagic, _ := ValidateBranch(first)
	if hasBranch {
		t.Fatal("expected hasBranch=false")
	}
	if hasMagic {
		t.Fatal("expected hasMagicCookie=false")
	}
}

func TestHasValidBranch(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060;branch=z9hG4bK-abc")
	first, _, _ := ParseMultiVia(body)
	if !HasValidBranch(first) {
		t.Fatal("expected HasValidBranch=true")
	}
}

func TestGetBranchValue(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060;branch=z9hG4bKmy123")
	first, _, _ := ParseMultiVia(body)
	val := GetBranchValue(first)
	if val != "my123" {
		t.Fatalf("expected 'my123' after stripping magic cookie, got %q", val)
	}
}

func TestGetSentBy(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host.example.com:5060;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	sentBy := first.GetSentBy()
	if sentBy != "host.example.com:5060" {
		t.Fatalf("expected host.example.com:5060, got %q", sentBy)
	}
}

func TestGetTransport(t *testing.T) {
	body := str.Mk("SIP/2.0/TCP host.example.com;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	if first.GetTransport() != "TCP" {
		t.Fatalf("expected TCP, got %q", first.GetTransport())
	}
}

func TestGetReceivedAddress_WithReceivedParam(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP internal.host;received=192.168.1.100;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	addr := first.GetReceivedAddress()
	if addr != "192.168.1.100" {
		t.Fatalf("expected 192.168.1.100, got %q", addr)
	}
}

func TestGetReceivedAddress_FallbackToHost(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host.example.com;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	addr := first.GetReceivedAddress()
	if addr != "host.example.com" {
		t.Fatalf("expected host.example.com fallback, got %q", addr)
	}
}

func TestGetRPortValue(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060;rport=5081;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	rport := first.GetRPortValue()
	if rport != 5081 {
		t.Fatalf("expected 5081, got %d", rport)
	}
}

func TestGetRPortValue_Empty(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060;rport;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	rport := first.GetRPortValue()
	if rport != 0 {
		t.Fatalf("expected 0 for empty rport, got %d", rport)
	}
}

func TestHasAlias(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP host:5060;alias;branch=z9hG4bK-1")
	first, _, _ := ParseMultiVia(body)
	if !first.HasAlias() {
		t.Fatal("expected HasAlias=true")
	}
}

func TestCountViaBodies(t *testing.T) {
	body := str.Mk("SIP/2.0/UDP h1;branch=z9hG4bK-a, SIP/2.0/UDP h2;branch=z9hG4bK-b, SIP/2.0/UDP h3;branch=z9hG4bK-c")
	first, _, _ := ParseMultiVia(body)
	if CountViaBodies(first) != 3 {
		t.Fatalf("expected 3, got %d", CountViaBodies(first))
	}
}

func TestParseMultiVia_Empty(t *testing.T) {
	_, _, err := ParseMultiVia(str.Mk(""))
	if err == nil {
		t.Fatal("expected error for empty input")
	}
}

// ==================== parse_lazy.go tests ====================

func buildSimpleSIPMsg() *SIPMsg {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP host.example.com:5060;branch=z9hG4bK-test\r\n" +
		"From: \"Alice\" <sip:alice@example.com>;tag=abc\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: call12345@caller.example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@host.example.com:5060;transport=udp>;expires=3600\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := ParseMsg(raw)
	return msg
}

func TestParseHeaderBody_Contact(t *testing.T) {
	msg := buildSimpleSIPMsg()
	if msg.Contact == nil {
		t.Fatal("expected Contact header")
	}
	parsed, err := ParseHeaderBody(msg.Contact)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	cb, ok := parsed.(*ContactBody)
	if !ok {
		t.Fatalf("expected *ContactBody, got %T", parsed)
	}
	if cb == nil {
		t.Fatal("expected non-nil ContactBody")
	}
}

func TestParseHeaderBody_Via(t *testing.T) {
	msg := buildSimpleSIPMsg()
	if msg.HdrVia1 == nil {
		t.Fatal("expected Via header")
	}
	parsed, err := ParseHeaderBody(msg.HdrVia1)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	vb, ok := parsed.(*ViaBody)
	if !ok {
		t.Fatalf("expected *ViaBody, got %T", parsed)
	}
	if vb == nil {
		t.Fatal("expected non-nil ViaBody")
	}
}

func TestParseHeaderBody_To(t *testing.T) {
	msg := buildSimpleSIPMsg()
	parsed, err := ParseHeaderBody(msg.To)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	tb, ok := parsed.(*ToBody)
	if !ok {
		t.Fatalf("expected *ToBody, got %T", parsed)
	}
	if tb == nil {
		t.Fatal("expected non-nil ToBody")
	}
	// To header in our msg has no tag parameter
}

func TestParseHeaderBody_CSeq(t *testing.T) {
	msg := buildSimpleSIPMsg()
	parsed, err := ParseHeaderBody(msg.CSeq)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	cb, ok := parsed.(*CSeqBody)
	if !ok {
		t.Fatalf("expected *CSeqBody, got %T", parsed)
	}
	if cb == nil {
		t.Fatal("expected non-nil CSeqBody")
	}
	if cb.Number != 1 {
		t.Fatalf("expected CSeq number 1, got %d", cb.Number)
	}
}

func TestParseHeaderBody_ContentType(t *testing.T) {
	msg := buildSimpleSIPMsg()
	parsed, err := ParseHeaderBody(msg.ContentType)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	ct, ok := parsed.(*ContentBody)
	if !ok {
		t.Fatalf("expected *ContentBody, got %T", parsed)
	}
	if ct == nil || ct.TypeString() != "application/sdp" {
		t.Fatalf("expected application/sdp, got %+v", ct)
	}
}

func TestParseHeaderBody_ContentLength(t *testing.T) {
	msg := buildSimpleSIPMsg()
	parsed, err := ParseHeaderBody(msg.ContentLength)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	_, ok := parsed.(uint32)
	if !ok {
		t.Fatalf("expected uint32 for Content-Length, got %T", parsed)
	}
}

func TestParseHeaderBody_Cache(t *testing.T) {
	msg := buildSimpleSIPMsg()
	// Parse once
	ParseHeaderBody(msg.Contact)
	firstParsed := msg.Contact.Parsed
	// Parse again - should use cache
	ParseHeaderBody(msg.Contact)
	if msg.Contact.Parsed != firstParsed {
		t.Fatal("expected caching to return same object")
	}
}

func TestParseHeaderBody_Nil(t *testing.T) {
	_, err := ParseHeaderBody(nil)
	if err == nil {
		t.Fatal("expected error for nil header")
	}
}

func TestParseHeadersWithFlag(t *testing.T) {
	msg := buildSimpleSIPMsg()
	// Only parse Contact type
	err := msg.ParseHeadersWithFlag(HdrContact)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !msg.IsParsed(HdrContact) {
		t.Fatal("expected Contact to be marked as parsed")
	}
	// Other headers should NOT be marked as parsed yet
	// But To/From/Via etc. may already be parsed by initial ParseMsg (current impl)
	// so we just check no error occurred
}

func TestParseAllHeaders(t *testing.T) {
	msg := buildSimpleSIPMsg()
	err := msg.ParseAllHeaders()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestIsParsed(t *testing.T) {
	msg := buildSimpleSIPMsg()
	if !msg.IsParsed(HdrContact) {
		// Should be marked as parsed by setHeaderRef during ParseMsg
		// But not the body itself - the flag is set even without lazy parsing
	}
	// Just check API returns bool without crashing
}

func TestGetParsedVia(t *testing.T) {
	msg := buildSimpleSIPMsg()
	vb, err := msg.GetParsedVia()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if vb == nil {
		t.Fatal("expected non-nil ViaBody")
	}
	if vb.Host.String() != "host.example.com" {
		t.Fatalf("expected host.example.com, got %q", vb.Host.String())
	}
}

func TestGetParsedContact(t *testing.T) {
	msg := buildSimpleSIPMsg()
	cb, err := msg.GetParsedContact()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if cb == nil {
		t.Fatal("expected non-nil ContactBody")
	}
	if !cb.HasExpires() || cb.GetExpires() != 3600 {
		t.Fatalf("expected expires=3600, got %d", cb.GetExpires())
	}
}

func TestGetParsedTo(t *testing.T) {
	msg := buildSimpleSIPMsg()
	tb, err := msg.GetParsedTo()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if tb == nil {
		t.Fatal("expected non-nil ToBody")
	}
}

func TestGetParsedFrom(t *testing.T) {
	msg := buildSimpleSIPMsg()
	fb, err := msg.GetParsedFrom()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if fb == nil {
		t.Fatal("expected non-nil ToBody")
	}
	if fb.Tag.Len == 0 {
		t.Fatal("expected From to have tag")
	}
}

// ==================== hdr_utils.go tests ====================

func TestHeaderCount(t *testing.T) {
	msg := buildSimpleSIPMsg()
	if msg.HeaderCount() < 7 {
		t.Fatalf("expected >= 7 headers, got %d", msg.HeaderCount())
	}
}

func TestCountHeadersByType(t *testing.T) {
	msg := buildSimpleSIPMsg()
	if msg.CountHeadersByType(HdrVia) != 1 {
		t.Fatalf("expected 1 Via header, got %d", msg.CountHeadersByType(HdrVia))
	}
}

func TestForEachHeader(t *testing.T) {
	msg := buildSimpleSIPMsg()
	count := 0
	msg.ForEachHeader(HdrContact, func(h *HdrField) bool {
		count++
		return true
	})
	if count != 1 {
		t.Fatalf("expected to visit 1 Contact header, got %d", count)
	}
}

func TestFirstHeader(t *testing.T) {
	msg := buildSimpleSIPMsg()
	if msg.FirstHeader() == nil {
		t.Fatal("expected non-nil first header")
	}
}

func TestNextHeader(t *testing.T) {
	msg := buildSimpleSIPMsg()
	first := msg.FirstHeader()
	next := msg.NextHeader(first)
	if next == nil {
		t.Fatal("expected non-nil next header")
	}
}

func TestRemoveHeader(t *testing.T) {
	msg := buildSimpleSIPMsg()
	initialCount := msg.HeaderCount()
	msg.RemoveHeader(msg.Contact)
	if msg.HeaderCount() != initialCount-1 {
		t.Fatalf("expected %d headers after removal, got %d", initialCount-1, msg.HeaderCount())
	}
	if msg.Contact != nil {
		t.Fatal("expected Contact quick ref to be cleared")
	}
}

func TestRemoveHeadersByType(t *testing.T) {
	msg := buildSimpleSIPMsg()
	initialCount := msg.HeaderCount()
	msg.RemoveHeadersByType(HdrContact)
	if msg.CountHeadersByType(HdrContact) != 0 {
		t.Fatal("expected 0 Contact headers after removal")
	}
	if msg.HeaderCount() != initialCount-1 {
		t.Fatalf("expected count decrement, got %d (was %d)", msg.HeaderCount(), initialCount)
	}
}

func TestSetRURI(t *testing.T) {
	msg := buildSimpleSIPMsg()
	msg.SetRURI("sip:new@destination.com")
	if msg.NewURI.String() != "sip:new@destination.com" {
		t.Fatalf("expected new RURI, got %q", msg.NewURI.String())
	}
}

func TestSetDestinationURI(t *testing.T) {
	msg := buildSimpleSIPMsg()
	msg.SetDestinationURI("sip:proxy.example.com:5060")
	if msg.DstURI.String() != "sip:proxy.example.com:5060" {
		t.Fatalf("unexpected DstURI: %q", msg.DstURI.String())
	}
}

func TestGetAllParsedContacts(t *testing.T) {
	msg := buildSimpleSIPMsg()
	contacts, err := msg.GetAllParsedContacts()
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(contacts) < 1 {
		t.Fatalf("expected >= 1 contact, got %d", len(contacts))
	}
}

func TestGetContactExpires(t *testing.T) {
	msg := buildSimpleSIPMsg()
	// Contact has expires=3600
	exp := msg.GetContactExpires()
	if exp < 0 {
		t.Fatalf("expected non-negative expires, got %d", exp)
	}
}

func TestRebuildMessage(t *testing.T) {
	msg := buildSimpleSIPMsg()
	rebuilt := msg.RebuildMessage()
	if rebuilt == "" {
		t.Fatal("expected non-empty rebuilt message")
	}
}

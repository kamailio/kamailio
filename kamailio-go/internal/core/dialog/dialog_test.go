// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for the dialog management module (Phase 6-1).
 */

package dialog

import (
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ---------------------------------------------------------------------------
// test helpers
// ---------------------------------------------------------------------------

// hdrType resolves a header name to its exported HdrType constant.
func hdrType(name string) parser.HdrType {
	switch strings.ToLower(name) {
	case "via":
		return parser.HdrVia
	case "from":
		return parser.HdrFrom
	case "to":
		return parser.HdrTo
	case "call-id", "call_id", "callid":
		return parser.HdrCallID
	case "cseq":
		return parser.HdrCSeq
	case "contact":
		return parser.HdrContact
	case "max-forwards":
		return parser.HdrMaxForwards
	case "route":
		return parser.HdrRoute
	case "record-route":
		return parser.HdrRecordRoute
	}
	return parser.HdrOther
}

// newHdr constructs a HdrField from a name and body.
func newHdr(name, body string) *parser.HdrField {
	return &parser.HdrField{
		Name: str.Mk(name),
		Body: str.Mk(body),
		Type: hdrType(name),
	}
}

// makeINVITE builds a minimal SIPMsg mimicking an incoming INVITE.
func makeINVITE(callID, fromTag, toTag, contact string, routeHeaders []string) *parser.SIPMsg {
	msg := &parser.SIPMsg{}
	msg.FirstLine = &parser.MsgStart{
		Type: parser.MsgRequest,
		Req: &parser.RequestLine{
			Method:      str.Mk("INVITE"),
			MethodValue: parser.MethodInvite,
			URI:         str.Mk("sip:bob@ims.example.com"),
			Version:     str.Mk("SIP/2.0"),
		},
	}

	fromBody := "Alice <sip:alice@ims.example.com>"
	if fromTag != "" {
		fromBody += ";tag=" + fromTag
	}
	toBody := "Bob <sip:bob@ims.example.com>"
	if toTag != "" {
		toBody += ";tag=" + toTag
	}

	via := newHdr("Via", "SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds")
	msg.Headers = append(msg.Headers, via)

	maxFwd := newHdr("Max-Forwards", "70")
	msg.Headers = append(msg.Headers, maxFwd)

	from := newHdr("From", fromBody)
	msg.Headers = append(msg.Headers, from)
	msg.From = from

	to := newHdr("To", toBody)
	msg.Headers = append(msg.Headers, to)
	msg.To = to

	cid := newHdr("Call-ID", callID)
	msg.Headers = append(msg.Headers, cid)
	msg.CallID = cid

	cseq := newHdr("CSeq", "1 INVITE")
	msg.Headers = append(msg.Headers, cseq)
	msg.CSeq = cseq

	contactHdr := newHdr("Contact", contact)
	msg.Headers = append(msg.Headers, contactHdr)
	msg.Contact = contactHdr

	for _, routeVal := range routeHeaders {
		r := newHdr("Route", routeVal)
		msg.Headers = append(msg.Headers, r)
		if msg.Route == nil {
			msg.Route = r
		}
	}

	return msg
}

// makeResponse builds a SIPMsg representing a 200 OK response to an INVITE,
// suitable for creating a UAC dialog.
func makeResponse(callID, fromTag, toTag, contact string, recordRoute []string) *parser.SIPMsg {
	msg := &parser.SIPMsg{}
	msg.FirstLine = &parser.MsgStart{
		Reply: &parser.ReplyLine{
			StatusCode: 200,
			Reason:     str.Mk("OK"),
		},
	}

	fromBody := "Alice <sip:alice@ims.example.com>;tag=" + fromTag
	toBody := "Bob <sip:bob@ims.example.com>;tag=" + toTag

	via := newHdr("Via", "SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds")
	msg.Headers = append(msg.Headers, via)

	from := newHdr("From", fromBody)
	msg.Headers = append(msg.Headers, from)
	msg.From = from

	to := newHdr("To", toBody)
	msg.Headers = append(msg.Headers, to)
	msg.To = to

	cid := newHdr("Call-ID", callID)
	msg.Headers = append(msg.Headers, cid)
	msg.CallID = cid

	cseq := newHdr("CSeq", "1 INVITE")
	msg.Headers = append(msg.Headers, cseq)
	msg.CSeq = cseq

	contactHdr := newHdr("Contact", contact)
	msg.Headers = append(msg.Headers, contactHdr)
	msg.Contact = contactHdr

	for _, rr := range recordRoute {
		r := newHdr("Record-Route", rr)
		msg.Headers = append(msg.Headers, r)
		if msg.RecordRoute == nil {
			msg.RecordRoute = r
		}
	}

	return msg
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

func TestCreateUASDialog_Basic(t *testing.T) {
	msg := makeINVITE("callid-test-001", "abc123", "", "<sip:alice@192.168.1.100>", nil)

	d, err := CreateUASDialog(msg, "<sip:bob@192.168.1.200>")
	if err != nil {
		t.Fatalf("CreateUASDialog returned error: %v", err)
	}

	if d == nil {
		t.Fatalf("CreateUASDialog returned nil dialog")
	}
	if d.CallID != "callid-test-001" {
		t.Errorf("expected CallID callid-test-001, got %q", d.CallID)
	}
	if d.RemoteTag != "abc123" {
		t.Errorf("expected RemoteTag abc123, got %q", d.RemoteTag)
	}
	if d.LocalTag == "" {
		t.Errorf("expected auto-generated LocalTag, got empty")
	}
	if d.Direction != DialogDirectionUAS {
		t.Errorf("expected direction UAS, got %v", d.Direction)
	}
	if d.State != DialogStateEarly {
		t.Errorf("expected Early state, got %v", d.State)
	}
	if d.LocalCSeq != 0 {
		t.Errorf("expected LocalCSeq 0, got %d", d.LocalCSeq)
	}
	if d.RemoteCSeq != 1 {
		t.Errorf("expected RemoteCSeq 1, got %d", d.RemoteCSeq)
	}
	if !strings.HasPrefix(d.RemoteTarget, "sip:alice@") {
		t.Errorf("expected RemoteTarget to be sip:alice@..., got %q", d.RemoteTarget)
	}
	if d.LocalURI == "" || d.RemoteURI == "" {
		t.Errorf("expected LocalURI and RemoteURI to be set, got %q / %q", d.LocalURI, d.RemoteURI)
	}
}

func TestCreateUACDialog_Basic(t *testing.T) {
	msg := makeResponse("callid-uac-001", "local-tag-xyz", "remote-tag-abc", "<sip:bob@192.168.1.200>", nil)

	d, err := CreateUACDialog(msg, "local-tag-xyz", "<sip:alice@192.168.1.100>")
	if err != nil {
		t.Fatalf("CreateUACDialog returned error: %v", err)
	}
	if d == nil {
		t.Fatalf("CreateUACDialog returned nil dialog")
	}
	if d.CallID != "callid-uac-001" {
		t.Errorf("expected CallID callid-uac-001, got %q", d.CallID)
	}
	if d.LocalTag != "local-tag-xyz" {
		t.Errorf("expected LocalTag local-tag-xyz, got %q", d.LocalTag)
	}
	if d.RemoteTag != "remote-tag-abc" {
		t.Errorf("expected RemoteTag remote-tag-abc, got %q", d.RemoteTag)
	}
	if d.Direction != DialogDirectionUAC {
		t.Errorf("expected direction UAC, got %v", d.Direction)
	}
	if d.State != DialogStateEarly {
		t.Errorf("expected Early state, got %v", d.State)
	}
	if d.LocalCSeq != 1 {
		t.Errorf("expected LocalCSeq 1, got %d", d.LocalCSeq)
	}
	if d.RemoteCSeq != 0 {
		t.Errorf("expected RemoteCSeq 0, got %d", d.RemoteCSeq)
	}
}

func TestDialogLookup(t *testing.T) {
	mgr := NewManager()

	msg := makeINVITE("callid-lookup-001", "fromtag-a", "", "<sip:alice@10.0.0.1>", nil)
	d, err := CreateUASDialog(msg, "<sip:bob@10.0.0.2>")
	if err != nil {
		t.Fatalf("CreateUASDialog error: %v", err)
	}
	if err := mgr.Add(d); err != nil {
		t.Fatalf("Manager.Add error: %v", err)
	}

	found := mgr.Lookup("callid-lookup-001", d.LocalTag, d.RemoteTag)
	if found == nil {
		t.Fatalf("Lookup returned nil (local=%q remote=%q)", d.LocalTag, d.RemoteTag)
	}
	if found.CallID != d.CallID {
		t.Errorf("found.CallID mismatch: %q != %q", found.CallID, d.CallID)
	}

	// Reversed tags should yield the same entry.
	reversed := mgr.Lookup("callid-lookup-001", d.RemoteTag, d.LocalTag)
	if reversed == nil {
		t.Fatalf("Lookup with reversed tags returned nil")
	}
	if reversed != found {
		t.Errorf("reversed lookup returned a different *Dialog pointer")
	}

	// Missing / wrong call-ID => nil.
	if mgr.Lookup("callid-missing-001", d.LocalTag, d.RemoteTag) != nil {
		t.Errorf("expected nil for non-existent call-ID")
	}
}

func TestDialogStateTransitions(t *testing.T) {
	d := &Dialog{State: DialogStateEarly}

	if d.IsConfirmed() || d.IsTerminated() {
		t.Errorf("fresh Early dialog should not be Confirmed or Terminated")
	}

	d.Confirm()
	if !d.IsConfirmed() {
		t.Errorf("dialog should be Confirmed after Confirm()")
	}
	if d.State != DialogStateConfirmed {
		t.Errorf("state should be Confirmed, got %v", d.State)
	}

	// Confirm is idempotent.
	d.Confirm()
	if d.State != DialogStateConfirmed {
		t.Errorf("Confirm() should be idempotent")
	}

	d.Terminate()
	if !d.IsTerminated() {
		t.Errorf("dialog should be Terminated after Terminate()")
	}
	if d.State != DialogStateTerminated {
		t.Errorf("state should be Terminated, got %v", d.State)
	}
}

func TestDialogCSeqManagement(t *testing.T) {
	d := &Dialog{LocalCSeq: 10, RemoteCSeq: 20}

	// Local CSeq increments monotonically.
	if v := d.NextLocalCSeq(); v != 11 {
		t.Errorf("expected NextLocalCSeq 11, got %d", v)
	}
	if v := d.NextLocalCSeq(); v != 12 {
		t.Errorf("expected NextLocalCSeq 12, got %d", v)
	}

	// Remote CSeq must strictly increase.
	if err := d.UpdateRemoteCSeq(21); err != nil {
		t.Errorf("UpdateRemoteCSeq(21) should succeed, got %v", err)
	}
	if err := d.UpdateRemoteCSeq(25); err != nil {
		t.Errorf("UpdateRemoteCSeq(25) should succeed, got %v", err)
	}
	if err := d.UpdateRemoteCSeq(25); err == nil {
		t.Errorf("UpdateRemoteCSeq(25) again should fail (not strictly greater)")
	}
	if err := d.UpdateRemoteCSeq(10); err == nil {
		t.Errorf("UpdateRemoteCSeq(10) should fail (less than current)")
	}
}

func TestDialogWithRouteSet(t *testing.T) {
	routes := []string{
		"<sip:proxy1.ims.example.com;lr>",
		"<sip:proxy2.ims.example.com;lr>",
	}
	msg := makeINVITE("callid-routes-001", "fromtag-r", "", "<sip:alice@10.0.0.1>", routes)

	d, err := CreateUASDialog(msg, "<sip:bob@10.0.0.2>")
	if err != nil {
		t.Fatalf("CreateUASDialog error: %v", err)
	}

	if len(d.RouteSet) != 2 {
		t.Fatalf("expected 2 entries in RouteSet, got %d", len(d.RouteSet))
	}
	if !strings.Contains(d.RouteSet[0], "proxy1.ims.example.com") {
		t.Errorf("RouteSet[0] unexpected: %q", d.RouteSet[0])
	}
	if !strings.Contains(d.RouteSet[1], "proxy2.ims.example.com") {
		t.Errorf("RouteSet[1] unexpected: %q", d.RouteSet[1])
	}

	headers := d.BuildRouteHeaders()
	if len(headers) != 2 {
		t.Fatalf("expected 2 Route headers, got %d", len(headers))
	}
	if headers[0][0] != "Route" {
		t.Errorf("expected header name 'Route', got %q", headers[0][0])
	}
	if !strings.Contains(headers[0][1], "<sip:proxy1.ims.example.com") {
		t.Errorf("Route header[0] unexpected body: %q", headers[0][1])
	}
	if !strings.Contains(headers[1][1], "<sip:proxy2.ims.example.com") {
		t.Errorf("Route header[1] unexpected body: %q", headers[1][1])
	}
}

func TestDialogManagerCount(t *testing.T) {
	mgr := NewManager()
	if mgr.Count() != 0 {
		t.Errorf("new manager should have count 0, got %d", mgr.Count())
	}

	for i := 0; i < 5; i++ {
		fromTag := "fromtag-" + string(rune('a'+i))
		toTag := "totag-" + string(rune('A'+i))
		callID := "callid-count-" + string(rune('0'+i))
		msg := makeINVITE(callID, fromTag, toTag, "<sip:alice@10.0.0.1>", nil)
		d, err := CreateUASDialog(msg, "<sip:bob@10.0.0.2>")
		if err != nil {
			t.Fatalf("CreateUASDialog error: %v", err)
		}
		if err := mgr.Add(d); err != nil {
			t.Fatalf("Manager.Add error: %v", err)
		}
	}
	if mgr.Count() != 5 {
		t.Errorf("expected count 5, got %d", mgr.Count())
	}

	// Remove one.
	firstCallID := "callid-count-0"
	mgr.Remove(firstCallID, "totag-A", "fromtag-a")
	if mgr.Count() != 4 {
		t.Errorf("expected count 4 after remove, got %d", mgr.Count())
	}
	if mgr.Lookup(firstCallID, "totag-A", "fromtag-a") != nil {
		t.Errorf("removed dialog still present in manager")
	}

	// Duplicate add should fail.
	msg := makeINVITE("callid-count-1", "fromtag-b", "totag-B", "<sip:alice@10.0.0.1>", nil)
	d, err := CreateUASDialog(msg, "<sip:bob@10.0.0.2>")
	if err != nil {
		t.Fatalf("CreateUASDialog error: %v", err)
	}
	if err := mgr.Add(d); err == nil {
		t.Errorf("expected error adding duplicate dialog, got nil")
	}
}

func TestCreateUASDialogWithRecordRoute(t *testing.T) {
	// When the INVITE uses a single comma-separated Route header, both entries
	// must still be extracted into the RouteSet.
	combinedRoute := "<sip:proxy1.ims.example.com;lr>, <sip:proxy2.ims.example.com;lr>, <sip:proxy3.ims.example.com;lr>"
	msg := makeINVITE("callid-rr-001", "fromtag-rr", "", "<sip:alice@10.0.0.1>", []string{combinedRoute})

	d, err := CreateUASDialog(msg, "<sip:bob@10.0.0.2>")
	if err != nil {
		t.Fatalf("CreateUASDialog error: %v", err)
	}

	if len(d.RouteSet) != 3 {
		t.Fatalf("expected 3 entries in RouteSet (comma-separated), got %d: %v", len(d.RouteSet), d.RouteSet)
	}
	for i, uri := range d.RouteSet {
		if !strings.HasPrefix(uri, "sip:proxy") || !strings.Contains(uri, ".ims.example.com") {
			t.Errorf("RouteSet[%d] unexpected URI: %q", i, uri)
		}
	}

	headers := d.BuildRouteHeaders()
	if len(headers) != 3 {
		t.Fatalf("expected 3 Route headers, got %d", len(headers))
	}
}

// ---------------------------------------------------------------------------
// Small helper coverage
// ---------------------------------------------------------------------------

func TestDialogKeyStability(t *testing.T) {
	// dialogKey should be order-independent regarding its two tag arguments.
	k1 := dialogKey("callid-x", "tag-a", "tag-b")
	k2 := dialogKey("callid-x", "tag-b", "tag-a")
	if k1 != k2 {
		t.Errorf("dialogKey should be order-independent: %q != %q", k1, k2)
	}

	// Different call-IDs should yield different keys even with same tags.
	k3 := dialogKey("callid-y", "tag-a", "tag-b")
	if k1 == k3 {
		t.Errorf("different call-IDs should yield different keys: %q == %q", k1, k3)
	}
}

func TestExtractTagAndURI(t *testing.T) {
	spec := "Alice <sip:alice@ims.example.com>;tag=abc123"
	if got := extractTag(str.Mk(spec)); got != "abc123" {
		t.Errorf("extractTag: expected abc123, got %q", got)
	}
	if got := extractURIFromAddrSpec(str.Mk(spec)); got != "sip:alice@ims.example.com" {
		t.Errorf("extractURIFromAddrSpec: unexpected %q", got)
	}

	if got := extractTag(str.Mk("no tag here")); got != "" {
		t.Errorf("extractTag: expected empty for no tag, got %q", got)
	}

	// tag= as the first thing (no leading semicolon) – still should parse.
	if got := extractTag(str.Mk("tag=xyz")); got != "xyz" {
		t.Errorf("extractTag: expected xyz, got %q", got)
	}
}

func TestParseRouteValuesSingleAndComma(t *testing.T) {
	single := "<sip:p1.example.com;lr>"
	if got := parseRouteValues(str.Mk(single)); len(got) != 1 || got[0] != "sip:p1.example.com;lr" {
		t.Errorf("parseRouteValues (single): unexpected %v", got)
	}

	combined := "<sip:p1.example.com;lr>, <sip:p2.example.com;lr>, <sip:p3.example.com;lr>"
	got := parseRouteValues(str.Mk(combined))
	if len(got) != 3 {
		t.Fatalf("parseRouteValues (combined): expected 3 entries, got %d: %v", len(got), got)
	}
	for i, uri := range got {
		if !strings.HasPrefix(uri, "sip:p") || !strings.Contains(uri, ".example.com") {
			t.Errorf("parseRouteValues[%d]: unexpected %q", i, uri)
		}
	}
}

func TestNilDialogSafe(t *testing.T) {
	var d *Dialog
	// These should not panic.
	d.Confirm()
	d.Terminate()
	if d.IsConfirmed() || d.IsTerminated() {
		t.Errorf("nil dialog should not report Confirmed/Terminated")
	}
	if v := d.NextLocalCSeq(); v != 0 {
		t.Errorf("nil dialog NextLocalCSeq should return 0, got %d", v)
	}
	if err := d.UpdateRemoteCSeq(1); err == nil {
		t.Errorf("nil dialog UpdateRemoteCSeq should return error")
	}
	if got := d.BuildRouteHeaders(); got != nil {
		t.Errorf("nil dialog BuildRouteHeaders should return nil, got %v", got)
	}
}

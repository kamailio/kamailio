// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for Phase 40 – high-level dialog state machine.
 */

package dialog

import (
	"strconv"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// hdrP40 resolves a SIP header name to its HdrType constant.
func hdrP40(name string) parser.HdrType {
	switch name {
	case "Via":
		return parser.HdrVia
	case "From":
		return parser.HdrFrom
	case "To":
		return parser.HdrTo
	case "Call-ID":
		return parser.HdrCallID
	case "CSeq":
		return parser.HdrCSeq
	case "Contact":
		return parser.HdrContact
	}
	return parser.HdrOther
}

// mkHdrP40 builds a simple HdrField with the given name and body.
func mkHdrP40(name, body string) *parser.HdrField {
	return &parser.HdrField{
		Name: str.Mk(name),
		Body: str.Mk(body),
		Type: hdrP40(name),
	}
}

// inviteMsgP40 builds a handcrafted SIPMsg that represents an INVITE
// request. The toTag argument may be empty, in which case the To header
// has no tag (the usual UAS-inbound case).
func inviteMsgP40(callID, fromTag, toTag, cseq string) *parser.SIPMsg {
	msg := &parser.SIPMsg{}
	msg.FirstLine = &parser.MsgStart{
		Type: parser.MsgRequest,
		Req: &parser.RequestLine{
			Method:      str.Mk("INVITE"),
			MethodValue: parser.MethodInvite,
			URI:         str.Mk("sip:bob@example.com"),
			Version:     str.Mk("SIP/2.0"),
		},
	}

	fromBody := "<sip:alice@example.com>;tag=" + fromTag
	toBody := "<sip:bob@example.com>"
	if toTag != "" {
		toBody += ";tag=" + toTag
	}

	via := mkHdrP40("Via", "SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKabc")
	msg.Headers = append(msg.Headers, via)
	msg.HdrVia1 = via

	from := mkHdrP40("From", fromBody)
	msg.Headers = append(msg.Headers, from)
	msg.From = from

	to := mkHdrP40("To", toBody)
	msg.Headers = append(msg.Headers, to)
	msg.To = to

	cid := mkHdrP40("Call-ID", callID)
	msg.Headers = append(msg.Headers, cid)
	msg.CallID = cid

	cseqHdr := mkHdrP40("CSeq", cseq+" INVITE")
	msg.Headers = append(msg.Headers, cseqHdr)
	msg.CSeq = cseqHdr

	contact := mkHdrP40("Contact", "<sip:alice@10.0.0.1:5060>")
	msg.Headers = append(msg.Headers, contact)
	msg.Contact = contact

	return msg
}

// inDialogMsgP40 builds a SIPMsg mimicking a request inside an existing
// dialog (BYE / CANCEL / ACK). It copies the Call-ID and tags from the
// given dialog.
func inDialogMsgP40(method parser.RequestMethod, d *Dialog, cseq string) *parser.SIPMsg {
	methodStr := "BYE"
	switch method {
	case parser.MethodCancel:
		methodStr = "CANCEL"
	case parser.MethodACK:
		methodStr = "ACK"
	case parser.MethodBye:
		methodStr = "BYE"
	}

	msg := &parser.SIPMsg{}
	msg.FirstLine = &parser.MsgStart{
		Type: parser.MsgRequest,
		Req: &parser.RequestLine{
			Method:      str.Mk(methodStr),
			MethodValue: method,
			URI:         str.Mk("sip:bob@example.com"),
			Version:     str.Mk("SIP/2.0"),
		},
	}

	// For UAS dialogs the From-tag is the remote and the To-tag is local.
	// For in-dialog messages we simply pick a stable (From: remote-tag,
	// To: local-tag) ordering so Lookup resolves the dialog.
	fromBody := "<sip:alice@example.com>;tag=" + d.RemoteTag
	toBody := "<sip:bob@example.com>;tag=" + d.LocalTag

	via := mkHdrP40("Via", "SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKdialog")
	msg.Headers = append(msg.Headers, via)
	msg.HdrVia1 = via

	from := mkHdrP40("From", fromBody)
	msg.Headers = append(msg.Headers, from)
	msg.From = from

	to := mkHdrP40("To", toBody)
	msg.Headers = append(msg.Headers, to)
	msg.To = to

	cid := mkHdrP40("Call-ID", d.CallID)
	msg.Headers = append(msg.Headers, cid)
	msg.CallID = cid

	cseqHdr := mkHdrP40("CSeq", cseq+" "+methodStr)
	msg.Headers = append(msg.Headers, cseqHdr)
	msg.CSeq = cseqHdr

	contact := mkHdrP40("Contact", "<sip:alice@10.0.0.1:5060>")
	msg.Headers = append(msg.Headers, contact)
	msg.Contact = contact

	return msg
}

// ---------------------------------------------------------------------------
// TestDialog_HandleInvite_New – a fresh INVITE creates an Early dialog
// ---------------------------------------------------------------------------

func TestDialog_HandleInvite_New(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-new@example.com", "alice-tag", "", "1")

	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}
	if d == nil {
		t.Fatal("HandleInvite returned nil dialog")
	}
	if d.State != DialogStateEarly {
		t.Errorf("expected state Early, got %s", d.State)
	}
	if d.Direction != DialogDirectionUAS {
		t.Errorf("expected Direction UAS, got %s", d.Direction)
	}
	if d.RemoteTag != "alice-tag" {
		t.Errorf("expected RemoteTag 'alice-tag', got %q", d.RemoteTag)
	}
	if d.LocalTag == "" {
		t.Error("expected a non-empty LocalTag to be generated")
	}
	if d.InviteStart.IsZero() {
		t.Error("expected non-zero InviteStart")
	}
	if mgr.Count() != 1 {
		t.Errorf("expected manager count 1, got %d", mgr.Count())
	}
}

// ---------------------------------------------------------------------------
// TestDialog_HandleInvite_Reinvite – re-INVITE moves dialog back to Early
// and later a 200 OK moves it back to Confirmed.
// ---------------------------------------------------------------------------

func TestDialog_HandleInvite_Reinvite(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-reinvite@example.com", "alice-tag", "", "1")

	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}

	// Emulate a 200 OK reply to move dialog to Confirmed.
	d.State = DialogStateConfirmed

	// re-INVITE using the same Call-ID and both tags populated.
	reInvite := inviteMsgP40("call-reinvite@example.com", "alice-tag", d.LocalTag, "3")
	d2, err := mgr.HandleInvite(reInvite)
	if err != nil {
		t.Fatalf("re-INVITE returned error: %v", err)
	}
	if d2.State != DialogStateEarly {
		t.Errorf("expected re-INVITE to push state back to Early, got %s", d2.State)
	}

	// Final 200 OK for the re-INVITE.
	d2.State = DialogStateConfirmed
	if d2.State != DialogStateConfirmed {
		t.Errorf("expected final state to be Confirmed, got %s", d2.State)
	}
}

// ---------------------------------------------------------------------------
// TestDialog_HandleReply_1xx – a provisional reply keeps/puts state at Early
// ---------------------------------------------------------------------------

func TestDialog_HandleReply_1xx(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-1xx@example.com", "alice-tag", "", "1")
	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}

	reply := inviteMsgP40("call-1xx@example.com", "alice-tag", d.LocalTag, "1")
	d1, err := mgr.HandleReply(reply, 180, "Ringing")
	if err != nil {
		t.Fatalf("HandleReply returned error: %v", err)
	}
	if d1.State != DialogStateEarly {
		t.Errorf("expected Early after 180, got %s", d1.State)
	}
	if d1.LastResponseCode != 180 {
		t.Errorf("expected LastResponseCode 180, got %d", d1.LastResponseCode)
	}
}

// ---------------------------------------------------------------------------
// TestDialog_HandleReply_2xx – a 200 OK reply moves the dialog to Confirmed
// ---------------------------------------------------------------------------

func TestDialog_HandleReply_2xx(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-2xx@example.com", "alice-tag", "", "1")
	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}

	reply := inviteMsgP40("call-2xx@example.com", "alice-tag", d.LocalTag, "1")
	d1, err := mgr.HandleReply(reply, 200, "OK")
	if err != nil {
		t.Fatalf("HandleReply returned error: %v", err)
	}
	if d1.State != DialogStateConfirmed {
		t.Errorf("expected Confirmed after 200, got %s", d1.State)
	}
	if d1.LastResponseCode != 200 {
		t.Errorf("expected LastResponseCode 200, got %d", d1.LastResponseCode)
	}
}

// ---------------------------------------------------------------------------
// TestDialog_HandleReply_4xx – a final non-2xx reply terminates the dialog
// ---------------------------------------------------------------------------

func TestDialog_HandleReply_4xx(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-4xx@example.com", "alice-tag", "", "1")
	_, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}

	reply := inviteMsgP40("call-4xx@example.com", "alice-tag", "bob-tag", "1")
	// Pre-create the dialog with a to-tag so the reply can resolve it.
	pre, _ := CreateUASDialog(invite, "")
	pre.LocalTag = "bob-tag"
	_ = mgr.Add(pre)

	d1, err := mgr.HandleReply(reply, 486, "Busy Here")
	if err != nil {
		t.Fatalf("HandleReply returned error: %v", err)
	}
	if d1.State != DialogStateTerminated {
		t.Errorf("expected Terminated after 486, got %s", d1.State)
	}
	if d1.LastResponseCode != 486 {
		t.Errorf("expected LastResponseCode 486, got %d", d1.LastResponseCode)
	}
}

// ---------------------------------------------------------------------------
// TestDialog_HandleBye – BYE terminates a confirmed dialog
// ---------------------------------------------------------------------------

func TestDialog_HandleBye(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-bye@example.com", "alice-tag", "", "1")
	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}
	d.State = DialogStateConfirmed

	bye := inDialogMsgP40(parser.MethodBye, d, "2")
	d1, err := mgr.HandleBye(bye)
	if err != nil {
		t.Fatalf("HandleBye returned error: %v", err)
	}
	if d1.State != DialogStateTerminated {
		t.Errorf("expected Terminated after BYE, got %s", d1.State)
	}
	if d1.LastResponseCode != 200 {
		t.Errorf("expected LastResponseCode 200 after BYE, got %d", d1.LastResponseCode)
	}
}

// ---------------------------------------------------------------------------
// TestDialog_HandleCancel – CANCEL terminates an early dialog
// ---------------------------------------------------------------------------

func TestDialog_HandleCancel(t *testing.T) {
	mgr := NewManager()
	invite := inviteMsgP40("call-cancel@example.com", "alice-tag", "", "1")
	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}
	if d.State != DialogStateEarly {
		t.Fatalf("expected Early after INVITE, got %s", d.State)
	}

	cancel := inDialogMsgP40(parser.MethodCancel, d, "1")
	d1, err := mgr.HandleCancel(cancel)
	if err != nil {
		t.Fatalf("HandleCancel returned error: %v", err)
	}
	if d1.State != DialogStateTerminated {
		t.Errorf("expected Terminated after CANCEL, got %s", d1.State)
	}
}

// ---------------------------------------------------------------------------
// TestDialog_Stats – aggregate counts across mixed dialog states
// ---------------------------------------------------------------------------

func TestDialog_Stats(t *testing.T) {
	mgr := NewManager()

	// 2x Early dialogs.
	for i := 0; i < 2; i++ {
		invite := inviteMsgP40("call-stats-"+strconv.Itoa(i)+"@example.com", "alice-tag-"+strconv.Itoa(i), "", "1")
		if _, err := mgr.HandleInvite(invite); err != nil {
			t.Fatalf("HandleInvite returned error: %v", err)
		}
	}

	// 3x Confirmed dialogs.
	for i := 0; i < 3; i++ {
		invite := inviteMsgP40("call-stats-conf-"+strconv.Itoa(i)+"@example.com", "alice-tag-conf-"+strconv.Itoa(i), "", "1")
		d, err := mgr.HandleInvite(invite)
		if err != nil {
			t.Fatalf("HandleInvite returned error: %v", err)
		}
		d.State = DialogStateConfirmed
	}

	// 1x Terminated dialog.
	invite := inviteMsgP40("call-stats-term@example.com", "alice-tag-term", "", "1")
	d, err := mgr.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite returned error: %v", err)
	}
	d.State = DialogStateTerminated

	count, early, confirmed, terminated := mgr.Stats()
	if count != 6 {
		t.Errorf("expected total count 6, got %d", count)
	}
	if early != 2 {
		t.Errorf("expected early 2, got %d", early)
	}
	if confirmed != 3 {
		t.Errorf("expected confirmed 3, got %d", confirmed)
	}
	if terminated != 1 {
		t.Errorf("expected terminated 1, got %d", terminated)
	}
}

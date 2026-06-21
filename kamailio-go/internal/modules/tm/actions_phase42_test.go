// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module - Phase 42 integration tests for the script-friendly action
 * helpers TReply / TRelay / TForwardNonInvite / TLookup / TIsLocalTransaction.
 *
 * All tests operate on isolated Managers (no shared state) so they can be
 * executed in parallel and in any order.
 */

package tm

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// parseINVITE is a test-scoped helper that parses a canonical INVITE and
// returns the resulting SIPMsg.
func parseINVITE(t *testing.T) *parser.SIPMsg {
	t.Helper()
	raw := []byte("INVITE sip:bob@ims.example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@ims.example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@ims.example.com>\r\n" +
		"Call-ID: a84b4c76e66710@192.168.1.100\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Contact: <sip:alice@192.168.1.100>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("parse INVITE: %v", err)
	}
	return msg
}

// parseREGISTER returns a fresh, independently-parsed REGISTER request.
func parseREGISTER(t *testing.T) *parser.SIPMsg {
	t.Helper()
	raw := []byte("REGISTER sip:ims.example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.200:5060;branch=z9hG4bKreg123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Bob <sip:bob@ims.example.com>;tag=reg42\r\n" +
		"To: Bob <sip:bob@ims.example.com>\r\n" +
		"Call-ID: reg-callid-42@192.168.1.200\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:bob@192.168.1.200;transport=udp>;expires=3600\r\n" +
		"Expires: 3600\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("parse REGISTER: %v", err)
	}
	return msg
}

// TestTReply_1xx_Proceeding verifies that calling TReply with a 1xx status
// code drives the INVITE cell into the Proceeding state.
func TestTReply_1xx_Proceeding(t *testing.T) {
	mgr := NewManager(64)
	msg := parseINVITE(t)

	// Create the cell first (the typical path: script engine calls
	// t_relay or NewTransaction before t_reply).
	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("NewTransaction: %v", err)
	}
	if cell.State != TStateTrying {
		t.Fatalf("expected TStateTrying after NewTransaction, got %d", cell.State)
	}

	if err := TReply(mgr, msg, 180, "Ringing"); err != nil {
		t.Fatalf("TReply(180): %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("after TReply(180) expected TStateProceeding, got %d (%s)",
			cell.State, cell.StateString())
	}
	if cell.UAS.Status != 180 {
		t.Errorf("expected UAS.Status == 180, got %d", cell.UAS.Status)
	}

	// A second provisional reply should still succeed (1xx may be repeated).
	if err := TReply(mgr, msg, 183, "Session Progress"); err != nil {
		t.Fatalf("TReply(183) after 180: %v", err)
	}
}

// TestTReply_2xx_Completed verifies that TReply with a 2xx status drives
// the INVITE cell to Completed.
func TestTReply_2xx_Completed(t *testing.T) {
	mgr := NewManager(64)
	msg := parseINVITE(t)

	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("NewTransaction: %v", err)
	}

	if err := TReply(mgr, msg, 200, "OK"); err != nil {
		t.Fatalf("TReply(200): %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("after TReply(200) expected TStateCompleted, got %d (%s)",
			cell.State, cell.StateString())
	}
	if cell.UAS.Status != 200 {
		t.Errorf("expected UAS.Status == 200, got %d", cell.UAS.Status)
	}

	// A second final reply on the same INVITE cell must fail with
	// ErrCompleted - duplicate 2xx/3xx/4xx/5xx/6xx are not allowed.
	if err := TReply(mgr, msg, 486, "Busy Here"); err == nil {
		t.Errorf("expected error re-replying to Completed INVITE, got nil")
	}
}

// TestTReply_Invalid_OnConfirmed verifies that once the cell is manually
// forced into a Confirmed state (simulating the proxy having processed an
// ACK), a subsequent TReply is rejected with ErrCompleted.
func TestTReply_Invalid_OnConfirmed(t *testing.T) {
	mgr := NewManager(64)
	msg := parseINVITE(t)

	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("NewTransaction: %v", err)
	}
	// Drive the cell through Proceeding → Completed → Confirmed,
	// simulating a proxy having sent a 1xx then a 2xx and then seen
	// the ACK come in.
	if err := cell.UpdateState(TStateProceeding); err != nil {
		t.Fatalf("UpdateState(Proceeding): %v", err)
	}
	if err := cell.UpdateState(TStateCompleted); err != nil {
		t.Fatalf("UpdateState(Completed): %v", err)
	}
	if err := cell.UpdateState(TStateConfirmed); err != nil {
		t.Fatalf("UpdateState(Confirmed): %v", err)
	}

	if err := TReply(mgr, msg, 200, "OK"); err == nil {
		t.Errorf("expected error from TReply on Confirmed cell, got nil")
	}
}

// TestTRelay_NewInvite verifies that calling TRelay with no pre-existing
// cell creates one, marks it as an INVITE, and leaves it in TStateTrying.
func TestTRelay_NewInvite(t *testing.T) {
	mgr := NewManager(64)
	msg := parseINVITE(t)

	cell, err := TRelay(mgr, msg)
	if err != nil {
		t.Fatalf("TRelay: %v", err)
	}
	if cell == nil {
		t.Fatal("TRelay returned nil cell")
	}
	if !cell.IsInvite() {
		t.Error("expected cell.IsInvite() == true")
	}
	if cell.State != TStateTrying {
		t.Errorf("expected TStateTrying after TRelay, got %d (%s)",
			cell.State, cell.StateString())
	}

	// A subsequent TLookup for the same request should find this cell.
	found, err := TLookup(mgr, msg)
	if err != nil {
		t.Fatalf("TLookup: %v", err)
	}
	if found == nil || found.HashIndex != cell.HashIndex {
		t.Errorf("TLookup did not return the cell created by TRelay")
	}
}

// TestTForwardNonInvite_REGISTER verifies that TForwardNonInvite accepts
// a REGISTER request and creates a cell (state != undefined) without
// marking it as INVITE.
func TestTForwardNonInvite_REGISTER(t *testing.T) {
	mgr := NewManager(64)
	msg := parseREGISTER(t)

	cell, err := TForwardNonInvite(mgr, msg)
	if err != nil {
		t.Fatalf("TForwardNonInvite: %v", err)
	}
	if cell == nil {
		t.Fatal("TForwardNonInvite returned nil cell")
	}
	if cell.IsInvite() {
		t.Error("REGISTER cell should not be marked as INVITE")
	}
	if cell.State == TStateUndefined {
		t.Error("cell State should not remain TStateUndefined after TForwardNonInvite")
	}

	// Calling it again with the same request should succeed (idempotent
	// from the caller's perspective; the cell already exists).
	cell2, err := TForwardNonInvite(mgr, msg)
	if err != nil {
		t.Fatalf("second TForwardNonInvite: %v", err)
	}
	if cell2 == nil {
		t.Error("second TForwardNonInvite returned nil cell")
	}

	// TForwardNonInvite must reject INVITE requests.
	invite := parseINVITE(t)
	if _, err := TForwardNonInvite(mgr, invite); err == nil {
		t.Error("expected error from TForwardNonInvite on INVITE, got nil")
	}
}

// TestTLookup_Request verifies that TLookup correctly resolves a request
// to the cell created via NewTransaction/TRelay.
func TestTLookup_Request(t *testing.T) {
	mgr := NewManager(64)
	msg := parseINVITE(t)

	// Before any transaction is created, lookup must fail cleanly.
	if _, err := TLookup(mgr, msg); err == nil {
		t.Error("expected error from TLookup on fresh Manager, got nil")
	}

	// Create the cell and then look it up.
	if _, err := TRelay(mgr, msg); err != nil {
		t.Fatalf("TRelay: %v", err)
	}
	found, err := TLookup(mgr, msg)
	if err != nil {
		t.Fatalf("TLookup: %v", err)
	}
	if found == nil {
		t.Fatal("TLookup returned nil cell")
	}
	if !found.IsInvite() {
		t.Error("found cell should be marked as INVITE")
	}
}

// TestTIsLocal verifies the behaviour of TIsLocalTransaction:
//   - Empty / nil domain list → returns true (default local).
//   - Request-URI host matching one of the configured domains → true.
//   - Request-URI host NOT matching any configured domain → false.
func TestTIsLocal(t *testing.T) {
	// Default behaviour with no domains configured → local.
	if !TIsLocalTransaction(nil, nil) {
		t.Error("TIsLocalTransaction(nil, nil) should return true")
	}

	msg := parseREGISTER(t)
	// Empty domain list → true.
	if !TIsLocalTransaction(msg, []string{}) {
		t.Error("empty domain list should default to local")
	}

	// Matching domain → true (case-insensitive).
	if !TIsLocalTransaction(msg, []string{"IMS.Example.COM"}) {
		t.Error("expected true for matching domain")
	}

	// Non-matching domain → false.
	if TIsLocalTransaction(msg, []string{"other.example.com"}) {
		t.Error("expected false for non-matching domain")
	}

	// A mix of matching and non-matching domains → true.
	if !TIsLocalTransaction(msg, []string{"other.example.com", "ims.example.com"}) {
		t.Error("expected true when at least one domain matches")
	}
}

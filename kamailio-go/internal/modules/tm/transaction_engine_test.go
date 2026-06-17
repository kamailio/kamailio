// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for the transaction state machine
 */

package tm

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// INVITE request test message (shared with tm_test.go)
var testInviteMsgEngine = []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"Max-Forwards: 70\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314159 INVITE\r\n" +
	"Contact: <sip:alice@pc33.example.com>\r\n" +
	"Content-Type: application/sdp\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

// TestEngineHandleRequestINVITE tests that an INVITE request creates a new
// transaction with the TIsInvite flag set.
func TestEngineHandleRequestINVITE(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(testInviteMsgEngine)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest error: %v", err)
	}
	if cell == nil {
		t.Fatal("expected cell to be created")
	}
	if !cell.IsInvite() {
		t.Error("expected TIsInvite flag to be set")
	}
	if cell.Flags&TIsInvite == 0 {
		t.Error("expected TIsInvite bit set in Flags")
	}
}

// TestEngineHandleRequestDuplicate tests that calling HandleRequest twice
// for the same request results in an error on the second call.
func TestEngineHandleRequestDuplicate(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(testInviteMsgEngine)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	if _, err := mgr.HandleRequest(msg); err != nil {
		t.Fatalf("first HandleRequest error: %v", err)
	}

	if _, err := mgr.HandleRequest(msg); err == nil {
		t.Error("expected error for duplicate HandleRequest, got nil")
	}
}

// TestEngineReplyTransitions tests that Reply() drives UAS state through
// Trying -> Proceeding -> Completed.
func TestEngineReplyTransitions(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(testInviteMsgEngine)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("NewTransaction error: %v", err)
	}

	if cell.State != TStateTrying {
		t.Errorf("expected Trying state, got %d", cell.State)
	}

	// 180 Ringing -> Proceeding
	if err := mgr.Reply(msg, 180, "Ringing"); err != nil {
		t.Fatalf("Reply(180) error: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("expected Proceeding state after 180, got %d", cell.State)
	}
	if cell.UAS.Status != 180 {
		t.Errorf("expected UAS.Status=180, got %d", cell.UAS.Status)
	}

	// 200 OK -> Completed
	if err := mgr.Reply(msg, 200, "OK"); err != nil {
		t.Fatalf("Reply(200) error: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("expected Completed state after 200, got %d", cell.State)
	}
	if cell.UAS.Status != 200 {
		t.Errorf("expected UAS.Status=200, got %d", cell.UAS.Status)
	}
}

// TestEngineIsFinalResponse tests isFinalResponse helper.
func TestEngineIsFinalResponse(t *testing.T) {
	tests := []struct {
		status int
		want   bool
	}{
		{99, false},
		{100, false},
		{180, false},
		{199, false},
		{200, true},
		{299, true},
		{404, true},
		{500, true},
		{600, true},
		{699, true},
		{700, false},
		{0, false},
	}

	for _, tc := range tests {
		got := isFinalResponse(tc.status)
		if got != tc.want {
			t.Errorf("isFinalResponse(%d) = %v, want %v", tc.status, got, tc.want)
		}
	}
}

// TestEngineIsProvisionalResponse tests isProvisionalResponse helper.
func TestEngineIsProvisionalResponse(t *testing.T) {
	tests := []struct {
		status int
		want   bool
	}{
		{99, false},
		{100, true},
		{180, true},
		{199, true},
		{200, false},
		{404, false},
		{0, false},
	}

	for _, tc := range tests {
		got := isProvisionalResponse(tc.status)
		if got != tc.want {
			t.Errorf("isProvisionalResponse(%d) = %v, want %v", tc.status, got, tc.want)
		}
	}
}

// TestEngineIs2xx tests the is2xx helper.
func TestEngineIs2xx(t *testing.T) {
	tests := []struct {
		status int
		want   bool
	}{
		{199, false},
		{200, true},
		{299, true},
		{300, false},
		{0, false},
	}

	for _, tc := range tests {
		got := is2xx(tc.status)
		if got != tc.want {
			t.Errorf("is2xx(%d) = %v, want %v", tc.status, got, tc.want)
		}
	}
}

// TestEngineCancel tests that Cancel marks the cell with the TCanceled flag
// and drives the state to TStateCompleted.
func TestEngineCancel(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(testInviteMsgEngine)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("NewTransaction error: %v", err)
	}

	if cell.IsCanceled() {
		t.Error("cell should not be canceled before Cancel()")
	}

	if err := mgr.Cancel(cell, "User Cancelled"); err != nil {
		t.Fatalf("Cancel error: %v", err)
	}

	if !cell.IsCanceled() {
		t.Error("expected cell to be marked with TCanceled")
	}
	if cell.Flags&TCanceled == 0 {
		t.Error("expected TCanceled bit set in Flags")
	}
	if cell.State != TStateCompleted {
		t.Errorf("expected TStateCompleted, got %d", cell.State)
	}
	if cell.UAS.CancelReason != "User Cancelled" {
		t.Errorf("unexpected cancel reason: %q", cell.UAS.CancelReason)
	}
}

// TestEngineRelayRequest tests that RelayRequest adds a new branch and
// returns (cell, branch, nil).
func TestEngineRelayRequest(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(testInviteMsgEngine)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	cell, branch, err := mgr.RelayRequest(msg, "10.0.0.1", 5060)
	if err != nil {
		t.Fatalf("RelayRequest error: %v", err)
	}
	if cell == nil {
		t.Fatal("expected non-nil cell")
	}
	if branch < 0 {
		t.Errorf("expected non-negative branch index, got %d", branch)
	}

	// A branch should now exist
	if cell.NrOfOutgoings != 1 {
		t.Errorf("expected 1 outgoing, got %d", cell.NrOfOutgoings)
	}
	if len(cell.UAC) == 0 || cell.UAC[branch] == nil {
		t.Fatal("expected UAC at returned branch")
	}
	if cell.UAC[branch].DstURI.String() != "10.0.0.1:5060" {
		t.Errorf("expected DstURI 10.0.0.1:5060, got %q", cell.UAC[branch].DstURI.String())
	}

	// Second RelayRequest should add branch 1
	_, branch2, err := mgr.RelayRequest(msg, "10.0.0.2", 5060)
	if err != nil {
		t.Fatalf("second RelayRequest error: %v", err)
	}
	if branch2 != 1 {
		t.Errorf("expected branch 1, got %d", branch2)
	}
	if cell.NrOfOutgoings != 2 {
		t.Errorf("expected 2 outgoings after second relay, got %d", cell.NrOfOutgoings)
	}
}

// TestEngineUpdateStateTransitions tests UpdateState through
// Trying -> Proceeding -> Completed.
func TestEngineUpdateStateTransitions(t *testing.T) {
	cell := &Cell{State: TStateTrying}

	// Trying -> Proceeding is allowed
	if err := cell.UpdateState(TStateProceeding); err != nil {
		t.Fatalf("Trying->Proceeding should be allowed, got: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("expected Proceeding, got %d", cell.State)
	}
	if !cell.IsProceeding() {
		t.Error("IsProceeding() should return true in Proceeding state")
	}

	// Proceeding -> Completed is allowed
	if err := cell.UpdateState(TStateCompleted); err != nil {
		t.Fatalf("Proceeding->Completed should be allowed, got: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("expected Completed, got %d", cell.State)
	}
	if !cell.IsCompleted() {
		t.Error("IsCompleted() should return true in Completed state")
	}

	// An invalid transition from Completed back to Trying should fail
	if err := cell.UpdateState(TStateTrying); err == nil {
		t.Error("expected error for Completed->Trying transition")
	}
}

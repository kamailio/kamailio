// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Phase 14 tests: TM state machine, callbacks, local transactions, timer config
 */

package tm

import (
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// --- State machine tests ---

func TestTM_CellStateString(t *testing.T) {
	cell := &Cell{State: TStateTrying}
	if cell.StateString() != "Trying" {
		t.Errorf("expected Trying, got %s", cell.StateString())
	}
	cell.State = TStateCompleted
	if cell.StateString() != "Completed" {
		t.Errorf("expected Completed, got %s", cell.StateString())
	}
	cell.State = TStateDestroyed
	if cell.StateString() != "Destroyed" {
		t.Errorf("expected Destroyed, got %s", cell.StateString())
	}
}

func TestTM_SetState(t *testing.T) {
	cell := &Cell{State: TStateTrying}
	cell.SetState(TStateProceeding)
	if cell.State != TStateProceeding {
		t.Errorf("expected Proceeding, got %d", cell.State)
	}
}

func TestTM_CellIsLocal(t *testing.T) {
	cell := &Cell{Flags: TIsLocal}
	if !cell.IsLocal() {
		t.Error("expected IsLocal to be true")
	}
	cell.Flags = 0
	if cell.IsLocal() {
		t.Error("expected IsLocal to be false")
	}
}

// --- Callback tests ---

func TestTM_ReplyCallback(t *testing.T) {
	mgr := NewManagerWithTimers(64)

	var callbackCalled bool
	var callbackCell *Cell

	mgr.SetCallbacks(RouteCallbacks{
		OnReply: func(cell *Cell, branch int, msg *parser.SIPMsg) {
			callbackCalled = true
			callbackCell = cell
			_ = branch // branch is available but not asserted in this test
		},
	})

	// Create an INVITE transaction
	inviteRaw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-cb-001@example.com\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	inviteMsg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	cell, err := mgr.HandleRequest(inviteMsg)
	if err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}

	// Add branch 0
	_, err = mgr.AddBranch(cell, 0)
	if err != nil {
		t.Fatalf("AddBranch failed: %v", err)
	}

	// Handle a 180 Ringing response
	reply180Raw := []byte("SIP/2.0 180 Ringing\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
		"From: Bob <sip:bob@example.com>;tag=9fxced76e8\r\n" +
		"To: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"Call-ID: test-cb-001@example.com\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	reply180Msg, err := parser.ParseMsg(reply180Raw)
	if err != nil {
		t.Fatalf("ParseMsg(180) failed: %v", err)
	}

	_, _, err = mgr.HandleResponse(reply180Msg)
	if err != nil {
		t.Fatalf("HandleResponse(180) failed: %v", err)
	}

	if !callbackCalled {
		t.Error("OnReply callback was not called for 180")
	}
	if callbackCell != cell {
		t.Error("callback received wrong cell")
	}
}

func TestTM_FailureCallback(t *testing.T) {
	mgr := NewManagerWithTimers(64)

	var failureCalled bool

	mgr.SetCallbacks(RouteCallbacks{
		OnFailure: func(cell *Cell, branch int, msg *parser.SIPMsg) {
			failureCalled = true
		},
	})

	inviteRaw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK-fail-test\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-fail-001@example.com\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	inviteMsg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	cell, err := mgr.HandleRequest(inviteMsg)
	if err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}
	_, err = mgr.AddBranch(cell, 0)
	if err != nil {
		t.Fatalf("AddBranch failed: %v", err)
	}

	// Handle a 500 failure response
	reply500Raw := []byte("SIP/2.0 500 Server Internal Error\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK-fail-test\r\n" +
		"From: Bob <sip:bob@example.com>;tag=9fxced76e8\r\n" +
		"To: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"Call-ID: test-fail-001@example.com\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	reply500Msg, err := parser.ParseMsg(reply500Raw)
	if err != nil {
		t.Fatalf("ParseMsg(500) failed: %v", err)
	}

	_, _, err = mgr.HandleResponse(reply500Msg)
	if err != nil {
		t.Fatalf("HandleResponse(500) failed: %v", err)
	}

	if !failureCalled {
		t.Error("OnFailure callback was not called for 500")
	}
}

// --- Local transaction tests ---

func TestTM_NewLocalTransaction(t *testing.T) {
	mgr := NewManager(64)

	cancelRaw := []byte("CANCEL sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK-local-cancel\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@example.com>;tag=9fxced76e8\r\n" +
		"Call-ID: test-local-001@example.com\r\n" +
		"CSeq: 314159 CANCEL\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	cancelMsg, err := parser.ParseMsg(cancelRaw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	cell, err := mgr.NewLocalTransaction(cancelMsg)
	if err != nil {
		t.Fatalf("NewLocalTransaction failed: %v", err)
	}
	if cell == nil {
		t.Fatal("expected non-nil cell")
	}
	if !cell.IsLocal() {
		t.Error("expected local transaction")
	}
	if cell.Method.String() != "CANCEL" {
		t.Errorf("expected CANCEL, got %s", cell.Method.String())
	}
	if mgr.GetT() != cell {
		t.Error("expected cell to be set as current transaction")
	}

	// Should be able to find it via LookupByMsg
	found := mgr.GetTable().LookupByMsg(cancelMsg)
	if found == nil {
		t.Error("LookupByMsg should find the local transaction")
	} else {
		found.Unref()
	}
}

func TestTM_IsLocalTransaction(t *testing.T) {
	mgr := NewManager(64)

	if mgr.IsLocalTransaction() {
		t.Error("expected false when no transaction")
	}

	cancelRaw := []byte("CANCEL sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK-local-check\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@example.com>;tag=9fxced76e8\r\n" +
		"Call-ID: test-local-check@example.com\r\n" +
		"CSeq: 314159 CANCEL\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	cancelMsg, err := parser.ParseMsg(cancelRaw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	_, err = mgr.NewLocalTransaction(cancelMsg)
	if err != nil {
		t.Fatalf("NewLocalTransaction failed: %v", err)
	}

	if !mgr.IsLocalTransaction() {
		t.Error("expected true for local transaction")
	}
}

// --- Timer config tests ---

func TestTM_DefaultConfig(t *testing.T) {
	cfg := DefaultTMConfig()
	if cfg.T1 != DefaultT1 {
		t.Errorf("T1 = %v, want %v", cfg.T1, DefaultT1)
	}
	if cfg.FRTimeout != DefaultFRTimeout {
		t.Errorf("FRTimeout = %v, want %v", cfg.FRTimeout, DefaultFRTimeout)
	}
	if !cfg.AutoInv100 {
		t.Error("AutoInv100 should be true by default")
	}
}

func TestTM_TimerManagerWithConfig(t *testing.T) {
	cfg := &TMConfig{
		T1:           200 * time.Millisecond,
		T2:           2 * time.Second,
		FRTimeout:    10 * time.Second,
		FRInvTimeout: 60 * time.Second,
	}
	tm := NewTimerManagerWithConfig(cfg)
	if tm.t1 != 200*time.Millisecond {
		t.Errorf("t1 = %v, want 200ms", tm.t1)
	}
	if tm.frTimeout != 10*time.Second {
		t.Errorf("frTimeout = %v, want 10s", tm.frTimeout)
	}
}

func TestTM_TimerSetConfig(t *testing.T) {
	tm := NewTimerManager()
	newCfg := &TMConfig{
		T1:           250 * time.Millisecond,
		FRTimeout:    15 * time.Second,
		FRInvTimeout: 45 * time.Second,
	}
	tm.SetConfig(newCfg)
	got := tm.GetConfig()
	if got.T1 != 250*time.Millisecond {
		t.Errorf("T1 = %v, want 250ms", got.T1)
	}
	if got.FRTimeout != 15*time.Second {
		t.Errorf("FRTimeout = %v, want 15s", got.FRTimeout)
	}
}

func TestTM_MaxLifetimeTimer(t *testing.T) {
	mgr := NewManagerWithTimers(64)
	tm := mgr.TimerManager()

	inviteRaw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK-maxlife\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-maxlife@example.com\r\n" +
		"CSeq: 314159 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	inviteMsg, err := parser.ParseMsg(inviteRaw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}

	cell, err := mgr.NewTransaction(inviteMsg)
	if err != nil {
		t.Fatalf("NewTransaction failed: %v", err)
	}

	// Start a very short max lifetime timer
	tm.StartMaxLifetimeTimer(cell, 50*time.Millisecond)
	if !tm.HasTimers(cell) {
		t.Error("expected timers to be active")
	}

	// Wait for the timer to fire
	time.Sleep(100 * time.Millisecond)

	// After the timer fires, the cell should have been removed
	found := mgr.GetTable().LookupByMsg(inviteMsg)
	if found != nil {
		found.Unref()
		t.Error("expected cell to be removed after max lifetime timer")
	}
}

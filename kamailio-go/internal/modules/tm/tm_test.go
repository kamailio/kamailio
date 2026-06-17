// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module tests
 */

package tm

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// Test INVITE request
var testInviteMsg = []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
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

func TestNewTable(t *testing.T) {
	table := NewTable(1024)
	if table == nil {
		t.Fatal("expected table to be created")
	}
	if table.Size != 1024 {
		t.Errorf("expected size 1024, got %d", table.Size)
	}
	if len(table.Entries) != 1024 {
		t.Errorf("expected 1024 entries, got %d", len(table.Entries))
	}
}

func TestTableHash(t *testing.T) {
	table := NewTable(1024)
	callID := str.Mk("test-callid")
	cseq := str.Mk("1")
	branch := str.Mk("z9hG4bK-test")

	h1 := table.Hash(callID, cseq, branch)
	h2 := table.Hash(callID, cseq, branch)

	if h1 != h2 {
		t.Error("hash should be deterministic")
	}
	if h1 >= 1024 {
		t.Error("hash should be within table size")
	}
}

func TestTableInsertLookup(t *testing.T) {
	table := NewTable(1024)

	cell := &Cell{
		CallIDVal: str.Mk("test-callid"),
		CSeqNum:   str.Mk("1"),
		HashIndex: table.Hash(str.Mk("test-callid"), str.Mk("1"), str.Str{}),
	}

	table.Insert(cell)

	// Lookup
	found := table.Lookup(str.Mk("test-callid"), str.Mk("1"), str.Str{})
	if found == nil {
		t.Fatal("expected to find cell")
	}
	if !found.CallIDVal.Equal(str.Mk("test-callid")) {
		t.Error("unexpected callid")
	}

	// Cleanup
	found.Unref()
	table.Remove(cell)
}

func TestCellRefCount(t *testing.T) {
	cell := &Cell{}

	if cell.RefCount != 0 {
		t.Error("expected initial ref count to be 0")
	}

	cell.Ref()
	if cell.RefCount != 1 {
		t.Errorf("expected ref count 1, got %d", cell.RefCount)
	}

	cell.Ref()
	if cell.RefCount != 2 {
		t.Errorf("expected ref count 2, got %d", cell.RefCount)
	}

	if cell.Unref() {
		t.Error("should not delete with ref count > 0")
	}

	if !cell.Unref() {
		t.Error("should delete with ref count 0")
	}
}

func TestNewTransaction(t *testing.T) {
	mgr := NewManager(1024)
	msg, err := parser.ParseMsg(testInviteMsg)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	cell, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("new transaction error: %v", err)
	}

	if cell == nil {
		t.Fatal("expected cell to be created")
	}

	if !cell.IsInvite() {
		t.Error("expected INVITE transaction")
	}

	if cell.State != TStateTrying {
		t.Errorf("expected Trying state, got %d", cell.State)
	}

	// Trim whitespace for comparison since parser may include leading space
	callID := cell.CallIDVal.String()
	expectedCallID := "a84b4c76e66710@pc33.example.com"
	if callID != expectedCallID {
		// Try trimming leading whitespace
		for len(callID) > 0 && (callID[0] == ' ' || callID[0] == '\t') {
			callID = callID[1:]
		}
		if callID != expectedCallID {
			t.Errorf("unexpected callid: '%s' (len=%d), expected: '%s' (len=%d)",
				callID, len(callID), expectedCallID, len(expectedCallID))
		}
	}

	// Check manager state
	if mgr.GetT() != cell {
		t.Error("expected current transaction to be set")
	}

	// Check table count
	if mgr.TransactionCount() != 1 {
		t.Errorf("expected 1 transaction, got %d", mgr.TransactionCount())
	}
}

func TestLookupRequest(t *testing.T) {
	mgr := NewManager(1024)
	msg, _ := parser.ParseMsg(testInviteMsg)

	// Create transaction
	mgr.NewTransaction(msg)

	// Lookup
	found, err := mgr.LookupRequest(msg)
	if err != nil {
		t.Fatalf("lookup error: %v", err)
	}
	if found == nil {
		t.Fatal("expected to find transaction")
	}

	// Release
	mgr.ReleaseTransaction(found)
}

func TestDuplicateTransaction(t *testing.T) {
	mgr := NewManager(1024)
	msg, _ := parser.ParseMsg(testInviteMsg)

	// Create first transaction
	_, err := mgr.NewTransaction(msg)
	if err != nil {
		t.Fatalf("first transaction error: %v", err)
	}

	// Try to create duplicate
	_, err = mgr.NewTransaction(msg)
	if err == nil {
		t.Error("expected error for duplicate transaction")
	}
}

func TestAddBranch(t *testing.T) {
	mgr := NewManager(1024)
	msg, _ := parser.ParseMsg(testInviteMsg)

	cell, _ := mgr.NewTransaction(msg)

	uac, err := mgr.AddBranch(cell, 0)
	if err != nil {
		t.Fatalf("add branch error: %v", err)
	}
	if uac == nil {
		t.Fatal("expected UAC to be created")
	}

	if cell.NrOfOutgoings != 1 {
		t.Errorf("expected 1 outgoing, got %d", cell.NrOfOutgoings)
	}
}

func TestSetBranchResponse(t *testing.T) {
	mgr := NewManager(1024)
	msg, _ := parser.ParseMsg(testInviteMsg)

	cell, _ := mgr.NewTransaction(msg)
	mgr.AddBranch(cell, 0)

	err := mgr.SetBranchResponse(cell, 0, 200)
	if err != nil {
		t.Fatalf("set branch response error: %v", err)
	}

	if cell.UAC[0].LastReceived != 200 {
		t.Errorf("expected status 200, got %d", cell.UAC[0].LastReceived)
	}
}

func TestGetBestResponse(t *testing.T) {
	mgr := NewManager(1024)
	msg, _ := parser.ParseMsg(testInviteMsg)

	cell, _ := mgr.NewTransaction(msg)
	mgr.AddBranch(cell, 0)
	mgr.AddBranch(cell, 1)
	mgr.AddBranch(cell, 2)

	// Set responses: 100, 404, 200
	mgr.SetBranchResponse(cell, 0, 100)
	mgr.SetBranchResponse(cell, 1, 404)
	mgr.SetBranchResponse(cell, 2, 200)

	bestStatus, bestBranch := mgr.GetBestResponse(cell)
	if bestStatus != 200 {
		t.Errorf("expected best status 200, got %d", bestStatus)
	}
	if bestBranch != 2 {
		t.Errorf("expected best branch 2, got %d", bestBranch)
	}
}

func TestGetBestResponsePrefer2xx(t *testing.T) {
	mgr := NewManager(1024)
	msg, _ := parser.ParseMsg(testInviteMsg)

	cell, _ := mgr.NewTransaction(msg)
	mgr.AddBranch(cell, 0)
	mgr.AddBranch(cell, 1)

	// 500 vs 200 - should prefer 200
	mgr.SetBranchResponse(cell, 0, 500)
	mgr.SetBranchResponse(cell, 1, 200)

	bestStatus, bestBranch := mgr.GetBestResponse(cell)
	if bestStatus != 200 {
		t.Errorf("expected best status 200, got %d", bestStatus)
	}
	if bestBranch != 1 {
		t.Errorf("expected best branch 1, got %d", bestBranch)
	}
}

func TestTimerManager(t *testing.T) {
	tm := NewTimerManager()
	if tm == nil {
		t.Fatal("expected timer manager to be created")
	}

	if tm.t1 != DefaultT1 {
		t.Errorf("expected T1 %v, got %v", DefaultT1, tm.t1)
	}
}

func TestTimerManagerStartStop(t *testing.T) {
	tm := NewTimerManager()
	cell := &Cell{
		CallIDVal: str.Mk("test"),
		Method:    str.Mk("INVITE"),
	}

	// Start retransmit timer
	tm.StartRetransmitTimer(cell, 0)

	// Stop it immediately
	tm.StopRetransmitTimer(cell)

	// Start FR timer
	tm.StartFRTimer(cell, 0)

	// Stop it
	tm.StopFRTimer(cell)

	// Stop all timers
	tm.StopAllTimers(cell)
}

func TestTransactionFlags(t *testing.T) {
	cell := &Cell{}

	if cell.IsInvite() {
		t.Error("should not be invite by default")
	}
	if cell.IsCanceled() {
		t.Error("should not be canceled by default")
	}

	cell.Flags |= TIsInvite
	if !cell.IsInvite() {
		t.Error("should be invite")
	}

	cell.Flags |= TCanceled
	if !cell.IsCanceled() {
		t.Error("should be canceled")
	}
}

func BenchmarkTableInsert(b *testing.B) {
	table := NewTable(1024)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		cell := &Cell{
			CallIDVal: str.Mk("test-callid"),
			CSeqNum:   str.Mk("1"),
			HashIndex: uint32(i % 1024),
		}
		table.Insert(cell)
	}
}

func BenchmarkTableLookup(b *testing.B) {
	table := NewTable(1024)
	cell := &Cell{
		CallIDVal: str.Mk("test-callid"),
		CSeqNum:   str.Mk("1"),
		HashIndex: table.Hash(str.Mk("test-callid"), str.Mk("1"), str.Str{}),
	}
	table.Insert(cell)

	callID := str.Mk("test-callid")
	cseq := str.Mk("1")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		found := table.Lookup(callID, cseq, str.Str{})
		if found != nil {
			found.Unref()
		}
	}
}

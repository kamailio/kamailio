// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Lump operations tests
 */

package parser

import (
	"testing"
)

func TestNewLump(t *testing.T) {
	lump := NewLump(LumpDel, 100, 50, nil)
	if lump.Op != LumpDel {
		t.Errorf("Expected Op=LumpDel, got %v", lump.Op)
	}
	if lump.Offset != 100 {
		t.Errorf("Expected Offset=100, got %d", lump.Offset)
	}
	if lump.Len != 50 {
		t.Errorf("Expected Len=50, got %d", lump.Len)
	}
}

func TestLumpList(t *testing.T) {
	ll := &LumpList{}

	// Test empty list
	if !ll.IsEmpty() {
		t.Error("Expected empty list")
	}
	if ll.Count() != 0 {
		t.Errorf("Expected count=0, got %d", ll.Count())
	}

	// Test append
	lump1 := NewLump(LumpDel, 10, 5, nil)
	ll.Append(lump1)
	if ll.Count() != 1 {
		t.Errorf("Expected count=1, got %d", ll.Count())
	}
	if ll.First() != lump1 {
		t.Error("First() should return lump1")
	}
	if ll.Last() != lump1 {
		t.Error("Last() should return lump1")
	}

	// Test prepend
	lump2 := NewLump(LumpAdd, 20, 0, []byte("test"))
	ll.Prepend(lump2)
	if ll.Count() != 2 {
		t.Errorf("Expected count=2, got %d", ll.Count())
	}
	if ll.First() != lump2 {
		t.Error("First() should return lump2")
	}
	if ll.Last() != lump1 {
		t.Error("Last() should return lump1")
	}

	// Test clear
	ll.Clear()
	if !ll.IsEmpty() {
		t.Error("Expected empty list after Clear()")
	}
}

func TestLumpInsertBeforeAfter(t *testing.T) {
	lump := NewLump(LumpDel, 100, 50, nil)

	// Insert before
	beforeLump := NewLump(LumpAdd, 90, 0, []byte("before"))
	lump.InsertBefore(beforeLump)
	if lump.Before != beforeLump {
		t.Error("Before should be set")
	}

	// Insert after
	afterLump := NewLump(LumpAdd, 150, 0, []byte("after"))
	lump.InsertAfter(afterLump)
	if lump.After != afterLump {
		t.Error("After should be set")
	}
}

func TestDeleteLump(t *testing.T) {
	lump := DeleteLump(100, 50)
	if lump.Op != LumpDel {
		t.Errorf("Expected Op=LumpDel, got %v", lump.Op)
	}
	if lump.Flags&LumpFlagIsDel == 0 {
		t.Error("Expected LumpFlagIsDel to be set")
	}
}

func TestAddLump(t *testing.T) {
	lump := AddLump(100, []byte("test"), false)
	if lump.Op != LumpAdd {
		t.Errorf("Expected Op=LumpAdd, got %v", lump.Op)
	}
	if lump.Flags&LumpFlagIsAdd == 0 {
		t.Error("Expected LumpFlagIsAdd to be set")
	}
	if lump.Flags&LumpFlagIsAfter == 0 {
		t.Error("Expected LumpFlagIsAfter to be set")
	}

	lumpBefore := AddLump(100, []byte("test"), true)
	if lumpBefore.Flags&LumpFlagIsBefore == 0 {
		t.Error("Expected LumpFlagIsBefore to be set")
	}
}

func TestReplaceLump(t *testing.T) {
	lump := ReplaceLump(100, 50, []byte("new"))
	if lump.Op != LumpAdd {
		t.Errorf("Expected Op=LumpAdd, got %v", lump.Op)
	}
	if lump.Flags&LumpFlagIsDel == 0 {
		t.Error("Expected LumpFlagIsDel to be set")
	}
	if lump.Flags&LumpFlagIsAdd == 0 {
		t.Error("Expected LumpFlagIsAdd to be set")
	}
}

func TestLumpManager(t *testing.T) {
	lm := NewLumpManager()

	// Test DelLump
	lump := lm.DelLump(100, 50, LumpFlagHeader)
	if lump == nil {
		t.Fatal("DelLump returned nil")
	}
	if lump.Op != LumpDel {
		t.Errorf("Expected Op=LumpDel, got %v", lump.Op)
	}
	if lump.Flags&LumpFlagHeader == 0 {
		t.Error("Expected LumpFlagHeader to be set")
	}

	// Test AddLumpToMsg
	lump2 := lm.AddLumpToMsg(200, []byte("test"), LumpFlagBody)
	if lump2 == nil {
		t.Fatal("AddLumpToMsg returned nil")
	}
	if lump2.Op != LumpAdd {
		t.Errorf("Expected Op=LumpAdd, got %v", lump2.Op)
	}

	// Test AddBodyLump
	lump3 := lm.AddBodyLump(300, []byte("body"), 0)
	if lump3 == nil {
		t.Fatal("AddBodyLump returned nil")
	}
	if lump3.Flags&LumpFlagBody == 0 {
		t.Error("Expected LumpFlagBody to be set")
	}
}

func TestLumpListClone(t *testing.T) {
	ll := &LumpList{}
	ll.Append(NewLump(LumpDel, 10, 5, nil))
	ll.Append(NewLump(LumpAdd, 20, 0, []byte("test")))

	clone := ll.Clone()
	if clone.Count() != 2 {
		t.Errorf("Expected clone count=2, got %d", clone.Count())
	}

	// Verify it's a true copy
	ll.Clear()
	if !ll.IsEmpty() {
		t.Error("Original should be empty")
	}
	if clone.IsEmpty() {
		t.Error("Clone should not be empty")
	}
}

func TestApplyLump(t *testing.T) {
	buf := []byte("Hello, World!")

	// Test delete - delete "World" (5 chars starting at offset 7)
	lumpDel := DeleteLump(7, 5) // Delete "World"
	result := ApplyLump(buf, lumpDel)
	if string(result) != "Hello, !" {
		t.Errorf("Expected 'Hello, !', got '%s'", string(result))
	}

	// Test add - insert "Go" at offset 7
	lumpAdd := AddLump(7, []byte("Go"), false)
	result = ApplyLump(buf, lumpAdd)
	if string(result) != "Hello, GoWorld!" {
		t.Errorf("Expected 'Hello, GoWorld!', got '%s'", string(result))
	}
}

func TestApplyLumpList(t *testing.T) {
	buf := []byte("Hello, World!")

	ll := &LumpList{}
	ll.Append(DeleteLump(7, 5))        // Delete "World"
	ll.Append(AddLump(7, []byte("Go"), false)) // Add "Go"

	result := ApplyLumpList(buf, ll)
	// After delete at 7 (len 5), then add at 7, we get "Hello, Go!"
	// But the order matters - since we sort descending, delete happens first
	// Delete: "Hello, !" (offset 7, len 5 removes "World")
	// Add: insert "Go" at offset 7 -> "Hello, Go!"
	if string(result) != "Hello, Go!" {
		t.Errorf("Expected 'Hello, Go!', got '%s'", string(result))
	}
}

func TestRebuildMsgSimple(t *testing.T) {
	// INVITE sip:bob@example.com SIP/2.0\r\n = 36 chars
	// Via: SIP/2.0/UDP 10.0.0.1\r\n = 27 chars
	// \r\n = 2 chars
	// Total = 65 chars
	original := "INVITE sip:bob@example.com SIP/2.0\r\nVia: SIP/2.0/UDP 10.0.0.1\r\n\r\n"
	msg := &SIPMsg{
		Buf: []byte(original),
		Len: len(original),
	}

	ll := &LumpList{}
	// Delete "Via: SIP/2.0/UDP 10.0.0.1\r\n" starting at offset 36 (after first header)
	ll.Append(DeleteLump(36, 27))

	result, err := RebuildMsgSimple(msg, ll)
	if err != nil {
		t.Fatalf("RebuildMsgSimple failed: %v", err)
	}

	expected := "INVITE sip:bob@example.com SIP/2.0\r\n\r\n"
	resultStr := string(result)
	if resultStr != expected {
		t.Errorf("Expected len=%d '%q', got len=%d '%q'", len(expected), expected, len(resultStr), resultStr)
	}
}

func TestCalculateNewLen(t *testing.T) {
	msg := &SIPMsg{
		Len: 100,
	}

	lumps := &MsgLumps{}
	lumps.AddRM.Append(DeleteLump(10, 20)) // -20
	lumps.AddRM.Append(AddLump(50, []byte("test123"), false)) // +7

	newLen := CalculateNewLen(msg, lumps)
	expected := 100 - 20 + 7
	if newLen != expected {
		t.Errorf("Expected newLen=%d, got %d", expected, newLen)
	}
}

func TestValidateLumps(t *testing.T) {
	msg := &SIPMsg{
		Buf: make([]byte, 100),
		Len: 100,
	}

	// Valid lumps
	lumps := &MsgLumps{}
	lumps.AddRM.Append(DeleteLump(10, 20))
	if !ValidateLumps(msg, lumps) {
		t.Error("Expected valid lumps")
	}

	// Invalid lump (offset out of bounds)
	invalidLumps := &MsgLumps{}
	invalidLumps.AddRM.Append(DeleteLump(150, 10))
	if ValidateLumps(msg, invalidLumps) {
		t.Error("Expected invalid lumps")
	}

	// Invalid lump (delete extends beyond buffer)
	invalidLumps2 := &MsgLumps{}
	invalidLumps2.AddRM.Append(DeleteLump(90, 20))
	if ValidateLumps(msg, invalidLumps2) {
		t.Error("Expected invalid lumps")
	}
}

func TestMergeLumpLists(t *testing.T) {
	list1 := &LumpList{}
	list1.Append(NewLump(LumpDel, 10, 5, nil))

	list2 := &LumpList{}
	list2.Append(NewLump(LumpAdd, 20, 0, []byte("test")))

	merged := MergeLumpLists(list1, list2)
	if merged.Count() != 2 {
		t.Errorf("Expected merged count=2, got %d", merged.Count())
	}
}

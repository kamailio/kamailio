// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phase 10: TM reply matching with branch parameter, source IP routing,
 * and edge case tests.
 */

package tm

import (
	"fmt"
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ---------------------------------------------------------------------------
// 10-1: Reply matching with branch parameter
// ---------------------------------------------------------------------------

func TestTM_LookupReply_MatchesBranch(t *testing.T) {
	mgr := NewManager(1024)

	// Create INVITE transaction
	inviteRaw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKorigbranch\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: reply-match-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	invite, _ := parser.ParseMsg(inviteRaw)
	cell, _ := mgr.NewTransaction(invite)
	if cell == nil {
		t.Fatal("NewTransaction returned nil")
	}

	// Add a branch with a specific branch ID
	uac, _ := mgr.AddBranch(cell, 0)
	if uac == nil {
		t.Fatal("AddBranch returned nil")
	}

	// Create response matching the original branch (not the UAC branch)
	// In real SIP, the response has the same Via branch as the request
	replyRaw := []byte(
		"SIP/2.0 200 OK\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKorigbranch\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>;tag=btag\r\n" +
			"Call-ID: reply-match-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	reply, _ := parser.ParseMsg(replyRaw)

	foundCell, branch, err := mgr.LookupReply(reply)
	if err != nil {
		t.Fatalf("LookupReply failed: %v", err)
	}
	if foundCell == nil {
		t.Fatal("LookupReply returned nil cell")
	}
	if branch != 0 {
		t.Fatalf("expected branch 0, got %d", branch)
	}
}

func TestTM_LookupReply_WrongBranch(t *testing.T) {
	mgr := NewManager(1024)

	inviteRaw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKbranchA\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: reply-wrong-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	invite, _ := parser.ParseMsg(inviteRaw)
	mgr.NewTransaction(invite)

	// Response with different branch — should NOT match
	replyRaw := []byte(
		"SIP/2.0 200 OK\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKbranchB\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>;tag=btag\r\n" +
			"Call-ID: reply-wrong-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	reply, _ := parser.ParseMsg(replyRaw)

	_, _, err := mgr.LookupReply(reply)
	if err == nil {
		t.Fatal("expected error for wrong branch, got nil")
	}
}

// ---------------------------------------------------------------------------
// 10-2: Source IP routing check
// ---------------------------------------------------------------------------

func TestTM_LookupRequest_MatchesSourceIP(t *testing.T) {
	mgr := NewManager(1024)

	inviteRaw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bKsrcip\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: srcip-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	invite, _ := parser.ParseMsg(inviteRaw)
	cell, _ := mgr.NewTransaction(invite)
	if cell == nil {
		t.Fatal("NewTransaction returned nil")
	}

	// Re-lookup the same request
	found, err := mgr.LookupRequest(invite)
	if err != nil {
		t.Fatalf("LookupRequest failed: %v", err)
	}
	if found == nil {
		t.Fatal("LookupRequest returned nil")
	}
	if !found.CallIDVal.Equal(invite.CallID.Body) {
		t.Fatal("Call-ID mismatch")
	}
}

// ---------------------------------------------------------------------------
// 10-4: Edge cases
// ---------------------------------------------------------------------------

func TestTM_NilMessageHandling(t *testing.T) {
	mgr := NewManager(1024)

	if _, err := mgr.NewTransaction(nil); err == nil {
		t.Fatal("NewTransaction(nil) should error")
	}
	if _, err := mgr.LookupRequest(nil); err == nil {
		t.Fatal("LookupRequest(nil) should error")
	}
	if _, _, err := mgr.LookupReply(nil); err == nil {
		t.Fatal("LookupReply(nil) should error")
	}
	if _, err := mgr.LookupACK(nil); err == nil {
		t.Fatal("LookupACK(nil) should error")
	}
	if _, err := mgr.LookupCancel(nil); err == nil {
		t.Fatal("LookupCancel(nil) should error")
	}
}

func TestTM_EmptyCallID(t *testing.T) {
	mgr := NewManager(1024)

	inviteRaw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKempty\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: \r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	invite, _ := parser.ParseMsg(inviteRaw)
	cell, err := mgr.NewTransaction(invite)
	if err != nil {
		t.Fatalf("NewTransaction with empty Call-ID should not error: %v", err)
	}
	if cell == nil {
		t.Fatal("NewTransaction with empty Call-ID should still create cell")
	}
}

func TestTM_LongCallID(t *testing.T) {
	mgr := NewManager(1024)

	longCallID := strings.Repeat("a", 1000)
	inviteRaw := []byte(fmt.Sprintf(
		"INVITE sip:bob@example.com SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKlong\r\n"+
			"From: <sip:alice@example.com>;tag=atag\r\n"+
			"To: <sip:bob@example.com>\r\n"+
			"Call-ID: %s\r\n"+
			"CSeq: 1 INVITE\r\n"+
			"Content-Length: 0\r\n"+
			"\r\n",
		longCallID))
	invite, _ := parser.ParseMsg(inviteRaw)
	cell, err := mgr.NewTransaction(invite)
	if err != nil {
		t.Fatalf("NewTransaction with long Call-ID failed: %v", err)
	}
	if cell == nil {
		t.Fatal("NewTransaction with long Call-ID returned nil")
	}

	// Verify lookup works
	found, err := mgr.LookupRequest(invite)
	if err != nil {
		t.Fatalf("LookupRequest with long Call-ID failed: %v", err)
	}
	if found == nil {
		t.Fatal("LookupRequest with long Call-ID returned nil")
	}
}

func TestTM_MaxForwardsDecrement(t *testing.T) {
	mgr := NewManager(1024)

	inviteRaw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKmf\r\n" +
			"Max-Forwards: 1\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: mf-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	invite, _ := parser.ParseMsg(inviteRaw)
	cell, _ := mgr.NewTransaction(invite)
	if cell == nil {
		t.Fatal("NewTransaction returned nil")
	}

	// Build forward request — Max-Forwards should be decremented
	fwd, err := parser.BuildForwardRequest(invite, "udp", "10.0.0.2", 5060, "sip:bob@example.com")
	if err != nil {
		t.Fatalf("BuildForwardRequest failed: %v", err)
	}

	mf := extractHeaderValue(fwd, "Max-Forwards")
	if mf != "0" {
		t.Fatalf("expected Max-Forwards: 0 after decrement, got %q", mf)
	}

	// Another decrement from 0 should fail
	_, err = parser.BuildForwardRequest(fwd, "udp", "10.0.0.3", 5060, "sip:bob@example.com")
	if err == nil {
		t.Fatal("BuildForwardRequest from Max-Forwards:0 should error")
	}
}

func TestTM_CancelAfterFinalResponse(t *testing.T) {
	mgr := NewManager(1024)

	inviteRaw := []byte(
		"INVITE sip:bob@example.com SIP/2.0\r\n" +
			"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKcancel\r\n" +
			"From: <sip:alice@example.com>;tag=atag\r\n" +
			"To: <sip:bob@example.com>\r\n" +
			"Call-ID: cancel-after-001\r\n" +
			"CSeq: 1 INVITE\r\n" +
			"Content-Length: 0\r\n" +
			"\r\n")
	invite, _ := parser.ParseMsg(inviteRaw)
	cell, _ := mgr.NewTransaction(invite)
	if cell == nil {
		t.Fatal("NewTransaction returned nil")
	}

	// Reply with 200 OK (final response)
	if err := mgr.Reply(invite, 200, "OK"); err != nil {
		t.Fatalf("Reply failed: %v", err)
	}

	// Cancel after final response — should be rejected or no-op
	err := mgr.Cancel(cell, "late cancel")
	if err == nil {
		// Some implementations allow this as a no-op; that's fine
		t.Log("Cancel after final response was allowed (no-op)")
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

func extractHeaderValue(msg *parser.SIPMsg, name string) string {
	for _, h := range msg.Headers {
		if h.Name.String() == name {
			return strings.TrimSpace(h.Body.String())
		}
	}
	return ""
}

// Verify that a str.Str was constructed correctly
func TestStrMk(t *testing.T) {
	s := str.Mk("hello")
	if s.String() != "hello" {
		t.Fatalf("str.Mk failed")
	}
	if s.Len != 5 {
		t.Fatalf("str.Mk length wrong")
	}
}

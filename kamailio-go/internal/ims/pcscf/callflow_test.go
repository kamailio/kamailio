// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * P-CSCF complete call flow tests
 *
 * Tests the full lifecycle: INVITE -> session created -> BYE -> session removed,
 * as well as edge cases like missing headers, invalid methods, and concurrent sessions.
 */

package pcscf

import (
	"fmt"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// buildTestMsg constructs a minimal SIPMsg for testing.
func buildTestMsg(method parser.RequestMethod, callID, from, to, contact string) *parser.SIPMsg {
	msg := &parser.SIPMsg{
		FirstLine: &parser.MsgStart{
			Type:  parser.MsgRequest,
			Flags: parser.FLINEFlagProtoSIP,
			Req: &parser.RequestLine{
				MethodValue: method,
			},
		},
		CallID: &parser.HdrField{
			Body: str.Mk(callID),
		},
		From: &parser.HdrField{
			Body: str.Mk(from),
		},
		To: &parser.HdrField{
			Body: str.Mk(to),
		},
		Contact: &parser.HdrField{
			Body: str.Mk(contact),
		},
	}
	return msg
}

// buildInviteMsg constructs an INVITE message with optional P-Access-Network-Info.
func buildInviteMsg(callID, fromTag, toTag, contact, pani string) *parser.SIPMsg {
	msg := buildTestMsg(
		parser.MethodInvite,
		callID,
		`"Alice" <sip:alice@example.com>;tag=`+fromTag,
		`"Bob" <sip:bob@example.com>;tag=`+toTag,
		contact,
	)
	if pani != "" {
		msg.PAccessNetworkInfo = &parser.HdrField{
			Type: parser.HdrPAccessNetworkInfo,
			Body: str.Mk(pani),
		}
	}
	return msg
}

// buildByeMsg constructs a BYE message.
func buildByeMsg(callID, fromTag, toTag, contact string) *parser.SIPMsg {
	return buildTestMsg(
		parser.MethodBye,
		callID,
		`"Alice" <sip:alice@example.com>;tag=`+fromTag,
		`"Bob" <sip:bob@example.com>;tag=`+toTag,
		contact,
	)
}

// TestCallFlow_RegisterInviteBye tests the complete P-CSCF call flow:
// INVITE -> session created -> BYE -> session removed.
func TestCallFlow_RegisterInviteBye(t *testing.T) {
	sh := NewSessionHandler()
	if sh == nil {
		t.Fatal("expected non-nil SessionHandler")
	}
	if sh.GetSessionCount() != 0 {
		t.Fatalf("expected 0 sessions, got %d", sh.GetSessionCount())
	}

	// 1. INVITE from Alice to Bob
	invite := buildInviteMsg(
		"call-flow-001",
		"alice-tag-001",
		"bob-tag-001",
		"sip:alice@192.168.1.100:5060",
		"3GPP-UTRAN-FDD;utran-cell-id-3gpp=1234;plmn=262-01",
	)

	result, err := sh.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite failed: %v", err)
	}
	if result.StatusCode != 100 {
		t.Errorf("expected status 100 Trying, got %d", result.StatusCode)
	}

	// Verify session was created
	if sh.GetSessionCount() != 1 {
		t.Fatalf("expected 1 session after INVITE, got %d", sh.GetSessionCount())
	}

	sess := sh.GetSession("call-flow-001")
	if sess == nil {
		t.Fatal("expected non-nil session")
	}
	if sess.CallID != "call-flow-001" {
		t.Errorf("expected CallID call-flow-001, got %s", sess.CallID)
	}
	if sess.UEContact != "sip:alice@192.168.1.100:5060" {
		t.Errorf("expected UEContact sip:alice@192.168.1.100:5060, got %s", sess.UEContact)
	}
	if !sess.IsMO {
		t.Error("expected IsMO to be true for mobile-originated call")
	}

	// Verify P-Visited-Network-ID header was added for MO
	if result.Headers == nil {
		t.Fatal("expected non-nil result headers")
	}
	if _, ok := result.Headers["P-Visited-Network-ID"]; !ok {
		t.Error("expected P-Visited-Network-ID header in MO response")
	}

	// 2. BYE to terminate the call
	bye := buildByeMsg(
		"call-flow-001",
		"alice-tag-001",
		"bob-tag-001",
		"sip:alice@192.168.1.100:5060",
	)

	byeResult, err := sh.HandleBye(bye)
	if err != nil {
		t.Fatalf("HandleBye failed: %v", err)
	}
	if byeResult.StatusCode != 200 {
		t.Errorf("expected status 200 OK for BYE, got %d", byeResult.StatusCode)
	}

	// Verify session was removed
	if sh.GetSessionCount() != 0 {
		t.Fatalf("expected 0 sessions after BYE, got %d", sh.GetSessionCount())
	}
	if sh.GetSession("call-flow-001") != nil {
		t.Error("expected nil session after BYE")
	}
}

// TestCallFlow_InviteWithoutPANI tests INVITE without P-Access-Network-Info.
func TestCallFlow_InviteWithoutPANI(t *testing.T) {
	sh := NewSessionHandler()

	invite := buildInviteMsg(
		"call-nopani",
		"ftag",
		"ttag",
		"sip:alice@192.168.1.100:5060",
		"", // no P-Access-Network-Info
	)

	result, err := sh.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite failed: %v", err)
	}
	if result.StatusCode != 100 {
		t.Errorf("expected status 100, got %d", result.StatusCode)
	}
	if sh.GetSessionCount() != 1 {
		t.Fatalf("expected 1 session, got %d", sh.GetSessionCount())
	}
}

// TestCallFlow_InviteInvalidMethod tests that non-INVITE methods are rejected.
func TestCallFlow_InviteInvalidMethod(t *testing.T) {
	sh := NewSessionHandler()

	// Try to send a REGISTER as an INVITE
	register := buildTestMsg(
		parser.MethodRegister,
		"call-reg",
		`"Alice" <sip:alice@example.com>;tag=ftag`,
		`"Bob" <sip:bob@example.com>;tag=ttag`,
		"sip:alice@192.168.1.100:5060",
	)

	_, err := sh.HandleInvite(register)
	if err == nil {
		t.Fatal("expected error for non-INVITE method")
	}
}

// TestCallFlow_InviteNilMessage tests nil message handling.
func TestCallFlow_InviteNilMessage(t *testing.T) {
	sh := NewSessionHandler()

	_, err := sh.HandleInvite(nil)
	if err == nil {
		t.Fatal("expected error for nil message")
	}
}

// TestCallFlow_ByeNonExistentSession tests BYE for a session that doesn't exist.
func TestCallFlow_ByeNonExistentSession(t *testing.T) {
	sh := NewSessionHandler()

	bye := buildByeMsg(
		"call-nonexist",
		"ftag",
		"ttag",
		"sip:alice@192.168.1.100:5060",
	)

	result, err := sh.HandleBye(bye)
	if err != nil {
		t.Fatalf("HandleBye failed: %v", err)
	}
	// Should still return a result even if session doesn't exist
	if result == nil {
		t.Fatal("expected non-nil result")
	}
}

// TestCallFlow_MultipleSessions tests handling multiple concurrent sessions.
func TestCallFlow_MultipleSessions(t *testing.T) {
	sh := NewSessionHandler()

	// Create 3 sessions
	for i := 0; i < 3; i++ {
		callID := fmt.Sprintf("call-multi-%d", i)
		invite := buildInviteMsg(
			callID,
			fmt.Sprintf("ftag-%d", i),
			fmt.Sprintf("ttag-%d", i),
			fmt.Sprintf("sip:alice@192.168.1.100:%d", 5060+i),
			"3GPP-E-UTRAN-FDD;cgi-3gpp=5678;plmn=310-260",
		)
		result, err := sh.HandleInvite(invite)
		if err != nil {
			t.Fatalf("HandleInvite %d failed: %v", i, err)
		}
		if result.StatusCode != 100 {
			t.Errorf("session %d: expected status 100, got %d", i, result.StatusCode)
		}
	}

	if sh.GetSessionCount() != 3 {
		t.Fatalf("expected 3 sessions, got %d", sh.GetSessionCount())
	}

	// Verify each session
	for i := 0; i < 3; i++ {
		callID := fmt.Sprintf("call-multi-%d", i)
		sess := sh.GetSession(callID)
		if sess == nil {
			t.Errorf("expected non-nil session for %s", callID)
			continue
		}
		if sess.CallID != callID {
			t.Errorf("session %d: expected CallID %s, got %s", i, callID, sess.CallID)
		}
	}

	// Terminate all sessions
	for i := 0; i < 3; i++ {
		callID := fmt.Sprintf("call-multi-%d", i)
		bye := buildByeMsg(
			callID,
			fmt.Sprintf("ftag-%d", i),
			fmt.Sprintf("ttag-%d", i),
			fmt.Sprintf("sip:alice@192.168.1.100:%d", 5060+i),
		)
		_, err := sh.HandleBye(bye)
		if err != nil {
			t.Fatalf("HandleBye %d failed: %v", i, err)
		}
	}

	if sh.GetSessionCount() != 0 {
		t.Fatalf("expected 0 sessions after all BYEs, got %d", sh.GetSessionCount())
	}
}

// TestCallFlow_SessionRecordFields verifies all fields of PCSCFSessionRecord.
func TestCallFlow_SessionRecordFields(t *testing.T) {
	sh := NewSessionHandler()

	invite := buildInviteMsg(
		"call-fields",
		"alice-ftag",
		"bob-ttag",
		"sip:alice@192.168.1.100:5060",
		"3GPP-UTRAN-TDD;utran-cell-id-3gpp=AAAA;location-area-3gpp=BBBB;routing-area-3gpp=CCCC;plmn=460-00",
	)

	result, err := sh.HandleInvite(invite)
	if err != nil {
		t.Fatalf("HandleInvite failed: %v", err)
	}

	sess := sh.GetSession("call-fields")
	if sess == nil {
		t.Fatal("expected non-nil session")
	}

	// Verify all expected fields
	if sess.FromTag == "" {
		t.Error("expected non-empty FromTag")
	}
	// Note: ToTag is not extracted by HandleInvite (only FromTag is parsed).
	// ToTag would typically be populated on subsequent responses (e.g., 200 OK).
	if sess.UEContact != "sip:alice@192.168.1.100:5060" {
		t.Errorf("expected UEContact sip:alice@192.168.1.100:5060, got %s", sess.UEContact)
	}
	if !sess.IsMO {
		t.Error("expected IsMO = true")
	}
	if sess.IsMT {
		t.Error("expected IsMT = false for MO call")
	}

	// Verify result headers
	if result == nil {
		t.Fatal("expected non-nil result")
	}
	if result.StatusCode != 100 {
		t.Errorf("expected 100, got %d", result.StatusCode)
	}
	if result.StatusReason != "Trying" {
		t.Errorf("expected 'Trying', got %s", result.StatusReason)
	}
}

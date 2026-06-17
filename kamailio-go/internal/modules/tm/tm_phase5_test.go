// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module - Phase 5 integration tests
 * Covers: Create/Reply lifecycle, relay + response handling,
 * state transitions, cancel semantics, and build/send roundtrip.
 */

package tm

import (
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// sampleINVITE returns a fresh INVITE SIPMsg parsed from raw bytes so that
// each test works on its own independent message object.
func sampleINVITE() *parser.SIPMsg {
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
		panic(err)
	}
	return msg
}

// sampleCANCEL returns a CANCEL request that shares the same Call-ID and
// From tag as the INVITE above, as required by RFC 3261 for CANCEL matching.
func sampleCANCEL() *parser.SIPMsg {
	raw := []byte("CANCEL sip:bob@ims.example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@ims.example.com>;tag=1928301774\r\n" +
		"To: Bob <sip:bob@ims.example.com>\r\n" +
		"Call-ID: a84b4c76e66710@192.168.1.100\r\n" +
		"CSeq: 314159 CANCEL\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		panic(err)
	}
	return msg
}

// ---- Test 1: Create transaction and reply with 100 Trying + 200 OK ----

func TestTM_CreateAndReply_INVITE(t *testing.T) {
	msg := sampleINVITE()
	mgr := NewManager(64)

	// Step 1: Create the transaction
	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}
	if cell == nil {
		t.Fatal("expected non-nil cell")
	}
	if !cell.IsInvite() {
		t.Error("expected cell.IsInvite() == true")
	}
	if cell.State != TStateTrying {
		t.Errorf("expected TStateTrying, got %d", cell.State)
	}

	// Step 2: Build 100 Trying response and verify the first line
	trying, err := parser.Create100Trying(msg)
	if err != nil {
		t.Fatalf("Create100Trying failed: %v", err)
	}
	tryingBytes, err := parser.BuildMessage(trying)
	if err != nil {
		t.Fatalf("BuildMessage failed for 100 Trying: %v", err)
	}
	firstLine := string(tryingBytes[:strings.Index(string(tryingBytes), "\r\n")])
	expected := "SIP/2.0 100 Trying"
	if firstLine != expected {
		t.Errorf("expected first line %q, got %q", expected, firstLine)
	}

	// Step 3: Reply with 100 Trying → state moves to Proceeding
	if err := mgr.Reply(msg, 100, "Trying"); err != nil {
		t.Fatalf("Reply(100) failed: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("after Reply(100) expected TStateProceeding, got %d", cell.State)
	}

	// Step 4: Build 200 OK and verify first line
	ok, err := parser.Create200OK(msg)
	if err != nil {
		t.Fatalf("Create200OK failed: %v", err)
	}
	okBytes, err := parser.BuildMessage(ok)
	if err != nil {
		t.Fatalf("BuildMessage failed for 200 OK: %v", err)
	}
	firstLine = string(okBytes[:strings.Index(string(okBytes), "\r\n")])
	expected = "SIP/2.0 200 OK"
	if firstLine != expected {
		t.Errorf("expected first line %q, got %q", expected, firstLine)
	}

	// Step 5: Reply with 200 → state moves to Completed
	if err := mgr.Reply(msg, 200, "OK"); err != nil {
		t.Fatalf("Reply(200) failed: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("after Reply(200) expected TStateCompleted, got %d", cell.State)
	}
}

// ---- Test 2: Relay request and handle response ----

func TestTM_RelayRequest_AndHandleResponse(t *testing.T) {
	msg := sampleINVITE()
	mgr := NewManager(64)

	// Create the transaction
	if _, err := mgr.HandleRequest(msg); err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}

	// Add a branch (relay)
	_, branch, err := mgr.RelayRequest(msg, "10.0.0.2", 5060)
	if err != nil {
		t.Fatalf("RelayRequest failed: %v", err)
	}
	if branch != 0 {
		t.Errorf("expected first branch == 0, got %d", branch)
	}

	// Build a forwarded request and generate a 200 OK response from it
	fwd, err := parser.BuildForwardRequest(msg, "UDP", "10.0.0.1", 5060, "sip:bob@10.0.0.2:5060")
	if err != nil {
		t.Fatalf("BuildForwardRequest failed: %v", err)
	}
	resp200, err := parser.Create200OK(fwd)
	if err != nil {
		t.Fatalf("Create200OK for fwd failed: %v", err)
	}

	// Serialize + re-parse the response to match it via HandleResponse
	respBytes, err := parser.BuildMessage(resp200)
	if err != nil {
		t.Fatalf("BuildMessage for response failed: %v", err)
	}
	parsedResp, err := parser.ParseMsg(respBytes)
	if err != nil {
		t.Fatalf("ParseMsg for response failed: %v", err)
	}

	// Match via HandleResponse
	matchCell, _, err := mgr.HandleResponse(parsedResp)
	if err != nil {
		t.Fatalf("HandleResponse failed: %v", err)
	}
	if matchCell == nil {
		t.Fatal("HandleResponse returned nil cell")
	}
	if matchCell.State != TStateCompleted {
		t.Errorf("expected TStateCompleted after HandleResponse(200), got %d", matchCell.State)
	}
	if matchCell.UAS.Status != 200 {
		t.Errorf("expected UAS.Status == 200, got %d", matchCell.UAS.Status)
	}
}

// ---- Test 3: State transitions ----

func TestTM_StateTransitions(t *testing.T) {
	msg := sampleINVITE()
	mgr := NewManager(64)

	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}
	if cell.State != TStateTrying {
		t.Fatalf("expected initial TStateTrying, got %d", cell.State)
	}

	// Trying → Proceeding (valid)
	if err := cell.UpdateState(TStateProceeding); err != nil {
		t.Fatalf("Trying->Proceeding failed: %v", err)
	}
	if cell.State != TStateProceeding {
		t.Errorf("expected TStateProceeding, got %d", cell.State)
	}

	// Proceeding → Completed (valid)
	if err := cell.UpdateState(TStateCompleted); err != nil {
		t.Fatalf("Proceeding->Completed failed: %v", err)
	}
	if cell.State != TStateCompleted {
		t.Errorf("expected TStateCompleted, got %d", cell.State)
	}

	// Completed → Trying (invalid)
	if err := cell.UpdateState(TStateTrying); err == nil {
		t.Error("expected error for Completed->Trying transition, got nil")
	}
	// State must remain unchanged
	if cell.State != TStateCompleted {
		t.Errorf("state must remain Completed after failed transition, got %d", cell.State)
	}
}

// ---- Test 4: Cancel ----

func TestTM_Cancel(t *testing.T) {
	msg := sampleINVITE()
	mgr := NewManager(64)

	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}

	// A CANCEL request shares Call-ID and From tag with the INVITE
	_ = sampleCANCEL()

	if err := mgr.Cancel(cell, "User cancelled"); err != nil {
		t.Fatalf("Cancel failed: %v", err)
	}

	if !cell.IsCanceled() {
		t.Error("expected cell.IsCanceled() == true after Cancel")
	}
	if cell.State != TStateCompleted {
		t.Errorf("expected TStateCompleted after Cancel, got %d", cell.State)
	}
	if cell.UAS.CancelReason != "User cancelled" {
		t.Errorf("expected CancelReason 'User cancelled', got %q", cell.UAS.CancelReason)
	}
}

// ---- Test 5: Build and send response roundtrip ----

func TestTM_BuildAndSendResponseRoundtrip(t *testing.T) {
	msg := sampleINVITE()
	mgr := NewManager(64)

	cell, err := mgr.HandleRequest(msg)
	if err != nil {
		t.Fatalf("HandleRequest failed: %v", err)
	}

	// Build a 480 response directly from the original request
	respBytes, err := parser.BuildSimpleResponse(msg, 480, "Temporarily Unavailable")
	if err != nil {
		t.Fatalf("BuildSimpleResponse failed: %v", err)
	}

	// Parse the response raw bytes back into a SIPMsg and verify status
	parsedResp, err := parser.ParseMsg(respBytes)
	if err != nil {
		t.Fatalf("ParseMsg for response failed: %v", err)
	}
	if parsedResp.StatusCode() != 480 {
		t.Errorf("expected parsed status 480, got %d", parsedResp.StatusCode())
	}

	// Use mgr.Reply to signal the final response to the transaction
	if err := mgr.Reply(msg, 480, "Temporarily Unavailable"); err != nil {
		t.Fatalf("Reply(480) failed: %v", err)
	}

	if cell.UAS.Status != 480 {
		t.Errorf("expected UAS.Status == 480, got %d", cell.UAS.Status)
	}
	if cell.State != TStateCompleted {
		t.Errorf("expected TStateCompleted after Reply(480), got %d", cell.State)
	}
}

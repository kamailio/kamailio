// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phase 8: End-to-end IMS integration tests
 *
 * Covers the complete IMS flow:
 *   1. UE -> S-CSCF: REGISTER (no auth)
 *   2. S-CSCF -> UE: 401 Unauthorized + AKA challenge
 *   3. UE -> S-CSCF: REGISTER + Authorization (AKA response)
 *   4. S-CSCF -> UE: 200 OK + Service-Route
 *   5. UE -> S-CSCF: INVITE (registered caller)
 *   6. S-CSCF -> UE: 100 Trying + RouteTarget
 *   7. Callee answers: 200 OK
 *   8. UE -> S-CSCF: BYE
 *   9. S-CSCF -> UE: 200 OK (session terminated)
 */

package integration

import (
	"bytes"
	"fmt"
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/ims/scscf"
)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

func buildREGISTER(impu, contact, authz string) []byte {
	var b strings.Builder
	b.WriteString("REGISTER sip:ims.example.com SIP/2.0\r\n")
	b.WriteString("Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bKreg001\r\n")
	b.WriteString("Max-Forwards: 70\r\n")
	b.WriteString(fmt.Sprintf("From: <%s>;tag=regtag001\r\n", impu))
	b.WriteString(fmt.Sprintf("To: <%s>\r\n", impu))
	b.WriteString("Call-ID: reg-callid-001\r\n")
	b.WriteString("CSeq: 1 REGISTER\r\n")
	b.WriteString(fmt.Sprintf("Contact: <%s>\r\n", contact))
	if authz != "" {
		b.WriteString(fmt.Sprintf("Authorization: %s\r\n", authz))
	}
	b.WriteString("Content-Length: 0\r\n")
	b.WriteString("\r\n")
	return []byte(b.String())
}

func buildINVITE(fromURI, toURI, fromTag, callID string, cseq int) []byte {
	var b strings.Builder
	b.WriteString(fmt.Sprintf("INVITE %s SIP/2.0\r\n", toURI))
	b.WriteString("Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bKinv001\r\n")
	b.WriteString("Max-Forwards: 70\r\n")
	b.WriteString(fmt.Sprintf("From: <%s>;tag=%s\r\n", fromURI, fromTag))
	b.WriteString(fmt.Sprintf("To: <%s>\r\n", toURI))
	b.WriteString(fmt.Sprintf("Call-ID: %s\r\n", callID))
	b.WriteString(fmt.Sprintf("CSeq: %d INVITE\r\n", cseq))
	b.WriteString("Contact: <sip:alice@192.168.1.100>\r\n")
	b.WriteString("Content-Type: application/sdp\r\n")
	b.WriteString("Content-Length: 0\r\n")
	b.WriteString("\r\n")
	return []byte(b.String())
}

func buildBYE(fromURI, toURI, fromTag, toTag, callID string, cseq int) []byte {
	var b strings.Builder
	b.WriteString(fmt.Sprintf("BYE %s SIP/2.0\r\n", toURI))
	b.WriteString("Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bKbye001\r\n")
	b.WriteString("Max-Forwards: 70\r\n")
	b.WriteString(fmt.Sprintf("From: <%s>;tag=%s\r\n", fromURI, fromTag))
	b.WriteString(fmt.Sprintf("To: <%s>;tag=%s\r\n", toURI, toTag))
	b.WriteString(fmt.Sprintf("Call-ID: %s\r\n", callID))
	b.WriteString(fmt.Sprintf("CSeq: %d BYE\r\n", cseq))
	b.WriteString("Contact: <sip:alice@192.168.1.100>\r\n")
	b.WriteString("Content-Length: 0\r\n")
	b.WriteString("\r\n")
	return []byte(b.String())
}

func extractHeaderValue(msg *parser.SIPMsg, name string) string {
	for _, h := range msg.Headers {
		if h.Name.String() == name {
			return h.Body.String()
		}
	}
	return ""
}

// ---------------------------------------------------------------------------
// Test: Complete IMS Registration + Call Flow
// ---------------------------------------------------------------------------

func TestIMS_CompleteRegistrationAndCallFlow(t *testing.T) {
	realm := "ims.example.com"
	impuAlice := "sip:alice@ims.example.com"
	contactAlice := "sip:alice@192.168.1.100"
	impuBob := "sip:bob@ims.example.com"
	contactBob := "sip:bob@192.168.1.200"

	registrar := scscf.NewRegistrar(realm)
	sessionH := scscf.NewSessionHandler(registrar)

	// ================================================================
	// Step 1: Initial REGISTER (no auth)
	// ================================================================
	raw1 := buildREGISTER(impuAlice, contactAlice, "")
	msg1, err := parser.ParseMsg(raw1)
	if err != nil {
		t.Fatalf("parse REGISTER failed: %v", err)
	}

	res1, err := registrar.HandleRegister(msg1)
	if err != nil {
		t.Fatalf("HandleRegister initial failed: %v", err)
	}
	if res1.StatusCode != 401 {
		t.Fatalf("expected 401, got %d", res1.StatusCode)
	}
	wwwAuth := res1.Headers["WWW-Authenticate"]
	if wwwAuth.Len == 0 {
		t.Fatal("missing WWW-Authenticate header")
	}

	// Extract challenge parameters
	challengeStr := wwwAuth.String()
	if !strings.Contains(challengeStr, "nonce=") {
		t.Fatal("WWW-Authenticate missing nonce")
	}
	if !strings.Contains(challengeStr, "opaque=") {
		t.Fatal("WWW-Authenticate missing opaque")
	}

	// Parse the challenge to get nonce and opaque for the response
	var nonce, opaque string
	parts := strings.Split(challengeStr, ",")
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if strings.HasPrefix(p, "nonce=") {
			nonce = strings.Trim(strings.TrimPrefix(p, "nonce="), "\"")
		}
		if strings.HasPrefix(p, "opaque=") {
			opaque = strings.Trim(strings.TrimPrefix(p, "opaque="), "\"")
		}
	}
	if nonce == "" || opaque == "" {
		t.Fatalf("failed to extract nonce/opaque from: %s", challengeStr)
	}

	// ================================================================
	// Step 2: REGISTER with Authorization (AKA response)
	// ================================================================
	// Get the stored auth vector to compute the correct response
	record := registrar.GetRecord(impuAlice)
	if record == nil || record.AuthState == nil {
		t.Fatal("no auth state found for pending registration")
	}
	av := record.AuthState.AuthVector
	expectedResp := fmt.Sprintf("%x", av.XRES)

	authz := fmt.Sprintf(
		`Digest username="alice", realm="%s", nonce="%s", uri="%s", response="%s", algorithm=AKAv1-MD5, opaque="%s"`,
		realm, nonce, impuAlice, expectedResp, opaque,
	)

	raw2 := buildREGISTER(impuAlice, contactAlice, authz)
	msg2, err := parser.ParseMsg(raw2)
	if err != nil {
		t.Fatalf("parse REGISTER with auth failed: %v", err)
	}

	res2, err := registrar.HandleRegister(msg2)
	if err != nil {
		t.Fatalf("HandleRegister auth response failed: %v", err)
	}
	if res2.StatusCode != 200 {
		t.Fatalf("expected 200 after auth, got %d", res2.StatusCode)
	}
	if res2.Headers["Service-Route"].Len == 0 {
		t.Fatal("missing Service-Route in 200 OK")
	}

	// Verify Alice is now registered
	if !registrar.IsRegistered(impuAlice) {
		t.Fatal("Alice should be registered after 200 OK")
	}
	gotContact := registrar.GetContact(impuAlice)
	// Contact may be stored with or without angle brackets — compare loosely
	if gotContact != contactAlice && gotContact != "<"+contactAlice+">" {
		t.Fatalf("contact mismatch: got %q, want %q", gotContact, contactAlice)
	}

	// ================================================================
	// Step 3: Register Bob (same flow, abbreviated)
	// ================================================================
	rawBob1 := buildREGISTER(impuBob, contactBob, "")
	msgBob1, _ := parser.ParseMsg(rawBob1)
	resBob1, _ := registrar.HandleRegister(msgBob1)
	if resBob1.StatusCode != 401 {
		t.Fatalf("Bob expected 401, got %d", resBob1.StatusCode)
	}

	// Extract Bob's challenge
	bobChallengeStr := resBob1.Headers["WWW-Authenticate"].String()
	var bobNonce, bobOpaque string
	for _, p := range strings.Split(bobChallengeStr, ",") {
		p = strings.TrimSpace(p)
		if strings.HasPrefix(p, "nonce=") {
			bobNonce = strings.Trim(strings.TrimPrefix(p, "nonce="), "\"")
		}
		if strings.HasPrefix(p, "opaque=") {
			bobOpaque = strings.Trim(strings.TrimPrefix(p, "opaque="), "\"")
		}
	}

	recordBob := registrar.GetRecord(impuBob)
	bobResp := fmt.Sprintf("%x", recordBob.AuthState.AuthVector.XRES)
	bobAuthz := fmt.Sprintf(
		`Digest username="bob", realm="%s", nonce="%s", uri="%s", response="%s", algorithm=AKAv1-MD5, opaque="%s"`,
		realm, bobNonce, impuBob, bobResp, bobOpaque,
	)

	rawBob2 := buildREGISTER(impuBob, contactBob, bobAuthz)
	msgBob2, _ := parser.ParseMsg(rawBob2)
	resBob2, _ := registrar.HandleRegister(msgBob2)
	if resBob2.StatusCode != 200 {
		t.Fatalf("Bob expected 200, got %d", resBob2.StatusCode)
	}

	// ================================================================
	// Step 4: Alice calls Bob (INVITE)
	// ================================================================
	rawInvite := buildINVITE(impuAlice, impuBob, "invtag001", "call-invite-001", 1)
	msgInvite, err := parser.ParseMsg(rawInvite)
	if err != nil {
		t.Fatalf("parse INVITE failed: %v", err)
	}

	resInvite, err := sessionH.HandleInvite(msgInvite)
	if err != nil {
		t.Fatalf("HandleInvite failed: %v", err)
	}
	if resInvite.StatusCode != 100 {
		t.Fatalf("expected 100 Trying, got %d", resInvite.StatusCode)
	}
	// RouteTarget may include angle brackets
	rt := resInvite.RouteTarget
	if rt != contactBob && rt != "<"+contactBob+">" {
		t.Fatalf("RouteTarget mismatch: got %q, want %q", rt, contactBob)
	}

	// ================================================================
	// Step 5: Build 200 OK response (simulating callee answer)
	// ================================================================
	replyOpts := parser.ReplyOptions{
		StatusCode:   200,
		ReasonPhrase: "OK",
		ExtraHeaders: [][2]string{
			{"Contact", contactBob},
			{"To", fmt.Sprintf("<%s>;tag=btag001", impuBob)},
		},
	}
	reply200, err := parser.CreateReply(msgInvite, replyOpts)
	if err != nil {
		t.Fatalf("CreateReply failed: %v", err)
	}
	replyBytes, err := parser.BuildMessage(reply200)
	if err != nil {
		t.Fatalf("BuildMessage failed: %v", err)
	}

	// Verify the 200 OK can be parsed
	parsed200, err := parser.ParseMsg(replyBytes)
	if err != nil {
		t.Fatalf("re-parse 200 OK failed: %v", err)
	}
	if parsed200.StatusCode() != 200 {
		t.Fatalf("expected status 200, got %d", parsed200.StatusCode())
	}

	// Handle the reply in session handler
	resReply, err := sessionH.HandleReply(parsed200)
	if err != nil {
		t.Fatalf("HandleReply failed: %v", err)
	}
	if resReply.StatusCode != 200 {
		t.Fatalf("expected 200 from HandleReply, got %d", resReply.StatusCode)
	}

	// ================================================================
	// Step 6: BYE to terminate session
	// ================================================================
	rawBye := buildBYE(impuAlice, impuBob, "invtag001", "btag001", "call-invite-001", 2)
	msgBye, err := parser.ParseMsg(rawBye)
	if err != nil {
		t.Fatalf("parse BYE failed: %v", err)
	}

	resBye, err := sessionH.HandleBye(msgBye)
	if err != nil {
		t.Fatalf("HandleBye failed: %v", err)
	}
	if resBye.StatusCode != 200 {
		t.Fatalf("expected 200 for BYE, got %d", resBye.StatusCode)
	}

	// Verify session is cleaned up
	if sessionH.GetSessionCount() != 0 {
		t.Fatalf("expected 0 sessions after BYE, got %d", sessionH.GetSessionCount())
	}

	// ================================================================
	// Step 7: Verify message round-trip integrity
	// ================================================================
	// All messages built with BuildMessage must be re-parseable
	for _, name := range []string{"REGISTER", "INVITE", "BYE"} {
		var raw []byte
		switch name {
		case "REGISTER":
			raw = raw2
		case "INVITE":
			raw = rawInvite
		case "BYE":
			raw = rawBye
		}
		msg, err := parser.ParseMsg(raw)
		if err != nil {
			t.Fatalf("parse %s failed: %v", name, err)
		}
		built, err := parser.BuildMessage(msg)
		if err != nil {
			t.Fatalf("BuildMessage %s failed: %v", name, err)
		}
		reparse, err := parser.ParseMsg(built)
		if err != nil {
			t.Fatalf("re-parse built %s failed: %v", name, err)
		}
		// Verify key fields survive round-trip
		if reparse.Method() != msg.Method() {
			t.Errorf("%s method mismatch after round-trip", name)
		}
		if !bytes.Equal(reparse.FirstLine.Req.URI.Bytes(), msg.FirstLine.Req.URI.Bytes()) {
			t.Errorf("%s RURI mismatch after round-trip", name)
		}
	}
}

// ---------------------------------------------------------------------------
// Test: 401 Challenge Response Verification
// ---------------------------------------------------------------------------

func TestIMS_AKAChallengeResponse(t *testing.T) {
	realm := "ims.example.com"
	impu := "sip:alice@ims.example.com"
	contact := "sip:alice@192.168.1.100"

	registrar := scscf.NewRegistrar(realm)

	// Initial REGISTER
	raw1 := buildREGISTER(impu, contact, "")
	msg1, _ := parser.ParseMsg(raw1)
	res1, _ := registrar.HandleRegister(msg1)
	if res1.StatusCode != 401 {
		t.Fatalf("expected 401, got %d", res1.StatusCode)
	}

	// Parse challenge
	challengeStr := res1.Headers["WWW-Authenticate"].String()
	var nonce, opaque string
	for _, p := range strings.Split(challengeStr, ",") {
		p = strings.TrimSpace(p)
		if strings.HasPrefix(p, "nonce=") {
			nonce = strings.Trim(strings.TrimPrefix(p, "nonce="), "\"")
		}
		if strings.HasPrefix(p, "opaque=") {
			opaque = strings.Trim(strings.TrimPrefix(p, "opaque="), "\"")
		}
	}

	// Get auth vector and build correct response
	record := registrar.GetRecord(impu)
	av := record.AuthState.AuthVector
	correctResp := fmt.Sprintf("%x", av.XRES)

	// Correct response → 200 OK
	authzCorrect := fmt.Sprintf(
		`Digest username="alice", realm="%s", nonce="%s", uri="%s", response="%s", algorithm=AKAv1-MD5, opaque="%s"`,
		realm, nonce, impu, correctResp, opaque,
	)
	raw2 := buildREGISTER(impu, contact, authzCorrect)
	msg2, _ := parser.ParseMsg(raw2)
	res2, _ := registrar.HandleRegister(msg2)
	if res2.StatusCode != 200 {
		t.Fatalf("correct response should yield 200, got %d", res2.StatusCode)
	}

	// Wrong response → new 401 (or 403 after 3 attempts)
	// Need a fresh registrar since the previous one now has Alice registered
	registrar2 := scscf.NewRegistrar(realm)
	raw3 := buildREGISTER(impu, contact, "")
	msg3, _ := parser.ParseMsg(raw3)
	res3, _ := registrar2.HandleRegister(msg3)

	challengeStr2 := res3.Headers["WWW-Authenticate"].String()
	var nonce2, opaque2 string
	for _, p := range strings.Split(challengeStr2, ",") {
		p = strings.TrimSpace(p)
		if strings.HasPrefix(p, "nonce=") {
			nonce2 = strings.Trim(strings.TrimPrefix(p, "nonce="), "\"")
		}
		if strings.HasPrefix(p, "opaque=") {
			opaque2 = strings.Trim(strings.TrimPrefix(p, "opaque="), "\"")
		}
	}

	wrongResp := "0000000000000000"
	authzWrong := fmt.Sprintf(
		`Digest username="alice", realm="%s", nonce="%s", uri="%s", response="%s", algorithm=AKAv1-MD5, opaque="%s"`,
		realm, nonce2, impu, wrongResp, opaque2,
	)
	raw4 := buildREGISTER(impu, contact, authzWrong)
	msg4, _ := parser.ParseMsg(raw4)
	res4, _ := registrar2.HandleRegister(msg4)
	// First wrong attempt → new 401
	if res4.StatusCode != 401 {
		t.Fatalf("first wrong attempt should yield 401, got %d", res4.StatusCode)
	}
}

// ---------------------------------------------------------------------------
// Test: Unregistered caller rejected
// ---------------------------------------------------------------------------

func TestIMS_UnregisteredCallerRejected(t *testing.T) {
	realm := "ims.example.com"
	impuAlice := "sip:alice@ims.example.com"
	impuBob := "sip:bob@ims.example.com"

	registrar := scscf.NewRegistrar(realm)
	sessionH := scscf.NewSessionHandler(registrar)

	// Neither Alice nor Bob registered
	rawInvite := buildINVITE(impuAlice, impuBob, "tag001", "call-001", 1)
	msgInvite, _ := parser.ParseMsg(rawInvite)
	res, _ := sessionH.HandleInvite(msgInvite)
	if res.StatusCode != 403 {
		t.Fatalf("expected 403 for unregistered caller, got %d", res.StatusCode)
	}
}

// ---------------------------------------------------------------------------
// Test: P-Asserted-Identity header injection
// ---------------------------------------------------------------------------

func TestIMS_PAssertedIdentity(t *testing.T) {
	realm := "ims.example.com"
	impuAlice := "sip:alice@ims.example.com"
	contactAlice := "sip:alice@192.168.1.100"
	impuBob := "sip:bob@ims.example.com"
	contactBob := "sip:bob@192.168.1.200"

	registrar := scscf.NewRegistrar(realm)
	sessionH := scscf.NewSessionHandler(registrar)

	// Register Alice and Bob (abbreviated)
	registerUser(t, registrar, impuAlice, contactAlice)
	registerUser(t, registrar, impuBob, contactBob)

	// Alice calls Bob
	rawInvite := buildINVITE(impuAlice, impuBob, "tag001", "call-pai-001", 1)
	msgInvite, _ := parser.ParseMsg(rawInvite)
	res, _ := sessionH.HandleInvite(msgInvite)
	if res.StatusCode != 100 {
		t.Fatalf("expected 100, got %d", res.StatusCode)
	}

	// Check P-Asserted-Identity was added
	pai := res.Headers["P-Asserted-Identity"]
	if pai.Len == 0 {
		t.Fatal("missing P-Asserted-Identity header")
	}
	if !strings.Contains(pai.String(), impuAlice) {
		t.Fatalf("PAI should contain %s, got %s", impuAlice, pai.String())
	}
}

// registerUser is a helper that performs the full AKA registration flow
func registerUser(t *testing.T, registrar *scscf.Registrar, impu, contact string) {
	t.Helper()
	realm := registrar.GetRealm()

	// Initial REGISTER
	raw1 := buildREGISTER(impu, contact, "")
	msg1, _ := parser.ParseMsg(raw1)
	res1, _ := registrar.HandleRegister(msg1)
	if res1.StatusCode != 401 {
		t.Fatalf("expected 401, got %d", res1.StatusCode)
	}

	challengeStr := res1.Headers["WWW-Authenticate"].String()
	var nonce, opaque string
	for _, p := range strings.Split(challengeStr, ",") {
		p = strings.TrimSpace(p)
		if strings.HasPrefix(p, "nonce=") {
			nonce = strings.Trim(strings.TrimPrefix(p, "nonce="), "\"")
		}
		if strings.HasPrefix(p, "opaque=") {
			opaque = strings.Trim(strings.TrimPrefix(p, "opaque="), "\"")
		}
	}

	record := registrar.GetRecord(impu)
	resp := fmt.Sprintf("%x", record.AuthState.AuthVector.XRES)
	user := strings.TrimPrefix(strings.Split(impu, "@")[0], "sip:")
	authz := fmt.Sprintf(
		`Digest username="%s", realm="%s", nonce="%s", uri="%s", response="%s", algorithm=AKAv1-MD5, opaque="%s"`,
		user, realm, nonce, impu, resp, opaque,
	)

	raw2 := buildREGISTER(impu, contact, authz)
	msg2, _ := parser.ParseMsg(raw2)
	res2, _ := registrar.HandleRegister(msg2)
	if res2.StatusCode != 200 {
		t.Fatalf("expected 200, got %d", res2.StatusCode)
	}
}

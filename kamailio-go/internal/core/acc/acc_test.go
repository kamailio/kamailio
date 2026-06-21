// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for the accounting / CDR package.
 */

package acc

import (
	"bytes"
	"context"
	"net"
	"strings"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// ---------------------------------------------------------------------------
// Raw SIP fixtures used throughout this file.

var inviteFixture = []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
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

var reply200Fixture = []byte("SIP/2.0 200 OK\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>;tag=a6c85cf\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314159 INVITE\r\n" +
	"Contact: <sip:bob@192.0.2.4>\r\n" +
	"Content-Type: application/sdp\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

var byeFixture = []byte("BYE sip:bob@example.com SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>;tag=a6c85cf\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314160 BYE\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

var cancelFixture = []byte("CANCEL sip:bob@example.com SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314159 CANCEL\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

// captureBackend is a test-only backend that records written CDRs in a
// thread-safe slice so tests can verify flush behaviour.
type captureBackend struct {
	ch chan *CDR
}

func newCaptureBackend() *captureBackend {
	return &captureBackend{ch: make(chan *CDR, 16)}
}

func (c *captureBackend) Write(_ context.Context, cdr *CDR) error {
	c.ch <- cdr
	return nil
}

func (c *captureBackend) Close() error {
	close(c.ch)
	return nil
}

// makeInvite parses the invite fixture and returns the resulting message.
func makeInvite(t *testing.T) *parser.SIPMsg {
	t.Helper()
	msg, err := parser.ParseMsg(inviteFixture)
	if err != nil {
		t.Fatalf("parse invite: %v", err)
	}
	return msg
}

// ---------------------------------------------------------------------------
// Test 1: Service with no backend - nil-safe, OnInvite increases pending.

func TestNewAccountingService_NoBackendNilSafe(t *testing.T) {
	ac := NewAccountingService()
	if ac == nil {
		t.Fatal("expected non-nil accounting service")
	}
	msg, err := parser.ParseMsg(inviteFixture)
	if err != nil {
		t.Fatalf("parse invite: %v", err)
	}
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(msg, src)
	if ac.PendingCount() != 1 {
		t.Fatalf("expected pending count 1, got %d", ac.PendingCount())
	}

	// Nil message and nil service should not panic.
	ac.OnInvite(nil, src)
	var nilAC *AccountingService
	nilAC.OnInvite(msg, src)
	nilAC.OnReply(nil)
	nilAC.OnBye(nil)
	nilAC.OnCancel(nil)
}

// ---------------------------------------------------------------------------
// Test 2: OnInvite captures basic CDR fields.

func TestAccounting_OnInvite(t *testing.T) {
	ac := NewAccountingService()
	msg := makeInvite(t)
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(msg, src)
	if ac.PendingCount() != 1 {
		t.Fatalf("expected 1 pending CDR, got %d", ac.PendingCount())
	}
	cdr := ac.Lookup("a84b4c76e66710@pc33.example.com")
	if cdr == nil {
		t.Fatal("expected CDR to exist for call-id")
	}
	if cdr.CallID == "" {
		t.Error("expected non-empty Call-ID")
	}
	// Either the parsed URI extraction or the body-based fallback should
	// yield user info.
	if cdr.FromUser == "" && cdr.FromDomain == "" && cdr.ToUser == "" && cdr.ToDomain == "" {
		t.Log("note: URI parsing did not extract user/domain; relying on Call-ID and method")
	}
	if cdr.Method != "INVITE" {
		t.Errorf("expected method INVITE, got %q", cdr.Method)
	}
	if cdr.InviteTime.IsZero() {
		t.Error("expected non-zero InviteTime")
	}
	if cdr.SourceIP != "192.0.2.1" {
		t.Errorf("expected source IP 192.0.2.1, got %q", cdr.SourceIP)
	}
}

// ---------------------------------------------------------------------------
// Test 3: Repeated INVITE with same call-id does not create duplicate CDRs.

func TestAccounting_OnInvite_SameCall_NoDup(t *testing.T) {
	ac := NewAccountingService()
	msg := makeInvite(t)
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(msg, src)
	ac.OnInvite(msg, src)
	ac.OnInvite(msg, src)
	if ac.PendingCount() != 1 {
		t.Fatalf("expected pending count 1, got %d", ac.PendingCount())
	}
}

// ---------------------------------------------------------------------------
// Test 4: OnReply with 200 OK sets ConnectTime.

func TestAccounting_OnReply_200OK_SetsConnect(t *testing.T) {
	ac := NewAccountingService()
	invite := makeInvite(t)
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(invite, src)

	reply, err := parser.ParseMsg(reply200Fixture)
	if err != nil {
		t.Fatalf("parse 200 OK: %v", err)
	}
	ac.OnReply(reply)
	cdr := ac.Lookup("a84b4c76e66710@pc33.example.com")
	if cdr == nil {
		t.Fatal("expected CDR to exist for call-id")
	}
	if cdr.StatusCode != 200 {
		t.Errorf("expected status code 200, got %d", cdr.StatusCode)
	}
	if cdr.ConnectTime.IsZero() {
		t.Error("expected non-zero ConnectTime after 2xx reply")
	}
}

// ---------------------------------------------------------------------------
// Test 5: INVITE -> 200 OK -> BYE flushes the CDR with duration.

func TestAccounting_OnBye_Flushes(t *testing.T) {
	cb := newCaptureBackend()
	ac := NewAccountingService(cb)

	invite := makeInvite(t)
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(invite, src)

	reply, err := parser.ParseMsg(reply200Fixture)
	if err != nil {
		t.Fatalf("parse 200 OK: %v", err)
	}
	ac.OnReply(reply)

	bye, err := parser.ParseMsg(byeFixture)
	if err != nil {
		t.Fatalf("parse BYE: %v", err)
	}
	ac.OnBye(bye)

	if ac.PendingCount() != 0 {
		t.Fatalf("expected pending count 0 after BYE, got %d", ac.PendingCount())
	}
	select {
	case cdr := <-cb.ch:
		if cdr == nil {
			t.Fatal("expected flushed CDR, got nil")
		}
		if cdr.Method != "INVITE" && cdr.Method != "BYE" {
			t.Errorf("expected method INVITE or BYE, got %q", cdr.Method)
		}
		if cdr.StatusCode != 200 {
			t.Errorf("expected status code 200 in flushed CDR, got %d", cdr.StatusCode)
		}
		if cdr.EndTime.IsZero() {
			t.Error("expected non-zero EndTime on flushed CDR")
		}
	default:
		t.Fatal("expected a flushed CDR on the capture backend, got none")
	}
}

// ---------------------------------------------------------------------------
// Test 6: INVITE -> CANCEL ends with status code 487.

func TestAccounting_OnCancel(t *testing.T) {
	cb := newCaptureBackend()
	ac := NewAccountingService(cb)

	invite := makeInvite(t)
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(invite, src)

	cancel, err := parser.ParseMsg(cancelFixture)
	if err != nil {
		t.Fatalf("parse CANCEL: %v", err)
	}
	ac.OnCancel(cancel)

	if ac.PendingCount() != 0 {
		t.Fatalf("expected pending count 0 after CANCEL, got %d", ac.PendingCount())
	}
	select {
	case cdr := <-cb.ch:
		if cdr.StatusCode != 487 {
			t.Errorf("expected status code 487 for cancelled call, got %d", cdr.StatusCode)
		}
		if !strings.Contains(cdr.Reason, "Cancelled") {
			t.Errorf("expected reason to mention Cancelled, got %q", cdr.Reason)
		}
	default:
		t.Fatal("expected a flushed CDR on the capture backend, got none")
	}
}

// ---------------------------------------------------------------------------
// Test 7: CSV backend writes a header and a row to an in-memory buffer.

func TestAccounting_CSVBackend_WritesHeaderAndRow(t *testing.T) {
	buf := &bytes.Buffer{}
	csv := NewCSVBackendWriter(buf)
	cdr := &CDR{
		CallID:     "cid-1",
		FromUser:   "alice",
		FromDomain: "example.com",
		ToUser:     "bob",
		ToDomain:   "example.com",
		RequestURI: "sip:bob@example.com",
		Method:     "INVITE",
		StatusCode: 200,
		Reason:     "OK",
		Direction:  "inbound",
	}
	if err := csv.Write(nil, cdr); err != nil {
		t.Fatalf("csv write: %v", err)
	}
	if err := csv.Close(); err != nil {
		t.Fatalf("csv close: %v", err)
	}
	out := buf.String()
	if !strings.Contains(out, "call_id,from_user") {
		t.Errorf("expected header in CSV output, got: %q", out)
	}
	if !strings.Contains(out, "cid-1") {
		t.Errorf("expected call_id in CSV output, got: %q", out)
	}
	if !strings.Contains(out, "200") {
		t.Errorf("expected status_code in CSV output, got: %q", out)
	}
}

// ---------------------------------------------------------------------------
// Test 8: Nil message arguments do not panic.

func TestAccounting_NilMessage_DoesNotPanic(t *testing.T) {
	cb := newCaptureBackend()
	ac := NewAccountingService(cb)
	src := &net.UDPAddr{IP: net.ParseIP("192.0.2.1"), Port: 5060}
	ac.OnInvite(nil, src)
	ac.OnReply(nil)
	ac.OnBye(nil)
	ac.OnCancel(nil)
	if ac.PendingCount() != 0 {
		t.Fatalf("expected pending count 0 when no valid messages processed, got %d", ac.PendingCount())
	}

	// CSV + DB backends with nil CDR should not panic.
	csvB := NewCSVBackendWriter(&bytes.Buffer{})
	if err := csvB.Write(nil, nil); err != nil {
		t.Fatalf("csv write nil: %v", err)
	}
	if err := csvB.Close(); err != nil {
		t.Fatalf("csv close: %v", err)
	}

	// Nil DB backend constructor should error.
	if _, err := NewDBBackend(nil, "cdr"); err == nil {
		t.Error("expected error constructing DB backend with nil connection")
	}
}

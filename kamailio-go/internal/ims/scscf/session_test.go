// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * S-CSCF Session tests
 */

package scscf

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// Test INVITE request
var testInvite = []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
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

// Test BYE request
var testBye = []byte("BYE sip:bob@example.com SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"Max-Forwards: 70\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>;tag=a6c85cf\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314160 BYE\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

func TestNewSessionHandler(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)
	if sh == nil {
		t.Fatal("expected session handler")
	}
	if sh.GetSessionCount() != 0 {
		t.Error("expected no sessions")
	}
}

func TestHandleInvite(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)

	msg, err := parser.ParseMsg(testInvite)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	// Pre-register the caller so INVITE is accepted
	record := &RegistrationRecord{
		IMPU:      "sip:alice@example.com",
		State:     RegStateRegistered,
		ContactURI: "<sip:alice@pc33.example.com>",
	}
	registrar.setRecord("sip:alice@example.com", record)

	result, err := sh.HandleInvite(msg)
	if err != nil {
		t.Fatalf("handle invite error: %v", err)
	}

	if result == nil {
		t.Fatal("expected result")
	}

	if result.StatusCode != 100 {
		t.Errorf("expected 100, got %d", result.StatusCode)
	}

	// Session should be created
	if sh.GetSessionCount() != 1 {
		t.Errorf("expected 1 session, got %d", sh.GetSessionCount())
	}
}

func TestHandleInviteNotRegistered(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)

	msg, _ := parser.ParseMsg(testInvite)

	// Caller (alice@example.com) is not registered
	result, err := sh.HandleInvite(msg)
	if err != nil {
		t.Fatalf("handle invite error: %v", err)
	}

	// Should be rejected with 403
	if result.StatusCode != 403 {
		t.Errorf("expected 403, got %d", result.StatusCode)
	}
}

func TestHandleInviteNull(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)

	_, err := sh.HandleInvite(nil)
	if err == nil {
		t.Error("expected error for null message")
	}
}

func TestHandleBye(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)

	// Pre-register caller
	record := &RegistrationRecord{
		IMPU:      "sip:alice@example.com",
		State:     RegStateRegistered,
		ContactURI: "<sip:alice@pc33.example.com>",
	}
	registrar.setRecord("sip:alice@example.com", record)

	// First create a session with INVITE
	inviteMsg, _ := parser.ParseMsg(testInvite)
	sh.HandleInvite(inviteMsg)

	if sh.GetSessionCount() != 1 {
		t.Fatal("expected 1 session")
	}

	// Now handle BYE
	byeMsg, _ := parser.ParseMsg(testBye)
	result, err := sh.HandleBye(byeMsg)
	if err != nil {
		t.Fatalf("handle bye error: %v", err)
	}

	if result.StatusCode != 200 {
		t.Errorf("expected 200, got %d", result.StatusCode)
	}

	// Session should be removed
	if sh.GetSessionCount() != 0 {
		t.Errorf("expected 0 sessions, got %d", sh.GetSessionCount())
	}
}

func TestHandleByeNoSession(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)

	byeMsg, _ := parser.ParseMsg(testBye)
	result, err := sh.HandleBye(byeMsg)
	if err != nil {
		t.Fatalf("handle bye error: %v", err)
	}

	if result.StatusCode != 481 {
		t.Errorf("expected 481, got %d", result.StatusCode)
	}
}

func TestExtractURI(t *testing.T) {
	sh := &SessionHandler{}

	tests := []struct {
		input    string
		expected string
	}{
		{"<sip:alice@example.com>", "sip:alice@example.com"},
		{"Alice <sip:alice@example.com>;tag=123", "sip:alice@example.com"},
		{"sip:alice@example.com;tag=123", "sip:alice@example.com"},
		{"tel:+1234567890", "tel:+1234567890"},
		{"", ""},
	}

	for _, tt := range tests {
		result := sh.extractURI(tt.input)
		if result != tt.expected {
			t.Errorf("extractURI(%q) = %q, expected %q", tt.input, result, tt.expected)
		}
	}
}

func TestExtractTag(t *testing.T) {
	sh := &SessionHandler{}

	tests := []struct {
		input    string
		expected string
	}{
		{"<sip:alice@example.com>;tag=1928301774", "1928301774"},
		{"<sip:alice@example.com>;tag=abc;other=xyz", "abc"},
		{"<sip:alice@example.com>", ""},
	}

	for _, tt := range tests {
		result := sh.extractTag(tt.input)
		if result != tt.expected {
			t.Errorf("extractTag(%q) = %q, expected %q", tt.input, result, tt.expected)
		}
	}
}

func TestGetSession(t *testing.T) {
	registrar := NewRegistrar("ims.mnc001.mcc460.gprs")
	sh := NewSessionHandler(registrar)

	// No session
	if sh.GetSession("nonexistent") != nil {
		t.Error("expected nil for nonexistent session")
	}
}

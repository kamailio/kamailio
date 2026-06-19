// SPDX-License-Identifier: GPL-2.0-or-later

package pcscf

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

func TestNewSessionHandler(t *testing.T) {
	sh := NewSessionHandler()
	if sh == nil {
		t.Fatal("NewSessionHandler returned nil")
	}
	if sh.sessions == nil {
		t.Error("sessions map not initialized")
	}
}

func TestSessionHandler_GetSessionCount_Empty(t *testing.T) {
	sh := NewSessionHandler()
	if sh.GetSessionCount() != 0 {
		t.Errorf("expected 0 sessions, got %d", sh.GetSessionCount())
	}
}

func TestSessionHandler_GetSession_NotFound(t *testing.T) {
	sh := NewSessionHandler()
	session := sh.GetSession("nonexistent-callid")
	if session != nil {
		t.Error("expected nil for nonexistent session")
	}
}

func TestExtractTag(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		{"From: <sip:user@host>;tag=abc123", "abc123"},
		{"From: <sip:user@host>;tag=xyz;other=param", "xyz"},
		{"From: <sip:user@host>;other=param;tag=def", "def"},
		{"From: <sip:user@host>", ""},                    // no tag
		{"From: <sip:user@host>;tag=", ""},              // empty tag
	}

	for _, tc := range tests {
		result := extractTag(tc.input)
		if result != tc.expected {
			t.Errorf("extractTag(%q): expected %q, got %q", tc.input, tc.expected, result)
		}
	}
}

func TestExtractContactURI(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		{"<sip:user@192.168.1.1:5060>", "sip:user@192.168.1.1:5060"},
		{"<sip:user@192.168.1.1:5060>;expires=3600", "sip:user@192.168.1.1:5060"},
		{"sip:user@192.168.1.1:5060", "sip:user@192.168.1.1:5060"},
		{"sip:user@192.168.1.1:5060;lr", "sip:user@192.168.1.1:5060"},
		{"", ""}, // empty
	}

	for _, tc := range tests {
		msg := &parser.SIPMsg{}
		if tc.input != "" {
			msg.Contact = &parser.HdrField{Body: str.Mk(tc.input)}
		}
		result := extractContactURI(msg)
		if result != tc.expected {
			t.Errorf("extractContactURI(%q): expected %q, got %q", tc.input, tc.expected, result)
		}
	}
}

func TestExtractContactURI_NoContact(t *testing.T) {
	msg := &parser.SIPMsg{}
	result := extractContactURI(msg)
	if result != "" {
		t.Errorf("expected empty string for nil contact, got %q", result)
	}
}

func TestPCSCFSessionResult_Fields(t *testing.T) {
	result := &PCSCFSessionResult{
		StatusCode:   200,
		StatusReason: "OK",
		Headers:      make(map[string]str.Str),
		Body:         []byte("test body"),
	}

	if result.StatusCode != 200 {
		t.Errorf("expected status code 200, got %d", result.StatusCode)
	}
	if result.StatusReason != "OK" {
		t.Errorf("expected status reason 'OK', got %s", result.StatusReason)
	}
	if len(result.Body) != 9 {
		t.Errorf("expected body length 9, got %d", len(result.Body))
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Header parser tests
 */

package parser

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

func TestParseVia(t *testing.T) {
	tests := []struct {
		input    string
		wantHost string
		wantPort uint16
		wantProto int16
		wantBranch string
	}{
		{
			input:    "SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK123",
			wantHost: "10.0.0.1",
			wantPort: 5060,
			wantProto: ProtoUDP,
			wantBranch: "z9hG4bK123",
		},
		{
			input:    "SIP/2.0/TCP 192.168.1.1;branch=z9hG4bKabc",
			wantHost: "192.168.1.1",
			wantPort: 0,
			wantProto: ProtoTCP,
			wantBranch: "z9hG4bKabc",
		},
		{
			input:    "SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bKipv6",
			wantHost: "[2001:db8::1]",
			wantPort: 5060,
			wantProto: ProtoUDP,
			wantBranch: "z9hG4bKipv6",
		},
	}

	for _, tt := range tests {
		vb, err := ParseVia(str.Mk(tt.input))
		if err != nil {
			t.Errorf("ParseVia(%q) error: %v", tt.input, err)
			continue
		}

		if vb.Host.String() != tt.wantHost {
			t.Errorf("ParseVia(%q) host = %q, want %q", tt.input, vb.Host.String(), tt.wantHost)
		}

		if vb.Port != tt.wantPort {
			t.Errorf("ParseVia(%q) port = %d, want %d", tt.input, vb.Port, tt.wantPort)
		}

		if vb.Proto != tt.wantProto {
			t.Errorf("ParseVia(%q) proto = %d, want %d", tt.input, vb.Proto, tt.wantProto)
		}

		if vb.Branch == nil || vb.Branch.Value.String() != tt.wantBranch {
			if vb.Branch == nil {
				t.Errorf("ParseVia(%q) branch = nil, want %q", tt.input, tt.wantBranch)
			} else {
				t.Errorf("ParseVia(%q) branch = %q, want %q", tt.input, vb.Branch.Value.String(), tt.wantBranch)
			}
		}
	}
}

func TestParseViaParams(t *testing.T) {
	input := "SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK123;received=192.168.1.1;rport"

	vb, err := ParseVia(str.Mk(input))
	if err != nil {
		t.Fatalf("ParseVia error: %v", err)
	}

	// Check branch
	if vb.Branch == nil || vb.Branch.Value.String() != "z9hG4bK123" {
		t.Error("Expected branch parameter")
	}

	// Check received
	if vb.Received == nil || vb.Received.Value.String() != "192.168.1.1" {
		t.Error("Expected received parameter")
	}

	// Check rport
	if vb.RPort == nil {
		t.Error("Expected rport parameter")
	}
}

func TestParseToBody(t *testing.T) {
	tests := []struct {
		input       string
		wantName    string
		wantURI     string
		wantTag     string
	}{
		{
			input:    "Alice <sip:alice@example.com>;tag=12345",
			wantName: "Alice",
			wantURI:  "sip:alice@example.com",
			wantTag:  "12345",
		},
		{
			input:    "<sip:bob@example.org>;tag=xyz",
			wantName: "",
			wantURI:  "sip:bob@example.org",
			wantTag:  "xyz",
		},
		{
			input:    "sip:carol@example.net;tag=abc",
			wantName: "",
			wantURI:  "sip:carol@example.net",
			wantTag:  "abc",
		},
	}

	for _, tt := range tests {
		tb, err := ParseToBody(str.Mk(tt.input))
		if err != nil {
			t.Errorf("ParseToBody(%q) error: %v", tt.input, err)
			continue
		}

		if tt.wantName != "" && tb.GetName() != tt.wantName {
			t.Errorf("ParseToBody(%q) name = %q, want %q", tt.input, tb.GetName(), tt.wantName)
		}

		if tb.GetURI() != tt.wantURI {
			t.Errorf("ParseToBody(%q) URI = %q, want %q", tt.input, tb.GetURI(), tt.wantURI)
		}

		if tb.GetTag() != tt.wantTag {
			t.Errorf("ParseToBody(%q) tag = %q, want %q", tt.input, tb.GetTag(), tt.wantTag)
		}
	}
}

func TestParseFromBody(t *testing.T) {
	input := "Bob <sip:bob@example.com>;tag=abc123"

	tb, err := ParseFromBody(str.Mk(input))
	if err != nil {
		t.Fatalf("ParseFromBody error: %v", err)
	}

	if tb.GetName() != "Bob" {
		t.Errorf("Expected name 'Bob', got %q", tb.GetName())
	}

	if tb.GetURI() != "sip:bob@example.com" {
		t.Errorf("Expected URI 'sip:bob@example.com', got %q", tb.GetURI())
	}

	if !tb.HasTag() {
		t.Error("Expected tag to be present")
	}

	if tb.GetTag() != "abc123" {
		t.Errorf("Expected tag 'abc123', got %q", tb.GetTag())
	}
}

func TestParseContact(t *testing.T) {
	tests := []struct {
		input       string
		wantName    string
		wantURI     string
		wantExpires int
		wantQValue  float32
	}{
		{
			input:    "Alice <sip:alice@example.com>;expires=3600",
			wantName: "Alice",
			wantURI:  "sip:alice@example.com",
			wantExpires: 3600,
		},
		{
			input:    "<sip:bob@example.org>;q=0.8;expires=1800",
			wantName: "",
			wantURI:  "sip:bob@example.org",
			wantExpires: 1800,
			wantQValue: 0.8,
		},
		{
			input:    "sip:carol@example.net",
			wantName: "",
			wantURI:  "sip:carol@example.net",
		},
	}

	for _, tt := range tests {
		cb, err := ParseContact(str.Mk(tt.input))
		if err != nil {
			t.Errorf("ParseContact(%q) error: %v", tt.input, err)
			continue
		}

		if tt.wantName != "" && cb.GetName() != tt.wantName {
			t.Errorf("ParseContact(%q) name = %q, want %q", tt.input, cb.GetName(), tt.wantName)
		}

		if cb.GetURI() != tt.wantURI {
			t.Errorf("ParseContact(%q) URI = %q, want %q", tt.input, cb.GetURI(), tt.wantURI)
		}

		if cb.Expires != tt.wantExpires {
			t.Errorf("ParseContact(%q) expires = %d, want %d", tt.input, cb.Expires, tt.wantExpires)
		}

		if tt.wantQValue != 0 && cb.QValue != tt.wantQValue {
			t.Errorf("ParseContact(%q) qvalue = %f, want %f", tt.input, cb.QValue, tt.wantQValue)
		}
	}
}

func TestParseContactList(t *testing.T) {
	input := "Alice <sip:alice@example.com>, Bob <sip:bob@example.org>"

	contacts, err := ParseContactList(str.Mk(input))
	if err != nil {
		t.Fatalf("ParseContactList error: %v", err)
	}

	if len(contacts) != 2 {
		t.Fatalf("Expected 2 contacts, got %d", len(contacts))
	}

	if contacts[0].GetName() != "Alice" {
		t.Errorf("Expected first contact name 'Alice', got %q", contacts[0].GetName())
	}

	if contacts[1].GetName() != "Bob" {
		t.Errorf("Expected second contact name 'Bob', got %q", contacts[1].GetName())
	}
}

func TestParseContactStar(t *testing.T) {
	cb, err := ParseContact(str.Mk("*"))
	if err != nil {
		t.Fatalf("ParseContact error: %v", err)
	}

	// STAR contact should have empty URI
	if cb.URI != nil {
		t.Error("Expected nil URI for STAR contact")
	}
}

func TestToBodyHasTag(t *testing.T) {
	// With tag
	tb1, _ := ParseToBody(str.Mk("<sip:alice@example.com>;tag=123"))
	if !tb1.HasTag() {
		t.Error("Expected HasTag() to return true")
	}

	// Without tag
	tb2, _ := ParseToBody(str.Mk("<sip:bob@example.com>"))
	if tb2.HasTag() {
		t.Error("Expected HasTag() to return false")
	}
}

func TestContactBodyHasExpires(t *testing.T) {
	// With expires
	cb1, _ := ParseContact(str.Mk("<sip:alice@example.com>;expires=3600"))
	if !cb1.HasExpires() {
		t.Error("Expected HasExpires() to return true")
	}

	// Without expires
	cb2, _ := ParseContact(str.Mk("<sip:bob@example.com>"))
	if cb2.HasExpires() {
		t.Error("Expected HasExpires() to return false")
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later
package pcscf

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/usrloc"
)

func inviteToUser(user string) *parser.SIPMsg {
	raw := "INVITE sip:" + user + "@ims.local SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-route\r\n" +
		"From: <sip:alice@ims.local>;tag=ft1\r\n" +
		"To: <sip:" + user + "@ims.local>\r\n" +
		"Call-ID: route-" + user + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		return nil
	}
	return msg
}

// Test 1: simple registration and route lookup.
func TestRouteFromRegistration_Basic(t *testing.T) {
	reg := usrloc.NewRegistrar()
	_, err := reg.Register("ims.local", "bob", []*usrloc.Contact{
		{URI: "sip:bob@10.0.0.2:5060"},
	})
	if err != nil {
		t.Fatalf("Register: %v", err)
	}
	msg := inviteToUser("bob")
	if msg == nil {
		t.Fatal("failed to parse invite")
	}
	contacts, err := RouteFromRegistration(reg, msg)
	if err != nil {
		t.Fatalf("RouteFromRegistration: %v", err)
	}
	if len(contacts) != 1 {
		t.Fatalf("expected 1 contact, got %d", len(contacts))
	}
	if contacts[0] != "sip:bob@10.0.0.2:5060" {
		t.Errorf("unexpected contact URI: %q", contacts[0])
	}
}

// Test 2: empty / missing registration returns an error.
func TestRouteFromRegistration_EmptyUser(t *testing.T) {
	reg := usrloc.NewRegistrar()
	msg := inviteToUser("bob")
	if _, err := RouteFromRegistration(reg, msg); err == nil {
		t.Error("expected error when user is not registered")
	}
	if _, err := RouteFromRegistration(nil, msg); err == nil {
		t.Error("expected error when registrar is nil")
	}
}

// Test 3: nil message returns error.
func TestRouteFromRegistration_NilMsg(t *testing.T) {
	reg := usrloc.NewRegistrar()
	if _, err := RouteFromRegistration(reg, nil); err == nil {
		t.Error("expected error when message is nil")
	}
}

// Test 4: multiple contacts returned for parallel forking.
func TestRouteFromRegistration_MultipleContacts(t *testing.T) {
	reg := usrloc.NewRegistrar()
	_, err := reg.Register("ims.local", "bob", []*usrloc.Contact{
		{URI: "sip:bob@10.0.0.2:5060", Q: 1.0},
		{URI: "sip:bob-mobile@10.0.0.3:5060", Q: 0.5},
	})
	if err != nil {
		t.Fatalf("Register: %v", err)
	}

	msg := inviteToUser("bob")
	contacts, err := RouteFromRegistration(reg, msg)
	if err != nil {
		t.Fatalf("RouteFromRegistration: %v", err)
	}
	if len(contacts) != 2 {
		t.Fatalf("expected 2 contacts, got %d", len(contacts))
	}
	// Top contact (higher q) should be first.
	if contacts[0] != "sip:bob@10.0.0.2:5060" {
		t.Errorf("expected primary contact first, got %q", contacts[0])
	}
}

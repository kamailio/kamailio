// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * URI parser tests
 */

package parser

import (
	"testing"
)

func TestParseURISIP(t *testing.T) {
	uri, err := ParseURI("sip:alice@example.com:5060;transport=udp")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.Type != SIPURIT {
		t.Errorf("expected SIP URI, got %d", uri.Type)
	}
	if uri.User.String() != "alice" {
		t.Errorf("expected user 'alice', got '%s'", uri.User.String())
	}
	if uri.Host.String() != "example.com" {
		t.Errorf("expected host 'example.com', got '%s'", uri.Host.String())
	}
	if uri.PortNo != 5060 {
		t.Errorf("expected port 5060, got %d", uri.PortNo)
	}
	if uri.Transport.String() != "transport" {
		t.Errorf("expected transport param, got '%s'", uri.Transport.String())
	}
	if uri.TransportVal.String() != "udp" {
		t.Errorf("expected transport value 'udp', got '%s'", uri.TransportVal.String())
	}
}

func TestParseURISIPS(t *testing.T) {
	uri, err := ParseURI("sips:alice@example.com:5061")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.Type != SIPSURIT {
		t.Errorf("expected SIPS URI, got %d", uri.Type)
	}
	if !uri.IsSecure() {
		t.Error("expected secure URI")
	}
}

func TestParseURITEL(t *testing.T) {
	uri, err := ParseURI("tel:+1234567890")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.Type != TELURIT {
		t.Errorf("expected TEL URI, got %d", uri.Type)
	}
}

func TestParseURIIPv6(t *testing.T) {
	uri, err := ParseURI("sip:alice@[2001:db8::1]:5060")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.Host.String() != "2001:db8::1" {
		t.Errorf("expected IPv6 host, got '%s'", uri.Host.String())
	}
	if uri.PortNo != 5060 {
		t.Errorf("expected port 5060, got %d", uri.PortNo)
	}
}

func TestParseURIWithPassword(t *testing.T) {
	uri, err := ParseURI("sip:alice:secret@example.com")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.User.String() != "alice" {
		t.Errorf("expected user 'alice', got '%s'", uri.User.String())
	}
	if uri.Passwd.String() != "secret" {
		t.Errorf("expected password 'secret', got '%s'", uri.Passwd.String())
	}
}

func TestParseURIWithParams(t *testing.T) {
	uri, err := ParseURI("sip:alice@example.com;transport=tcp;lr")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.TransportVal.String() != "tcp" {
		t.Errorf("expected transport tcp, got '%s'", uri.TransportVal.String())
	}
	if uri.LR.String() != "lr" {
		t.Errorf("expected lr param, got '%s'", uri.LR.String())
	}
}

func TestParseURIWithHeaders(t *testing.T) {
	uri, err := ParseURI("sip:alice@example.com?Subject=Hello")
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if uri.Headers.String() != "Subject=Hello" {
		t.Errorf("expected headers, got '%s'", uri.Headers.String())
	}
}

func TestParseURIInvalid(t *testing.T) {
	_, err := ParseURI("invalid")
	if err == nil {
		t.Error("expected error for invalid URI")
	}

	_, err = ParseURI("")
	if err == nil {
		t.Error("expected error for empty URI")
	}
}

func TestURIToString(t *testing.T) {
	uri, _ := ParseURI("sip:alice@example.com:5060;transport=udp")
	str := uri.String()
	if str == "" {
		t.Error("expected non-empty string")
	}
}

func TestParseMsgURI(t *testing.T) {
	msg := []byte("INVITE sip:alice@example.com SIP/2.0\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	err = ParseMsgURI(parsed)
	if err != nil {
		t.Fatalf("parse URI error: %v", err)
	}

	if parsed.ParsedURI == nil {
		t.Fatal("expected parsed URI")
	}

	if parsed.ParsedURI.User.String() != "alice" {
		t.Errorf("expected user 'alice', got '%s'", parsed.ParsedURI.User.String())
	}
}

func BenchmarkParseURI(b *testing.B) {
	uriStr := "sip:alice:secret@example.com:5060;transport=udp;lr"
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := ParseURI(uriStr)
		if err != nil {
			b.Fatal(err)
		}
	}
}

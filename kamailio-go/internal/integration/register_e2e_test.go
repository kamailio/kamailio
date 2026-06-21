// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 45
 *
 * End-to-end REGISTER tests covering:
 *   - Basic 200 OK registration
 *   - Digest authentication (401 -> 200)
 *   - NAT received IP tracking
 *   - Contact refresh (re-registration)
 *   - Wildcard unregistration
 */

package integration

import (
	"fmt"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/registrar"
)

// buildRegisterMsg constructs a raw SIP REGISTER message.
func buildRegisterMsg(user, contact, expires string) []byte {
	var b strings.Builder
	b.WriteString("REGISTER sip:local SIP/2.0\r\n")
	b.WriteString("Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKreg1\r\n")
	b.WriteString("From: <sip:" + user + "@local>;tag=alice-reg\r\n")
	b.WriteString("To: <sip:" + user + "@local>\r\n")
	b.WriteString("Call-ID: reg-1@local\r\n")
	b.WriteString("CSeq: 1 REGISTER\r\n")
	if contact != "" {
		b.WriteString("Contact: " + contact + "\r\n")
	}
	if expires != "" {
		b.WriteString("Expires: " + expires + "\r\n")
	}
	b.WriteString("Content-Length: 0\r\n\r\n")
	return []byte(b.String())
}

// buildRegisterWithAuth constructs a REGISTER with Authorization header.
func buildRegisterWithAuth(user, contact, expires, authHeader string) []byte {
	var b strings.Builder
	b.WriteString("REGISTER sip:local SIP/2.0\r\n")
	b.WriteString("Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKreg2\r\n")
	b.WriteString("From: <sip:" + user + "@local>;tag=alice-reg2\r\n")
	b.WriteString("To: <sip:" + user + "@local>\r\n")
	b.WriteString("Call-ID: reg-2@local\r\n")
	b.WriteString("CSeq: 2 REGISTER\r\n")
	if contact != "" {
		b.WriteString("Contact: " + contact + "\r\n")
	}
	if expires != "" {
		b.WriteString("Expires: " + expires + "\r\n")
	}
	b.WriteString("Authorization: " + authHeader + "\r\n")
	b.WriteString("Content-Length: 0\r\n\r\n")
	return []byte(b.String())
}

func TestRegisterE2E_Basic200(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	reg := registrar.New(&registrar.Config{
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
		Realm:          "local",
	})
	pc.SetRegistrar(reg)

	raw := buildRegisterMsg("alice", "<sip:alice@10.0.0.1:5060>", "3600")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}
	action := pc.ProcessRequest(msg, src, nil)

	if action.Status != 200 {
		t.Fatalf("expected 200, got %d %s", action.Status, action.Reason)
	}
	if reg.Count() == 0 {
		t.Fatal("expected at least one contact registered")
	}
}

func TestRegisterE2E_Auth401Then200(t *testing.T) {
	store := auth.NewInMemoryStore()
	store.AddUser("alice", "secret123", "", "local")

	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	reg := registrar.New(&registrar.Config{
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
		Realm:          "local",
		AuthRequired:   true,
		AuthBackend:    store,
	})
	pc.SetRegistrar(reg)

	// First REGISTER without auth -> 401
	raw1 := buildRegisterMsg("alice", "<sip:alice@10.0.0.1:5060>", "3600")
	msg1, _ := parser.ParseMsg(raw1)
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}
	action1 := pc.ProcessRequest(msg1, src, nil)
	if action1.Status != 401 {
		t.Fatalf("expected 401, got %d", action1.Status)
	}

	// Extract nonce from WWW-Authenticate header
	var nonce string
	for _, h := range action1.ExtraHeaders {
		if strings.Contains(h, "nonce=") {
			start := strings.Index(h, "nonce=") + 6
			if h[start] == '"' {
				start++
				end := strings.Index(h[start:], "\"")
				if end > 0 {
					nonce = h[start : start+end]
				}
			}
		}
	}
	if nonce == "" {
		t.Fatal("no nonce in 401 challenge")
	}

	// Compute digest response using CalcHA1 + CalcHA2 + CalcResponse
	ha1 := auth.CalcHA1(parser.AlgMD5, "alice", "local", "secret123", nonce, "")
	ha2 := auth.CalcHA2(parser.AlgMD5, "REGISTER", "sip:local", "", parser.QopNone)
	resp := auth.CalcResponse(parser.AlgMD5, ha1, nonce, "", "", "", ha2)
	authHeader := fmt.Sprintf("Digest username=\"alice\", realm=\"local\", nonce=\"%s\", uri=\"sip:local\", response=\"%s\"", nonce, resp)

	raw2 := buildRegisterWithAuth("alice", "<sip:alice@10.0.0.1:5060>", "3600", authHeader)
	msg2, _ := parser.ParseMsg(raw2)
	action2 := pc.ProcessRequest(msg2, src, nil)
	if action2.Status != 200 {
		t.Fatalf("expected 200 after auth, got %d %s", action2.Status, action2.Reason)
	}
}

func TestRegisterE2E_NatReceived(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	reg := registrar.New(&registrar.Config{
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
		Realm:          "local",
	})
	pc.SetRegistrar(reg)

	raw := buildRegisterMsg("alice", "<sip:alice@10.0.0.1:5060>", "3600")
	msg, _ := parser.ParseMsg(raw)
	src := &net.UDPAddr{IP: net.ParseIP("192.168.1.100"), Port: 5060}
	pc.ProcessRequest(msg, src, nil)

	domain := reg.Domain("local")
	aor := domain.GetAOR("sip:alice@local")
	if aor == nil {
		t.Fatal("AOR not found")
	}
	contacts := aor.ActiveContacts()
	if len(contacts) == 0 {
		t.Fatal("no contacts")
	}
	// Received may include port; check prefix.
	if !strings.HasPrefix(contacts[0].Received, "192.168.1.100") {
		t.Fatalf("expected Received starting with 192.168.1.100, got %q", contacts[0].Received)
	}
}

func TestRegisterE2E_Refresh(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	reg := registrar.New(&registrar.Config{
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
		Realm:          "local",
	})
	pc.SetRegistrar(reg)

	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// First register with expires=60
	raw1 := buildRegisterMsg("alice", "<sip:alice@10.0.0.1:5060>;expires=60", "")
	msg1, _ := parser.ParseMsg(raw1)
	pc.ProcessRequest(msg1, src, nil)

	// Refresh with expires=120
	raw2 := buildRegisterMsg("alice", "<sip:alice@10.0.0.1:5060>;expires=120", "")
	msg2, _ := parser.ParseMsg(raw2)
	pc.ProcessRequest(msg2, src, nil)

	domain := reg.Domain("local")
	aor := domain.GetAOR("sip:alice@local")
	if aor == nil {
		t.Fatal("AOR not found")
	}
	contacts := aor.ActiveContacts()
	if len(contacts) != 1 {
		t.Fatalf("expected 1 contact, got %d", len(contacts))
	}
	// The refresh should have updated the expires to be well into the future (120s)
	if time.Until(contacts[0].Expires) < 100*time.Second {
		t.Fatalf("contact expiry too short after refresh: %v", contacts[0].Expires)
	}
}

func TestRegisterE2E_WildcardUnregister(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	reg := registrar.New(&registrar.Config{
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
		Realm:          "local",
	})
	pc.SetRegistrar(reg)

	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// Register first
	raw1 := buildRegisterMsg("alice", "<sip:alice@10.0.0.1:5060>", "3600")
	msg1, _ := parser.ParseMsg(raw1)
	pc.ProcessRequest(msg1, src, nil)
	if reg.Count() == 0 {
		t.Fatal("expected contact after initial register")
	}

	// Unregister with wildcard
	raw2 := buildRegisterMsg("alice", "*;expires=0", "")
	msg2, _ := parser.ParseMsg(raw2)
	action := pc.ProcessRequest(msg2, src, nil)
	if action.Status != 200 {
		t.Fatalf("expected 200 for unregister, got %d", action.Status)
	}
	if reg.Count() != 0 {
		t.Fatalf("expected 0 contacts after wildcard unregister, got %d", reg.Count())
	}
}

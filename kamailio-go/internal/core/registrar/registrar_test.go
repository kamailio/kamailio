// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Unit tests for the registrar package.
 *
 * Test coverage:
 *   - TestRegistrar_Process_BasicRegister         (basic REGISTER with one contact)
 *   - TestRegistrar_Process_ContactExpires        (expires param on Contact)
 *   - TestRegistrar_Process_ExpiresHeaderFallback (Expires header as fallback)
 *   - TestRegistrar_Process_IntervalTooBrief      (423 when expires < MinExpires)
 *   - TestRegistrar_Process_WildcardUnregister    (Contact: * removes bindings)
 *   - TestRegistrar_Process_Auth401               (WWW-Authenticate on missing auth)
 *   - TestRegistrar_Process_QueryOnlyNoContact    (no Contact -> echo all)
 *   - TestRegistrar_Stats                         (per-AOR counters)
 */

package registrar

import (
	"context"
	"fmt"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

// makeRegisterMsg parses a raw SIP REGISTER into a *parser.SIPMsg.
// The caller is expected to embed the correct headers; if no Request-URI is
// relevant we default to sip:example.com (the parser only needs a valid line).
func makeRegisterMsg(t *testing.T, raw string) *parser.SIPMsg {
	t.Helper()
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		t.Fatalf("ParseMsg: %v\nraw=%q", err, raw)
	}
	return msg
}

// testAddr returns a trivial net.Addr for use as the source transport.
func testAddr(host string) net.Addr {
	return &fakeAddr{network: "udp", addr: host}
}

type fakeAddr struct {
	network string
	addr    string
}

func (f *fakeAddr) Network() string { return f.network }
func (f *fakeAddr) String() string  { return f.addr }

// hasPrefixIn checks whether any of the given headers starts with prefix.
func hasPrefixIn(headers []string, prefix string) bool {
	for _, h := range headers {
		if strings.HasPrefix(h, prefix) {
			return true
		}
	}
	return false
}

// defaultConfig returns a short-duration configuration suitable for unit
// tests (small defaults and bounded max).  Tests that need specific Min/Max
// values will override them.
func defaultConfig() *Config {
	return &Config{
		DefaultExpires: 600 * time.Second,
		MaxExpires:     3600 * time.Second,
		MinExpires:     30 * time.Second,
		Realm:          "test.example",
	}
}

// ---------------------------------------------------------------------------
// 1. BasicRegister
// ---------------------------------------------------------------------------

func TestRegistrar_Process_BasicRegister(t *testing.T) {
	reg := New(defaultConfig())

	raw := "REGISTER sip:alice@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK1\r\n" +
		"From: <sip:alice@example.com>;tag=fb1\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: call-1@10.0.0.1\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"
	msg := makeRegisterMsg(t, raw)

	status, reason, headers, err := reg.Process(msg, testAddr("10.0.0.1:5060"))
	if err != nil {
		t.Fatalf("Process returned unexpected error: %v", err)
	}
	if status != 200 {
		t.Fatalf("status=%d, want 200 (reason=%q)", status, reason)
	}
	if got := reg.Count(); got < 1 {
		t.Fatalf("Count=%d, want >= 1", got)
	}
	// Echoed Contact header must reflect the registered URI.
	if !hasPrefixIn(headers, "Contact: <sip:alice@10.0.0.1:5060>") {
		t.Fatalf("missing expected Contact echo in %v", headers)
	}
}

// ---------------------------------------------------------------------------
// 2. ContactExpires
// ---------------------------------------------------------------------------

func TestRegistrar_Process_ContactExpires(t *testing.T) {
	reg := New(defaultConfig())

	raw := "REGISTER sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.2:5060;branch=z9hG4bK2\r\n" +
		"From: <sip:bob@example.com>;tag=fb2\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: call-2@10.0.0.2\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:bob@10.0.0.2:5060>;expires=60\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	status, reason, _, err := reg.Process(makeRegisterMsg(t, raw), testAddr("10.0.0.2:5060"))
	if err != nil || status != 200 {
		t.Fatalf("status=%d reason=%q err=%v", status, reason, err)
	}

	// Inspect the stored contact: Expires should be within 60 seconds from now.
	domain := reg.Domain("example.com")
	aor := domain.GetAOR("sip:bob@example.com")
	if aor == nil {
		t.Fatalf("GetAOR returned nil for bob@example.com")
	}
	active := aor.ActiveContacts()
	if len(active) != 1 {
		t.Fatalf("got %d active contacts, want 1", len(active))
	}
	remaining := time.Until(active[0].Expires)
	if remaining < 0 || remaining > 60*time.Second {
		t.Fatalf("contact Expires remaining=%s, want (0,60s]", remaining)
	}
}

// ---------------------------------------------------------------------------
// 3. ExpiresHeaderFallback
// ---------------------------------------------------------------------------

func TestRegistrar_Process_ExpiresHeaderFallback(t *testing.T) {
	reg := New(defaultConfig())

	raw := "REGISTER sip:carol@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.3:5060;branch=z9hG4bK3\r\n" +
		"From: <sip:carol@example.com>;tag=fb3\r\n" +
		"To: <sip:carol@example.com>\r\n" +
		"Call-ID: call-3@10.0.0.3\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Expires: 120\r\n" +
		"Contact: <sip:carol@10.0.0.3:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	if status, _, _, err := reg.Process(makeRegisterMsg(t, raw), testAddr("10.0.0.3:5060")); err != nil || status != 200 {
		t.Fatalf("status=%d err=%v", status, err)
	}

	domain := reg.Domain("example.com")
	aor := domain.GetAOR("sip:carol@example.com")
	if aor == nil {
		t.Fatalf("GetAOR returned nil for carol@example.com")
	}
	active := aor.ActiveContacts()
	if len(active) != 1 {
		t.Fatalf("active=%d want 1", len(active))
	}
	remaining := time.Until(active[0].Expires)
	upper := 120 * time.Second
	if remaining < 0 || remaining > upper {
		t.Fatalf("remaining=%s want (0,%s]", remaining, upper)
	}
}

// ---------------------------------------------------------------------------
// 4. IntervalTooBrief
// ---------------------------------------------------------------------------

func TestRegistrar_Process_IntervalTooBrief(t *testing.T) {
	cfg := defaultConfig()
	cfg.MinExpires = 120 * time.Second
	reg := New(cfg)

	raw := "REGISTER sip:dave@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.4:5060;branch=z9hG4bK4\r\n" +
		"From: <sip:dave@example.com>;tag=fb4\r\n" +
		"To: <sip:dave@example.com>\r\n" +
		"Call-ID: call-4@10.0.0.4\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:dave@10.0.0.4:5060>;expires=30\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	status, reason, headers, err := reg.Process(makeRegisterMsg(t, raw), testAddr("10.0.0.4:5060"))
	if status != 423 {
		t.Fatalf("status=%d reason=%q err=%v want 423", status, reason, err)
	}
	if !hasPrefixIn(headers, "Min-Expires:") {
		t.Fatalf("423 reply missing Min-Expires header in %v", headers)
	}
	// Contact must NOT have been stored because the request was rejected.
	if reg.Count() != 0 {
		t.Fatalf("Count=%d want 0 after 423", reg.Count())
	}
}

// ---------------------------------------------------------------------------
// 5. WildcardUnregister
// ---------------------------------------------------------------------------

func TestRegistrar_Process_WildcardUnregister(t *testing.T) {
	reg := New(defaultConfig())

	// First register a contact for eve@example.com so we have something to
	// unregister.
	regMsg := "REGISTER sip:eve@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.5:5060;branch=z9hG4bK5\r\n" +
		"From: <sip:eve@example.com>;tag=fb5\r\n" +
		"To: <sip:eve@example.com>\r\n" +
		"Call-ID: call-5@10.0.0.5\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:eve@10.0.0.5:5060>;expires=300\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	if status, _, _, err := reg.Process(makeRegisterMsg(t, regMsg), testAddr("10.0.0.5:5060")); err != nil || status != 200 {
		t.Fatalf("initial REGISTER failed: status=%d err=%v", status, err)
	}
	if reg.Count() < 1 {
		t.Fatalf("contact not stored after initial REGISTER")
	}

	// Now send the wildcard unregister.
	raw := "REGISTER sip:eve@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.5:5060;branch=z9hG4bK5b\r\n" +
		"From: <sip:eve@example.com>;tag=fb5b\r\n" +
		"To: <sip:eve@example.com>\r\n" +
		"Call-ID: call-5b@10.0.0.5\r\n" +
		"CSeq: 2 REGISTER\r\n" +
		"Contact: *;expires=0\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	status, reason, _, err := reg.Process(makeRegisterMsg(t, raw), testAddr("10.0.0.5:5060"))
	if err != nil || status != 200 {
		t.Fatalf("wildcard status=%d reason=%q err=%v", status, reason, err)
	}
	// Binding should be gone.
	domain := reg.Domain("example.com")
	aor := domain.GetAOR("sip:eve@example.com")
	if aor != nil && len(aor.ActiveContacts()) != 0 {
		t.Fatalf("expected 0 contacts after wildcard unregister, got %d", len(aor.ActiveContacts()))
	}
}

// ---------------------------------------------------------------------------
// 6. Auth401
// ---------------------------------------------------------------------------

func TestRegistrar_Process_Auth401(t *testing.T) {
	cfg := defaultConfig()
	cfg.AuthRequired = true
	// AuthBackend intentionally nil -> no Authorization header must still
	// trigger a 401 challenge.
	reg := New(cfg)

	raw := "REGISTER sip:frank@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.6:5060;branch=z9hG4bK6\r\n" +
		"From: <sip:frank@example.com>;tag=fb6\r\n" +
		"To: <sip:frank@example.com>\r\n" +
		"Call-ID: call-6@10.0.0.6\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:frank@10.0.0.6:5060>;expires=300\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	status, reason, headers, err := reg.Process(makeRegisterMsg(t, raw), testAddr("10.0.0.6:5060"))
	if status != 401 {
		t.Fatalf("status=%d reason=%q err=%v want 401", status, reason, err)
	}
	if !hasPrefixIn(headers, "WWW-Authenticate:") {
		t.Fatalf("401 reply missing WWW-Authenticate: headers=%v", headers)
	}
	// Contact must NOT have been stored because the request was rejected.
	if reg.Count() != 0 {
		t.Fatalf("Count=%d want 0 after 401", reg.Count())
	}
}

// ---------------------------------------------------------------------------
// 7. QueryOnlyNoContact
// ---------------------------------------------------------------------------

func TestRegistrar_Process_QueryOnlyNoContact(t *testing.T) {
	reg := New(defaultConfig())

	// Pre-populate: register a contact for grace@example.com.
	regMsg := "REGISTER sip:grace@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.7:5060;branch=z9hG4bK7\r\n" +
		"From: <sip:grace@example.com>;tag=fb7\r\n" +
		"To: <sip:grace@example.com>\r\n" +
		"Call-ID: call-7@10.0.0.7\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:grace@10.0.0.7:5060>;expires=600\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	if status, _, _, err := reg.Process(makeRegisterMsg(t, regMsg), testAddr("10.0.0.7:5060")); err != nil || status != 200 {
		t.Fatalf("initial REGISTER failed: status=%d err=%v", status, err)
	}

	// Now a "query" REGISTER (no Contact header) should echo every binding.
	query := "REGISTER sip:grace@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.7:5060;branch=z9hG4bK7b\r\n" +
		"From: <sip:grace@example.com>;tag=fb7b\r\n" +
		"To: <sip:grace@example.com>\r\n" +
		"Call-ID: call-7b@10.0.0.7\r\n" +
		"CSeq: 2 REGISTER\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	status, reason, headers, err := reg.Process(makeRegisterMsg(t, query), testAddr("10.0.0.7:5060"))
	if err != nil || status != 200 {
		t.Fatalf("query status=%d reason=%q err=%v", status, reason, err)
	}
	if len(headers) == 0 {
		t.Fatalf("expected at least one Contact echo, got none")
	}
	if !hasPrefixIn(headers, "Contact: <sip:grace@10.0.0.7:5060>") {
		t.Fatalf("expected Contact echo for grace, got %v", headers)
	}
}

// ---------------------------------------------------------------------------
// 8. Stats
// ---------------------------------------------------------------------------

// fakeStore is a tiny in-memory AuthDB used by the Stats test to demonstrate
// a registrar wired up with a real credential backend.
type fakeStore struct {
	users map[string]*auth.Credentials
}

func (f *fakeStore) Lookup(_ context.Context, username string) (*auth.Credentials, error) {
	if c, ok := f.users[username]; ok {
		return c, nil
	}
	return nil, fmt.Errorf("user %q unknown", username)
}

func TestRegistrar_Stats(t *testing.T) {
	reg := New(defaultConfig())

	// Register two different AORs so the per-AOR map is non-trivial.
	for i, user := range []string{"henry", "ivy"} {
		raw := fmt.Sprintf("REGISTER sip:%s@example.com SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 10.0.0.%d:5060;branch=z9hG4bK%d\r\n"+
			"From: <sip:%s@example.com>;tag=tag%d\r\n"+
			"To: <sip:%s@example.com>\r\n"+
			"Call-ID: call-stat-%d@10.0.0.%d\r\n"+
			"CSeq: 1 REGISTER\r\n"+
			"Contact: <sip:%s@10.0.0.%d:5060>;expires=600\r\n"+
			"Content-Length: 0\r\n"+
			"\r\n",
			user, 8+i, 8+i, user, i, user, i, 8+i, user, 8+i)
		if status, _, _, err := reg.Process(makeRegisterMsg(t, raw), testAddr(fmt.Sprintf("10.0.0.%d:5060", 8+i))); err != nil || status != 200 {
			t.Fatalf("REGISTER(%s) failed: status=%d err=%v", user, status, err)
		}
	}

	stats := reg.Stats("example.com")
	// Both AORs should be present with positive contact counts.
	henry := stats["sip:henry@example.com"]
	ivy := stats["sip:ivy@example.com"]
	if henry <= 0 {
		t.Errorf("henry stats=%d want >0", henry)
	}
	if ivy <= 0 {
		t.Errorf("ivy stats=%d want >0", ivy)
	}
	if reg.Count() < 2 {
		t.Errorf("Count=%d want >=2", reg.Count())
	}

	// Unknown domain returns an empty (but non-nil) map.
	empty := reg.Stats("unknown.test")
	if empty == nil {
		t.Fatalf("Stats(unknown domain) returned nil map")
	}
	if len(empty) != 0 {
		t.Fatalf("Stats(unknown domain) = %v, want {}", empty)
	}
}

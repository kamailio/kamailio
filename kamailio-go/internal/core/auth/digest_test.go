// SPDX-License-Identifier: GPL-2.0-or-later
package auth

import (
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

func TestNonceGenerate(t *testing.T) {
	nm := NewNonceManager(nil)
	nonce := nm.Generate()
	if nonce == "" {
		t.Fatal("expected non-empty nonce")
	}
	if !nm.Validate(nonce) {
		t.Error("freshly generated nonce should be valid")
	}
}

func TestNonceValidateExpired(t *testing.T) {
	cfg := &NonceConfig{Expiry: 50 * time.Millisecond}
	nm := NewNonceManager(cfg)
	nonce := nm.Generate()

	time.Sleep(100 * time.Millisecond)
	if nm.Validate(nonce) {
		t.Error("expired nonce should be invalid")
	}
}

func TestNonceCleanup(t *testing.T) {
	cfg := &NonceConfig{Expiry: 50 * time.Millisecond}
	nm := NewNonceManager(cfg)
	nm.Generate()
	nm.Generate()

	time.Sleep(100 * time.Millisecond)
	nm.Cleanup()

	// After cleanup, no nonces should remain
	if len(nm.nonces) != 0 {
		t.Errorf("expected 0 nonces after cleanup, got %d", len(nm.nonces))
	}
}

func TestCalcHA1_MD5(t *testing.T) {
	// RFC 2617 example: Mufasa:testrealm@host.com:Circle Of Life
	ha1 := CalcHA1(parser.AlgUnknown, "Mufasa", "testrealm@host.com", "Circle Of Life", "", "")
	expected := "939e7578ed9e3c518a452acee763bce9"
	if ha1 != expected {
		t.Errorf("HA1 = %q, want %q", ha1, expected)
	}
}

func TestCalcHA2_MD5(t *testing.T) {
	ha2 := CalcHA2(parser.AlgUnknown, "REGISTER", "sip:registrar.example.com", "", parser.QopNone)
	// Just verify it's a valid MD5 hex string (32 chars)
	if len(ha2) != 32 {
		t.Errorf("HA2 length = %d, want 32", len(ha2))
	}
}

func TestCalcResponse_MD5(t *testing.T) {
	// Computed from: HA1=Mufasa:testrealm@host.com:Circle Of Life,
	// HA2=REGISTER:sip:registrar.example.com
	ha1 := "939e7578ed9e3c518a452acee763bce9"
	nonce := "dcd98b7102dd2f0e8b11d0f600bfb0c093"
	nc := "00000001"
	cnonce := "0a4f113b"
	qop := "auth"
	ha2 := "f1ed17dd745d5b3c6faf41acbcf26d64"
	response := CalcResponse(parser.AlgUnknown, ha1, nonce, nc, cnonce, qop, ha2)
	expected := "e16944676b765decef6ce692f3e6459b"
	if response != expected {
		t.Errorf("response = %q, want %q", response, expected)
	}
}

func TestVerifyDigestResponse(t *testing.T) {
	authBody := &parser.AuthBody{
		Type:      parser.AuthDigest,
		Algorithm: parser.AlgMD5,
		Username:  str.Mk("Mufasa"),
		Realm:     str.Mk("testrealm@host.com"),
		Nonce:     str.Mk("dcd98b7102dd2f0e8b11d0f600bfb0c093"),
		URI:       str.Mk("sip:registrar.example.com"),
		Response:  str.Mk("e16944676b765decef6ce692f3e6459b"),
		CNonce:    str.Mk("0a4f113b"),
		NC:        str.Mk("00000001"),
		QopStr:    str.Mk("auth"),
	}
	if !VerifyDigestResponse(authBody, "REGISTER", "Circle Of Life") {
		t.Error("expected Digest response to verify")
	}
}

func TestBuildWWWAuthenticate(t *testing.T) {
	challenge := BuildWWWAuthenticate(ChallengeOptions{
		Realm:     "example.com",
		Nonce:     "abc123",
		Algorithm: parser.AlgMD5,
		Qop:       "auth",
	})
	if !strings.Contains(challenge, "Digest realm=\"example.com\"") {
		t.Errorf("missing realm in challenge: %s", challenge)
	}
	if !strings.Contains(challenge, "nonce=\"abc123\"") {
		t.Errorf("missing nonce in challenge: %s", challenge)
	}
	if !strings.Contains(challenge, "algorithm=MD5") {
		t.Errorf("missing algorithm in challenge: %s", challenge)
	}
	if !strings.Contains(challenge, "qop=\"auth\"") {
		t.Errorf("missing qop in challenge: %s", challenge)
	}
}

func TestBuildWWWAuthenticate_Stale(t *testing.T) {
	challenge := BuildWWWAuthenticate(ChallengeOptions{
		Realm:     "example.com",
		Nonce:     "newnonce",
		Stale:     true,
	})
	if !strings.Contains(challenge, "stale=true") {
		t.Errorf("missing stale in challenge: %s", challenge)
	}
}

func TestHashPassword(t *testing.T) {
	hashed := HashPassword("alice", "example.com", "password123")
	if len(hashed) != 32 {
		t.Errorf("expected 32-char MD5 hex, got %d", len(hashed))
	}
	if !VerifyStoredHA1(hashed, "alice", "example.com", "password123") {
		t.Error("VerifyStoredHA1 should match")
	}
	if VerifyStoredHA1(hashed, "alice", "example.com", "wrongpassword") {
		t.Error("VerifyStoredHA1 should not match wrong password")
	}
}

func TestBasicAuth(t *testing.T) {
	encoded := BasicAuthEncode("alice", "secret")
	user, pass, err := BasicAuthDecode(encoded)
	if err != nil {
		t.Fatalf("BasicAuthDecode error: %v", err)
	}
	if user != "alice" {
		t.Errorf("username = %q, want alice", user)
	}
	if pass != "secret" {
		t.Errorf("password = %q, want secret", pass)
	}
}

func TestDigestHash_SHA256(t *testing.T) {
	h := DigestHash(parser.AlgSHA256, "test")
	if len(h) != 64 { // SHA-256 = 32 bytes = 64 hex chars
		t.Errorf("SHA-256 hash length = %d, want 64", len(h))
	}
}

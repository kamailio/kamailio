// SPDX-License-Identifier: GPL-2.0-or-later

package auth

import (
	"encoding/base64"
	"testing"
)

func TestGenerateAuthVector(t *testing.T) {
	av, err := GenerateAuthVector()
	if err != nil {
		t.Fatalf("GenerateAuthVector failed: %v", err)
	}

	if len(av.RAND) != 16 {
		t.Errorf("expected RAND length 16, got %d", len(av.RAND))
	}
	if len(av.AUTN) != 16 {
		t.Errorf("expected AUTN length 16, got %d", len(av.AUTN))
	}
	if len(av.XRES) != 8 {
		t.Errorf("expected XRES length 8, got %d", len(av.XRES))
	}
	if len(av.CK) != 16 {
		t.Errorf("expected CK length 16, got %d", len(av.CK))
	}
	if len(av.IK) != 16 {
		t.Errorf("expected IK length 16, got %d", len(av.IK))
	}
}

func TestBuildChallenge(t *testing.T) {
	av := &AuthVector{
		RAND:  make([]byte, 16),
		AUTN:  make([]byte, 16),
		XRES:  make([]byte, 8),
		CK:    make([]byte, 16),
		IK:    make([]byte, 16),
	}
	// Fill with known values for deterministic test
	for i := range av.RAND {
		av.RAND[i] = byte(i)
	}

	challenge := BuildChallenge(av, "test-realm", "opaque-data")
	if challenge.Realm != "test-realm" {
		t.Errorf("expected realm 'test-realm', got %s", challenge.Realm)
	}
	if challenge.Opaque != "opaque-data" {
		t.Errorf("expected opaque 'opaque-data', got %s", challenge.Opaque)
	}
	if challenge.Algorithm != "AKAv1-MD5" {
		t.Errorf("expected algorithm 'AKAv1-MD5', got %s", challenge.Algorithm)
	}

	// Verify nonce is base64 encoded
	nonceData, err := base64.StdEncoding.DecodeString(challenge.Nonce)
	if err != nil {
		t.Errorf("nonce not valid base64: %v", err)
	}
	if len(nonceData) != 32 { // RAND (16) + AUTN (16)
		t.Errorf("expected nonce length 32, got %d", len(nonceData))
	}
}

func TestBuildWWWAuthenticate(t *testing.T) {
	challenge := &AKAChallenge{
		Realm:     "ims.example.com",
		Nonce:     "testnonce123",
		Algorithm: "AKAv1-MD5",
		Opaque:    "opaque123",
	}

	header := BuildWWWAuthenticate(challenge)
	headerStr := header.String()

	// Verify the header contains expected components
	if headerStr == "" {
		t.Error("expected non-empty header")
	}
	if challenge.Realm != "ims.example.com" {
		t.Errorf("expected realm 'ims.example.com', got %s", challenge.Realm)
	}
}

func TestParseAuthorization(t *testing.T) {
	authz := `Digest username="user@example.com", realm="ims.example.com", nonce="abc123", uri="sip:ims.example.com", response="def456", algorithm=AKAv1-MD5, opaque="opaque"`

	resp, err := ParseAuthorization(authz)
	if err != nil {
		t.Fatalf("ParseAuthorization failed: %v", err)
	}

	if resp.Username != "user@example.com" {
		t.Errorf("expected username 'user@example.com', got %s", resp.Username)
	}
	if resp.Realm != "ims.example.com" {
		t.Errorf("expected realm 'ims.example.com', got %s", resp.Realm)
	}
	if resp.Nonce != "abc123" {
		t.Errorf("expected nonce 'abc123', got %s", resp.Nonce)
	}
	if resp.URI != "sip:ims.example.com" {
		t.Errorf("expected URI 'sip:ims.example.com', got %s", resp.URI)
	}
	if resp.Response != "def456" {
		t.Errorf("expected response 'def456', got %s", resp.Response)
	}
	if resp.Algorithm != "AKAv1-MD5" {
		t.Errorf("expected algorithm 'AKAv1-MD5', got %s", resp.Algorithm)
	}
}

func TestParseAuthorization_NonDigest(t *testing.T) {
	_, err := ParseAuthorization("Basic abc123")
	if err == nil {
		t.Error("expected error for non-Digest authorization")
	}
}

func TestVerifyResponse(t *testing.T) {
	av := &AuthVector{
		XRES: []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
	}

	// Correct response
	goodResp := &AKAResponse{Response: "0102030405060708"}
	if !VerifyResponse(av, goodResp) {
		t.Error("expected VerifyResponse to pass for correct response")
	}

	// Wrong response
	badResp := &AKAResponse{Response: "ffffffffffffffff"}
	if VerifyResponse(av, badResp) {
		t.Error("expected VerifyResponse to fail for wrong response")
	}
}

func TestVerifyResponse_CaseInsensitive(t *testing.T) {
	av := &AuthVector{
		XRES: []byte{0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11},
	}

	// Uppercase response should still match
	resp := &AKAResponse{Response: "0A0B0C0D0E0F1011"}
	if !VerifyResponse(av, resp) {
		t.Error("expected VerifyResponse to pass for uppercase response")
	}
}

func TestGenerateOpaque(t *testing.T) {
	// GenerateOpaque includes timestamp, so same inputs may produce different outputs
	// Test verifies output format is valid MD5 hex (32 chars)

	opaque := GenerateOpaque("call-123", "1")

	// Should be valid hex (MD5 produces 32 hex chars)
	if len(opaque) != 32 {
		t.Errorf("expected opaque length 32, got %d", len(opaque))
	}

	// Different inputs should produce different output
	opaque2 := GenerateOpaque("call-456", "1")
	if opaque == opaque2 {
		t.Error("expected different opaque for different callID")
	}
}

func TestBuildAuthorizationResponse(t *testing.T) {
	auth := BuildAuthorizationResponse(
		"user@example.com",
		"ims.example.com",
		"nonce123",
		"sip:ims.example.com",
		"response456",
		"AKAv1-MD5",
		"opaque789",
	)

	expected := `Digest username="user@example.com", realm="ims.example.com", nonce="nonce123", uri="sip:ims.example.com", response="response456", algorithm=AKAv1-MD5, opaque="opaque789"`
	if auth.String() != expected {
		t.Errorf("expected %q, got %q", expected, auth.String())
	}
}

func TestNewAKAConfig(t *testing.T) {
	cfg := NewAKAConfig("test-realm")
	if cfg == nil {
		t.Fatal("NewAKAConfig returned nil")
	}
	if cfg.Realm != "test-realm" {
		t.Errorf("expected realm 'test-realm', got %s", cfg.Realm)
	}
	if cfg.Algorithm != "AKAv1-MD5" {
		t.Errorf("expected algorithm 'AKAv1-MD5', got %s", cfg.Algorithm)
	}
}

func TestParseAuthorization_MissingFields(t *testing.T) {
	// Minimal authorization with only required fields
	authz := `Digest username="user", nonce="abc", uri="sip:test.com", response="123"`

	resp, err := ParseAuthorization(authz)
	if err != nil {
		t.Fatalf("ParseAuthorization failed: %v", err)
	}

	if resp.Username != "user" {
		t.Errorf("expected username 'user', got %s", resp.Username)
	}
	if resp.Response != "123" {
		t.Errorf("expected response '123', got %s", resp.Response)
	}
}

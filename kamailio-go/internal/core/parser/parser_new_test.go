// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - unit tests for newly added parsers
 *
 * Covers: parse_allow, parse_require, parse_auth, parse_retry
 */

package parser

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ---- Allow header ----
func TestParseAllow_InviteBye(t *testing.T) {
	body := str.Mk("INVITE, BYE, ACK")
	ab, err := ParseAllow(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(ab.Methods) != 3 {
		t.Fatalf("expected 3 methods, got %d", len(ab.Methods))
	}
	if !ab.HasInvite() || !ab.HasBye() || !ab.HasAck() {
		t.Fatalf("expected invite/bye/ack present, got %+v", ab)
	}
	if ab.HasOptions() {
		t.Fatalf("expected options absent")
	}
}

func TestParseAllow_PrackAndUpdate(t *testing.T) {
	body := str.Mk("PRACK, UPDATE")
	ab, err := ParseAllow(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ab.HasPrack() || !ab.HasUpdate() {
		t.Fatalf("expected prack/update present, got %+v", ab)
	}
}

func TestParseAllow_StringRoundtrip(t *testing.T) {
	body := str.Mk("INVITE, ACK, BYE")
	ab, err := ParseAllow(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	s := ab.String()
	if s == "" {
		t.Fatalf("expected non-empty string")
	}
}

func TestParseAllow_Empty(t *testing.T) {
	// Empty Allow header -> empty methods slice
	body := str.Mk("")
	ab, err := ParseAllow(body)
	if err != nil {
		t.Fatalf("unexpected error for empty: %v", err)
	}
	if len(ab.Methods) != 0 {
		t.Fatalf("expected 0 methods, got %d", len(ab.Methods))
	}
}

// ---- Require/Supported ----
func TestParseRequire_100rel(t *testing.T) {
	body := str.Mk("100rel, timer")
	rb, err := ParseRequire(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !rb.Has100Rel() {
		t.Fatalf("expected 100rel present")
	}
	if !rb.HasTimer() {
		t.Fatalf("expected timer present")
	}
	if rb.Count != 2 {
		t.Fatalf("expected count=2, got %d", rb.Count)
	}
}

func TestParseSupported_Extension(t *testing.T) {
	body := str.Mk("path, gruu, my-custom-option")
	rb, err := ParseSupported(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !rb.HasPath() || !rb.HasGruu() {
		t.Fatalf("expected path/gruu present")
	}
	// At least one option should be "extension" (my-custom-option)
	foundExt := false
	for _, o := range rb.Options {
		if o.Type == OptionExtension {
			foundExt = true
		}
	}
	if !foundExt {
		t.Fatalf("expected at least one extension option")
	}
}

func TestParseProxyRequire_RequiresContent(t *testing.T) {
	_, err := ParseProxyRequire(str.Mk(""))
	if err == nil {
		t.Fatalf("expected error for empty proxy-require body")
	}
}

// ---- Authentication headers ----
func TestParseAuthenticate_Digest(t *testing.T) {
	body := str.Mk(`Digest realm="sip.example.com", nonce="abc123", qop="auth,auth-int", algorithm=MD5, stale=false`)
	ab, err := ParseAuthenticate(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ab.IsDigest() {
		t.Fatalf("expected digest auth")
	}
	if ab.Realm.String() != "sip.example.com" {
		t.Fatalf("unexpected realm: %q", ab.Realm.String())
	}
	if ab.Nonce.String() != "abc123" {
		t.Fatalf("unexpected nonce: %q", ab.Nonce.String())
	}
	if ab.Algorithm != AlgMD5 {
		t.Fatalf("expected MD5 algorithm, got %d", ab.Algorithm)
	}
	if (ab.Qop & QopAuth) == 0 {
		t.Fatalf("expected qop=auth to be present")
	}
	if (ab.Qop & QopAuthInt) == 0 {
		t.Fatalf("expected qop=auth-int to be present")
	}
}

func TestParseAuthorization_Credentials(t *testing.T) {
	body := str.Mk(`Digest username="alice", realm="sip.example.com", nonce="abc123", uri="sip:bob@example.com", response="xyz789", algorithm=SHA-256, qop=auth, nc=00000001, cnonce="abc"`)
	ab, err := ParseAuthorization(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if ab.Username.String() != "alice" {
		t.Fatalf("unexpected username: %q", ab.Username.String())
	}
	if ab.Response.String() != "xyz789" {
		t.Fatalf("unexpected response: %q", ab.Response.String())
	}
	if ab.Algorithm != AlgSHA256 {
		t.Fatalf("expected SHA-256 algorithm, got %d", ab.Algorithm)
	}
}

func TestParseAuthenticate_Basic(t *testing.T) {
	body := str.Mk(`Basic realm="secure"`)
	ab, err := ParseAuthenticate(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ab.IsBasic() {
		t.Fatalf("expected basic auth")
	}
	if ab.Realm.String() != "secure" {
		t.Fatalf("unexpected realm: %q", ab.Realm.String())
	}
}

// ---- Retry-After ----
func TestParseRetryAfter_Simple(t *testing.T) {
	body := str.Mk("120")
	rab, err := ParseRetryAfter(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rab.DeltaSeconds != 120 {
		t.Fatalf("expected 120, got %d", rab.DeltaSeconds)
	}
}

func TestParseRetryAfter_WithDurationAndComment(t *testing.T) {
	body := str.Mk("180 (please try later); duration=300")
	rab, err := ParseRetryAfter(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rab.DeltaSeconds != 180 {
		t.Fatalf("expected delta=180, got %d", rab.DeltaSeconds)
	}
	if rab.Duration != 300 {
		t.Fatalf("expected duration=300, got %d", rab.Duration)
	}
	if rab.Comment.String() != "please try later" {
		t.Fatalf("unexpected comment: %q", rab.Comment.String())
	}
}

// ---- Refer-To ----
func TestParseReferTo_NameAddr(t *testing.T) {
	body := str.Mk(`"Bob Smith" <sip:bob@example.com>`)
	rtb, err := ParseReferTo(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rtb.DisplayName.String() != "Bob Smith" {
		t.Fatalf("unexpected display name: %q", rtb.DisplayName.String())
	}
	if rtb.URIString.String() != "sip:bob@example.com" {
		t.Fatalf("unexpected URI string: %q", rtb.URIString.String())
	}
}

func TestParseReferTo_JustURI(t *testing.T) {
	body := str.Mk("sip:alice@example.com")
	rtb, err := ParseReferTo(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rtb.URIString.String() != "sip:alice@example.com" {
		t.Fatalf("unexpected URI: %q", rtb.URIString.String())
	}
}

func TestParseReferTo_WithParams(t *testing.T) {
	body := str.Mk(`<sip:bob@example.com>;method=INVITE;screen=no`)
	rtb, err := ParseReferTo(body)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	methodParam := rtb.GetParam("method")
	if methodParam == nil || methodParam.Value.String() != "INVITE" {
		t.Fatalf("expected method=INVITE param")
	}
	screenParam := rtb.GetParam("screen")
	if screenParam == nil || screenParam.Value.String() != "no" {
		t.Fatalf("expected screen=no param")
	}
}

// ---- Integration: parse through HdrField ----
func TestParseAllowFromHeader(t *testing.T) {
	hdr := &HdrField{
		Body: str.Mk("INVITE, BYE"),
	}
	ab, err := ParseAllowFromHeader(hdr)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ab.HasInvite() || !ab.HasBye() {
		t.Fatalf("expected invite/bye")
	}
}

func TestParseRequireFromHeader_NilHeader(t *testing.T) {
	_, err := ParseRequireFromHeader(nil)
	if err == nil {
		t.Fatalf("expected error for nil header")
	}
}

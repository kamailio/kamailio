// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - unit tests for Phase 3: session timers (RFC 4028)
 */

package parser

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ==================== Session-Expires / Min-SE parsing ====================

func TestParseSessionExpiresBasic(t *testing.T) {
	// Build a small INVITE with a Session-Expires header
	raw := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.1:5060;branch=z9hG4bK12345\r\n" +
		"From: <sip:alice@example.com>;tag=abc\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: session-timers-test-1\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Session-Expires: 1800;refresher=uas\r\n" +
		"Min-SE: 90\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n"

	msg, err := ParseMsg([]byte(raw))
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}
	if msg == nil {
		t.Fatalf("ParseMsg returned nil")
	}

	// Find Session-Expires header by iteration
	var seHdr, minHdr *HdrField
	for _, h := range msg.Headers {
		if h.Type == HdrSessionExpires {
			seHdr = h
		}
		if h.Type == HdrMinSE {
			minHdr = h
		}
	}
	if seHdr == nil {
		t.Fatalf("Session-Expires header not found")
	}
	if minHdr == nil {
		t.Fatalf("Min-SE header not found")
	}

	se, err := ParseSessionExpires(seHdr)
	if err != nil {
		t.Fatalf("ParseSessionExpires failed: %v", err)
	}
	if se.Seconds != 1800 {
		t.Fatalf("expected seconds=1800, got %d", se.Seconds)
	}
	if !se.IsUASRefresher() {
		t.Fatalf("expected refresher=UAS")
	}
	if se.IsUACRefresher() {
		t.Fatalf("expected not UAC refresher")
	}
	if !se.AboveMinSE(90) {
		t.Fatalf("expected 1800 >= 90")
	}

	min, err := ParseMinSE(minHdr)
	if err != nil {
		t.Fatalf("ParseMinSE failed: %v", err)
	}
	if min.Seconds != 90 {
		t.Fatalf("expected seconds=90, got %d", min.Seconds)
	}
}

func TestParseSessionExpiresNoRefresher(t *testing.T) {
	hdr := &HdrField{Body: str.Mk("3600")}
	se, err := ParseSessionExpires(hdr)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if se.Seconds != 3600 {
		t.Fatalf("expected 3600, got %d", se.Seconds)
	}
	if se.IsUACRefresher() || se.IsUASRefresher() {
		t.Fatalf("expected no refresher")
	}
}

func TestParseSessionExpiresUACRefresher(t *testing.T) {
	hdr := &HdrField{Body: str.Mk("900;refresher=uac")}
	se, err := ParseSessionExpires(hdr)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if se.Seconds != 900 {
		t.Fatalf("expected 900, got %d", se.Seconds)
	}
	if !se.IsUACRefresher() {
		t.Fatalf("expected UAC refresher")
	}
}

func TestParseSessionExpiresInvalid(t *testing.T) {
	_, err := ParseSessionExpires(nil)
	if err == nil {
		t.Fatalf("expected error for nil header")
	}

	hdr := &HdrField{Body: str.Mk("")}
	_, err = ParseSessionExpires(hdr)
	if err == nil {
		t.Fatalf("expected error for empty value")
	}

	hdr = &HdrField{Body: str.Mk("not-a-number")}
	_, err = ParseSessionExpires(hdr)
	if err == nil {
		t.Fatalf("expected error for non-numeric")
	}
}

func TestBuildSessionExpires(t *testing.T) {
	if s := BuildSessionExpires(1800, RefresherUAS); s != "1800;refresher=uas" {
		t.Fatalf("unexpected: %q", s)
	}
	if s := BuildSessionExpires(900, RefresherUAC); s != "900;refresher=uac" {
		t.Fatalf("unexpected: %q", s)
	}
	if s := BuildSessionExpires(1200, RefresherUnknown); s != "1200" {
		t.Fatalf("unexpected: %q", s)
	}
	if s := BuildMinSE(90); s != "90" {
		t.Fatalf("unexpected: %q", s)
	}
}

func TestParseSessionExpiresBelowMinSE(t *testing.T) {
	hdr := &HdrField{Body: str.Mk("60")}
	se, err := ParseSessionExpires(hdr)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if se.AboveMinSE(90) {
		t.Fatalf("expected 60 < 90 to fail AboveMinSE")
	}
}

// ==================== Router R-URI tests ====================
// (import router package indirectly through test helpers below)

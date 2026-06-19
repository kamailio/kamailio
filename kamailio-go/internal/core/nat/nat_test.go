// SPDX-License-Identifier: GPL-2.0-or-later
package nat

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

func TestNAT_Detect_NoNAT(t *testing.T) {
	raw := []byte("REGISTER sip:example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK-nattest1\r\n" +
		"From: <sip:alice@example.com>;tag=abc\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: nat-test-1@example.com\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@192.168.1.10:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}

	result := Detect(msg, "192.168.1.10")
	if result.IsNAT {
		t.Errorf("expected no NAT detection, reason: %s", result.Reason)
	}
}

func TestNAT_Detect_ContactMismatch(t *testing.T) {
	raw := []byte("REGISTER sip:example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK-nattest2\r\n" +
		"From: <sip:alice@example.com>;tag=abc\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: nat-test-2@example.com\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}

	result := Detect(msg, "192.168.1.10")
	if !result.IsNAT {
		t.Error("expected NAT detection (source IP != Contact IP)")
	}
}

func TestNAT_Detect_SDPMismatch(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.10:5060;branch=z9hG4bK-nattest3\r\n" +
		"From: <sip:alice@example.com>;tag=abc\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: nat-test-3@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 100\r\n" +
		"\r\n" +
		"v=0\r\no=- 0 0 IN IP4 10.0.0.1\r\ns=-\r\nc=IN IP4 10.0.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}

	result := Detect(msg, "192.168.1.10")
	if !result.IsNAT {
		t.Error("expected NAT detection (source IP != SDP IP)")
	}
}

func TestNAT_FixContact(t *testing.T) {
	raw := []byte("REGISTER sip:example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-fixcontact\r\n" +
		"From: <sip:alice@example.com>;tag=abc\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: fix-contact@example.com\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@10.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}

	err = FixContact(msg, "203.0.113.50", 12345)
	if err != nil {
		t.Fatalf("FixContact: %v", err)
	}
}

func TestNAT_NATHelper(t *testing.T) {
	nh := NewNATHelper()

	nh.AddKeepaliveTarget("10.0.0.1:5060")
	nh.AddKeepaliveTarget("10.0.0.2:5060")

	targets := nh.GetKeepaliveTargets()
	if len(targets) != 2 {
		t.Errorf("expected 2 targets, got %d", len(targets))
	}

	nh.RemoveKeepaliveTarget("10.0.0.1:5060")
	targets = nh.GetKeepaliveTargets()
	if len(targets) != 1 {
		t.Errorf("expected 1 target after removal, got %d", len(targets))
	}
}

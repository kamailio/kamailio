// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phase 9-1: Real network I/O tests — UDP send/receive round-trip.
 *
 * These tests start a real UDP listener, send SIP messages over the
 * network, and verify the responses come back correctly.
 */

package main

import (
	"fmt"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// findFreeUDPPort asks the kernel for an ephemeral UDP port.
func findFreeUDPPort() (int, error) {
	addr, err := net.ResolveUDPAddr("udp", "127.0.0.1:0")
	if err != nil {
		return 0, err
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		return 0, err
	}
	defer conn.Close()
	return conn.LocalAddr().(*net.UDPAddr).Port, nil
}

// buildREGISTER creates a raw REGISTER message.
func buildREGISTERMsg(from, to, callID string, cseq int) []byte {
	return []byte(fmt.Sprintf(
		"REGISTER sip:ims.example.com SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKnettest\r\n"+
			"Max-Forwards: 70\r\n"+
			"From: <%s>;tag=nettag\r\n"+
			"To: <%s>\r\n"+
			"Call-ID: %s\r\n"+
			"CSeq: %d REGISTER\r\n"+
			"Contact: <sip:test@127.0.0.1>\r\n"+
			"Content-Length: 0\r\n"+
			"\r\n",
		from, to, callID, cseq,
	))
}

// buildINVITEMsg creates a raw INVITE message.
func buildINVITEMsg(from, to, callID string, cseq int) []byte {
	return []byte(fmt.Sprintf(
		"INVITE %s SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKnetinv\r\n"+
			"Max-Forwards: 70\r\n"+
			"From: <%s>;tag=invtag\r\n"+
			"To: <%s>\r\n"+
			"Call-ID: %s\r\n"+
			"CSeq: %d INVITE\r\n"+
			"Contact: <sip:test@127.0.0.1>\r\n"+
			"Content-Type: application/sdp\r\n"+
			"Content-Length: 0\r\n"+
			"\r\n",
		to, from, to, callID, cseq,
	))
}

// TestNetwork_REGISTER_RoundTrip sends a REGISTER over UDP and verifies
// the 200 OK response comes back.
func TestNetwork_REGISTER_RoundTrip(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	// Give the listener a moment to start
	time.Sleep(50 * time.Millisecond)

	// Send REGISTER
	conn, err := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		t.Fatalf("Dial: %v", err)
	}
	defer conn.Close()

	msg := buildREGISTERMsg(
		"sip:alice@ims.example.com",
		"sip:alice@ims.example.com",
		"net-reg-001", 1,
	)
	if _, err := conn.Write(msg); err != nil {
		t.Fatalf("Write: %v", err)
	}

	// Read response
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, 4096)
	n, err := conn.Read(buf)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}

	resp, err := parser.ParseMsg(buf[:n])
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	if !resp.IsReply() {
		t.Fatal("expected reply, got request")
	}
	if resp.StatusCode() != 200 {
		t.Fatalf("expected 200, got %d", resp.StatusCode())
	}
	firstLine := string(resp.FirstLine.Reply.Status.Bytes())
	if !strings.Contains(firstLine, "200") {
		t.Fatalf("status line missing 200: %s", firstLine)
	}
}

// TestNetwork_INVITE_100Trying sends an INVITE and verifies 100 Trying
// is returned immediately.
func TestNetwork_INVITE_100Trying(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	time.Sleep(50 * time.Millisecond)

	conn, err := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		t.Fatalf("Dial: %v", err)
	}
	defer conn.Close()

	msg := buildINVITEMsg(
		"sip:alice@ims.example.com",
		"sip:bob@ims.example.com",
		"net-inv-001", 1,
	)
	if _, err := conn.Write(msg); err != nil {
		t.Fatalf("Write: %v", err)
	}

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, 4096)
	n, err := conn.Read(buf)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}

	resp, err := parser.ParseMsg(buf[:n])
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	if resp.StatusCode() != 100 {
		t.Fatalf("expected 100 Trying, got %d", resp.StatusCode())
	}
}

// TestNetwork_MultipleMessages sends multiple messages in sequence and
// verifies each gets a response.
func TestNetwork_MultipleMessages(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	time.Sleep(50 * time.Millisecond)

	conn, err := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		t.Fatalf("Dial: %v", err)
	}
	defer conn.Close()

	messages := []struct {
		name   string
		data   []byte
		expect int
	}{
		{"REGISTER", buildREGISTERMsg("sip:a@b", "sip:a@b", "multi-1", 1), 200},
		{"INVITE", buildINVITEMsg("sip:a@b", "sip:c@d", "multi-2", 1), 100},
		{"REGISTER", buildREGISTERMsg("sip:x@y", "sip:x@y", "multi-3", 1), 200},
	}

	for _, m := range messages {
		if _, err := conn.Write(m.data); err != nil {
			t.Fatalf("%s write: %v", m.name, err)
		}

		conn.SetReadDeadline(time.Now().Add(2 * time.Second))
		buf := make([]byte, 4096)
		n, err := conn.Read(buf)
		if err != nil {
			t.Fatalf("%s read: %v", m.name, err)
		}

		resp, err := parser.ParseMsg(buf[:n])
		if err != nil {
			t.Fatalf("%s parse: %v", m.name, err)
		}
		if int(resp.StatusCode()) != m.expect {
			t.Fatalf("%s: expected %d, got %d", m.name, m.expect, resp.StatusCode())
		}
	}
}

// TestNetwork_MaxForwardsZero sends a message with Max-Forwards: 0 and
// expects 483 Too Many Hops.
func TestNetwork_MaxForwardsZero(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	time.Sleep(50 * time.Millisecond)

	conn, err := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		t.Fatalf("Dial: %v", err)
	}
	defer conn.Close()

	msg := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKmf\r\n" +
		"Max-Forwards: 0\r\n" +
		"From: <sip:alice@example.com>;tag=tag1\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: mf-001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")

	if _, err := conn.Write(msg); err != nil {
		t.Fatalf("Write: %v", err)
	}

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, 4096)
	n, err := conn.Read(buf)
	if err != nil {
		t.Fatalf("Read: %v", err)
	}

	resp, err := parser.ParseMsg(buf[:n])
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	if resp.StatusCode() != 483 {
		t.Fatalf("expected 483, got %d", resp.StatusCode())
	}
}

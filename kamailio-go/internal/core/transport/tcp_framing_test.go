// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * TCP SIP message framing tests — RFC 3261 §18.3
 */

package transport

import (
	"bufio"
	"bytes"
	"net"
	"testing"
	"time"
)

// buildSIPMessage constructs a raw SIP message with the given body.
func buildSIPMessage(body string) []byte {
	cl := len(body)
	msg := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/TCP 192.168.1.1:5060;branch=z9hG4bKabc\r\n" +
		"From: <sip:alice@example.com>;tag=1234\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: test-call-id-001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: " + itoa(cl) + "\r\n" +
		"\r\n" +
		body
	return []byte(msg)
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var buf [20]byte
	i := len(buf)
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	return string(buf[i:])
}

// TestTCP_FramingWithBody verifies that a complete SIP message with a
// body is read correctly over TCP.
func TestTCP_FramingWithBody(t *testing.T) {
	body := "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=Test\r\n"
	raw := buildSIPMessage(body)

	reader := bufio.NewReader(bytes.NewReader(raw))
	got, err := (&TCPListener{}).readSIPMessage(reader)
	if err != nil {
		t.Fatalf("readSIPMessage failed: %v", err)
	}
	if !bytes.Equal(got, raw) {
		t.Fatalf("message mismatch:\ngot  (%d): %q\nwant (%d): %q", len(got), got, len(raw), raw)
	}
}

// TestTCP_FramingNoBody verifies that a SIP message with Content-Length: 0
// is read correctly (only headers).
func TestTCP_FramingNoBody(t *testing.T) {
	raw := buildSIPMessage("")

	reader := bufio.NewReader(bytes.NewReader(raw))
	got, err := (&TCPListener{}).readSIPMessage(reader)
	if err != nil {
		t.Fatalf("readSIPMessage failed: %v", err)
	}
	if !bytes.Equal(got, raw) {
		t.Fatalf("message mismatch:\ngot  (%d): %q\nwant (%d): %q", len(got), got, len(raw), raw)
	}
}

// TestTCP_FramingCompactForm verifies parsing of compact "l:" header.
func TestTCP_FramingCompactForm(t *testing.T) {
	body := "test body"
	msg := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/TCP 192.168.1.1:5060;branch=z9hG4bKabc\r\n" +
		"From: <sip:alice@example.com>;tag=1234\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: test-call-id-002\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"l: " + itoa(len(body)) + "\r\n" +
		"\r\n" +
		body

	reader := bufio.NewReader(bytes.NewReader([]byte(msg)))
	got, err := (&TCPListener{}).readSIPMessage(reader)
	if err != nil {
		t.Fatalf("readSIPMessage failed: %v", err)
	}
	if string(got) != msg {
		t.Fatalf("message mismatch:\ngot  (%d): %q\nwant (%d): %q", len(got), got, len(msg), msg)
	}
}

// TestTCP_FramingMultipleMessages verifies that two back-to-back SIP
// messages on the same TCP stream are read correctly.
func TestTCP_FramingMultipleMessages(t *testing.T) {
	body1 := "v=0\r\no=- 1 1 IN IP4 1.1.1.1\r\n"
	msg1 := buildSIPMessage(body1)

	body2 := "v=0\r\no=- 2 2 IN IP4 2.2.2.2\r\n"
	msg2 := buildSIPMessage(body2)

	combined := append(msg1, msg2...)
	reader := bufio.NewReader(bytes.NewReader(combined))

	// Read first message
	got1, err := (&TCPListener{}).readSIPMessage(reader)
	if err != nil {
		t.Fatalf("read first message failed: %v", err)
	}
	if !bytes.Equal(got1, msg1) {
		t.Fatalf("first message mismatch")
	}

	// Read second message
	got2, err := (&TCPListener{}).readSIPMessage(reader)
	if err != nil {
		t.Fatalf("read second message failed: %v", err)
	}
	if !bytes.Equal(got2, msg2) {
		t.Fatalf("second message mismatch")
	}
}

// TestTCP_FramingEndToEnd sends a SIP message over a real TCP
// connection and verifies it is received correctly.
func TestTCP_FramingEndToEnd(t *testing.T) {
	body := "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=EndToEnd\r\n"
	raw := buildSIPMessage(body)

	// Start a TCP listener
	si := &SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0, // ephemeral port
		Protocol: ProtoTCP,
	}

	var received []byte
	done := make(chan struct{})
	handler := func(data []byte, conn *TCPConnection, rcvInfo *ReceiveInfo) {
		received = data
		close(done)
	}

	listener := NewTCPListener(si, handler)
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe failed: %v", err)
	}
	defer listener.Shutdown(nil)

	// Get the actual listening port
	addr := listener.LocalAddr().(*net.TCPAddr)

	// Connect and send
	sender, err := NewTCPSender(addr.String())
	if err != nil {
		t.Fatalf("NewTCPSender failed: %v", err)
	}
	defer sender.Close()

	if err := sender.Send(raw); err != nil {
		t.Fatalf("Send failed: %v", err)
	}

	// Wait for reception
	select {
	case <-done:
		// ok
	case <-time.After(2 * time.Second):
		t.Fatal("timeout waiting for message")
	}

	if !bytes.Equal(received, raw) {
		t.Fatalf("received message mismatch:\ngot  (%d): %q\nwant (%d): %q", len(received), received, len(raw), raw)
	}
}

// TestParseContentLength verifies the parseContentLength helper.
func TestParseContentLength(t *testing.T) {
	tests := []struct {
		name string
		hdrs string
		want int
	}{
		{"simple", "Content-Length: 123\r\n", 123},
		{"with spaces", "Content-Length:   456\r\n", 456},
		{"compact", "l: 789\r\n", 789},
		{"mixed case", "content-length: 42\r\n", 42},
		{"not present", "Via: SIP/2.0/TCP 1.1.1.1\r\n", 0},
		{"last wins", "Content-Length: 1\r\nContent-Length: 99\r\n", 99},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := parseContentLength([]byte(tt.hdrs))
			if got != tt.want {
				t.Errorf("parseContentLength(%q) = %d, want %d", tt.hdrs, got, tt.want)
			}
		})
	}
}

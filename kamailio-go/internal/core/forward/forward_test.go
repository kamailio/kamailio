// SPDX-License-Identifier: GPL-2.0-or-later

package forward

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

func TestNewForwarder(t *testing.T) {
	f := NewForwarder()
	if f == nil {
		t.Fatal("NewForwarder returned nil")
	}
	if f.udpListeners == nil {
		t.Error("udpListeners map not initialized")
	}
	if f.tcpListeners == nil {
		t.Error("tcpListeners map not initialized")
	}
}

func TestForwarder_SetSendAddress(t *testing.T) {
	f := NewForwarder()
	f.SetSendAddress("192.168.1.1", 5060)

	if f.sendHost != "192.168.1.1" {
		t.Errorf("expected sendHost '192.168.1.1', got %s", f.sendHost)
	}
	if f.sendPort != 5060 {
		t.Errorf("expected sendPort 5060, got %d", f.sendPort)
	}
}

func TestParseHostPort(t *testing.T) {
	tests := []struct {
		input       string
		expectedHost string
		expectedPort int
	}{
		{"192.168.1.1", "192.168.1.1", 5060},
		{"192.168.1.1:5061", "192.168.1.1", 5061},
		{"[::1]", "::1", 5060},
		{"[::1]:5062", "::1", 5062},
		{"example.com", "example.com", 5060},
		{"example.com:8080", "example.com", 8080},
		{"  192.168.1.1  ", "192.168.1.1", 5060},
	}

	for _, tc := range tests {
		host, port := parseHostPort(tc.input)
		if host != tc.expectedHost {
			t.Errorf("parseHostPort(%q): expected host %q, got %q", tc.input, tc.expectedHost, host)
		}
		if port != tc.expectedPort {
			t.Errorf("parseHostPort(%q): expected port %d, got %d", tc.input, tc.expectedPort, port)
		}
	}
}

func TestExtractSentBy(t *testing.T) {
	tests := []struct {
		input         string
		expectedHost  string
		expectedPort  uint16
	}{
		{"192.168.1.1", "192.168.1.1", 5060},
		{"192.168.1.1:5061", "192.168.1.1", 5061},
		{"[::1]", "::1", 5060},
		{"[::1]:5062", "::1", 5062},
	}

	for _, tc := range tests {
		host, port := extractSentBy(tc.input)
		if host != tc.expectedHost {
			t.Errorf("extractSentBy(%q): expected host %q, got %q", tc.input, tc.expectedHost, host)
		}
		if port != tc.expectedPort {
			t.Errorf("extractSentBy(%q): expected port %d, got %d", tc.input, tc.expectedPort, port)
		}
	}
}

func TestForwarder_ResolveNextHop_Empty(t *testing.T) {
	f := NewForwarder()
	_, _, err := f.resolveNextHop("")
	if err == nil {
		t.Error("expected error for empty next hop")
	}
}

func TestForwarder_ResolveNextHop_PlainHost(t *testing.T) {
	f := NewForwarder()
	host, port, err := f.resolveNextHop("192.168.1.1:5060")
	if err != nil {
		t.Errorf("unexpected error: %v", err)
	}
	if host != "192.168.1.1" {
		t.Errorf("expected host '192.168.1.1', got %s", host)
	}
	if port != 5060 {
		t.Errorf("expected port 5060, got %d", port)
	}
}

func TestForwarder_ResolveNextHop_SIPURI(t *testing.T) {
	f := NewForwarder()
	host, port, err := f.resolveNextHop("sip:user@192.168.1.1:5060")
	if err != nil {
		t.Errorf("unexpected error: %v", err)
	}
	if host != "192.168.1.1" {
		t.Errorf("expected host '192.168.1.1', got %s", host)
	}
	if port != 5060 {
		t.Errorf("expected port 5060, got %d", port)
	}
}

func TestForwarder_SendToUDP_EmptyHost(t *testing.T) {
	f := NewForwarder()
	err := f.SendToUDP("", 5060, []byte("test"))
	if err == nil {
		t.Error("expected error for empty host")
	}
}

func TestForwarder_SendToUDP_EmptyPayload(t *testing.T) {
	f := NewForwarder()
	err := f.SendToUDP("127.0.0.1", 5060, []byte{})
	if err == nil {
		t.Error("expected error for empty payload")
	}
}

func TestForwarder_SelectUDPListener_Empty(t *testing.T) {
	f := NewForwarder()
	l := f.selectUDPListener()
	if l != nil {
		t.Error("expected nil listener for empty map")
	}
}

func TestForwarder_GetSendSocket_UDP(t *testing.T) {
	f := NewForwarder()
	socket := f.getSendSocket(0, nil) // ProtoUDP = 0
	if socket != nil {
		t.Error("expected nil socket for empty listeners")
	}
}

func TestParseViaBody_EmptyHost(t *testing.T) {
	// ViaBody with empty host should return error
	// This tests the error path
	v := &parser.ViaBody{
		Host: str.Str{},
		Port: 0,
	}
	_, _, err := parseViaBody(v)
	if err == nil {
		t.Error("expected error for empty Via host")
	}
}

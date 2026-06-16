// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Transport layer tests
 */

package transport

import (
	"context"
	"net"
	"testing"
	"time"
)

func TestParseSocketInfo(t *testing.T) {
	tests := []struct {
		input       string
		expectProto Protocol
		expectPort  uint16
	}{
		{"udp:127.0.0.1:5060", ProtoUDP, 5060},
		{"tcp:192.168.1.1:5060", ProtoTCP, 5060},
		{"tls:10.0.0.1:5061", ProtoTLS, 5061},
	}

	for _, tt := range tests {
		si, err := ParseSocketInfo(tt.input)
		if err != nil {
			t.Fatalf("ParseSocketInfo(%q) error: %v", tt.input, err)
		}
		if si.Protocol != tt.expectProto {
			t.Errorf("expected protocol %v, got %v", tt.expectProto, si.Protocol)
		}
		if si.Port != tt.expectPort {
			t.Errorf("expected port %d, got %d", tt.expectPort, si.Port)
		}
	}
}

func TestProtocolString(t *testing.T) {
	if ProtoUDP.String() != "udp" {
		t.Error("expected udp")
	}
	if ProtoTCP.String() != "tcp" {
		t.Error("expected tcp")
	}
	if ProtoTLS.String() != "tls" {
		t.Error("expected tls")
	}
}

func TestProtocolSIPPort(t *testing.T) {
	if ProtoUDP.SIPPort() != 5060 {
		t.Error("expected UDP port 5060")
	}
	if ProtoTLS.SIPPort() != 5061 {
		t.Error("expected TLS port 5061")
	}
}

func TestUDPListener(t *testing.T) {
	si := &SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0, // ephemeral port
		Protocol: ProtoUDP,
	}

	received := make(chan []byte, 1)
	handler := func(data []byte, srcAddr *net.UDPAddr, rcvInfo *ReceiveInfo) {
		received <- data
	}

	listener := NewUDPListener(si, handler)
	err := listener.ListenAndServe()
	if err != nil {
		t.Fatalf("failed to start UDP listener: %v", err)
	}
	defer listener.Shutdown(context.Background())

	// Get actual port
	localAddr := listener.LocalAddr().(*net.UDPAddr)

	// Send a test message
	sender, err := NewUDPSender()
	if err != nil {
		t.Fatalf("failed to create UDP sender: %v", err)
	}
	defer sender.Close()

	testData := []byte("TEST SIP MESSAGE")
	dst := &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: localAddr.Port}
	err = sender.Send(dst, testData)
	if err != nil {
		t.Fatalf("failed to send UDP message: %v", err)
	}

	// Wait for receive
	select {
	case data := <-received:
		if string(data) != string(testData) {
			t.Errorf("expected %q, got %q", testData, data)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("timeout waiting for UDP message")
	}
}

func TestUDPSender(t *testing.T) {
	sender, err := NewUDPSender()
	if err != nil {
		t.Fatalf("failed to create UDP sender: %v", err)
	}
	defer sender.Close()

	if sender.LocalAddr() == nil {
		t.Error("expected local address to be set")
	}
}

func TestReceiveInfo(t *testing.T) {
	rcvInfo := &ReceiveInfo{
		SrcIP:   net.ParseIP("192.168.1.1"),
		DstIP:   net.ParseIP("10.0.0.1"),
		SrcPort: 5060,
		DstPort: 5060,
		Proto:   ProtoUDP,
	}

	if !rcvInfo.SrcIP.Equal(net.ParseIP("192.168.1.1")) {
		t.Error("unexpected src IP")
	}
	if rcvInfo.SrcPort != 5060 {
		t.Error("unexpected src port")
	}
}

func TestDestInfo(t *testing.T) {
	di := &DestInfo{
		To:    &net.UDPAddr{IP: net.ParseIP("192.168.1.1"), Port: 5060},
		Proto: ProtoUDP,
	}

	if di.Proto != ProtoUDP {
		t.Error("unexpected protocol")
	}
}

func BenchmarkUDPListenerSend(b *testing.B) {
	si := &SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: ProtoUDP,
	}

	listener := NewUDPListener(si, nil)
	err := listener.ListenAndServe()
	if err != nil {
		b.Fatal(err)
	}
	defer listener.Shutdown(context.Background())

	localAddr := listener.LocalAddr().(*net.UDPAddr)
	sender, _ := NewUDPSender()
	defer sender.Close()

	dst := &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: localAddr.Port}
	testData := []byte("BENCHMARK TEST MESSAGE")

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		sender.Send(dst, testData)
	}
}

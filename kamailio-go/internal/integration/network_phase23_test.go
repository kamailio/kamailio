// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Phase 23 - Real network end-to-end tests.
 *
 * These tests spin up transport listeners (UDP and TCP) on random
 * loopback ports and drive real network traffic through the proxy
 * core. They verify:
 *
 *   - Messages are correctly received and parsed over UDP.
 *   - SIP messages transmitted over TCP (a stream protocol) are
 *     correctly framed by Content-Length boundaries.
 *   - Concurrent clients do not trigger data races or lost updates
 *     in the metrics counters.
 *   - Messages carrying a non-zero Content-Length body can be read
 *     completely over TCP.
 *
 * In contrast to the proxy_phase22 tests, which call into
 * ProxyCore.ProcessRequest directly with in-memory buffers, every
 * message here traverses the kernel network stack via the transport
 * package's UDP/TCP listeners and MessageHandler callbacks.
 */

package integration

import (
	"bytes"
	"context"
	"fmt"
	"net"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// TestUDPTransport_RoundTrip verifies a full SIP REGISTER can be sent
// over a real UDP socket to a UDP listener backed by ProxyCore.
func TestUDPTransport_RoundTrip(t *testing.T) {
	cfg := &proxy.ProxyConfig{Realm: "test.local"}
	core := proxy.NewProxyCore(cfg)

	// Set up UDP listener on a random port
	udpAddr := &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0}
	listener := transport.NewUDPListener(&transport.SocketInfo{
		Name: "udp-test", Address: udpAddr.IP, Port: uint16(udpAddr.Port), Protocol: transport.ProtoUDP,
	}, func(data []byte, src *net.UDPAddr, rcv *transport.ReceiveInfo) {
		msg, err := parser.ParseMsg(data)
		if err != nil {
			return
		}
		// Hand off to proxy core
		core.ProcessRequest(msg, src, rcv)
	})
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	local := listener.LocalAddr()
	if local == nil {
		t.Fatal("listener has no local addr")
	}

	// Send 5 REGISTER messages, each with a new Call-ID.
	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("client udp: %v", err)
	}
	defer conn.Close()

	serverUDPAddr, ok := local.(*net.UDPAddr)
	if !ok {
		t.Fatal("listener local addr is not UDPAddr")
	}

	for i := 0; i < 5; i++ {
		raw := fmt.Sprintf(
			"REGISTER sip:test.local SIP/2.0\r\n"+
				"Via: SIP/2.0/UDP 127.0.0.1:%d;branch=z9hG4bK%x\r\n"+
				"From: <sip:alice@test.local>;tag=client%d\r\n"+
				"To: <sip:alice@test.local>\r\n"+
				"Call-ID: udp-call-%d-%x\r\n"+
				"CSeq: 1 REGISTER\r\n"+
				"Contact: <sip:alice@127.0.0.1:%d>\r\n"+
				"Content-Length: 0\r\n\r\n",
			conn.LocalAddr().(*net.UDPAddr).Port, time.Now().UnixNano(),
			i, i, time.Now().UnixNano(), conn.LocalAddr().(*net.UDPAddr).Port,
		)
		if _, err := conn.WriteToUDP([]byte(raw), serverUDPAddr); err != nil {
			t.Fatalf("write: %v", err)
		}
	}

	// Give the server a moment to process
	time.Sleep(50 * time.Millisecond)

	// Sanity check: the proxy metrics should show requests being counted.
	metrics := core.Metrics()
	if metrics == nil {
		t.Fatal("expected non-nil metrics")
	}
	snap := metrics.Snapshot()
	if snap.Requests < 5 {
		t.Errorf("requests = %d, want >= 5", snap.Requests)
	}
}

// TestTCPTransport_MessageFraming verifies SIP messages arrive correctly
// when transmitted over TCP (which is a stream rather than datagram).
func TestTCPTransport_MessageFraming(t *testing.T) {
	cfg := &proxy.ProxyConfig{Realm: "tcp-test.local"}
	core := proxy.NewProxyCore(cfg)

	si := &transport.SocketInfo{
		Name:     "tcp-test",
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: transport.ProtoTCP,
	}

	var processed atomic.Int32
	listener := transport.NewTCPListener(si, func(data []byte, conn *transport.TCPConnection, rcv *transport.ReceiveInfo) {
		msg, err := parser.ParseMsg(data)
		if err != nil {
			return
		}
		core.ProcessRequest(msg, nil, rcv)
		processed.Add(1)
	})
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	local := listener.LocalAddr()
	if local == nil {
		t.Fatal("no local addr")
	}

	conn, err := net.DialTimeout("tcp", local.String(), 2*time.Second)
	if err != nil {
		t.Fatalf("dial tcp: %v", err)
	}
	defer conn.Close()

	// Send 3 messages back-to-back to ensure TCP framing works.
	msg := "OPTIONS sip:tcp-test.local SIP/2.0\r\n" +
		"Via: SIP/2.0/TCP 127.0.0.1:55555;branch=z9hG4bK1\r\n" +
		"From: <sip:alice@tcp-test.local>;tag=t1\r\n" +
		"To: <sip:tcp-test.local>\r\n" +
		"Call-ID: tcp-test-%x\r\n" +
		"CSeq: 1 OPTIONS\r\n" +
		"Content-Length: 0\r\n\r\n"

	for i := 0; i < 3; i++ {
		formatted := fmt.Sprintf(msg, time.Now().UnixNano())
		if _, err := conn.Write([]byte(formatted)); err != nil {
			t.Fatalf("tcp write: %v", err)
		}
	}

	// Allow for processing — TCP read loop may need multiple iterations.
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) && processed.Load() < 3 {
		time.Sleep(20 * time.Millisecond)
	}

	if got := processed.Load(); got < 3 {
		t.Errorf("processed = %d, want >= 3", got)
	}
}

// TestNetwork_ConcurrentClients fires up several concurrent clients, each
// sending multiple messages. It verifies the proxy core's counters are
// monotonic and that no listener panics occur.
func TestNetwork_ConcurrentClients(t *testing.T) {
	cfg := &proxy.ProxyConfig{Realm: "concurrent-test.local"}
	core := proxy.NewProxyCore(cfg)

	udpAddr := &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0}
	listener := transport.NewUDPListener(&transport.SocketInfo{
		Name: "udp-concurrent", Address: udpAddr.IP, Port: uint16(udpAddr.Port), Protocol: transport.ProtoUDP,
	}, func(data []byte, src *net.UDPAddr, rcv *transport.ReceiveInfo) {
		msg, err := parser.ParseMsg(data)
		if err != nil {
			return
		}
		core.ProcessRequest(msg, src, rcv)
	})
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	serverAddr := listener.LocalAddr().(*net.UDPAddr)

	const clients = 8
	const perClient = 10
	var wg sync.WaitGroup
	wg.Add(clients)

	errs := make([]error, clients)
	for c := 0; c < clients; c++ {
		go func(clientID int) {
			defer wg.Done()
			conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
			if err != nil {
				errs[clientID] = err
				return
			}
			defer conn.Close()
			for i := 0; i < perClient; i++ {
				raw := fmt.Sprintf(
					"OPTIONS sip:concurrent-test.local SIP/2.0\r\n"+
						"Via: SIP/2.0/UDP 127.0.0.1:%d;branch=z9hG4bKclient%dmsg%d\r\n"+
						"From: <sip:c%d@concurrent-test.local>;tag=t%d\r\n"+
						"To: <sip:concurrent-test.local>\r\n"+
						"Call-ID: conc-%d-%d\r\n"+
						"CSeq: 1 OPTIONS\r\n"+
						"Content-Length: 0\r\n\r\n",
					conn.LocalAddr().(*net.UDPAddr).Port, clientID, i, clientID, clientID, clientID, time.Now().UnixNano(),
				)
				if _, err := conn.WriteToUDP([]byte(raw), serverAddr); err != nil {
					errs[clientID] = err
					return
				}
				time.Sleep(time.Microsecond)
			}
		}(c)
	}

	done := make(chan struct{})
	go func() { wg.Wait(); close(done) }()
	select {
	case <-done:
	case <-time.After(5 * time.Second):
		t.Fatal("timeout waiting for concurrent clients")
	}

	for _, err := range errs {
		if err != nil {
			t.Fatalf("client error: %v", err)
		}
	}

	// Wait for final messages to be processed
	time.Sleep(100 * time.Millisecond)

	snap := core.Metrics().Snapshot()
	if snap.Requests < uint64(clients*perClient) {
		t.Errorf("requests = %d, want >= %d", snap.Requests, clients*perClient)
	}
}

// TestNetwork_LargeBody verifies Content-Length > 0 SIP bodies are
// correctly read from TCP streams.
func TestNetwork_LargeBody(t *testing.T) {
	cfg := &proxy.ProxyConfig{Realm: "body-test.local"}
	core := proxy.NewProxyCore(cfg)

	si := &transport.SocketInfo{
		Name: "tcp-large", Address: net.ParseIP("127.0.0.1"), Port: 0, Protocol: transport.ProtoTCP,
	}

	var received atomic.Int32
	listener := transport.NewTCPListener(si, func(data []byte, conn *transport.TCPConnection, rcv *transport.ReceiveInfo) {
		msg, err := parser.ParseMsg(data)
		if err != nil {
			return
		}
		core.ProcessRequest(msg, nil, rcv)
		// Verify body presence by inspecting the raw bytes.
		if bytes.Contains(data, []byte("v=0")) {
			received.Add(1)
		}
	})
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	local := listener.LocalAddr()
	conn, err := net.DialTimeout("tcp", local.String(), 2*time.Second)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close()

	body := strings.Repeat("v=0\r\no=- 1234 0 IN IP4 10.0.0.1\r\ns=Test\r\nt=0 0\r\nc=IN IP4 10.0.0.1\r\n", 2)
	raw := fmt.Sprintf(
		"INVITE sip:bob@body-test.local SIP/2.0\r\n"+
			"Via: SIP/2.0/TCP 127.0.0.1:55555;branch=z9hG4bKbody\r\n"+
			"From: <sip:alice@body-test.local>;tag=t1\r\n"+
			"To: <sip:bob@body-test.local>\r\n"+
			"Call-ID: body-%x\r\n"+
			"CSeq: 1 INVITE\r\n"+
			"Contact: <sip:alice@127.0.0.1:55555>\r\n"+
			"Content-Type: application/sdp\r\n"+
			"Content-Length: %d\r\n\r\n%s",
		time.Now().UnixNano(), len(body), body,
	)

	if _, err := conn.Write([]byte(raw)); err != nil {
		t.Fatalf("tcp write: %v", err)
	}

	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) && received.Load() == 0 {
		time.Sleep(20 * time.Millisecond)
	}
	if received.Load() == 0 {
		t.Error("no message with body received")
	}
}

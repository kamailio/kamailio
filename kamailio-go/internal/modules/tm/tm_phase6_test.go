// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module Phase 6-2 tests — Timer integration + network send/forward.
 */

package tm

import (
	"context"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// testInviteBytes returns a deterministic SIP INVITE request suitable for
// creating transactions in tests.
func testInviteBytes() []byte {
	return []byte("INVITE sip:bob@127.0.0.1 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKtest001\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@127.0.0.1>;tag=tag-alice-001\r\n" +
		"To: Bob <sip:bob@127.0.0.1>\r\n" +
		"Call-ID: test-call-id-0001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

// parseMsg parses a SIP message or fails the test.
func parseMsg(t *testing.T, raw []byte) *parser.SIPMsg {
	t.Helper()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg failed: %v", err)
	}
	return msg
}

// ---------------------------------------------------------------------------
// Test 1: CreateWithTimerManager
// ---------------------------------------------------------------------------

func TestTM_CreateWithTimerManager(t *testing.T) {
	mgr := NewManagerWithTimers(16)
	if mgr == nil {
		t.Fatal("NewManagerWithTimers returned nil")
	}
	if mgr.TimerManager() == nil {
		t.Fatal("Manager has nil TimerManager")
	}

	// Verify the timer manager is actually initialized and can drive
	// transactions (e.g. the internal timers map is non-nil).
	cell := &Cell{State: TStateTrying}
	mgr.TimerManager().StartFRTimer(cell, 0)
	if !mgr.TimerManager().HasTimers(cell) {
		t.Error("expected HasTimers to be true after StartFRTimer")
	}
	mgr.TimerManager().StopAllTimers(cell)
	if mgr.TimerManager().HasTimers(cell) {
		t.Error("expected HasTimers to be false after StopAllTimers")
	}
}

// ---------------------------------------------------------------------------
// Test 2: SendReply via listener
// ---------------------------------------------------------------------------
//
// Strategy:
//   - Bind a UDP listener on an ephemeral port and register it with a
//     Manager (via AddListener).
//   - Bind a separate UDP "receiver" socket on another ephemeral port.
//   - Call Manager.SendReply(..., receiver-host, receiver-port).
//   - Read from the receiver socket and verify it is a valid 200 OK.

func TestTM_SendReplyViaListener(t *testing.T) {
	// Create receiver socket.
	receiverConn, err := net.ListenUDP("udp4", &net.UDPAddr{
		IP: net.ParseIP("127.0.0.1"), Port: 0,
	})
	if err != nil {
		t.Fatalf("receiver ListenUDP: %v", err)
	}
	defer receiverConn.Close()
	raddr := receiverConn.LocalAddr().(*net.UDPAddr)

	// Create a separate sender socket used as the "listener" for the
	// Manager (it needs to be bound to *some* local port to send from).
	senderConn, err := net.ListenUDP("udp4", &net.UDPAddr{
		IP: net.ParseIP("127.0.0.1"), Port: 0,
	})
	if err != nil {
		t.Fatalf("sender ListenUDP: %v", err)
	}
	defer senderConn.Close()
	saddr := senderConn.LocalAddr().(*net.UDPAddr)

	// Wrap the sender in a transport.UDPListener. We need to reach into
	// the package's internal conn field: use NewUDPListener + a
	// ListenAndServe call on the chosen port, then drop our pre-bound
	// senderConn since we don't actually need it.
	si := &transport.SocketInfo{
		Address:  saddr.IP,
		Port:     uint16(saddr.Port),
		Protocol: transport.ProtoUDP,
	}
	// Use the sender port we already got, but close that socket first.
	senderConn.Close()
	listener := transport.NewUDPListener(si, nil)
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	mgr := NewManager(16)
	mgr.AddListener(listener)

	invite := parseMsg(t, testInviteBytes())

	// Send a 200 OK to the receiver port.
	if err := mgr.SendReply(invite, 200, "OK", raddr.IP.String(), uint16(raddr.Port)); err != nil {
		t.Fatalf("SendReply: %v", err)
	}

	// Read from the receiver socket with a short deadline.
	buf := make([]byte, 4096)
	_ = receiverConn.SetReadDeadline(time.Now().Add(1 * time.Second))
	n, _, err := receiverConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("receiver ReadFromUDP: %v", err)
	}

	received := string(buf[:n])

	// Verify the response is a SIP/2.0 200 OK with proper headers.
	if !strings.HasPrefix(received, "SIP/2.0 200 OK") {
		t.Fatalf("expected response to start with SIP/2.0 200 OK, got:\n%s", received)
	}
	if !strings.Contains(received, "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKtest001") {
		t.Errorf("missing Via header in response:\n%s", received)
	}
	if !strings.Contains(received, "Call-ID: test-call-id-0001") {
		t.Errorf("missing Call-ID header in response:\n%s", received)
	}
	if !strings.Contains(received, "CSeq: 1 INVITE") {
		t.Errorf("missing CSeq header in response:\n%s", received)
	}
}

// ---------------------------------------------------------------------------
// Test 3: ForwardRequest
// ---------------------------------------------------------------------------
//
// Strategy:
//   - Bind a listener socket for the Manager (used to send forwarded
//     requests from), and a receiver socket (the "next hop").
//   - Call Manager.ForwardRequest(msg, nextHopURI, proxyHost, proxyPort,
//     receiverHost, receiverPort).
//   - Verify the received message has an additional Via header and the
//     original R-URI is preserved (or rewritten).

func TestTM_ForwardRequest(t *testing.T) {
	receiverConn, err := net.ListenUDP("udp4", &net.UDPAddr{
		IP: net.ParseIP("127.0.0.1"), Port: 0,
	})
	if err != nil {
		t.Fatalf("receiver ListenUDP: %v", err)
	}
	defer receiverConn.Close()
	raddr := receiverConn.LocalAddr().(*net.UDPAddr)

	// Bind a listener socket used by the Manager.
	si := &transport.SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: transport.ProtoUDP,
	}
	listener := transport.NewUDPListener(si, nil)
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	mgr := NewManager(16)
	mgr.AddListener(listener)

	invite := parseMsg(t, testInviteBytes())

	proxyAddr := listener.LocalAddr().(*net.UDPAddr)

	if err := mgr.ForwardRequest(invite, "sip:next-hop@127.0.0.1",
		proxyAddr.IP.String(), proxyAddr.Port,
		raddr.IP.String(), uint16(raddr.Port)); err != nil {
		t.Fatalf("ForwardRequest: %v", err)
	}

	buf := make([]byte, 4096)
	_ = receiverConn.SetReadDeadline(time.Now().Add(1 * time.Second))
	n, _, err := receiverConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("receiver ReadFromUDP: %v", err)
	}
	received := string(buf[:n])

	// The forwarded message must look like a SIP INVITE with a new Via
	// at the top.
	if !strings.HasPrefix(received, "INVITE ") {
		t.Fatalf("expected forwarded request to start with INVITE, got:\n%s", received)
	}
	if !strings.Contains(received, "Via: SIP/2.0/UDP "+proxyAddr.IP.String()+":"+
		itoa(proxyAddr.Port)+";branch=") {
		t.Errorf("expected forwarded request to include a fresh Via for proxy (%s:%d); got:\n%s",
			proxyAddr.IP, proxyAddr.Port, received)
	}
	if !strings.Contains(received, "Call-ID: test-call-id-0001") {
		t.Errorf("missing Call-ID in forwarded request:\n%s", received)
	}
}

// ---------------------------------------------------------------------------
// Test 4: TimerLifecycle — Verify timers start/stop with state changes.
// ---------------------------------------------------------------------------
//
// Strategy:
//   - Create a Manager with timers.
//   - Handle an INVITE (should start the FR timer).
//   - Confirm via TimerManager.HasTimers.
//   - Call Reply(..., 200, "OK") — should stop FR and start delete timer.
//   - Confirm via HasTimers (still tracked but the *old* FR should be
//     gone; our StopFRTimer clears the entry).
//   - Call Cancel() on a fresh transaction; verify HasTimers becomes
//     false.

func TestTM_TimerLifecycle(t *testing.T) {
	mgr := NewManagerWithTimers(16)
	tm := mgr.TimerManager()
	if tm == nil {
		t.Fatal("expected non-nil TimerManager")
	}

	// --- FR timer starts after HandleRequest for INVITE ---
	invite := parseMsg(t, testInviteBytes())
	cell, err := mgr.HandleRequest(invite)
	if err != nil {
		t.Fatalf("HandleRequest: %v", err)
	}
	if !tm.HasTimers(cell) {
		t.Fatal("expected FR timer to be tracked after HandleRequest(INVITE)")
	}

	// --- Final Reply stops FR, starts delete/wait timer ---
	if err := mgr.Reply(invite, 200, "OK"); err != nil {
		t.Fatalf("Reply 200 OK: %v", err)
	}
	// After final reply the FR timer entry is stopped but a delete
	// timer entry exists, so HasTimers should still be true.
	if !tm.HasTimers(cell) {
		t.Error("expected cell to still be tracked after Reply (delete timer)")
	}

	// --- A separate transaction: Cancel should stop all timers ---
	invite2Raw := []byte("INVITE sip:bob@127.0.0.1 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKtest002\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: Alice <sip:alice@127.0.0.1>;tag=tag-alice-002\r\n" +
		"To: Bob <sip:bob@127.0.0.1>\r\n" +
		"Call-ID: test-call-id-0002\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	invite2 := parseMsg(t, invite2Raw)
	cell2, err := mgr.HandleRequest(invite2)
	if err != nil {
		t.Fatalf("HandleRequest 2: %v", err)
	}
	if !tm.HasTimers(cell2) {
		t.Fatal("expected FR timer to be tracked after HandleRequest 2")
	}
	if err := mgr.Cancel(cell2, "Client Cancelled"); err != nil {
		t.Fatalf("Cancel: %v", err)
	}
	if tm.HasTimers(cell2) {
		t.Error("expected HasTimers to return false after Cancel")
	}
}

// ---------------------------------------------------------------------------
// Test 5: Send100TryingAndThen200OK — full state-machine send path.
// ---------------------------------------------------------------------------
//
// Strategy:
//   - Create Manager with timers, and register a UDP listener.
//   - Bind a separate receiver socket.
//   - Create an INVITE transaction via HandleRequest.
//   - Call SendReply for 100 Trying, verify receiver gets a 100.
//   - Call SendReply for 200 OK, verify receiver gets a 200.
//   - Verify state machine (cell.State == TStateCompleted).

func TestTM_Send100TryingAndThen200OK(t *testing.T) {
	// Receiver socket for the UAC-side "network".
	receiverConn, err := net.ListenUDP("udp4", &net.UDPAddr{
		IP: net.ParseIP("127.0.0.1"), Port: 0,
	})
	if err != nil {
		t.Fatalf("receiver ListenUDP: %v", err)
	}
	defer receiverConn.Close()
	raddr := receiverConn.LocalAddr().(*net.UDPAddr)

	// Sender UDP socket — bound via a transport.UDPListener.
	si := &transport.SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: transport.ProtoUDP,
	}
	listener := transport.NewUDPListener(si, nil)
	if err := listener.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer listener.Shutdown(context.Background())

	mgr := NewManagerWithTimers(16)
	mgr.AddListener(listener)

	invite := parseMsg(t, testInviteBytes())
	cell, err := mgr.HandleRequest(invite)
	if err != nil {
		t.Fatalf("HandleRequest: %v", err)
	}

	// -- Step 1: Send 100 Trying.
	if err := mgr.SendReply(invite, 100, "Trying", raddr.IP.String(), uint16(raddr.Port)); err != nil {
		t.Fatalf("SendReply 100 Trying: %v", err)
	}
	buf := make([]byte, 4096)
	_ = receiverConn.SetReadDeadline(time.Now().Add(1 * time.Second))
	n, _, err := receiverConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("receiver ReadFromUDP (100 Trying): %v", err)
	}
	trying := string(buf[:n])
	if !strings.HasPrefix(trying, "SIP/2.0 100 Trying") {
		t.Fatalf("expected 100 Trying response, got:\n%s", trying)
	}

	// -- Step 2: Update state via Reply to 180 Ringing.
	if err := mgr.Reply(invite, 180, "Ringing"); err != nil {
		t.Fatalf("Reply 180 Ringing: %v", err)
	}
	// -- Step 3: Send 200 OK via SendReply.
	if err := mgr.SendReply(invite, 200, "OK", raddr.IP.String(), uint16(raddr.Port)); err != nil {
		t.Fatalf("SendReply 200 OK: %v", err)
	}
	_ = receiverConn.SetReadDeadline(time.Now().Add(1 * time.Second))
	n, _, err = receiverConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("receiver ReadFromUDP (200 OK): %v", err)
	}
	ok := string(buf[:n])
	if !strings.HasPrefix(ok, "SIP/2.0 200 OK") {
		t.Fatalf("expected 200 OK response, got:\n%s", ok)
	}

	// -- Step 4: Call Reply to finalize state.
	if err := mgr.Reply(invite, 200, "OK"); err != nil {
		t.Fatalf("Reply 200 OK: %v", err)
	}

	if cell.State != TStateCompleted {
		t.Errorf("expected cell.State to be TStateCompleted, got %d", cell.State)
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 7 tests: Router ActionForward and ActionSend
 *
 * Tests for forwarding and sending via Router with Forwarder integration.
 */

package router

import (
	"context"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/forward"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

func buildInviteMsgForPhase7(fromHost string, ruri string) *parser.SIPMsg {
	msg := &parser.SIPMsg{
		FirstLine: &parser.MsgStart{
			Type:  parser.MsgRequest,
			Flags: parser.FLINEFlagProtoSIP,
			Req: &parser.RequestLine{
				Method:  strMk("INVITE"),
				URI:     strMk(ruri),
				Version: strMk("2.0"),
			},
		},
	}
	msg.AddHeader("Via", "SIP/2.0/UDP "+fromHost+":5060;branch=z9hG4bKtest")
	msg.AddHeader("From", "<sip:alice@"+fromHost+">;tag=abc123")
	msg.AddHeader("To", "<sip:bob@example.com>")
	msg.AddHeader("Call-ID", "call-1234@"+fromHost)
	msg.AddHeader("CSeq", "1 INVITE")
	msg.AddHeader("Max-Forwards", "70")
	msg.AddHeader("Content-Length", "0")
	return msg
}

func strMk(s string) str.Str {
	return str.Mk(s)
}

func zeroDeadline(seconds int) time.Time {
	return time.Now().Add(time.Duration(seconds) * time.Second)
}

// ---------------------------------------------------------------------------
// TestRouter_ActionForward
// ---------------------------------------------------------------------------

func TestRouter_ActionForward(t *testing.T) {
	// 1. Create a receiving UDP socket on a random port.
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()

	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

	// 2. Create a UDP listener (source socket) for the forwarder.
	senderAddr := &transport.SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: transport.ProtoUDP,
	}
	sender := transport.NewUDPListener(senderAddr, nil)
	if err := sender.ListenAndServe(); err != nil {
		t.Fatalf("failed to start UDP listener: %v", err)
	}
	defer sender.Shutdown(context.Background())

	// 3. Create Forwarder and Router.
	fwd := forward.NewForwarder()
	fwd.RegisterUDPListener(senderAddr, sender)
	fwd.SetSendAddress("127.0.0.1", recvPort+1)

	r := NewRouter()
	r.SetForwarder(fwd)

	// 4. Build route with ActionForward.
	msg := buildInviteMsgForPhase7("127.0.0.1", "sip:bob@127.0.0.1:5060")
	nextHop := "sip:bob@127.0.0.1:" + itoa(recvPort)

	actions := &Action{
		Type: ActionForward,
		Elements: []*ActionElem{
			{Type: ElemString, Value: nextHop},
		},
	}
	r.AddRoute("forward_route", 1, actions)

	// 5. Run the route.
	result := r.Run(context.Background(), "forward_route", msg)
	if result != ResultContinue {
		t.Errorf("Expected ResultContinue, got %v", result)
	}

	// 6. Receive and verify forwarded message.
	buf := make([]byte, 4096)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	raw := string(buf[:n])

	// Verify: new Via present.
	if !strings.Contains(raw, "Via:") {
		t.Fatalf("forwarded message missing Via header: %s", raw)
	}
	// Verify: Max-Forwards decremented (was 70, should now be 69).
	if !strings.Contains(raw, "Max-Forwards: 69") {
		t.Errorf("expected decremented Max-Forwards (69) in: %s", raw)
	}
	// Verify: still an INVITE request.
	if !strings.HasPrefix(raw, "INVITE ") {
		t.Errorf("expected INVITE prefix, got: %q", firstLine(raw))
	}
}

// ---------------------------------------------------------------------------
// TestRouter_ActionSend
// ---------------------------------------------------------------------------

func TestRouter_ActionSend(t *testing.T) {
	// 1. Create a receiving UDP socket on a random port.
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()

	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

	// 2. Create Forwarder and Router.
	fwd := forward.NewForwarder()

	r := NewRouter()
	r.SetForwarder(fwd)

	// 3. Build route with ActionSend.
	msg := buildInviteMsgForPhase7("127.0.0.1", "sip:bob@127.0.0.1:5060")
	dst := "127.0.0.1:" + itoa(recvPort)

	actions := &Action{
		Type: ActionSend,
		Elements: []*ActionElem{
			{Type: ElemString, Value: dst},
		},
	}
	r.AddRoute("send_route", 1, actions)

	// 4. Run the route.
	result := r.Run(context.Background(), "send_route", msg)
	if result != ResultContinue {
		t.Errorf("Expected ResultContinue, got %v", result)
	}

	// 5. Receive and verify sent message.
	buf := make([]byte, 4096)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	raw := string(buf[:n])

	// Verify: still an INVITE request.
	if !strings.HasPrefix(raw, "INVITE ") {
		t.Errorf("expected INVITE prefix, got: %q", firstLine(raw))
	}
	// Verify: Max-Forwards should NOT be decremented (send does not modify).
	if !strings.Contains(raw, "Max-Forwards: 70") {
		t.Errorf("expected Max-Forwards unchanged (70) in: %s", raw)
	}
	// Verify: original Via should be present.
	if !strings.Contains(raw, "Via:") {
		t.Errorf("expected Via header in sent message: %s", raw)
	}
}

// ---------------------------------------------------------------------------
// TestRouter_ForwardAndSendChain
// ---------------------------------------------------------------------------

func TestRouter_ForwardAndSendChain(t *testing.T) {
	// 1. Create two receiving UDP sockets on random ports.
	recvConnFwd, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP for forward: %v", err)
	}
	defer recvConnFwd.Close()

	recvConnSend, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP for send: %v", err)
	}
	defer recvConnSend.Close()

	fwdPort := recvConnFwd.LocalAddr().(*net.UDPAddr).Port
	sendPort := recvConnSend.LocalAddr().(*net.UDPAddr).Port

	// 2. Create a UDP listener for the forwarder.
	senderAddr := &transport.SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: transport.ProtoUDP,
	}
	sender := transport.NewUDPListener(senderAddr, nil)
	if err := sender.ListenAndServe(); err != nil {
		t.Fatalf("failed to start UDP listener: %v", err)
	}
	defer sender.Shutdown(context.Background())

	// 3. Create Forwarder and Router.
	fwd := forward.NewForwarder()
	fwd.RegisterUDPListener(senderAddr, sender)
	fwd.SetSendAddress("127.0.0.1", fwdPort+1)

	r := NewRouter()
	r.SetForwarder(fwd)

	// 4. Build route with Forward + Send chain.
	msg := buildInviteMsgForPhase7("127.0.0.1", "sip:bob@127.0.0.1:5060")

	actForward := &Action{
		Type: ActionForward,
		Elements: []*ActionElem{
			{Type: ElemString, Value: "sip:bob@127.0.0.1:" + itoa(fwdPort)},
		},
	}
	actSend := &Action{
		Type: ActionSend,
		Elements: []*ActionElem{
			{Type: ElemString, Value: "127.0.0.1:" + itoa(sendPort)},
		},
	}
	actForward.Next = actSend

	r.AddRoute("chain_route", 1, actForward)

	// 5. Run the route.
	result := r.Run(context.Background(), "chain_route", msg)
	if result != ResultContinue {
		t.Errorf("Expected ResultContinue, got %v", result)
	}

	// 6. Verify forwarded message.
	buf := make([]byte, 4096)
	recvConnFwd.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConnFwd.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP (forward) failed: %v", err)
	}
	rawFwd := string(buf[:n])
	if !strings.Contains(rawFwd, "Max-Forwards: 69") {
		t.Errorf("expected decremented Max-Forwards (69) in forwarded message: %s", rawFwd)
	}

	// 7. Verify sent message.
	// Note: ForwardRequest clones the message before modifying it,
	// so the original msg is unchanged. Send should see the original msg.
	buf2 := make([]byte, 4096)
	recvConnSend.SetReadDeadline(zeroDeadline(2))
	n2, _, err := recvConnSend.ReadFromUDP(buf2)
	if err != nil {
		t.Fatalf("ReadFromUDP (send) failed: %v", err)
	}
	rawSend := string(buf2[:n2])
	if !strings.Contains(rawSend, "Max-Forwards: 70") {
		t.Errorf("expected Max-Forwards (70) in sent message (original msg unchanged): %s", rawSend)
	}
}

// ---------------------------------------------------------------------------
// TestRouter_ForwardWithDefaultDestination
// ---------------------------------------------------------------------------

func TestRouter_ForwardWithDefaultDestination(t *testing.T) {
	// 1. Create a receiving UDP socket on a random port.
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()

	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

	// 2. Create a UDP listener for the forwarder.
	senderAddr := &transport.SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     0,
		Protocol: transport.ProtoUDP,
	}
	sender := transport.NewUDPListener(senderAddr, nil)
	if err := sender.ListenAndServe(); err != nil {
		t.Fatalf("failed to start UDP listener: %v", err)
	}
	defer sender.Shutdown(context.Background())

	// 3. Create Forwarder and Router with default destination.
	fwd := forward.NewForwarder()
	fwd.RegisterUDPListener(senderAddr, sender)
	fwd.SetSendAddress("127.0.0.1", recvPort+1)

	r := NewRouter()
	r.SetForwarder(fwd)
	r.SetDefaultDestination("sip:bob@127.0.0.1:" + itoa(recvPort))

	// 4. Build route with ActionForward but NO explicit destination.
	msg := buildInviteMsgForPhase7("127.0.0.1", "sip:bob@127.0.0.1:5060")

	actions := &Action{
		Type: ActionForward,
	}
	r.AddRoute("default_fwd_route", 1, actions)

	// 5. Run the route.
	result := r.Run(context.Background(), "default_fwd_route", msg)
	if result != ResultContinue {
		t.Errorf("Expected ResultContinue, got %v", result)
	}

	// 6. Receive and verify forwarded message.
	buf := make([]byte, 4096)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	raw := string(buf[:n])

	// Verify: new Via present.
	if !strings.Contains(raw, "Via:") {
		t.Fatalf("forwarded message missing Via header: %s", raw)
	}
	// Verify: Max-Forwards decremented.
	if !strings.Contains(raw, "Max-Forwards: 69") {
		t.Errorf("expected decremented Max-Forwards (69) in: %s", raw)
	}
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	var digits [20]byte
	i := len(digits)
	for n > 0 {
		i--
		digits[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		digits[i] = '-'
	}
	return string(digits[i:])
}

func firstLine(s string) string {
	idx := strings.Index(s, "\r\n")
	if idx < 0 {
		idx = strings.Index(s, "\n")
	}
	if idx < 0 {
		return s
	}
	return s[:idx]
}

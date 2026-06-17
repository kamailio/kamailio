// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 6-3 integration tests
 *
 * Tests for DNS resolution, message forwarding, and router selection.
 */

package forward

import (
	"context"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/dns"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/router"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// ---------------------------------------------------------------------------
// Helper: build a minimal INVITE SIPMsg for testing.
// ---------------------------------------------------------------------------

func buildInviteMsg(fromHost string, ruri string) *parser.SIPMsg {
	msg := &parser.SIPMsg{
		FirstLine: &parser.MsgStart{
			Type:  parser.MsgRequest,
			Flags: parser.FLINEFlagProtoSIP,
			Req: &parser.RequestLine{
				Method:  str.Mk("INVITE"),
				URI:     str.Mk(ruri),
				Version: str.Mk("2.0"),
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

// ---------------------------------------------------------------------------
// TestForwarder_BasicSend - Raw UDP send/recv.
// ---------------------------------------------------------------------------

func TestForwarder_BasicSend(t *testing.T) {
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

	// 3. Send raw bytes via Forwarder.
	fwd := NewForwarder()
	fwd.RegisterUDPListener(senderAddr, sender)

	payload := []byte("HELLO SIP WORLD")
	if err := fwd.SendToUDP("127.0.0.1", uint16(recvPort), payload); err != nil {
		t.Fatalf("SendToUDP failed: %v", err)
	}

	// 4. Receive and verify.
	buf := make([]byte, 1024)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	got := string(buf[:n])
	want := string(payload)
	if got != want {
		t.Errorf("payload mismatch: got=%q want=%q", got, want)
	}
}

// ---------------------------------------------------------------------------
// TestForwarder_ForwardRequest - End-to-end request forwarding with proper
// Via header prepending and Max-Forwards decrement.
// ---------------------------------------------------------------------------

func TestForwarder_ForwardRequest(t *testing.T) {
	// Receiving socket.
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()
	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

	// Build the original INVITE.
	msg := buildInviteMsg("127.0.0.1", "sip:bob@127.0.0.1:5060")

	// Forward to receiver port.
	nextHop := "sip:bob@127.0.0.1:" + itoa(recvPort)

	// Source socket info: we'll create a UDP listener for the forwarder.
	sendSI := &transport.SocketInfo{
		Address:  net.ParseIP("127.0.0.1"),
		Port:     uint16(recvPort + 1),
		Protocol: transport.ProtoUDP,
	}
	udpListener := transport.NewUDPListener(sendSI, nil)
	if err := udpListener.ListenAndServe(); err != nil {
		t.Fatalf("listen UDP failed: %v", err)
	}
	defer udpListener.Shutdown(context.Background())

	fwd := NewForwarder()
	fwd.RegisterUDPListener(sendSI, udpListener)
	fwd.SetSendAddress("127.0.0.1", recvPort+1)

	if err := fwd.ForwardRequest(msg, nextHop, sendSI); err != nil {
		t.Fatalf("ForwardRequest failed: %v", err)
	}

	// Read received bytes.
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
	// Verify: still an INVITE request
	if !strings.HasPrefix(raw, "INVITE ") {
		t.Errorf("expected INVITE prefix, got: %q", firstLine(raw))
	}
}

// ---------------------------------------------------------------------------
// TestForwarder_ForwardReply - Reply forwarding via Via destination.
// ---------------------------------------------------------------------------

func TestForwarder_ForwardReply(t *testing.T) {
	// Receive socket (where reply should be routed).
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{
		IP:   net.ParseIP("127.0.0.1"),
		Port: 0,
	})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()
	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

	// Build a 200 OK reply. Its top Via points to recvPort.
	reply := &parser.SIPMsg{
		FirstLine: &parser.MsgStart{
			Type:  parser.MsgReply,
			Flags: parser.FLINEFlagProtoSIP,
			Reply: &parser.ReplyLine{
				Version:    str.Mk("2.0"),
				Status:     str.Mk("200"),
				Reason:     str.Mk("OK"),
				StatusCode: 200,
			},
		},
	}
	viaBody := "SIP/2.0/UDP 127.0.0.1:" + itoa(recvPort) + ";branch=z9hG4bKreply;rport"
	reply.AddHeader("Via", viaBody)
	reply.AddHeader("From", "<sip:bob@example.com>;tag=server")
	reply.AddHeader("To", "<sip:alice@127.0.0.1>;tag=abc123")
	reply.AddHeader("Call-ID", "call-1234@127.0.0.1")
	reply.AddHeader("CSeq", "1 INVITE")
	reply.AddHeader("Content-Length", "0")

	// Forward the reply - should send via UDP to the Via destination.
	fwd := NewForwarder()
	if err := fwd.ForwardReply(reply, nil, nil); err != nil {
		t.Fatalf("ForwardReply failed: %v", err)
	}

	// Receive and verify.
	buf := make([]byte, 4096)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	raw := string(buf[:n])

	if !strings.Contains(raw, "SIP/2.0 200 OK") {
		t.Errorf("expected status line in reply, got: %q", firstLine(raw))
	}
	if !strings.Contains(raw, "Via: "+viaBody) {
		t.Errorf("expected Via header preserved in reply")
	}
}

// ---------------------------------------------------------------------------
// DNS resolver tests.
// ---------------------------------------------------------------------------

func TestDNS_ResolveLocalHost(t *testing.T) {
	r := dns.NewResolver()
	addrs, err := r.Resolve("localhost", dns.ProtoUDP, 5060)
	if err != nil {
		t.Fatalf("Resolve(localhost) failed: %v", err)
	}
	if len(addrs) == 0 {
		t.Fatalf("expected at least one address for localhost")
	}
	// localhost always resolves to 127.0.0.1 or ::1
	found := false
	for _, a := range addrs {
		if a.IP.IsLoopback() {
			found = true
		}
		if a.Port == 0 {
			t.Errorf("unexpected zero port in result")
		}
	}
	if !found {
		t.Errorf("expected loopback address, got: %+v", addrs)
	}
}

func TestDNS_ResolveSIPURI(t *testing.T) {
	r := dns.NewResolver()

	// Pure IP SIP URI - no DNS lookup needed.
	addrs, err := r.ResolveSIPURI("sip:alice@127.0.0.1:5060")
	if err != nil {
		t.Fatalf("ResolveSIPURI(127.0.0.1:5060) failed: %v", err)
	}
	if len(addrs) == 0 {
		t.Fatalf("expected at least one address from SIP URI")
	}
	// Check resolved host is 127.0.0.1
	found := false
	for _, a := range addrs {
		if a.IP.String() == "127.0.0.1" {
			found = true
		}
	}
	if !found {
		t.Errorf("expected 127.0.0.1 in resolved addresses, got: %+v", addrs)
	}

	// SIPS URI -> should be TLS protocol and port 5061.
	addrs2, err := r.ResolveSIPURI("sips:alice@127.0.0.1:5061")
	if err != nil {
		t.Fatalf("ResolveSIPURI(sips:) failed: %v", err)
	}
	if len(addrs2) == 0 {
		t.Fatalf("expected at least one address from sips URI")
	}

	// Resolve localhost via SIP URI (requires A record lookup).
	addrs3, err := r.ResolveSIPURI("sip:alice@localhost")
	if err != nil {
		t.Fatalf("ResolveSIPURI(localhost) failed: %v", err)
	}
	if len(addrs3) == 0 {
		t.Fatalf("expected at least one address for @localhost")
	}
}

func TestDNS_CacheIntegration(t *testing.T) {
	// Use a custom cache on the resolver.
	r := dns.NewResolver()
	cache := dns.NewCache(60)
	r.SetCache(cache)

	// First resolution populates cache.
	first, err := r.Resolve("localhost", dns.ProtoUDP, 5060)
	if err != nil {
		t.Fatalf("first resolve failed: %v", err)
	}
	if len(first) == 0 {
		t.Fatalf("no addresses from first resolution")
	}

	// Second resolution should return cached data.
	second, err := r.Resolve("localhost", dns.ProtoUDP, 5060)
	if err != nil {
		t.Fatalf("second resolve failed: %v", err)
	}
	if len(second) != len(first) {
		t.Errorf("cached result length mismatch: first=%d second=%d", len(first), len(second))
	}

	// Verify cache stats: at least one entry.
	if cache.Size() == 0 {
		t.Errorf("expected cache size > 0 after two resolutions")
	}
}

// ---------------------------------------------------------------------------
// RouterWithDNS tests.
// ---------------------------------------------------------------------------

func TestRouterWithDNS_PickBest(t *testing.T) {
	r := router.NewRouterWithDNS()

	dests := []*router.ForwardDestination{
		{Host: "10.0.0.1", Port: 5060, Order: 0},
		{Host: "10.0.0.2", Port: 5060, Order: 1},
		{Host: "10.0.0.3", Port: 5060, Order: 2},
	}
	best := r.PickBestDestination(dests)
	if best == nil {
		t.Fatalf("PickBestDestination returned nil")
	}
	if best.Host != "10.0.0.1" {
		t.Errorf("expected first destination, got %q", best.Host)
	}

	// Edge case: empty destinations.
	if r.PickBestDestination(nil) != nil {
		t.Errorf("PickBestDestination(nil) should return nil")
	}
}

func TestRouterWithDNS_ResolveNextHopByForceHost(t *testing.T) {
	r := router.NewRouterWithDNS()

	// Resolve by force host (IP, no DNS lookup).
	dests, err := r.ResolveNextHop(nil, "192.168.1.50", 5061)
	if err != nil {
		t.Fatalf("ResolveNextHop force-host failed: %v", err)
	}
	if len(dests) == 0 {
		t.Fatalf("expected at least one destination")
	}
	if dests[0].Host != "192.168.1.50" || dests[0].Port != 5061 {
		t.Errorf("unexpected destination: %+v", dests[0])
	}

	// Port zero -> default SIP port 5060.
	dests2, err := r.ResolveNextHop(nil, "192.168.1.50", 0)
	if err != nil {
		t.Fatalf("ResolveNextHop default port failed: %v", err)
	}
	if len(dests2) == 0 {
		t.Fatalf("expected at least one destination with default port")
	}
	if dests2[0].Port != 5060 {
		t.Errorf("expected default port 5060, got %d", dests2[0].Port)
	}
}

func TestRouterWithDNS_ResolveViaMsg(t *testing.T) {
	r := router.NewRouterWithDNS()
	msg := buildInviteMsg("127.0.0.1", "sip:bob@127.0.0.1:5060")
	dests, err := r.ResolveNextHop(msg, "", 0)
	if err != nil {
		t.Fatalf("ResolveNextHop(msg) failed: %v", err)
	}
	if len(dests) == 0 {
		t.Fatalf("expected at least one destination")
	}
	if dests[0].Host != "127.0.0.1" {
		t.Errorf("expected host 127.0.0.1, got %q", dests[0].Host)
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

func zeroDeadline(seconds int) time.Time {
	return time.Now().Add(time.Duration(seconds) * time.Second)
}

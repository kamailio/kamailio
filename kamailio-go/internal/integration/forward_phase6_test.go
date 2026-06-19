// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phase 6-3 integration tests for DNS resolution, message forwarding,
 * and router selection. Lives in the integration package to avoid
 * import cycles (router imports forward, so forward tests cannot
 * import router).
 */

package integration

import (
	"context"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/dns"
	"github.com/kamailio/kamailio-go/internal/core/forward"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/router"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

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

func zeroDeadline(seconds int) time.Time {
	return time.Now().Add(time.Duration(seconds) * time.Second)
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



// ---------------------------------------------------------------------------
// Forwarder tests
// ---------------------------------------------------------------------------

func TestForwarder_BasicSend(t *testing.T) {
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()
	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

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

	fwd := forward.NewForwarder()
	fwd.RegisterUDPListener(senderAddr, sender)

	payload := []byte("HELLO SIP WORLD")
	if err := fwd.SendToUDP("127.0.0.1", uint16(recvPort), payload); err != nil {
		t.Fatalf("SendToUDP failed: %v", err)
	}

	buf := make([]byte, 1024)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	if string(buf[:n]) != string(payload) {
		t.Errorf("payload mismatch")
	}
}

func TestForwarder_ForwardRequest(t *testing.T) {
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()
	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

	msg := buildInviteMsg("127.0.0.1", "sip:bob@127.0.0.1:5060")
	nextHop := "sip:bob@127.0.0.1:" + itoa(recvPort)

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

	fwd := forward.NewForwarder()
	fwd.RegisterUDPListener(sendSI, udpListener)
	fwd.SetSendAddress("127.0.0.1", recvPort+1)

	if err := fwd.ForwardRequest(msg, nextHop, sendSI); err != nil {
		t.Fatalf("ForwardRequest failed: %v", err)
	}

	buf := make([]byte, 4096)
	recvConn.SetReadDeadline(zeroDeadline(2))
	n2, _, err := recvConn.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("ReadFromUDP failed: %v", err)
	}
	raw := string(buf[:n2])

	if !strings.Contains(raw, "Via:") {
		t.Fatalf("forwarded message missing Via header")
	}
	if !strings.Contains(raw, "Max-Forwards: 69") {
		t.Errorf("expected decremented Max-Forwards (69)")
	}
	if !strings.HasPrefix(raw, "INVITE ") {
		t.Errorf("expected INVITE prefix, got: %q", firstLine(raw))
	}
}

func TestForwarder_ForwardReply(t *testing.T) {
	recvConn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("failed to listen UDP: %v", err)
	}
	defer recvConn.Close()
	recvPort := recvConn.LocalAddr().(*net.UDPAddr).Port

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

	fwd := forward.NewForwarder()
	if err := fwd.ForwardReply(reply, nil, nil); err != nil {
		t.Fatalf("ForwardReply failed: %v", err)
	}

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
}

// ---------------------------------------------------------------------------
// DNS resolver tests
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
	found := false
	for _, a := range addrs {
		if a.IP.IsLoopback() {
			found = true
		}
	}
	if !found {
		t.Errorf("expected loopback address")
	}
}

func TestDNS_ResolveSIPURI(t *testing.T) {
	r := dns.NewResolver()
	addrs, err := r.ResolveSIPURI("sip:alice@127.0.0.1:5060")
	if err != nil {
		t.Fatalf("ResolveSIPURI failed: %v", err)
	}
	if len(addrs) == 0 {
		t.Fatalf("expected at least one address")
	}
	found := false
	for _, a := range addrs {
		if a.IP.String() == "127.0.0.1" {
			found = true
		}
	}
	if !found {
		t.Errorf("expected 127.0.0.1 in resolved addresses")
	}
}

func TestDNS_CacheIntegration(t *testing.T) {
	r := dns.NewResolver()
	cache := dns.NewCache(60)
	r.SetCache(cache)

	first, err := r.Resolve("localhost", dns.ProtoUDP, 5060)
	if err != nil {
		t.Fatalf("first resolve failed: %v", err)
	}
	if len(first) == 0 {
		t.Fatalf("no addresses from first resolution")
	}

	second, err := r.Resolve("localhost", dns.ProtoUDP, 5060)
	if err != nil {
		t.Fatalf("second resolve failed: %v", err)
	}
	if len(second) != len(first) {
		t.Errorf("cached result length mismatch")
	}
	if cache.Size() == 0 {
		t.Errorf("expected cache size > 0")
	}
}

// ---------------------------------------------------------------------------
// RouterWithDNS tests
// ---------------------------------------------------------------------------

func TestRouterWithDNS_PickBest(t *testing.T) {
	r := router.NewRouterWithDNS()
	dests := []*router.ForwardDestination{
		{Host: "10.0.0.1", Port: 5060, Order: 0},
		{Host: "10.0.0.2", Port: 5060, Order: 1},
	}
	best := r.PickBestDestination(dests)
	if best == nil {
		t.Fatalf("PickBestDestination returned nil")
	}
	if best.Host != "10.0.0.1" {
		t.Errorf("expected first destination, got %q", best.Host)
	}
	if r.PickBestDestination(nil) != nil {
		t.Errorf("PickBestDestination(nil) should return nil")
	}
}

func TestRouterWithDNS_ResolveNextHopByForceHost(t *testing.T) {
	r := router.NewRouterWithDNS()
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

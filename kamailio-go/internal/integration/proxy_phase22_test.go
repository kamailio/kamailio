// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 22 integration tests
 *
 * Exercises the proxy core end-to-end: request handling,
 * authentication challenges, metrics accumulation, presence
 * subscriptions, and listener registration.
 */

package integration

import (
	"context"
	"fmt"
	"net"
	"sync"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// parseMsg is a thin wrapper around parser.ParseMsg that fails the
// test on error so callers can safely assume a non-nil *SIPMsg.
func parseMsg(t *testing.T, raw string) *parser.SIPMsg {
	t.Helper()
	msg, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	return msg
}

// TestProxyCore_RequestResponse verifies that ProcessRequest returns
// the expected response actions for common request types.
func TestProxyCore_RequestResponse(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:               "test.local",
		AuthRequired:        false,
		NATDetectionEnabled: false,
		PresenceEnabled:     true,
		RecordRouteEnabled:  false,
	})

	tests := []struct {
		name       string
		rawMsg     string
		wantStatus int
	}{
		{
			name: "OPTIONS keep-alive",
			rawMsg: "OPTIONS sip:test.local SIP/2.0\r\n" +
				"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKopts1\r\n" +
				"From: <sip:alice@test.local>;tag=alice1\r\n" +
				"To: <sip:bob@test.local>\r\n" +
				"Call-ID: opts1@test.local\r\n" +
				"CSeq: 1 OPTIONS\r\n" +
				"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
				"Content-Length: 0\r\n\r\n",
			wantStatus: 200,
		},
		{
			name: "REGISTER without auth challenge",
			rawMsg: "REGISTER sip:test.local SIP/2.0\r\n" +
				"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKreg1\r\n" +
				"From: <sip:alice@test.local>;tag=aliceReg1\r\n" +
				"To: <sip:alice@test.local>\r\n" +
				"Call-ID: reg1@test.local\r\n" +
				"CSeq: 1 REGISTER\r\n" +
				"Contact: <sip:alice@127.0.0.1:5060>;expires=3600\r\n" +
				"Authorization: Digest username=\"alice\", realm=\"test.local\", nonce=\"123\", uri=\"sip:test.local\", response=\"xyz\"\r\n" +
				"Content-Length: 0\r\n\r\n",
			wantStatus: 200,
		},
		{
			name: "INVITE without auth challenge",
			rawMsg: "INVITE sip:bob@test.local SIP/2.0\r\n" +
				"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKinv1\r\n" +
				"From: <sip:alice@test.local>;tag=aliceInv1\r\n" +
				"To: <sip:bob@test.local>\r\n" +
				"Call-ID: inv1@test.local\r\n" +
				"CSeq: 1 INVITE\r\n" +
				"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
				"Proxy-Authorization: Digest username=\"alice\", realm=\"test.local\", nonce=\"123\", uri=\"sip:bob@test.local\", response=\"xyz\"\r\n" +
				"Content-Type: application/sdp\r\n" +
				"Content-Length: 20\r\n\r\n" +
				"v=0\r\no=alice 1 2 IN IP4 127.0.0.1\r\n",
			wantStatus: 100,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			msg := parseMsg(t, tt.rawMsg)
			action := pc.ProcessRequest(msg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
			if action.Status != tt.wantStatus {
				t.Errorf("Status = %d, want %d", action.Status, tt.wantStatus)
			}
		})
	}
}

// TestProxyCore_AuthChallenge verifies that without auth headers,
// challenges are issued for REGISTER and INVITE.
func TestProxyCore_AuthChallenge(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:               "test.local",
		AuthRequired:        true,
		NATDetectionEnabled: false,
		PresenceEnabled:     false,
	})

	registerMsg := parseMsg(t,
		"REGISTER sip:test.local SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKauth1\r\n"+
			"From: <sip:alice@test.local>;tag=a1\r\n"+
			"To: <sip:alice@test.local>\r\n"+
			"Call-ID: auth1@test.local\r\n"+
			"CSeq: 1 REGISTER\r\n"+
			"Contact: <sip:alice@127.0.0.1:5060>\r\n"+
			"Content-Length: 0\r\n\r\n",
	)
	action := pc.ProcessRequest(registerMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 401 {
		t.Errorf("REGISTER without auth: status = %d, want 401", action.Status)
	}

	inviteMsg := parseMsg(t,
		"INVITE sip:bob@test.local SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKauth2\r\n"+
			"From: <sip:alice@test.local>;tag=a2\r\n"+
			"To: <sip:bob@test.local>\r\n"+
			"Call-ID: auth2@test.local\r\n"+
			"CSeq: 1 INVITE\r\n"+
			"Contact: <sip:alice@127.0.0.1:5060>\r\n"+
			"Content-Length: 0\r\n\r\n",
	)
	action = pc.ProcessRequest(inviteMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 407 {
		t.Errorf("INVITE without auth: status = %d, want 407", action.Status)
	}
}

// TestProxyCore_Metrics_Concurrent tests that request counters are
// correctly accumulated under concurrent invocations.
func TestProxyCore_Metrics_Concurrent(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:               "test.local",
		AuthRequired:        false,
		NATDetectionEnabled: false,
		PresenceEnabled:     false,
	})

	const n = 50
	var wg sync.WaitGroup
	wg.Add(n)
	for i := 0; i < n; i++ {
		go func(i int) {
			defer wg.Done()
			raw := fmt.Sprintf(
				"OPTIONS sip:test.local SIP/2.0\r\n"+
					"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKconc%d\r\n"+
					"From: <sip:user%d@test.local>;tag=c%d\r\n"+
					"To: <sip:test.local>\r\n"+
					"Call-ID: conc%d@test.local\r\n"+
					"CSeq: 1 OPTIONS\r\n"+
					"Contact: <sip:user%d@127.0.0.1:5060>\r\n"+
					"Content-Length: 0\r\n\r\n",
				i, i, i, i, i)
			msg, _ := parser.ParseMsg([]byte(raw))
			pc.ProcessRequest(msg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
		}(i)
	}

	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()

	select {
	case <-done:
	case <-time.After(5 * time.Second):
		t.Fatal("timeout waiting for concurrent requests")
	}

	snap := pc.Metrics().Snapshot()
	if snap.Requests != n {
		t.Errorf("requests = %d, want %d", snap.Requests, n)
	}
	if snap.Uptime <= 0 {
		t.Error("uptime should be positive")
	}
}

// TestProxyCore_PresenceSubscribe tests SUBSCRIBE handling with
// presence enabled and with invalid event packages.
func TestProxyCore_PresenceSubscribe(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:           "test.local",
		AuthRequired:    false,
		PresenceEnabled: true,
	})

	subMsg := parseMsg(t,
		"SUBSCRIBE sip:bob@test.local SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKpres1\r\n"+
			"From: <sip:alice@test.local>;tag=aliceSub1\r\n"+
			"To: <sip:bob@test.local>\r\n"+
			"Call-ID: pres1@test.local\r\n"+
			"CSeq: 1 SUBSCRIBE\r\n"+
			"Event: presence\r\n"+
			"Contact: <sip:alice@127.0.0.1:5060>\r\n"+
			"Content-Length: 0\r\n\r\n",
	)
	action := pc.ProcessRequest(subMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 200 {
		t.Errorf("SUBSCRIBE status = %d, want 200", action.Status)
	}

	badEvent := parseMsg(t,
		"SUBSCRIBE sip:bob@test.local SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKpres2\r\n"+
			"From: <sip:alice@test.local>;tag=aliceSub2\r\n"+
			"To: <sip:bob@test.local>\r\n"+
			"Call-ID: pres2@test.local\r\n"+
			"CSeq: 1 SUBSCRIBE\r\n"+
			"Event: invalid-event-package\r\n"+
			"Contact: <sip:alice@127.0.0.1:5060>\r\n"+
			"Content-Length: 0\r\n\r\n",
	)
	action = pc.ProcessRequest(badEvent, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 489 {
		t.Errorf("SUBSCRIBE with invalid event: status = %d, want 489", action.Status)
	}
}

// TestProxyCore_AddListener verifies that listener adapters can be
// attached to a ProxyCore and that LocalAddr / SendSocketInfo are
// populated.
func TestProxyCore_AddListener(t *testing.T) {
	pc := proxy.NewProxyCore(nil)

	si := &transport.SocketInfo{
		Name:     "test",
		Address:  net.ParseIP("127.0.0.1"),
		Port:     5060,
		Protocol: transport.ProtoUDP,
	}
	l := transport.NewUDPListener(si, nil)
	if err := l.ListenAndServe(); err != nil {
		t.Fatalf("ListenAndServe: %v", err)
	}
	defer l.Shutdown(context.Background())

	adapter := &proxy.UDPListenerAdapter{L: l, SI: si}
	pc.AddListener(adapter)

	if adapter.LocalAddr() == nil {
		t.Error("expected non-nil LocalAddr")
	}
	if adapter.SendSocketInfo() == nil {
		t.Error("expected non-nil SocketInfo")
	}
}

// TestProxyCore_PresenceDisabled tests that SUBSCRIBE is rejected
// with 405 when the presence feature is disabled.
func TestProxyCore_PresenceDisabled(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:           "test.local",
		AuthRequired:    false,
		PresenceEnabled: false,
	})

	subMsg := parseMsg(t,
		"SUBSCRIBE sip:bob@test.local SIP/2.0\r\n"+
			"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKpd1\r\n"+
			"From: <sip:alice@test.local>;tag=a1\r\n"+
			"To: <sip:bob@test.local>\r\n"+
			"Call-ID: pd1@test.local\r\n"+
			"CSeq: 1 SUBSCRIBE\r\n"+
			"Event: presence\r\n"+
			"Contact: <sip:alice@127.0.0.1:5060>\r\n"+
			"Content-Length: 0\r\n\r\n",
	)
	action := pc.ProcessRequest(subMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 405 {
		t.Errorf("SUBSCRIBE with presence disabled: status = %d, want 405", action.Status)
	}
}

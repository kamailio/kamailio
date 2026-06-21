// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 44 integration tests
 *
 * End-to-end tests for registrar, dialog, and TM integration into
 * the ProxyCore pipeline.
 */

package integration

import (
	"net"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/registrar"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
)

// newTestProxyCore builds a ProxyCore with registrar, dialog manager,
// and TM wired in, using the supplied realm.
func newTestProxyCore(t *testing.T, realm string, authRequired bool) (*proxy.ProxyCore, *registrar.Registrar, *dialog.Manager, *tm.Manager) {
	t.Helper()
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:               realm,
		AuthRequired:        authRequired,
		NATDetectionEnabled: false,
		PresenceEnabled:     false,
		RecordRouteEnabled:  false,
	})

	dm := dialog.NewManager()
	pc.SetDialogs(dm)

	tmMgr := tm.NewManager(1024)
	pc.SetTM(tmMgr)

	reg := registrar.New(&registrar.Config{
		Realm:          realm,
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
		AuthRequired:   authRequired,
	})
	pc.SetRegistrar(reg)

	return pc, reg, dm, tmMgr
}

// TestProxy_Register_200 sends a REGISTER and asserts that the proxy
// returns 200 OK and the registrar stores one contact for the domain.
func TestProxy_Register_200(t *testing.T) {
	realm := "test.local"
	pc, reg, _, _ := newTestProxyCore(t, realm, false)

	raw := "REGISTER sip:" + realm + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKreg1\r\n" +
		"From: <sip:alice@" + realm + ">;tag=reg1\r\n" +
		"To: <sip:alice@" + realm + ">\r\n" +
		"Call-ID: reg1@" + realm + "\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>;expires=3600\r\n" +
		"Content-Length: 0\r\n\r\n"

	msg := parseMsg(t, raw)
	action := pc.ProcessRequest(msg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 200 {
		t.Fatalf("REGISTER: expected status 200, got %d %s", action.Status, action.Reason)
	}

	// The registrar should have one contact across all domains.
	if reg.Count() != 1 {
		t.Fatalf("registrar Count = %d, want 1", reg.Count())
	}
}

// TestProxy_Register_401 enables AuthRequired on the registrar and
// sends a REGISTER without Authorization. The proxy should return 401.
func TestProxy_Register_401(t *testing.T) {
	realm := "test.local"
	pc, _, _, _ := newTestProxyCore(t, realm, true)

	raw := "REGISTER sip:" + realm + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKreg2\r\n" +
		"From: <sip:alice@" + realm + ">;tag=reg2\r\n" +
		"To: <sip:alice@" + realm + ">\r\n" +
		"Call-ID: reg2@" + realm + "\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>;expires=3600\r\n" +
		"Content-Length: 0\r\n\r\n"

	msg := parseMsg(t, raw)
	action := pc.ProcessRequest(msg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 401 {
		t.Fatalf("REGISTER without auth: expected status 401, got %d %s", action.Status, action.Reason)
	}
}

// TestProxy_INVITE_DialogCreated sends an INVITE and asserts that the
// dialog manager contains a dialog whose state is Early.
func TestProxy_INVITE_DialogCreated(t *testing.T) {
	realm := "test.local"
	pc, _, dm, _ := newTestProxyCore(t, realm, false)

	raw := "INVITE sip:bob@" + realm + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKinv1\r\n" +
		"From: <sip:alice@" + realm + ">;tag=inv1\r\n" +
		"To: <sip:bob@" + realm + ">\r\n" +
		"Call-ID: inv1@" + realm + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 0\r\n\r\n"

	msg := parseMsg(t, raw)
	action := pc.ProcessRequest(msg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 100 {
		t.Fatalf("INVITE: expected status 100, got %d %s", action.Status, action.Reason)
	}

	if dm.Count() == 0 {
		t.Fatal("dialog manager Count = 0, want > 0")
	}

	dlgs := dm.List(0)
	foundEarly := false
	for _, d := range dlgs {
		if d.State == dialog.DialogStateEarly {
			foundEarly = true
			break
		}
	}
	if !foundEarly {
		t.Fatalf("expected at least one dialog in Early state, got %v", dlgs[0].State)
	}
}

// TestProxy_INVITE_200OK_DialogConfirmed sends an INVITE, then passes
// a 200 OK reply through ProcessReply and asserts that the dialog
// state becomes Confirmed.
func TestProxy_INVITE_200OK_DialogConfirmed(t *testing.T) {
	realm := "test.local"
	pc, _, dm, _ := newTestProxyCore(t, realm, false)

	callID := "inv2@" + realm
	fromTag := "inv2"
	toTag := "toinv2"

	// 1. Send INVITE.
	inviteRaw := "INVITE sip:bob@" + realm + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKinv2\r\n" +
		"From: <sip:alice@" + realm + ">;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@" + realm + ">\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 0\r\n\r\n"

	invMsg := parseMsg(t, inviteRaw)
	pc.ProcessRequest(invMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)

	if dm.Count() == 0 {
		t.Fatal("dialog manager Count = 0 after INVITE")
	}

	// 2. Send 200 OK reply.
	replyRaw := "SIP/2.0 200 OK\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKinv2\r\n" +
		"From: <sip:alice@" + realm + ">;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@" + realm + ">;tag=" + toTag + "\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:bob@127.0.0.1:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 0\r\n\r\n"

	replyMsg := parseMsg(t, replyRaw)
	pc.ProcessReply(replyMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060})

	dlgs := dm.List(0)
	foundConfirmed := false
	for _, d := range dlgs {
		if d.State == dialog.DialogStateConfirmed {
			foundConfirmed = true
			break
		}
	}
	if !foundConfirmed {
		t.Fatalf("expected at least one dialog in Confirmed state after 200 OK")
	}
}

// TestProxy_BYE_DialogTerminated sends an INVITE followed by a BYE
// and asserts that the dialog state becomes Terminated.
func TestProxy_BYE_DialogTerminated(t *testing.T) {
	realm := "test.local"
	pc, _, dm, _ := newTestProxyCore(t, realm, false)

	callID := "bye1@" + realm
	fromTag := "bye1"
	toTag := "toterm"

	// 1. Send INVITE to create the dialog.
	// Include a To-tag so the dialog key is deterministic and matches the BYE.
	inviteRaw := "INVITE sip:bob@" + realm + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKbye1\r\n" +
		"From: <sip:alice@" + realm + ">;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@" + realm + ">;tag=" + toTag + "\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: 0\r\n\r\n"

	invMsg := parseMsg(t, inviteRaw)
	pc.ProcessRequest(invMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)

	if dm.Count() == 0 {
		t.Fatal("dialog manager Count = 0 after INVITE")
	}

	// 2. Send BYE.
	byeRaw := "BYE sip:bob@" + realm + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5060;branch=z9hG4bKbye2\r\n" +
		"From: <sip:alice@" + realm + ">;tag=" + fromTag + "\r\n" +
		"To: <sip:bob@" + realm + ">;tag=" + toTag + "\r\n" +
		"Call-ID: " + callID + "\r\n" +
		"CSeq: 2 BYE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5060>\r\n" +
		"Content-Length: 0\r\n\r\n"

	byeMsg := parseMsg(t, byeRaw)
	action := pc.ProcessRequest(byeMsg, &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 5060}, nil)
	if action.Status != 200 {
		t.Fatalf("BYE: expected status 200, got %d %s", action.Status, action.Reason)
	}

	dlgs := dm.List(0)
	foundTerminated := false
	for _, d := range dlgs {
		if d.State == dialog.DialogStateTerminated {
			foundTerminated = true
			break
		}
	}
	if !foundTerminated {
		t.Fatalf("expected at least one dialog in Terminated state after BYE")
	}
}

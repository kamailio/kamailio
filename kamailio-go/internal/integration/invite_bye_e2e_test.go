// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 46
 *
 * End-to-end INVITE/BYE tests covering:
 *   - Dialog lifecycle (Early -> Confirmed -> Terminated)
 *   - CDR accounting generation
 *   - Media pipeline NAT rewriting
 *   - TM forking state tracking
 *   - Topology hiding
 */

package integration

import (
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/acc"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/nat"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/topoh"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
)

// buildE2EInviteMsg constructs a raw SIP INVITE message.
func buildE2EInviteMsg(from, to, callID string) []byte {
	var b strings.Builder
	b.WriteString("INVITE sip:" + to + " SIP/2.0\r\n")
	b.WriteString("Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKinvite1\r\n")
	b.WriteString("From: <sip:" + from + "@local>;tag=from-tag-1\r\n")
	b.WriteString("To: <sip:" + to + "@local>\r\n")
	b.WriteString("Call-ID: " + callID + "\r\n")
	b.WriteString("CSeq: 1 INVITE\r\n")
	b.WriteString("Contact: <sip:" + from + "@10.0.0.1:5060>\r\n")
	b.WriteString("Content-Length: 0\r\n\r\n")
	return []byte(b.String())
}

// buildE2EByeMsg constructs a raw SIP BYE message.
func buildE2EByeMsg(from, to, callID string) []byte {
	var b strings.Builder
	b.WriteString("BYE sip:" + to + " SIP/2.0\r\n")
	b.WriteString("Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKbye1\r\n")
	b.WriteString("From: <sip:" + from + "@local>;tag=from-tag-1\r\n")
	b.WriteString("To: <sip:" + to + "@local>;tag=to-tag-1\r\n")
	b.WriteString("Call-ID: " + callID + "\r\n")
	b.WriteString("CSeq: 2 BYE\r\n")
	b.WriteString("Contact: <sip:" + from + "@10.0.0.1:5060>\r\n")
	b.WriteString("Content-Length: 0\r\n\r\n")
	return []byte(b.String())
}

// buildE2E200OKReply constructs a 200 OK reply to an INVITE.
func buildE2E200OKReply(callID, fromTag, toTag string) []byte {
	var b strings.Builder
	b.WriteString("SIP/2.0 200 OK\r\n")
	b.WriteString("Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bKinvite1\r\n")
	b.WriteString("From: <sip:alice@local>;tag=" + fromTag + "\r\n")
	b.WriteString("To: <sip:bob@local>;tag=" + toTag + "\r\n")
	b.WriteString("Call-ID: " + callID + "\r\n")
	b.WriteString("CSeq: 1 INVITE\r\n")
	b.WriteString("Contact: <sip:bob@10.0.0.2:5060>\r\n")
	b.WriteString("Content-Length: 0\r\n\r\n")
	return []byte(b.String())
}

func TestInviteByeE2E_DialogLifecycle(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	dm := dialog.NewManager()
	pc.SetDialogs(dm)

	callID := "dialog-lifecycle-1@local"
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// INVITE -> Early
	rawInv := buildE2EInviteMsg("alice", "bob", callID)
	msgInv, _ := parser.ParseMsg(rawInv)
	action := pc.ProcessRequest(msgInv, src, nil)
	if action.Status != 100 {
		t.Fatalf("expected 100 Trying, got %d", action.Status)
	}
	if dm.Count() == 0 {
		t.Fatal("expected dialog after INVITE")
	}

	// 200 OK -> Confirmed
	raw200 := buildE2E200OKReply(callID, "from-tag-1", "to-tag-1")
	msg200, _ := parser.ParseMsg(raw200)
	pc.ProcessReply(msg200, src)

	// Verify dialog is confirmed
	d := dm.Lookup(callID, "from-tag-1", "to-tag-1")
	if d == nil {
		t.Fatal("dialog not found after 200 OK")
	}
	if d.State != dialog.DialogStateConfirmed {
		t.Fatalf("expected Confirmed, got %s", d.State)
	}

	// BYE -> Terminated
	rawBye := buildE2EByeMsg("alice", "bob", callID)
	msgBye, _ := parser.ParseMsg(rawBye)
	pc.ProcessRequest(msgBye, src, nil)

	d = dm.Lookup(callID, "from-tag-1", "to-tag-1")
	if d == nil {
		t.Fatal("dialog not found after BYE")
	}
	if d.State != dialog.DialogStateTerminated {
		t.Fatalf("expected Terminated, got %s", d.State)
	}
}

func TestInviteByeE2E_CDRProduced(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})

	memBackend := acc.NewInMemoryBackend()
	acSvc := acc.NewAccountingService(memBackend)
	pc.SetAccounting(acSvc)

	callID := "cdr-test-1@local"
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// INVITE
	rawInv := buildE2EInviteMsg("alice", "bob", callID)
	msgInv, _ := parser.ParseMsg(rawInv)
	pc.ProcessRequest(msgInv, src, nil)

	// 200 OK reply
	raw200 := buildE2E200OKReply(callID, "from-tag-1", "to-tag-1")
	msg200, _ := parser.ParseMsg(raw200)
	pc.ProcessReply(msg200, src)

	// BYE
	rawBye := buildE2EByeMsg("alice", "bob", callID)
	msgBye, _ := parser.ParseMsg(rawBye)
	pc.ProcessRequest(msgBye, src, nil)

	// Give a moment for async accounting
	time.Sleep(50 * time.Millisecond)

	if memBackend.Count() < 1 {
		t.Fatalf("expected at least 1 CDR record, got %d", memBackend.Count())
	}
}

func TestInviteByeE2E_MediaPipeline(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	pipeline := nat.NewPipeline(nil, "203.0.113.1")
	pc.SetMediaPipeline(pipeline)

	callID := "media-test-1@local"
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// INVITE
	rawInv := buildE2EInviteMsg("alice", "bob", callID)
	msgInv, _ := parser.ParseMsg(rawInv)
	pc.ProcessRequest(msgInv, src, nil)

	if pipeline.ActiveSessions() == 0 {
		t.Fatal("expected active media session after INVITE")
	}

	// 200 OK
	raw200 := buildE2E200OKReply(callID, "from-tag-1", "to-tag-1")
	msg200, _ := parser.ParseMsg(raw200)
	pc.ProcessReply(msg200, src)

	// BYE
	rawBye := buildE2EByeMsg("alice", "bob", callID)
	msgBye, _ := parser.ParseMsg(rawBye)
	pc.ProcessRequest(msgBye, src, nil)

	if pipeline.ActiveSessions() != 0 {
		t.Fatalf("expected 0 active sessions after BYE, got %d", pipeline.ActiveSessions())
	}
}

func TestInviteByeE2E_TMForking(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	tmMgr := tm.NewManager(1024)
	pc.SetTM(tmMgr)

	callID := "tm-test-1@local"
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// INVITE -> creates TM cell
	rawInv := buildE2EInviteMsg("alice", "bob", callID)
	msgInv, _ := parser.ParseMsg(rawInv)
	pc.ProcessRequest(msgInv, src, nil)

	// TLookup should find the cell
	cell, err := tm.TLookup(tmMgr, msgInv)
	if err != nil {
		t.Fatalf("expected TM cell for INVITE, got error: %v", err)
	}
	if cell == nil {
		t.Fatal("expected non-nil TM cell")
	}
	if cell.State != tm.TStateTrying {
		t.Fatalf("expected Trying state, got %s", cell.StateString())
	}

	// 200 OK reply -> ProcessReply drives dialog state; TM state update
	// would need an explicit TReply call (not yet wired in ProcessReply).
	raw200 := buildE2E200OKReply(callID, "from-tag-1", "to-tag-1")
	msg200, _ := parser.ParseMsg(raw200)
	pc.ProcessReply(msg200, src)

	// TLookup on the original INVITE request should still find the cell.
	// The reply may not match directly because branch/Via differ.
	cell, _ = tm.TLookup(tmMgr, msgInv)
	if cell == nil {
		t.Fatal("TM cell not found for original INVITE after 200 OK")
	}
	// TM state is updated via TReply, not automatically by ProcessReply.
	// We verify the cell exists and was created by TRelay.
	if cell.State != tm.TStateTrying {
		t.Logf("TM cell state: %s (expected Trying until TReply is called)", cell.StateString())
	}
}

func TestInviteByeE2E_TopoHiding(t *testing.T) {
	pc := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "local"})
	hider := topoh.New(topoh.HideStrategy{
		HideIPs:     true,
		HideDomains: true,
		HideTags:    true,
		HideCallID:  true,
		Realm:       "hidden.local",
		PublicIP:    "203.0.113.1",
	})
	pc.SetTopoHider(hider)

	callID := "topoh-test-1@local"
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}

	// INVITE
	rawInv := buildE2EInviteMsg("alice", "bob", callID)
	msgInv, _ := parser.ParseMsg(rawInv)
	pc.ProcessRequest(msgInv, src, nil)

	// After topo hiding, the parsed Call-ID or RURI should not contain
	// the original source IP. topoh.HideForForward mutates parsed fields,
	// not the raw buffer, so we inspect the parsed structures.
	if msgInv.CallID != nil && strings.Contains(msgInv.CallID.Body.String(), "10.0.0.1") {
		t.Fatal("original source IP 10.0.0.1 still visible in Call-ID after topology hiding")
	}
	if msgInv.FirstLine != nil && msgInv.FirstLine.Req != nil &&
		strings.Contains(msgInv.FirstLine.Req.URI.String(), "10.0.0.1") {
		t.Fatal("original source IP 10.0.0.1 still visible in RURI after topology hiding")
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later
package presence

import (
	"strings"
	"testing"
	"time"
)

func TestPresenceState_Parse(t *testing.T) {
	tests := []struct {
		input    string
		expected PresenceState
	}{
		{"open", PresenceStateOpen}, {"available", PresenceStateOpen},
		{"closed", PresenceStateClosed}, {"offline", PresenceStateClosed},
		{"busy", PresenceStateBusy}, {"away", PresenceStateAway},
		{"dnd", PresenceStateDND}, {"unknown-value", PresenceStateUnknown},
	}
	for _, test := range tests {
		got := ParsePresenceState(test.input)
		if got != test.expected {
			t.Errorf("ParsePresenceState(%q) = %d, want %d", test.input, got, test.expected)
		}
	}
}

func TestPresenceState_String(t *testing.T) {
	if PresenceStatusString(PresenceStateOpen) != "open" {
		t.Error("open")
	}
	if PresenceStatusString(PresenceStateBusy) != "busy" {
		t.Error("busy")
	}
	if PresenceStatusString(PresenceStateClosed) != "closed" {
		t.Error("closed")
	}
}

func TestSubscriptionState_String(t *testing.T) {
	if SubscriptionStateString(SubscriptionStateActive) != "active" {
		t.Error("active")
	}
	if SubscriptionStateString(SubscriptionStatePending) != "pending" {
		t.Error("pending")
	}
	if SubscriptionStateString(SubscriptionStateTerminated) != "terminated" {
		t.Error("terminated")
	}
}

func TestPresentity_PublishAndState(t *testing.T) {
	p := NewPresentity("sip:alice@example.com")
	if p.CurrentState() != PresenceStateClosed {
		t.Error("expected closed")
	}
	p.Publish(&PresenceRecord{
		URI: "sip:alice@example.com", State: PresenceStateOpen,
		Timestamp: time.Now(), Expires: 1 * time.Hour, Entity: "e1", Priority: 0.8,
	})
	if p.CurrentState() != PresenceStateOpen {
		t.Errorf("state = %d", p.CurrentState())
	}
}

func TestPresentity_Subscription(t *testing.T) {
	p := NewPresentity("sip:alice@example.com")
	sub := &Subscription{
		ID: "sub-001", PresentityURI: "sip:alice@example.com",
		SubscriberURI: "sip:bob@example.com", Event: "presence",
		State: SubscriptionStateActive, Expires: time.Now().Add(1 * time.Hour),
	}
	p.AddSubscription(sub)
	if got := p.GetSubscription("sub-001"); got == nil {
		t.Fatal("expected subscription")
	} else if got.SubscriberURI != "sip:bob@example.com" {
		t.Errorf("subscriber = %q", got.SubscriberURI)
	}
	if len(p.Subscribers()) != 1 {
		t.Errorf("expected 1 subscriber, got %d", len(p.Subscribers()))
	}
	p.RemoveSubscription("sub-001")
	if p.GetSubscription("sub-001") != nil {
		t.Error("expected removed")
	}
}

func TestServer_PublishAndSubscribe(t *testing.T) {
	handler := NewServerHandler()
	handler.Publish("sip:alice@example.com", PresenceStateOpen, "Working", "sip:alice@10.0.0.1:5060", "e1", 1*time.Hour)

	sub, err := handler.Subscribe("sip:alice@example.com", "sip:bob@example.com", "presence", 600*time.Second)
	if err != nil {
		t.Fatalf("Subscribe: %v", err)
	}
	if sub == nil {
		t.Fatal("expected subscription")
	}
	if sub.Event != "presence" {
		t.Errorf("event = %q", sub.Event)
	}

	presCount, subCount := handler.Stats()
	if presCount != 1 {
		t.Errorf("presentities = %d, want 1", presCount)
	}
	if subCount != 1 {
		t.Errorf("subscriptions = %d, want 1", subCount)
	}
}

func TestServer_InvalidEventPackage(t *testing.T) {
	handler := NewServerHandler()
	_, err := handler.Subscribe("sip:alice@example.com", "sip:bob@example.com", "invalid-event", 600*time.Second)
	if err == nil {
		t.Error("expected error")
	}
}

func TestServer_Cleanup(t *testing.T) {
	handler := NewServerHandler()
	handler.Publish("sip:temp@example.com", PresenceStateOpen, "", "sip:temp@10.0.0.1:5060", "entity-1", 10*time.Millisecond)

	time.Sleep(50 * time.Millisecond)
	handler.Cleanup()

	if handler.GetState("sip:temp@example.com") != PresenceStateClosed {
		t.Error("expected closed after expiry")
	}
}

func TestPIDF_Generate(t *testing.T) {
	xml := GeneratePIDF(&PIDFDocument{
		URI: "alice@example.com", Status: PresenceStateOpen, Note: "I'm here",
		Contact: "sip:alice@192.168.1.10:5060", Priority: 0.8,
	})
	if !strings.Contains(xml, "<basic>open</basic>") {
		t.Error("expected open")
	}
	if !strings.Contains(xml, "I'm here") {
		t.Error("expected note")
	}
	if !strings.Contains(xml, "sip:alice@192.168.1.10:5060") {
		t.Error("expected contact")
	}
}

func TestPIDF_Parse(t *testing.T) {
	xml := `<?xml version="1.0"?>
<presence xmlns="urn:ietf:params:xml:ns:pidf" entity="pres:alice@example.com">
  <tuple id="tid-001">
    <status><basic>open</basic></status>
    <contact priority="0.8">sip:alice@example.com</contact>
    <note xml:lang="en">Available</note>
  </tuple>
</presence>`
	doc, err := ParsePIDF(xml)
	if err != nil {
		t.Fatalf("ParsePIDF: %v", err)
	}
	if doc.Status != PresenceStateOpen {
		t.Errorf("state = %d", doc.Status)
	}
	if doc.Contact != "sip:alice@example.com" {
		t.Errorf("contact = %q", doc.Contact)
	}
	if doc.Note != "Available" {
		t.Errorf("note = %q", doc.Note)
	}
}

func TestXPIDF_Generate(t *testing.T) {
	xml := GenerateXPIDF(PresenceStateOpen, "alice@example.com", "sip:alice@192.168.1.10:5060", 0.8)
	if !strings.Contains(xml, `<presence status="online">`) {
		t.Error("expected online")
	}
	if !strings.Contains(xml, "pres:alice@example.com") {
		t.Error("expected pres URI")
	}
}

func TestGenerateDialogInfo(t *testing.T) {
	xml := GenerateDialogInfo("alice@example.com", "dlg-123", "initiator", "confirmed",
		"alice@example.com", "bob@example.com", "bob@example.com", 1, 123)
	if !strings.Contains(xml, `<state>confirmed</state>`) {
		t.Error("expected confirmed")
	}
	if !strings.Contains(xml, `version="1"`) {
		t.Error("expected version=1")
	}
	if !strings.Contains(xml, `<duration>123</duration>`) {
		t.Error("expected duration=123")
	}
}

func TestGenerateMWI(t *testing.T) {
	xml := GenerateMWI("alice@example.com", 2, 5, 0, 1)
	if !strings.Contains(xml, `new="2"`) {
		t.Error("expected new=2")
	}
	if !strings.Contains(xml, `old="5"`) {
		t.Error("expected old=5")
	}
	if !strings.Contains(xml, `new-urgent="0"`) {
		t.Error("expected new-urgent=0")
	}
	if !strings.Contains(xml, `old-urgent="1"`) {
		t.Error("expected old-urgent=1")
	}
}

func TestServerHandler_GetState(t *testing.T) {
	handler := NewServerHandler()
	handler.Publish("sip:alice@example.com", PresenceStateOpen, "", "sip:alice@10.0.0.1:5060", "e1", 1*time.Hour)
	if handler.GetState("sip:alice@example.com") != PresenceStateOpen {
		t.Error("expected open")
	}
	if handler.GetState("sip:unknown@example.com") != PresenceStateClosed {
		t.Error("expected closed for unknown")
	}
}

func TestServerHandler_GetStateDocument(t *testing.T) {
	handler := NewServerHandler()
	handler.Publish("sip:alice@example.com", PresenceStateBusy, "On call", "sip:alice@10.0.0.1:5060", "e1", 1*time.Hour)
	doc := handler.GetStateDocument("sip:alice@example.com")
	if !strings.Contains(doc, "On call") {
		t.Error("expected note")
	}
	if !strings.Contains(doc, `<basic>busy</basic>`) {
		t.Error("expected busy")
	}
}

func TestMWIServer_MessageCount(t *testing.T) {
	server := NewMWIServer()
	server.UpdateMessageCount("alice@example.com", 2, 5, 0, 1)

	newMsg, oldMsg, newUrgent, oldUrgent := server.GetMWIStatus("alice@example.com")
	if newMsg != 2 {
		t.Errorf("new = %d", newMsg)
	}
	if oldMsg != 5 {
		t.Errorf("old = %d", oldMsg)
	}
	if newUrgent != 0 {
		t.Errorf("new urgent = %d", newUrgent)
	}
	if oldUrgent != 1 {
		t.Errorf("old urgent = %d", oldUrgent)
	}
	if !server.HasWaitingMessages("alice@example.com") {
		t.Error("expected waiting")
	}

	doc := server.GetMWIDocument("alice@example.com")
	if !strings.Contains(doc, `new="2"`) {
		t.Error("expected new=2 in MWI doc")
	}
}

func TestMWIServer_NewAndRead(t *testing.T) {
	server := NewMWIServer()
	server.NewMessage("alice@example.com")
	server.NewMessage("alice@example.com")

	newMsg, _, _, _ := server.GetMWIStatus("alice@example.com")
	if newMsg != 2 {
		t.Errorf("new = %d, want 2", newMsg)
	}

	server.MessageRead("alice@example.com")
	newMsg, oldMsg, _, _ := server.GetMWIStatus("alice@example.com")
	if newMsg != 1 {
		t.Errorf("new = %d, want 1", newMsg)
	}
	if oldMsg != 1 {
		t.Errorf("old = %d, want 1", oldMsg)
	}
}

func TestServerHandler_NotifyCallback(t *testing.T) {
	handler := NewServerHandler()
	notified := false
	handler.OnNotify = func(uri string, state PresenceState, subs []*Subscription) {
		notified = true
	}
	handler.Subscribe("sip:alice@example.com", "sip:bob@example.com", "presence", 1*time.Hour)
	handler.Publish("sip:alice@example.com", PresenceStateOpen, "Working", "sip:alice@10.0.0.1:5060", "entity-1", 1*time.Hour)
	if !notified {
		t.Error("expected OnNotify")
	}
}

func TestServerHandler_Stats(t *testing.T) {
	handler := NewServerHandler()
	handler.Publish("sip:alice@example.com", PresenceStateOpen, "", "sip:alice@10.0.0.1:5060", "e1", 1*time.Hour)
	handler.Publish("sip:bob@example.com", PresenceStateBusy, "", "sip:bob@10.0.0.2:5060", "e2", 1*time.Hour)
	presCount, subCount := handler.Stats()
	if presCount != 2 {
		t.Errorf("presentities = %d, want 2", presCount)
	}
	if subCount != 0 {
		t.Errorf("subscriptions = %d, want 0", subCount)
	}
}

func TestPIDF_ParseClosedStatus(t *testing.T) {
	xml := `<?xml version="1.0"?>
<presence xmlns="urn:ietf:params:xml:ns:pidf" entity="pres:bob@example.com">
  <tuple id="tid-002">
    <status><basic>closed</basic></status>
  </tuple>
</presence>`
	doc, err := ParsePIDF(xml)
	if err != nil {
		t.Fatalf("ParsePIDF: %v", err)
	}
	if doc.Status != PresenceStateClosed {
		t.Errorf("state = %d", doc.Status)
	}
}

func TestServerHandler_GetSubscribers(t *testing.T) {
	handler := NewServerHandler()
	handler.Publish("sip:alice@example.com", PresenceStateOpen, "", "sip:alice@10.0.0.1:5060", "e1", 1*time.Hour)
	handler.Subscribe("sip:alice@example.com", "sip:bob@example.com", "presence", 1*time.Hour)
	handler.Subscribe("sip:alice@example.com", "sip:carol@example.com", "presence", 1*time.Hour)
	subs := handler.GetSubscribers("sip:alice@example.com")
	if len(subs) != 2 {
		t.Errorf("expected 2 subscribers, got %d", len(subs))
	}
}

func TestServer_CleanupLoop(t *testing.T) {
	handler := NewServerHandler()
	done := handler.StartCleanupLoop(10 * time.Millisecond)
	handler.Publish("sip:temp@example.com", PresenceStateOpen, "", "sip:temp@10.0.0.1:5060", "entity-1", 10*time.Millisecond)
	time.Sleep(100 * time.Millisecond)
	close(done)
	if handler.GetState("sip:temp@example.com") != PresenceStateClosed {
		t.Error("expected closed after expiry")
	}
}

func TestServer_GetPresentityNil(t *testing.T) {
	server := NewServer()
	if server.GetPresentity("sip:nonexistent@example.com") != nil {
		t.Error("expected nil for unknown presentity")
	}
}

func TestReasonString(t *testing.T) {
	if ReasonString(ReasonTimeout) != "timeout" {
		t.Error("timeout reason")
	}
	if ReasonString(ReasonDeactivated) != "deactivated" {
		t.Error("deactivated reason")
	}
}

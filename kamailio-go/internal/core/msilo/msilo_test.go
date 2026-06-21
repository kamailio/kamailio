// SPDX-License-Identifier: GPL-2.0-or-later
package msilo

import (
	"sync"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

func msgMessage(toUser, fromUser, body string) *parser.SIPMsg {
	raw := "MESSAGE sip:" + toUser + "@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-msg\r\n" +
		"From: <sip:" + fromUser + "@example.com>;tag=ft-msg\r\n" +
		"To: <sip:" + toUser + "@example.com>\r\n" +
		"Call-ID: msg-" + toUser + "-" + fromUser + "\r\n" +
		"CSeq: 1 MESSAGE\r\n" +
		"Content-Type: text/plain\r\n" +
		"Content-Length: " + itoa(len(body)) + "\r\n" +
		"\r\n" + body
	m, err := parser.ParseMsg([]byte(raw))
	if err != nil {
		return nil
	}
	return m
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	buf := [20]byte{}
	pos := len(buf)
	for n > 0 {
		pos--
		buf[pos] = byte('0' + n%10)
		n /= 10
	}
	return string(buf[pos:])
}

// Test 1: Store, query, deliver.
func TestMsilo_StoreAndDeliver(t *testing.T) {
	m := New(nil, "")
	msg := msgMessage("bob", "alice", "Hello Bob!")
	if msg == nil {
		t.Fatal("failed to parse message")
	}
	if _, err := m.Store(msg); err != nil {
		t.Fatalf("Store: %v", err)
	}
	if m.QueueLength("bob") != 1 {
		t.Errorf("expected queue length 1, got %d", m.QueueLength("bob"))
	}
	if m.QueueLength("unknown") != 0 {
		t.Errorf("expected queue length 0 for unknown user, got %d", m.QueueLength("unknown"))
	}
	if m.TotalQueued() != 1 {
		t.Errorf("expected total 1, got %d", m.TotalQueued())
	}

	delivered := m.DeliverFor("bob", 10)
	if len(delivered) != 1 {
		t.Fatalf("expected 1 delivered, got %d", len(delivered))
	}
	if delivered[0].To != "bob" {
		t.Errorf("expected To=Bob, got %q", delivered[0].To)
	}
	if delivered[0].Tries != 1 {
		t.Errorf("expected Tries=1, got %d", delivered[0].Tries)
	}
	if m.QueueLength("bob") != 0 {
		t.Errorf("expected 0 after deliver, got %d", m.QueueLength("bob"))
	}
}

// Test 2: DeliverFor on empty user returns nil.
func TestMsilo_Deliver_EmptyUser(t *testing.T) {
	m := New(nil, "")
	if m.DeliverFor("nobody", 10) != nil {
		t.Error("expected nil for empty user")
	}
	if m.DeliverFor("", 10) != nil {
		t.Error("expected nil for empty string user")
	}
	if m.TotalQueued() != 0 {
		t.Errorf("expected 0 total, got %d", m.TotalQueued())
	}
}

// Test 3: Concurrent Store/DeliverFor/QueueLength.
func TestMsilo_Concurrent(t *testing.T) {
	m := New(nil, "")
	const producers = 4
	const consumers = 2
	const perProducer = 50

	var wg sync.WaitGroup
	wg.Add(producers + consumers)
	for p := 0; p < producers; p++ {
		go func(p int) {
			defer wg.Done()
			for i := 0; i < perProducer; i++ {
				m.Store(msgMessage("user"+itoa(p%2), "alice", "hi"))
			}
		}(p)
	}
	for c := 0; c < consumers; c++ {
		go func(c int) {
			defer wg.Done()
			for i := 0; i < perProducer; i++ {
				m.DeliverFor("user"+itoa(c%2), 1)
				_ = m.QueueLength("user" + itoa(c%2))
				_ = m.TotalQueued()
			}
		}(c)
	}
	wg.Wait()
	// After draining, it should still function.
	m.Store(msgMessage("final", "alice", "bye"))
	if m.TotalQueued() < 1 {
		t.Errorf("expected at least 1 total, got %d", m.TotalQueued())
	}
}

// Test 4: No DB provided; all in-memory operations work.
func TestMsilo_InMemoryOnly_NoDB(t *testing.T) {
	m := New(nil, "msilo")
	for i := 0; i < 5; i++ {
		if _, err := m.Store(msgMessage("carol", "dan", "m"+itoa(i))); err != nil {
			t.Fatalf("Store: %v", err)
		}
	}
	if m.TotalQueued() != 5 {
		t.Errorf("expected 5 total, got %d", m.TotalQueued())
	}
	out := m.DeliverFor("carol", 3)
	if len(out) != 3 {
		t.Fatalf("expected 3 delivered, got %d", len(out))
	}
	remain := m.DeliverFor("carol", 10)
	if len(remain) != 2 {
		t.Fatalf("expected 2 remaining, got %d", len(remain))
	}
	if m.TotalQueued() != 0 {
		t.Errorf("expected 0 after full delivery, got %d", m.TotalQueued())
	}
}

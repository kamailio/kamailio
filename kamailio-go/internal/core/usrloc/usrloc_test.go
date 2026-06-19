// SPDX-License-Identifier: GPL-2.0-or-later
package usrloc

import (
	"testing"
	"time"
)

func TestDomain_AddContact(t *testing.T) {
	d := NewDomain("example.com")

	contact := &Contact{
		AOR:     "sip:alice@example.com",
		URI:     "sip:alice@192.168.1.10:5060",
		Expires: time.Now().Add(3600 * time.Second),
		Q:       1.0,
		CSeq:    1,
	}

	added, isNew := d.AddContact("sip:alice@example.com", contact)
	if !isNew {
		t.Error("expected new contact")
	}
	if added.URI != "sip:alice@192.168.1.10:5060" {
		t.Errorf("URI = %q", added.URI)
	}

	// Verify lookup
	contacts := d.Lookup("sip:alice@example.com")
	if len(contacts) != 1 {
		t.Fatalf("expected 1 contact, got %d", len(contacts))
	}
	if contacts[0].Q != 1.0 {
		t.Errorf("Q = %f, want 1.0", contacts[0].Q)
	}
}

func TestDomain_UpdateContact(t *testing.T) {
	d := NewDomain("example.com")

	c1 := &Contact{
		AOR:     "sip:alice@example.com",
		URI:     "sip:alice@192.168.1.10:5060",
		Expires: time.Now().Add(3600 * time.Second),
		Q:       0.5,
		CSeq:    1,
	}
	d.AddContact("sip:alice@example.com", c1)

	// Update with same URI, different Q
	c2 := &Contact{
		AOR:     "sip:alice@example.com",
		URI:     "sip:alice@192.168.1.10:5060",
		Expires: time.Now().Add(7200 * time.Second),
		Q:       1.0,
		CSeq:    2,
	}
	_, isNew := d.AddContact("sip:alice@example.com", c2)
	if isNew {
		t.Error("expected existing contact to be updated, not new")
	}

	contacts := d.Lookup("sip:alice@example.com")
	if len(contacts) != 1 {
		t.Fatalf("expected 1 contact after update, got %d", len(contacts))
	}
	if contacts[0].Q != 1.0 {
		t.Errorf("Q = %f after update, want 1.0", contacts[0].Q)
	}
	if contacts[0].CSeq != 2 {
		t.Errorf("CSeq = %d after update, want 2", contacts[0].CSeq)
	}
}

func TestDomain_QValueSorting(t *testing.T) {
	d := NewDomain("example.com")

	d.AddContact("sip:bob@example.com", &Contact{
		URI: "sip:bob@10.0.0.1", Q: 0.3, Expires: time.Now().Add(3600 * time.Second),
	})
	d.AddContact("sip:bob@example.com", &Contact{
		URI: "sip:bob@10.0.0.2", Q: 0.9, Expires: time.Now().Add(3600 * time.Second),
	})
	d.AddContact("sip:bob@example.com", &Contact{
		URI: "sip:bob@10.0.0.3", Q: 0.5, Expires: time.Now().Add(3600 * time.Second),
	})

	contacts := d.Lookup("sip:bob@example.com")
	if len(contacts) != 3 {
		t.Fatalf("expected 3 contacts, got %d", len(contacts))
	}
	if contacts[0].Q < contacts[1].Q {
		t.Error("contacts should be sorted by Q descending")
	}
	if contacts[0].URI != "sip:bob@10.0.0.2" {
		t.Errorf("first contact should have highest Q, got %s", contacts[0].URI)
	}
}

func TestDomain_RemoveContact(t *testing.T) {
	d := NewDomain("example.com")

	d.AddContact("sip:alice@example.com", &Contact{
		URI: "sip:alice@10.0.0.1", Expires: time.Now().Add(3600 * time.Second),
	})
	d.AddContact("sip:alice@example.com", &Contact{
		URI: "sip:alice@10.0.0.2", Expires: time.Now().Add(3600 * time.Second),
	})

	removed := d.RemoveContact("sip:alice@example.com", "sip:alice@10.0.0.1")
	if !removed {
		t.Error("expected contact to be removed")
	}

	contacts := d.Lookup("sip:alice@example.com")
	if len(contacts) != 1 {
		t.Errorf("expected 1 contact after removal, got %d", len(contacts))
	}
}

func TestDomain_PurgeExpired(t *testing.T) {
	d := NewDomain("example.com")

	d.AddContact("sip:alice@example.com", &Contact{
		URI: "sip:alice@10.0.0.1", Expires: time.Now().Add(50 * time.Millisecond),
	})
	d.AddContact("sip:alice@example.com", &Contact{
		URI: "sip:alice@10.0.0.2", Expires: time.Now().Add(3600 * time.Second),
	})

	time.Sleep(100 * time.Millisecond)
	purged := d.PurgeExpired()
	if purged != 1 {
		t.Errorf("expected 1 purged contact, got %d", purged)
	}

	contacts := d.Lookup("sip:alice@example.com")
	if len(contacts) != 1 {
		t.Errorf("expected 1 remaining contact, got %d", len(contacts))
	}
}

func TestRegistrar_RegisterAndQuery(t *testing.T) {
	reg := NewRegistrar()
	reg.SetDefaultExpires(3600 * time.Second)

	contacts, err := reg.Register("example.com", "sip:alice@example.com", []*Contact{
		{
			URI: "sip:alice@192.168.1.10:5060",
			Q:   1.0,
			CSeq: 1,
		},
	})
	if err != nil {
		t.Fatalf("Register failed: %v", err)
	}
	if len(contacts) != 1 {
		t.Fatalf("expected 1 contact, got %d", len(contacts))
	}

	// Query
	found := reg.Query("example.com", "sip:alice@example.com")
	if len(found) != 1 {
		t.Fatalf("expected 1 contact from query, got %d", len(found))
	}
	if found[0].URI != "sip:alice@192.168.1.10:5060" {
		t.Errorf("URI = %q", found[0].URI)
	}
}

func TestRegistrar_MultipleContacts(t *testing.T) {
	reg := NewRegistrar()

	reg.Register("example.com", "sip:bob@example.com", []*Contact{
		{URI: "sip:bob@10.0.0.1", Q: 0.5, CSeq: 1},
		{URI: "sip:bob@10.0.0.2", Q: 0.9, CSeq: 1},
	})

	found := reg.Query("example.com", "sip:bob@example.com")
	if len(found) != 2 {
		t.Fatalf("expected 2 contacts, got %d", len(found))
	}
	// Should be sorted by Q descending
	if found[0].Q < found[1].Q {
		t.Error("contacts should be sorted by Q descending")
	}
}

func TestRegistrar_Stats(t *testing.T) {
	reg := NewRegistrar()
	reg.Register("example.com", "sip:alice@example.com", []*Contact{
		{URI: "sip:alice@10.0.0.1", CSeq: 1},
	})
	reg.Register("other.com", "sip:bob@other.com", []*Contact{
		{URI: "sip:bob@10.0.0.1", CSeq: 1},
	})

	stats := reg.Stats()
	if stats["example.com"] != 1 {
		t.Errorf("example.com AOR count = %d, want 1", stats["example.com"])
	}
	if stats["other.com"] != 1 {
		t.Errorf("other.com AOR count = %d, want 1", stats["other.com"])
	}
}

func TestContact_IsExpired(t *testing.T) {
	c := &Contact{Expires: time.Now().Add(-1 * time.Second)}
	if !c.IsExpired() {
		t.Error("contact should be expired")
	}

	c2 := &Contact{Expires: time.Now().Add(3600 * time.Second)}
	if c2.IsExpired() {
		t.Error("contact should not be expired")
	}

	// Zero expiry = never expires
	c3 := &Contact{Expires: time.Time{}}
	if c3.IsExpired() {
		t.Error("contact with zero expiry should not be expired")
	}
}

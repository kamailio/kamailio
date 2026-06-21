// SPDX-License-Identifier: GPL-2.0-or-later

package htable

import (
	"testing"
	"time"
)

// TestTable_SetGet verifies that a Set followed by a Get returns the value.
func TestTable_SetGet(t *testing.T) {
	tbl := New("users", 0)
	tbl.Set("alice", "sip:alice@example.com", 0)
	v, ok := tbl.Get("alice")
	if !ok {
		t.Fatal("expected alice to be present")
	}
	if v != "sip:alice@example.com" {
		t.Errorf("expected 'sip:alice@example.com', got %q", v)
	}
	if tbl.Size() != 1 {
		t.Errorf("expected size 1, got %d", tbl.Size())
	}
}

// TestTable_Expiry verifies that entries disappear after their TTL has
// elapsed.
func TestTable_Expiry(t *testing.T) {
	tbl := New("sessions", 0)
	tbl.Set("session-1", "active", 50*time.Millisecond)
	v, ok := tbl.Get("session-1")
	if !ok || v != "active" {
		t.Fatalf("expected session-1 to be present with value 'active', got (%q,%v)", v, ok)
	}
	time.Sleep(80 * time.Millisecond)
	_, ok = tbl.Get("session-1")
	if ok {
		t.Error("expected session-1 to be absent after TTL elapsed")
	}
}

// TestTable_Inc_Numeric verifies that Inc increments a numeric stored value
// correctly, treating missing/non-numeric as 0.
func TestTable_Inc_Numeric(t *testing.T) {
	tbl := New("counters", 0)
	tbl.Set("n", "5", 0)
	got := tbl.Inc("n", 10)
	if got != 15 {
		t.Errorf("expected Inc to return 15, got %d", got)
	}
	v, ok := tbl.Get("n")
	if !ok || v != "15" {
		t.Errorf("expected stored value '15', got (%q,%v)", v, ok)
	}

	// missing key behaves as 0.
	if v := tbl.Inc("missing", 3); v != 3 {
		t.Errorf("expected Inc on missing key to return 3, got %d", v)
	}
}

// TestManager_GetMissingNil verifies that Get on an unregistered name returns
// nil, whereas a previously created table is returned.
func TestManager_GetMissingNil(t *testing.T) {
	mgr := NewManager()
	if mgr.Get("unknown") != nil {
		t.Error("expected nil for missing table")
	}
	created := mgr.Create("stats", 0)
	if got := mgr.Get("stats"); got != created {
		t.Error("expected Get to return the previously created table")
	}
	if created.Name() != "stats" {
		t.Errorf("expected table name 'stats', got %q", created.Name())
	}
}

// TestTable_CleanupExpired verifies that a janitor sweep removes expired
// entries and leaves non-expiring entries alone.
func TestTable_CleanupExpired(t *testing.T) {
	tbl := New("mixed", 0)
	tbl.Set("expired-1", "old", 10*time.Millisecond)
	tbl.Set("expired-2", "older", 10*time.Millisecond)
	tbl.Set("permanent", "never", 0)
	time.Sleep(40 * time.Millisecond)
	removed := tbl.CleanupExpired()
	if removed != 2 {
		t.Errorf("expected to remove 2 expired entries, got %d", removed)
	}
	if tbl.Size() != 1 {
		t.Errorf("expected size 1 after cleanup, got %d", tbl.Size())
	}
	if _, ok := tbl.Get("permanent"); !ok {
		t.Error("expected permanent entry to remain")
	}
}

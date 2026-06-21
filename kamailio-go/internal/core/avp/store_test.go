// SPDX-License-Identifier: GPL-2.0-or-later

package avp

import "testing"

// TestStore_AddString_First verifies that a string AVP added via AddString is
// returned by First with the correct Kind and value.
func TestStore_AddString_First(t *testing.T) {
	s := NewStore()
	s.AddString("ruri", "sip:bob@example.com")
	v, ok := s.First("ruri")
	if !ok {
		t.Fatal("expected ruri to be set")
	}
	if v.Kind != KindString {
		t.Errorf("expected KindString, got %d", v.Kind)
	}
	if v.S != "sip:bob@example.com" {
		t.Errorf("expected 'sip:bob@example.com', got %q", v.S)
	}
}

// TestStore_MultiValues_All verifies that multiple AVPs with the same name are
// stored and returned in FIFO order.
func TestStore_MultiValues_All(t *testing.T) {
	s := NewStore()
	s.AddString("via", "first")
	s.AddString("via", "second")
	s.AddString("via", "third")
	vs := s.All("via")
	if len(vs) != 3 {
		t.Fatalf("expected 3 via values, got %d", len(vs))
	}
	if vs[0].S != "first" || vs[1].S != "second" || vs[2].S != "third" {
		t.Errorf("unexpected FIFO order: %+v", vs)
	}
	first, ok := s.First("via")
	if !ok || first.S != "first" {
		t.Errorf("expected first value via First, got %+v (ok=%v)", first, ok)
	}
}

// TestStore_AddIntAndStringMixed verifies that integer and string AVPs live
// side by side without cross-contamination.
func TestStore_AddIntAndStringMixed(t *testing.T) {
	s := NewStore()
	s.AddInt("status", 407)
	s.AddString("status", "proxy-auth")
	s.AddInt("seq", 1)
	values := s.All("status")
	if len(values) != 2 {
		t.Fatalf("expected 2 status values, got %d", len(values))
	}
	if values[0].Kind != KindInt || values[0].I != 407 {
		t.Errorf("expected int 407 as first status, got %+v", values[0])
	}
	if values[1].Kind != KindString || values[1].S != "proxy-auth" {
		t.Errorf("expected string 'proxy-auth' as second status, got %+v", values[1])
	}
	if v, ok := s.First("seq"); !ok || v.Kind != KindInt || v.I != 1 {
		t.Errorf("expected seq=1, got %+v (ok=%v)", v, ok)
	}
}

// TestStore_Has_Del verifies Has returns false for unset AVPs, true once
// added, and false again after Del.
func TestStore_Has_Del(t *testing.T) {
	s := NewStore()
	if s.Has("tag") {
		t.Error("Has returned true for unset AVP")
	}
	s.AddString("tag", "abc-123")
	if !s.Has("tag") {
		t.Error("Has returned false after AddString")
	}
	s.Del("tag")
	if s.Has("tag") {
		t.Error("Has returned true after Del")
	}
}

// TestStore_SizeAndNilSafety ensures Size accumulates across names and that
// calling methods on a nil receiver is safe.
func TestStore_SizeAndNilSafety(t *testing.T) {
	s := NewStore()
	s.AddString("a", "x")
	s.AddString("a", "y")
	s.AddInt("b", 7)
	if s.Size() != 3 {
		t.Errorf("expected Size 3, got %d", s.Size())
	}

	var ns *Store
	ns.AddString("x", "y")        // must not panic
	ns.AddInt("x", 1)             // must not panic
	if _, ok := ns.First("x"); ok {
		t.Error("First on nil store should return false")
	}
}

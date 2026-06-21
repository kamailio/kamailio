// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for the DNS SRV helpers. These tests avoid real network activity
 * and only exercise the deterministic sorting / host resolution logic.
 */

package dns

import (
	"testing"
)

// Test 1 — SortByRFC2782 with different priorities orders low-priority first.
func TestSortByRFC2782_PriorityOnly(t *testing.T) {
	in := []SRVTarget{
		{Target: "high.example.com", Port: 5060, Priority: 20, Weight: 10},
		{Target: "low.example.com", Port: 5060, Priority: 5, Weight: 10},
		{Target: "mid.example.com", Port: 5060, Priority: 10, Weight: 10},
	}
	out := SortByRFC2782(in)
	if len(out) != 3 {
		t.Fatalf("expected 3 targets, got %d", len(out))
	}
	if out[0].Priority >= out[1].Priority || out[1].Priority >= out[2].Priority {
		t.Fatalf("expected ascending priority, got %v %v %v",
			out[0].Priority, out[1].Priority, out[2].Priority)
	}
}

// Test 2 — within a single priority, the weighted shuffle returns the full
// set (non-empty, non-duplicate targets). Since the ordering is deterministic
// we just verify count + uniqueness.
func TestSortByRFC2782_WeightWithinPriority(t *testing.T) {
	in := []SRVTarget{
		{Target: "a.example.com", Port: 5060, Priority: 10, Weight: 100},
		{Target: "b.example.com", Port: 5060, Priority: 10, Weight: 50},
		{Target: "c.example.com", Port: 5060, Priority: 10, Weight: 10},
	}
	out := SortByRFC2782(in)
	if len(out) != 3 {
		t.Fatalf("expected 3 targets, got %d", len(out))
	}
	seen := map[string]bool{}
	for _, tg := range out {
		if seen[tg.Target] {
			t.Fatalf("duplicate target %q in output", tg.Target)
		}
		seen[tg.Target] = true
	}
	// Two consecutive calls should yield the same deterministic order.
	out2 := SortByRFC2782(in)
	for i := range out {
		if out[i].Target != out2[i].Target {
			t.Fatalf("non-deterministic order at index %d: %q vs %q",
				i, out[i].Target, out2[i].Target)
		}
	}
}

// Test 3 — nil / empty input produces nil output.
func TestSortByRFC2782_Empty(t *testing.T) {
	if SortByRFC2782(nil) != nil {
		t.Fatalf("expected nil for nil input")
	}
	if SortByRFC2782([]SRVTarget{}) != nil {
		t.Fatalf("expected nil for empty input")
	}
}

// Test 4 — ResolveSIPHost with explicit host:port returns it directly.
func TestResolveSIPHost_IPPort(t *testing.T) {
	r := NewSRVResolver()
	out := r.ResolveSIPHost("192.0.2.1:5060")
	if len(out) != 1 {
		t.Fatalf("expected 1 target, got %d", len(out))
	}
	if out[0] != "192.0.2.1:5060" {
		t.Fatalf("expected 192.0.2.1:5060, got %q", out[0])
	}

	out2 := r.ResolveSIPHost("sip.example.com:5070")
	if len(out2) != 1 || out2[0] != "sip.example.com:5070" {
		t.Fatalf("expected sip.example.com:5070, got %v", out2)
	}
}

// Test 5 — ResolveSIPHost with a plain domain (no SRV reachable in tests)
// should fall back to host:5060.
func TestResolveSIPHost_DomainFallback(t *testing.T) {
	r := NewSRVResolver()
	// "example.invalid" is reserved by RFC 2606 and will not resolve SRV.
	out := r.ResolveSIPHost("example.invalid")
	if len(out) != 1 {
		t.Fatalf("expected 1 fallback target, got %d: %v", len(out), out)
	}
	if out[0] != "example.invalid:5060" {
		t.Fatalf("expected example.invalid:5060 fallback, got %q", out[0])
	}

	// bare hostname
	out2 := r.ResolveSIPHost("myhost.local")
	if len(out2) != 1 || out2[0] != "myhost.local:5060" {
		t.Fatalf("expected myhost.local:5060 fallback, got %v", out2)
	}
}

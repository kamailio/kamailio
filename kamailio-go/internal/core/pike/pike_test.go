// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go — Pike unit tests.
 */

package pike

import (
	"sync"
	"testing"
	"time"
)

// TestPike_Hit_UnderLimit verifies that a modest number of hits all
// report "allowed" and that remaining/reset values are sensible.
func TestPike_Hit_UnderLimit(t *testing.T) {
	p := New(5, 5*time.Second)
	defer p.Close()

	for i := 0; i < 5; i++ {
		allowed, remaining, resetIn := p.Hit("10.0.0.1")
		if !allowed {
			t.Fatalf("hit %d: expected allowed=true, got false", i)
		}
		if resetIn <= 0 {
			t.Fatalf("hit %d: expected positive resetIn, got %v", i, resetIn)
		}
		if remaining < 0 {
			t.Fatalf("hit %d: expected remaining >= 0, got %d", i, remaining)
		}
	}
	if got := p.Count("10.0.0.1"); got != 5 {
		t.Fatalf("expected count=5, got %d", got)
	}
}

// TestPike_Hit_OverLimit verifies that exceeding the limit yields
// allowed=false for every subsequent hit in the window.
func TestPike_Hit_OverLimit(t *testing.T) {
	p := New(3, 5*time.Second)
	defer p.Close()

	for i := 0; i < 3; i++ {
		allowed, _, _ := p.Hit("10.0.0.2")
		if !allowed {
			t.Fatalf("hit %d: expected allowed=true before limit", i)
		}
	}
	for i := 0; i < 5; i++ {
		allowed, remaining, _ := p.Hit("10.0.0.2")
		if allowed {
			t.Fatalf("overflow hit %d: expected allowed=false", i)
		}
		if remaining != 0 {
			t.Fatalf("overflow hit %d: expected remaining=0, got %d", i, remaining)
		}
	}
}

// TestPike_Hit_EmptyIP_NoOp verifies that an empty IP does not
// record any hits and never blocks.
func TestPike_Hit_EmptyIP_NoOp(t *testing.T) {
	p := New(2, 5*time.Second)
	defer p.Close()

	for i := 0; i < 100; i++ {
		allowed, _, _ := p.Hit("")
		if !allowed {
			t.Fatalf("empty IP should never be blocked")
		}
	}
	if got := p.Count(""); got != 0 {
		t.Fatalf("expected empty-IP count=0, got %d", got)
	}
}

// TestPike_CountAndClear verifies that Clear removes all state.
func TestPike_CountAndClear(t *testing.T) {
	p := New(10, 5*time.Second)
	defer p.Close()

	for i := 0; i < 5; i++ {
		p.Hit("10.0.0.3")
	}
	if got := p.Count("10.0.0.3"); got != 5 {
		t.Fatalf("expected count=5, got %d", got)
	}
	p.Clear("10.0.0.3")
	if got := p.Count("10.0.0.3"); got != 0 {
		t.Fatalf("expected count=0 after Clear, got %d", got)
	}
}

// TestPike_ConcurrentHits verifies that concurrent Hit calls do not
// panic and that the final count is the sum of all hits.
func TestPike_ConcurrentHits(t *testing.T) {
	p := New(100000, 10*time.Second)
	defer p.Close()

	const goroutines = 20
	const perG = 500

	var wg sync.WaitGroup
	wg.Add(goroutines)
	for g := 0; g < goroutines; g++ {
		go func(ip string) {
			defer wg.Done()
			for i := 0; i < perG; i++ {
				_, _, _ = p.Hit(ip)
			}
		}("10.0.0.4")
	}
	wg.Wait()

	if got := p.Count("10.0.0.4"); got != goroutines*perG {
		t.Fatalf("expected count=%d, got %d", goroutines*perG, got)
	}
}

// TestPike_Close_StopsJanitor verifies that closing the Pike and
// subsequently calling Hit does not panic or otherwise misbehave.
func TestPike_Close_StopsJanitor(t *testing.T) {
	p := New(10, 50*time.Millisecond)
	p.Close()

	// Allow a moment for the janitor to observe the close signal.
	time.Sleep(100 * time.Millisecond)

	for i := 0; i < 5; i++ {
		allowed, _, _ := p.Hit("10.0.0.5")
		if !allowed {
			t.Fatalf("expected allowed=true after close")
		}
	}
	// Nil Pike must also be safe to call.
	var nilP *Pike
	allowed, _, _ := nilP.Hit("10.0.0.5")
	if !allowed {
		t.Fatalf("nil Pike should always return allowed=true")
	}
	nilP.Close() // should be a no-op.
}

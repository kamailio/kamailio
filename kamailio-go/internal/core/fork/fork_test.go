// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for the forking coordinator.
 */

package fork

import (
	"context"
	"errors"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// fakeForwardFn inspects the branch URI to decide the SIP reply.
//
//   - contains "ok"       -> 200 OK
//   - contains "accept"   -> 202 Accepted
//   - contains "redirect" -> 302 Moved Temporarily
//   - contains "fail"     -> 404 Not Found
//   - contains "slow"     -> sleeps 100ms then returns 200
//   - contains "error"    -> returns a transport error
//   - otherwise           -> 500 Server Internal Error
func fakeForwardFn(ctx context.Context, _ *parser.SIPMsg, uri string, _ net.Addr) (int, string, error) {
	if err := ctx.Err(); err != nil {
		return 0, "", err
	}
	lower := strings.ToLower(uri)
	switch {
	case strings.Contains(lower, "slow"):
		select {
		case <-time.After(100 * time.Millisecond):
		case <-ctx.Done():
			return 0, "", ctx.Err()
		}
		return 200, "OK", nil
	case strings.Contains(lower, "error"):
		return 0, "", errors.New("transport error")
	case strings.Contains(lower, "accept"):
		return 202, "Accepted", nil
	case strings.Contains(lower, "ok"):
		return 200, "OK", nil
	case strings.Contains(lower, "redirect"):
		return 302, "Moved Temporarily", nil
	case strings.Contains(lower, "fail"):
		return 404, "Not Found", nil
	}
	return 500, "Server Internal Error", nil
}

// Test 1 — default timeout when zero is passed to NewForker.
func TestForker_NewForker_Default(t *testing.T) {
	f := NewForker(0)
	if f.timeout != 5*time.Second {
		t.Fatalf("expected default 5s timeout, got %v", f.timeout)
	}
}

// Test 2 — AddBranch / BranchCount.
func TestForker_AddBranch(t *testing.T) {
	f := NewForker(time.Second)
	if f.BranchCount() != 0 {
		t.Fatalf("expected 0 branches, got %d", f.BranchCount())
	}
	f.AddBranch("sip:a@example.com")
	f.AddBranch("sip:b@example.com")
	if f.BranchCount() != 2 {
		t.Fatalf("expected 2 branches, got %d", f.BranchCount())
	}
}

// Test 3 — parallel fork, first 2xx wins.
func TestForker_Run_Parallel_FirstWins(t *testing.T) {
	f := NewForker(2 * time.Second)
	f.AddBranch("sip:ok1@example.com")
	f.AddBranch("sip:fail@example.com")
	f.AddBranch("sip:ok2@example.com")

	branches, err := f.Run(context.Background(), fakeForwardFn, nil, nil)
	if err != nil {
		t.Fatalf("Run returned unexpected error: %v", err)
	}
	if len(branches) != 3 {
		t.Fatalf("expected 3 branches, got %d", len(branches))
	}
	winner := f.Winner()
	if winner == nil {
		t.Fatalf("expected a winner, got nil")
	}
	if winner.Status < 200 || winner.Status >= 300 {
		t.Fatalf("winner status should be 2xx, got %d", winner.Status)
	}
}

// Test 4 — all branches fail -> no winner, BestStatus reports the failure.
func TestForker_Run_Parallel_AllFail(t *testing.T) {
	f := NewForker(2 * time.Second)
	f.AddBranch("sip:fail1@example.com")
	f.AddBranch("sip:fail2@example.com")

	branches, err := f.Run(context.Background(), fakeForwardFn, nil, nil)
	if err != nil {
		t.Fatalf("Run returned unexpected error: %v", err)
	}
	if len(branches) != 2 {
		t.Fatalf("expected 2 branches, got %d", len(branches))
	}
	if f.Winner() != nil {
		t.Fatalf("expected no winner, got %+v", f.Winner())
	}
	status, reason := f.BestStatus()
	if status != 404 {
		t.Fatalf("expected best status 404, got %d (%q)", status, reason)
	}
}

// Test 5 — global timeout causes unanswered branches to be flagged.
func TestForker_Run_Timeout_Unanswered(t *testing.T) {
	f := NewForker(20 * time.Millisecond)
	f.AddBranch("sip:slow1@example.com")
	f.AddBranch("sip:slow2@example.com")

	branches, err := f.Run(context.Background(), fakeForwardFn, nil, nil)
	if err != nil {
		t.Fatalf("Run returned unexpected error: %v", err)
	}
	if len(branches) != 2 {
		t.Fatalf("expected 2 branches, got %d", len(branches))
	}
	for i, br := range branches {
		if br.Status == 200 {
			t.Logf("branch %d finished before timeout (%s)", i, br.URI)
			continue
		}
		if br.Err == nil {
			t.Fatalf("branch %d (%s) should have timeout error, got nil", i, br.URI)
		}
		if !br.Dead {
			t.Fatalf("branch %d (%s) should be dead after timeout", i, br.URI)
		}
	}
}

// Test 6 — BestStatus prefers 200 over 202.
func TestForker_BestStatus_PrefersLower2xx(t *testing.T) {
	f := NewForker(2 * time.Second)
	f.AddBranch("sip:accept@example.com")
	f.AddBranch("sip:ok@example.com")

	_, err := f.Run(context.Background(), fakeForwardFn, nil, nil)
	if err != nil {
		t.Fatalf("Run returned unexpected error: %v", err)
	}

	status, reason := f.BestStatus()
	if status != 200 {
		t.Fatalf("expected best status 200, got %d (%q)", status, reason)
	}
}

// Test 7 — empty Forker returns nil winner.
func TestForker_Winner_NilWhenEmpty(t *testing.T) {
	f := NewForker(time.Second)
	if f.Winner() != nil {
		t.Fatalf("expected nil winner on empty forker, got %+v", f.Winner())
	}
}

// Test 8 — serial fork returns immediately on first 2xx.
func TestSerialFork_FirstWins(t *testing.T) {
	branches := []string{
		"sip:fail1@example.com",
		"sip:ok@example.com",
		"sip:fail2@example.com",
	}
	results := SerialFork(context.Background(), fakeForwardFn, nil, nil, branches, time.Second)
	if len(results) != 2 {
		t.Fatalf("expected 2 branches (fail + ok), got %d", len(results))
	}
	if !results[1].Winner {
		t.Fatalf("expected the ok branch to be winner, got winner=%v on %q", results[1].Winner, results[1].URI)
	}
}

// Test 9 — serial fork with a cancelled context marks remaining branches dead.
func TestSerialFork_ContextCancel(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel() // cancel immediately

	branches := []string{
		"sip:ok1@example.com",
		"sip:ok2@example.com",
	}
	results := SerialFork(ctx, fakeForwardFn, nil, nil, branches, time.Second)
	if len(results) != 2 {
		t.Fatalf("expected 2 branches, got %d", len(results))
	}
	for i, br := range results {
		if !br.Dead || br.Err == nil {
			t.Fatalf("branch %d should be dead with error, got Dead=%v Err=%v", i, br.Dead, br.Err)
		}
	}
}

// Test 10 — mixed statuses, 200 wins against 302 and 404.
func TestForker_MixedStatus(t *testing.T) {
	f := NewForker(2 * time.Second)
	f.AddBranch("sip:fail@example.com")
	f.AddBranch("sip:redirect@example.com")
	f.AddBranch("sip:ok@example.com")

	_, err := f.Run(context.Background(), fakeForwardFn, nil, nil)
	if err != nil {
		t.Fatalf("Run returned unexpected error: %v", err)
	}
	winner := f.Winner()
	if winner == nil {
		t.Fatalf("expected a winner, got nil")
	}
	if winner.Status != 200 {
		t.Fatalf("expected 200 winning status, got %d", winner.Status)
	}
	bestStatus, bestReason := f.BestStatus()
	if bestStatus != 200 {
		t.Fatalf("expected best status 200, got %d (%q)", bestStatus, bestReason)
	}
}

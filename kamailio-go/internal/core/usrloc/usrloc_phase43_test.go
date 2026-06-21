// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for the Phase 43 usrloc Backend abstraction.
 *
 * Covers the MemoryBackend as well as a best-effort RedisBackend.
 * Redis tests are skipped when no server is reachable (see
 * TestRedisBackend_SkipIfNoServer).
 */

package usrloc

import (
	"sort"
	"testing"
	"time"
)

// ---------------------------------------------------------------------------
// MemoryBackend contract tests
// ---------------------------------------------------------------------------

func TestMemoryBackend_Basic(t *testing.T) {
	b := NewMemoryBackend()
	domain, aor := "example.com", "sip:alice@example.com"

	c1 := &Contact{
		AOR:       aor,
		URI:       "sip:alice@10.0.0.1:5060",
		Expires:   time.Now().Add(1 * time.Hour),
		Q:         1.0,
		Received:  "10.0.0.1",
		CallID:    "call-id-1",
		Instance:  "urn:uuid:abc",
		RegID:     1,
		UserAgent: "kamailio-go",
	}

	if err := b.UpsertContact(domain, aor, c1); err != nil {
		t.Fatalf("UpsertContact: %v", err)
	}

	// Second contact for a different AOR should not bleed.
	c2 := &Contact{
		AOR:     "sip:bob@example.com",
		URI:     "sip:bob@10.0.0.2",
		Expires: time.Now().Add(1 * time.Hour),
		Q:       0.5,
	}
	if err := b.UpsertContact(domain, c2.AOR, c2); err != nil {
		t.Fatalf("UpsertContact(bob): %v", err)
	}

	got, err := b.ListContacts(domain, aor)
	if err != nil {
		t.Fatalf("ListContacts: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("expected 1 contact for alice, got %d", len(got))
	}
	if got[0].URI != c1.URI || got[0].Q != c1.Q || got[0].UserAgent != c1.UserAgent {
		t.Errorf("contact round-trip mismatch: got=%#v want=%#v", got[0], c1)
	}

	// Upsert existing URI with updated value.
	c1Updated := &Contact{
		AOR:     c1.AOR,
		URI:     c1.URI,
		Expires: time.Now().Add(2 * time.Hour),
		Q:       0.7,
	}
	if err := b.UpsertContact(domain, aor, c1Updated); err != nil {
		t.Fatalf("UpsertContact update: %v", err)
	}
	got, _ = b.ListContacts(domain, aor)
	if len(got) != 1 || got[0].Q != 0.7 {
		t.Errorf("expected single updated contact with Q=0.7, got %#v", got)
	}

	// Remove by URI.
	if err := b.RemoveContact(domain, aor, c1.URI); err != nil {
		t.Fatalf("RemoveContact: %v", err)
	}
	got, _ = b.ListContacts(domain, aor)
	if len(got) != 0 {
		t.Errorf("expected 0 contacts after removal, got %d", len(got))
	}

	// Other AOR unaffected.
	got, _ = b.ListContacts(domain, "sip:bob@example.com")
	if len(got) != 1 {
		t.Errorf("expected bob's contact intact, got %d", len(got))
	}

	// Removing a non-existent key is a no-op.
	if err := b.RemoveContact(domain, aor, "sip:nobody"); err != nil {
		t.Fatalf("RemoveContact(no-op) returned error: %v", err)
	}
}

func TestMemoryBackend_PurgeExpired(t *testing.T) {
	b := NewMemoryBackend()
	domain, aor := "example.com", "sip:alice@example.com"

	past := &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.1",
		Expires: time.Now().Add(-1 * time.Minute),
	}
	future := &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.2",
		Expires: time.Now().Add(1 * time.Hour),
	}
	noExpiry := &Contact{
		AOR: aor,
		URI: "sip:alice@10.0.0.3",
		// zero Expires => never considered expired
	}
	_ = b.UpsertContact(domain, aor, past)
	_ = b.UpsertContact(domain, aor, future)
	_ = b.UpsertContact(domain, aor, noExpiry)

	purged, err := b.PurgeExpired(domain, time.Now())
	if err != nil {
		t.Fatalf("PurgeExpired: %v", err)
	}
	if purged != 1 {
		t.Errorf("expected to purge 1 contact, got %d", purged)
	}
	got, _ := b.ListContacts(domain, aor)
	if len(got) != 2 {
		t.Errorf("expected 2 contacts surviving purge, got %d", len(got))
	}
}

// ---------------------------------------------------------------------------
// Domain + Backend integration (in-memory)
// ---------------------------------------------------------------------------

func TestDomain_WithMemoryBackend(t *testing.T) {
	b := NewMemoryBackend()
	d := NewDomainWithBackend("example.com", b)
	aor := "sip:alice@example.com"

	d.AddContact(aor, &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.1",
		Expires: time.Now().Add(1 * time.Hour),
		Q:       1.0,
	})
	d.AddContact(aor, &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.2",
		Expires: time.Now().Add(1 * time.Hour),
		Q:       0.5,
	})

	// In-memory state reflects the additions.
	if n := d.AORCount(); n != 1 {
		t.Errorf("domain AOR count = %d, want 1", n)
	}
	if n := d.TotalContactCount(); n != 2 {
		t.Errorf("domain total contacts = %d, want 2", n)
	}

	// Backend mirrors the writes.
	backed, err := b.ListContacts("example.com", aor)
	if err != nil {
		t.Fatalf("ListContacts: %v", err)
	}
	if len(backed) != 2 {
		t.Fatalf("backend has %d contacts, want 2", len(backed))
	}

	// Domain removal also removes from the backend.
	if removed := d.RemoveContact(aor, "sip:alice@10.0.0.1"); !removed {
		t.Errorf("expected RemoveContact to return true")
	}
	backed, _ = b.ListContacts("example.com", aor)
	if len(backed) != 1 {
		t.Errorf("backend has %d contacts after remove, want 1", len(backed))
	}

	// PurgeExpired flows through to the backend.
	_ = d.RemoveContact(aor, "sip:alice@10.0.0.2")
	// Replace the last remaining contact with an already-expired one.
	d.AddContact(aor, &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.9",
		Expires: time.Now().Add(-5 * time.Minute),
	})
	purged := d.PurgeExpired()
	if purged < 1 {
		t.Errorf("expected Domain.PurgeExpired to report at least 1, got %d", purged)
	}
	backed, _ = b.ListContacts("example.com", aor)
	if len(backed) != 0 {
		t.Errorf("backend should be empty after purge, got %d", len(backed))
	}
}

// TestDomain_MixedBackends_Comparable exercises two completely independent
// backends wired into separate Domain instances to ensure there is no
// accidental cross-talk between them.
func TestDomain_MixedBackends_Comparable(t *testing.T) {
	bMem := NewMemoryBackend()
	bMem2 := NewMemoryBackend()

	d1 := NewDomainWithBackend("d1.example.com", bMem)
	d2 := NewDomainWithBackend("d2.example.com", bMem2)

	aor := "sip:shared@example.com"
	for i := 0; i < 3; i++ {
		c := &Contact{
			AOR:     aor,
			URI:     "sip:shared@10.0.0." + itoa(i+1),
			Expires: time.Now().Add(1 * time.Hour),
			Q:       1.0,
		}
		d1.AddContact(aor, c)
		d2.AddContact(aor, c)
	}

	l1, _ := bMem.ListContacts("d1.example.com", aor)
	l2, _ := bMem2.ListContacts("d2.example.com", aor)
	sortContactsByURI(l1)
	sortContactsByURI(l2)
	if len(l1) != len(l2) {
		t.Fatalf("independent backends disagree: %d vs %d", len(l1), len(l2))
	}
	for i := range l1 {
		if l1[i].URI != l2[i].URI {
			t.Errorf("contact URI mismatch at %d: %q vs %q", i, l1[i].URI, l2[i].URI)
		}
	}

	// Now mutate d1 only and assert d2's backend is untouched.
	d1.RemoveContact(aor, "sip:shared@10.0.0.1")
	l1, _ = bMem.ListContacts("d1.example.com", aor)
	l2, _ = bMem2.ListContacts("d2.example.com", aor)
	if len(l1) != 2 || len(l2) != 3 {
		t.Errorf("unexpected counts after isolated removal: d1=%d d2=%d", len(l1), len(l2))
	}
}

func sortContactsByURI(cs []*Contact) {
	sort.Slice(cs, func(i, j int) bool { return cs[i].URI < cs[j].URI })
}

func itoa(i int) string {
	// tiny helper to avoid importing strconv everywhere
	if i == 0 {
		return "0"
	}
	neg := false
	if i < 0 {
		neg = true
		i = -i
	}
	var buf [20]byte
	pos := len(buf)
	for i > 0 {
		pos--
		buf[pos] = byte('0' + i%10)
		i /= 10
	}
	if neg {
		pos--
		buf[pos] = '-'
	}
	return string(buf[pos:])
}

// ---------------------------------------------------------------------------
// RedisBackend tests
// ---------------------------------------------------------------------------

// newTestRedisBackend constructs a RedisBackend for tests and returns the
// backend plus a teardown that flushes test-prefixed keys. Tests that
// strictly require a live server should Skip when backend.Connected()
// returns false.
func newTestRedisBackend(t *testing.T) (*RedisBackend, func()) {
	t.Helper()
	b, err := NewRedisBackend(EnvRedisAddr(), "", 0, 10*time.Second)
	if err != nil {
		t.Fatalf("NewRedisBackend: %v", err)
	}
	teardown := func() {
		if b.Connected() {
			// flush all keys under our test-prefix to avoid cross-test
			// pollution when the same Redis instance is reused.
			keys, err := b.client.scan("kamailio-go:usrloc:*")
			if err == nil && len(keys) > 0 {
				_, _ = b.client.del(keys...)
			}
		}
		_ = b.Close()
	}
	return b, teardown
}

// TestRedisBackend_SkipIfNoServer probes for a live Redis and explicitly
// Skip()s the test if none is available. Other Redis tests depend on the
// same Connected() probe; this test merely makes the behaviour explicit.
func TestRedisBackend_SkipIfNoServer(t *testing.T) {
	b, teardown := newTestRedisBackend(t)
	defer teardown()
	if !b.Connected() {
		t.Skip("redis not reachable; skipping redis tests")
	}
	_ = b
}

func TestRedisBackend_LiveUpsertAndList(t *testing.T) {
	b, teardown := newTestRedisBackend(t)
	defer teardown()
	if !b.Connected() {
		t.Skip("redis not reachable; skipping live redis test")
	}

	domain, aor := "phase43.example.com", "sip:redis-test@example.com"
	c := &Contact{
		AOR:       aor,
		URI:       "sip:redis-test@10.0.0.1",
		Expires:   time.Now().Add(5 * time.Minute),
		Q:         0.8,
		Received:  "10.0.0.1",
		CallID:    "call-redis",
		Instance:  "urn:uuid:redis",
		RegID:     2,
		UserAgent: "phase43-test",
	}

	if err := b.UpsertContact(domain, aor, c); err != nil {
		t.Fatalf("UpsertContact: %v", err)
	}

	got, err := b.ListContacts(domain, aor)
	if err != nil {
		t.Fatalf("ListContacts: %v", err)
	}
	if len(got) != 1 {
		t.Fatalf("expected 1 contact after upsert, got %d", len(got))
	}
	if got[0].URI != c.URI || got[0].Q != c.Q || got[0].UserAgent != c.UserAgent {
		t.Errorf("round-trip mismatch: got=%#v want=%#v", got[0], c)
	}

	// Remove and verify list is empty.
	if err := b.RemoveContact(domain, aor, c.URI); err != nil {
		t.Fatalf("RemoveContact: %v", err)
	}
	got, _ = b.ListContacts(domain, aor)
	if len(got) != 0 {
		t.Errorf("expected 0 contacts after removal, got %d", len(got))
	}
}

func TestDomain_WithRedisBackend_SkipIfNoServer(t *testing.T) {
	b, teardown := newTestRedisBackend(t)
	defer teardown()
	if !b.Connected() {
		t.Skip("redis not reachable; skipping redis-backed domain test")
	}

	domain := "phase43-redis-domain.example.com"
	aor := "sip:alice@" + domain
	d := NewDomainWithBackend(domain, b)

	c1 := &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.1",
		Expires: time.Now().Add(1 * time.Hour),
		Q:       1.0,
	}
	c2 := &Contact{
		AOR:     aor,
		URI:     "sip:alice@10.0.0.2",
		Expires: time.Now().Add(1 * time.Hour),
		Q:       0.5,
	}
	d.AddContact(aor, c1)
	d.AddContact(aor, c2)

	backed, err := b.ListContacts(domain, aor)
	if err != nil {
		t.Fatalf("ListContacts: %v", err)
	}
	if len(backed) != 2 {
		t.Fatalf("backend has %d contacts after Domain.AddContact, want 2", len(backed))
	}

	d.RemoveContact(aor, c1.URI)
	backed, _ = b.ListContacts(domain, aor)
	if len(backed) != 1 {
		t.Errorf("backend has %d contacts after removal, want 1", len(backed))
	}
}

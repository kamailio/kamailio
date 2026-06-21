// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * usrloc Backend abstraction - optional persistence layer for contacts.
 *
 * Provides a unified Backend interface that can be backed by:
 *   - an in-memory map (default, always available)
 *   - a Redis server (optional, enabled via NewDomainWithBackend)
 *
 * The Domain struct keeps its in-memory behaviour unchanged.  When an
 * optional Backend is attached, Domain will additionally forward writes
 * (AddContact / RemoveContact / PurgeExpired) to it.
 */

package usrloc

import (
	"sync"
	"time"
)

// Backend abstracts the persistence layer for contacts.
// Implementations:
//
//   - MemoryBackend   — map based, always available (default)
//   - RedisBackend    — Redis string-based storage, optional
type Backend interface {
	// ListContacts returns contacts currently stored for aor within domain.
	ListContacts(domain, aor string) ([]*Contact, error)

	// UpsertContact creates or updates a single contact.
	UpsertContact(domain, aor string, c *Contact) error

	// RemoveContact removes a single contact by URI.
	RemoveContact(domain, aor, contactURI string) error

	// PurgeExpired iterates the backend and drops expired contacts.
	PurgeExpired(domain string, now time.Time) (int, error)
}

// ---------------------------------------------------------------------------
// MemoryBackend
// ---------------------------------------------------------------------------

// memoryAOR is the per-AOR storage inside MemoryBackend.
type memoryAOR struct {
	contacts map[string]*Contact
}

// MemoryBackend is a thread-safe, map-based Backend implementation.
// It is a convenience wrapper used by tests and by callers that want
// to program against the Backend interface while still keeping data
// fully in-process.
type MemoryBackend struct {
	mu    sync.RWMutex
	store map[string]map[string]*memoryAOR // domain -> aor-key -> memoryAOR
}

// NewMemoryBackend creates a new in-memory backend.
func NewMemoryBackend() *MemoryBackend {
	return &MemoryBackend{
		store: make(map[string]map[string]*memoryAOR),
	}
}

func (m *MemoryBackend) ensureBucket(domain, aor string) *memoryAOR {
	bucket, ok := m.store[domain]
	if !ok {
		bucket = make(map[string]*memoryAOR)
		m.store[domain] = bucket
	}
	a, ok := bucket[aor]
	if !ok {
		a = &memoryAOR{contacts: make(map[string]*Contact)}
		bucket[aor] = a
	}
	return a
}

// ListContacts returns the contacts stored for (domain, aor).
// The returned slice is a fresh copy; callers may freely mutate it.
func (m *MemoryBackend) ListContacts(domain, aor string) ([]*Contact, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	bucket, ok := m.store[domain]
	if !ok {
		return nil, nil
	}
	entry, ok := bucket[aor]
	if !ok {
		return nil, nil
	}
	result := make([]*Contact, 0, len(entry.contacts))
	for _, c := range entry.contacts {
		// return a shallow copy so callers cannot poison our internal map
		cp := *c
		result = append(result, &cp)
	}
	return result, nil
}

// UpsertContact creates or updates a contact keyed by its URI.
func (m *MemoryBackend) UpsertContact(domain, aor string, c *Contact) error {
	if c == nil {
		return nil
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	bucket := m.ensureBucket(domain, aor)
	cp := *c
	bucket.contacts[c.URI] = &cp
	return nil
}

// RemoveContact removes a contact by URI if present.
func (m *MemoryBackend) RemoveContact(domain, aor, contactURI string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	bucket, ok := m.store[domain]
	if !ok {
		return nil
	}
	entry, ok := bucket[aor]
	if !ok {
		return nil
	}
	delete(entry.contacts, contactURI)
	// cleanup empty containers to keep the map tidy
	if len(entry.contacts) == 0 {
		delete(bucket, aor)
	}
	if len(bucket) == 0 {
		delete(m.store, domain)
	}
	return nil
}

// PurgeExpired drops contacts whose Expires value is before `now` and
// returns the number of contacts removed.
func (m *MemoryBackend) PurgeExpired(domain string, now time.Time) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	bucket, ok := m.store[domain]
	if !ok {
		return 0, nil
	}
	purged := 0
	for aorKey, entry := range bucket {
		for uri, c := range entry.contacts {
			if !c.Expires.IsZero() && now.After(c.Expires) {
				delete(entry.contacts, uri)
				purged++
			}
		}
		if len(entry.contacts) == 0 {
			delete(bucket, aorKey)
		}
	}
	if len(bucket) == 0 {
		delete(m.store, domain)
	}
	return purged, nil
}

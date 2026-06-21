// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * In-memory accounting backend. Useful for tests and as a reference
 * implementation of the Backend interface. It keeps every CDR written
 * to it in a slice, protected by a mutex.
 */

package acc

import (
	"context"
	"sync"
)

// InMemoryBackend is a Backend that accumulates CDRs in memory. It is
// primarily used by tests; callers may inspect Written/FlushCount after
// exercising the accounting service to verify CDR production.
type InMemoryBackend struct {
	mu       sync.RWMutex
	records  []*CDR
	writes   int
}

// NewInMemoryBackend constructs an empty InMemoryBackend.
func NewInMemoryBackend() *InMemoryBackend {
	return &InMemoryBackend{}
}

// Write appends a copy of cdr to the in-memory slice. It never returns
// an error but implements the Backend contract so the type is pluggable
// into AccountingService.
func (b *InMemoryBackend) Write(ctx context.Context, cdr *CDR) error {
	if b == nil || cdr == nil {
		return nil
	}
	copyCDR := *cdr
	b.mu.Lock()
	b.records = append(b.records, &copyCDR)
	b.writes++
	b.mu.Unlock()
	return nil
}

// Close is a no-op for the in-memory backend. It satisfies the Backend
// interface so the backend can be attached alongside persistent ones.
func (b *InMemoryBackend) Close() error {
	return nil
}

// Count returns the number of CDRs that have been written to the
// backend so far.
func (b *InMemoryBackend) Count() int {
	if b == nil {
		return 0
	}
	b.mu.RLock()
	defer b.mu.RUnlock()
	return b.writes
}

// Snapshot returns a copy of the CDRs written so far, ordered from
// oldest to newest. Callers may safely modify the returned slice.
func (b *InMemoryBackend) Snapshot() []*CDR {
	if b == nil {
		return nil
	}
	b.mu.RLock()
	defer b.mu.RUnlock()
	out := make([]*CDR, len(b.records))
	for i, r := range b.records {
		cp := *r
		out[i] = &cp
	}
	return out
}

// Reset clears any previously-written CDRs. Useful between test cases
// that share a backend.
func (b *InMemoryBackend) Reset() {
	if b == nil {
		return
	}
	b.mu.Lock()
	b.records = b.records[:0]
	b.writes = 0
	b.mu.Unlock()
}

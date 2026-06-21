// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * AVP (Attribute-Value Pair) Store - a lightweight per-request store that
 * mirrors Kamailio's avpops module semantics. Values are either strings or
 * integers, a name can have multiple values (FIFO order), and every access is
 * guarded by a read-write mutex so the store remains safe to share across
 * goroutines.
 *
 * This complements the richer AVP infrastructure in avp.go by exposing a
 * minimal API optimised for per-request lookup/accumulation.
 */

package avp

import "sync"

// Kind distinguishes the two AVP value types supported by Store.
type Kind int

const (
	// KindString marks a string-typed AVP value.
	KindString Kind = 1
	// KindInt marks an integer-typed AVP value.
	KindInt Kind = 2
)

// Value is one AVP. Use the S or I field depending on Kind.
type Value struct {
	Kind Kind
	S    string
	I    int64
}

// Store owns per-request AVPs, indexed by name; each name can have multiple
// values (FIFO order). A zero Store is ready for use (lazy init on first Add).
type Store struct {
	mu   sync.RWMutex
	data map[string][]Value
}

// NewStore returns an empty Store.
func NewStore() *Store {
	return &Store{data: make(map[string][]Value)}
}

// AddString appends a string value to the AVP with the given name.
func (s *Store) AddString(name, value string) {
	if s == nil {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.data == nil {
		s.data = make(map[string][]Value)
	}
	s.data[name] = append(s.data[name], Value{Kind: KindString, S: value})
}

// AddInt appends an integer value to the AVP with the given name.
func (s *Store) AddInt(name string, value int64) {
	if s == nil {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.data == nil {
		s.data = make(map[string][]Value)
	}
	s.data[name] = append(s.data[name], Value{Kind: KindInt, I: value})
}

// First returns the first value for the given AVP. The second return value
// reports whether the AVP was set at all.
func (s *Store) First(name string) (Value, bool) {
	if s == nil {
		return Value{}, false
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, ok := s.data[name]
	if !ok || len(v) == 0 {
		return Value{}, false
	}
	return v[0], true
}

// All returns a snapshot copy of all values for the given AVP, preserving
// insertion order.
func (s *Store) All(name string) []Value {
	if s == nil {
		return nil
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	src := s.data[name]
	out := make([]Value, len(src))
	copy(out, src)
	return out
}

// Del removes all values for the given AVP.
func (s *Store) Del(name string) {
	if s == nil {
		return
	}
	s.mu.Lock()
	delete(s.data, name)
	s.mu.Unlock()
}

// Has reports whether any value is stored for name.
func (s *Store) Has(name string) bool {
	if s == nil {
		return false
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, ok := s.data[name]
	return ok && len(v) > 0
}

// Size returns the total number of AVP entries (across all names).
func (s *Store) Size() int {
	if s == nil {
		return 0
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	total := 0
	for _, vs := range s.data {
		total += len(vs)
	}
	return total
}

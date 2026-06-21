// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * htable - in-memory hash table with optional TTL expiry, equivalent to
 * Kamailio's htable module.
 *
 * The Table exposes a string→string map with atomic integer helpers and a
 * janitor-like CleanupExpired sweep. Manager groups named tables under a
 * single registry.
 */

package htable

import (
	"sync"
	"time"
)

// Entry is one stored value. ExpiresAt zero means never expires.
type Entry struct {
	Value     string
	ExpiresAt time.Time
}

// Table is a named string map with optional TTL support.
type Table struct {
	mu         sync.RWMutex
	entries    map[string]*Entry
	name       string
	defaultTTL time.Duration
}

// New creates a standalone Table.
func New(name string, defaultTTL time.Duration) *Table {
	return &Table{
		entries:    make(map[string]*Entry),
		name:       name,
		defaultTTL: defaultTTL,
	}
}

// Name returns the table's name.
func (t *Table) Name() string {
	if t == nil {
		return ""
	}
	return t.name
}

// Set inserts or overwrites key→value; zero ttl falls back to the table default.
func (t *Table) Set(key, value string, ttl time.Duration) {
	if t == nil {
		return
	}
	if ttl <= 0 {
		ttl = t.defaultTTL
	}
	t.mu.Lock()
	defer t.mu.Unlock()
	exp := time.Time{}
	if ttl > 0 {
		exp = time.Now().Add(ttl)
	}
	t.entries[key] = &Entry{Value: value, ExpiresAt: exp}
}

// Get fetches the value for key, returning ("", false) if missing or expired.
func (t *Table) Get(key string) (string, bool) {
	if t == nil {
		return "", false
	}
	t.mu.RLock()
	defer t.mu.RUnlock()
	e, ok := t.entries[key]
	if !ok {
		return "", false
	}
	if !e.ExpiresAt.IsZero() && time.Now().After(e.ExpiresAt) {
		return "", false
	}
	return e.Value, true
}

// Del removes a key; no-op if key is unknown.
func (t *Table) Del(key string) {
	if t == nil {
		return
	}
	t.mu.Lock()
	delete(t.entries, key)
	t.mu.Unlock()
}

// Inc atomically increments the numeric value at key by n. Non-numeric or
// missing values are treated as 0 before the increment. Returns the new
// numeric value.
func (t *Table) Inc(key string, n int) int {
	if t == nil {
		return 0
	}
	t.mu.Lock()
	defer t.mu.Unlock()
	cur, _ := t.entries[key]
	var val int
	if cur != nil {
		val = parseAsInt(cur.Value)
	}
	val += n
	exp := time.Time{}
	if t.defaultTTL > 0 {
		exp = time.Now().Add(t.defaultTTL)
	}
	t.entries[key] = &Entry{Value: itoa(val), ExpiresAt: exp}
	return val
}

// Dec is the symmetric decrement; negative values are allowed.
func (t *Table) Dec(key string, n int) int {
	return t.Inc(key, -n)
}

// Size returns the number of entries currently held in the map.
func (t *Table) Size() int {
	if t == nil {
		return 0
	}
	t.mu.RLock()
	defer t.mu.RUnlock()
	return len(t.entries)
}

// CleanupExpired scans the table and removes entries whose TTL has elapsed.
// Returns the number of entries removed.
func (t *Table) CleanupExpired() int {
	if t == nil {
		return 0
	}
	now := time.Now()
	t.mu.Lock()
	defer t.mu.Unlock()
	removed := 0
	for k, e := range t.entries {
		if !e.ExpiresAt.IsZero() && now.After(e.ExpiresAt) {
			delete(t.entries, k)
			removed++
		}
	}
	return removed
}

// Manager owns named Tables, typically a single instance per process.
type Manager struct {
	mu     sync.RWMutex
	tables map[string]*Table
}

// NewManager returns a new empty Manager.
func NewManager() *Manager {
	return &Manager{tables: make(map[string]*Table)}
}

// Create returns a new Table by name, overwriting any previous table by
// that name.
func (m *Manager) Create(name string, defaultTTL time.Duration) *Table {
	if m == nil {
		return New(name, defaultTTL)
	}
	t := New(name, defaultTTL)
	m.mu.Lock()
	m.tables[name] = t
	m.mu.Unlock()
	return t
}

// Get returns the existing table by name, or nil if missing.
func (m *Manager) Get(name string) *Table {
	if m == nil {
		return nil
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.tables[name]
}

// TableNames returns a snapshot of the current table names.
func (m *Manager) TableNames() []string {
	if m == nil {
		return nil
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	out := make([]string, 0, len(m.tables))
	for n := range m.tables {
		out = append(out, n)
	}
	return out
}

// ---------------------------------------------------------------
// small helpers

// parseAsInt parses s as a base-10 integer, returning 0 on any malformed
// input. This matches the behaviour of Kamailio's $shtinc for non-numeric
// values.
func parseAsInt(s string) int {
	if s == "" {
		return 0
	}
	neg := false
	i := 0
	if s[0] == '-' {
		neg = true
		i = 1
	}
	n := 0
	for ; i < len(s); i++ {
		if s[i] < '0' || s[i] > '9' {
			return 0
		}
		n = n*10 + int(s[i]-'0')
	}
	if neg {
		return -n
	}
	return n
}

// itoa converts n to its base-10 string representation.
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	buf := make([]byte, 0, 16)
	for n > 0 {
		buf = append([]byte{byte('0' + n%10)}, buf...)
		n /= 10
	}
	if neg {
		buf = append([]byte{'-'}, buf...)
	}
	return string(buf)
}

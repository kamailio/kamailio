// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * In-memory database backend - matching C db_memcache / flatstore
 *
 * Provides an in-memory database implementation for testing and
 * lightweight deployments. Data is not persisted.
 */

package db

import (
	"fmt"
	"sort"
	"strings"
	"sync"
)

// MemoryDB represents an in-memory database.
type MemoryDB struct {
	mu     sync.RWMutex
	tables map[string]*MemoryTable
}

// MemoryTable represents an in-memory table.
type MemoryTable struct {
	mu   sync.RWMutex
	rows []*DBRow
	keys []DBKey
}

// MemoryConn represents an in-memory database connection.
type MemoryConn struct {
	db *MemoryDB
}

// MemoryDriver is the in-memory database driver.
type MemoryDriver struct{}

// NewMemoryDB creates a new in-memory database.
func NewMemoryDB() *MemoryDB {
	return &MemoryDB{
		tables: make(map[string]*MemoryTable),
	}
}

// Name returns the driver name.
func (d *MemoryDriver) Name() string {
	return "memory"
}

// Open creates a new in-memory connection.
func (d *MemoryDriver) Open(url string) (DBConn, error) {
	db := NewMemoryDB()
	return &MemoryConn{db: db}, nil
}

// getOrCreateTable returns a table, creating it if needed.
func (c *MemoryConn) getOrCreateTable(name string) *MemoryTable {
	c.db.mu.Lock()
	defer c.db.mu.Unlock()
	if t, ok := c.db.tables[name]; ok {
		return t
	}
	t := &MemoryTable{
		rows: make([]*DBRow, 0),
	}
	c.db.tables[name] = t
	return t
}

// Query executes a SELECT.
func (c *MemoryConn) Query(table string, keys []DBKey, where []DBCondition, orderBy string, limit, offset int) (*DBResult, error) {
	t := c.getOrCreateTable(table)
	t.mu.RLock()
	defer t.mu.RUnlock()

	var rows []*DBRow
	for _, row := range t.rows {
		if matchRow(row, where) {
			rows = append(rows, row)
		}
	}

	// Sort
	if orderBy != "" {
		sortRows(rows, orderBy)
	}

	// Offset and limit
	if offset > 0 && offset < len(rows) {
		rows = rows[offset:]
	}
	if limit > 0 && limit < len(rows) {
		rows = rows[:limit]
	}

	return &DBResult{Rows: rows, Keys: keys}, nil
}

// Insert inserts a row.
func (c *MemoryConn) Insert(table string, keys []DBKey, values []DBValue) error {
	t := c.getOrCreateTable(table)
	t.mu.Lock()
	defer t.mu.Unlock()

	t.keys = keys
	t.rows = append(t.rows, &DBRow{Keys: keys, Values: values})
	return nil
}

// Update updates matching rows.
func (c *MemoryConn) Update(table string, keys []DBKey, values []DBValue, where []DBCondition) (int64, error) {
	t := c.getOrCreateTable(table)
	t.mu.Lock()
	defer t.mu.Unlock()

	var count int64
	for _, row := range t.rows {
		if matchRow(row, where) {
			for i, k := range keys {
				for j, rk := range row.Keys {
					if rk.Name == k.Name && i < len(values) {
						row.Values[j] = values[i]
					}
				}
			}
			count++
		}
	}
	return count, nil
}

// Delete deletes matching rows.
func (c *MemoryConn) Delete(table string, where []DBCondition) (int64, error) {
	t := c.getOrCreateTable(table)
	t.mu.Lock()
	defer t.mu.Unlock()

	var remaining []*DBRow
	var count int64
	for _, row := range t.rows {
		if matchRow(row, where) {
			count++
		} else {
			remaining = append(remaining, row)
		}
	}
	t.rows = remaining
	return count, nil
}

// Replace inserts or updates a row.
func (c *MemoryConn) Replace(table string, keys []DBKey, values []DBValue) error {
	t := c.getOrCreateTable(table)
	t.mu.Lock()
	defer t.mu.Unlock()

	// Check if row exists (match on first key)
	for _, row := range t.rows {
		if len(row.Keys) > 0 && len(keys) > 0 && len(row.Values) > 0 && len(values) > 0 {
			if row.Values[0].String() == values[0].String() {
				row.Values = values
				return nil
			}
		}
	}

	t.keys = keys
	t.rows = append(t.rows, &DBRow{Keys: keys, Values: values})
	return nil
}

// Raw executes a raw query (not supported for memory backend).
func (c *MemoryConn) Raw(query string, args ...interface{}) (*DBResult, error) {
	return nil, fmt.Errorf("raw queries not supported by memory backend")
}

// Close closes the connection.
func (c *MemoryConn) Close() error {
	return nil
}

// Ping checks connection.
func (c *MemoryConn) Ping() error {
	return nil
}

// matchRow checks if a row matches all conditions.
func matchRow(row *DBRow, conditions []DBCondition) bool {
	for _, cond := range conditions {
		val := row.Get(cond.Key)
		switch strings.ToUpper(cond.Op) {
		case "=", "==":
			if val.String() != cond.Value.String() {
				return false
			}
		case "!=", "<>":
			if val.String() == cond.Value.String() {
				return false
			}
		case "LIKE":
			if !strings.Contains(strings.ToLower(val.String()), strings.ToLower(cond.Value.String())) {
				return false
			}
		default:
			return false
		}
	}
	return true
}

// sortRows sorts rows by a column name (ascending).
func sortRows(rows []*DBRow, orderBy string) {
	col := strings.TrimPrefix(orderBy, "+")
	desc := false
	if strings.HasPrefix(col, "-") {
		col = col[1:]
		desc = true
	}

	sort.SliceStable(rows, func(i, j int) bool {
		vi := rows[i].Get(col).String()
		vj := rows[j].Get(col).String()
		if desc {
			return vi > vj
		}
		return vi < vj
	})
}

func init() {
	RegisterDriver(&MemoryDriver{})
}

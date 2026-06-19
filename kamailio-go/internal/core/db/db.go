// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Database abstraction layer - matching C db/db.h / db/db_res.c
 *
 * Provides a unified database interface that can be backed by different
 * storage engines (memory, Redis, PostgreSQL, MySQL, etc.).
 *
 * C equivalent types:
 *   db_con_t   -> DBConn
 *   db_key_t   -> DBKey
 *   db_val_t   -> DBValue
 *   db_res_t   -> DBResult
 *   db_row_t   -> DBRow
 *   db_cmd_t   -> DBQuery
 */

package db

import (
	"fmt"
	"sync"
)

// ValueType represents the type of a database value.
type ValueType int

const (
	// DBValNull represents a NULL value.
	DBValNull ValueType = iota
	// DBValInt is an integer value.
	DBValInt
	// DBValString is a string value.
	DBValString
	// DBValFloat is a float value.
	DBValFloat
	// DBValBlob is a binary blob value.
	DBValBlob
	// DBValDatetime is a datetime value.
	DBValDatetime
)

// DBValue represents a database value.
type DBValue struct {
	Type     ValueType
	IntVal   int64
	StrVal   string
	FloatVal float64
	BlobVal  []byte
	IsNull   bool
}

// String returns the string representation of a DBValue.
func (v DBValue) String() string {
	switch v.Type {
	case DBValNull:
		return ""
	case DBValInt:
		return fmt.Sprintf("%d", v.IntVal)
	case DBValString:
		return v.StrVal
	case DBValFloat:
		return fmt.Sprintf("%f", v.FloatVal)
	case DBValBlob:
		return string(v.BlobVal)
	default:
		return v.StrVal
	}
}

// Int returns the integer value.
func (v DBValue) Int() int64 {
	if v.Type == DBValInt {
		return v.IntVal
	}
	return 0
}

// Float returns the float value.
func (v DBValue) Float() float64 {
	if v.Type == DBValFloat {
		return v.FloatVal
	}
	return 0
}

// DBKey represents a database column key.
type DBKey struct {
	Name string
	Type ValueType // expected value type for this column
}

// DBRow represents a single database row.
type DBRow struct {
	Values []DBValue
	Keys   []DBKey
}

// Get returns the value for a column by name.
func (r *DBRow) Get(colName string) DBValue {
	for i, k := range r.Keys {
		if k.Name == colName && i < len(r.Values) {
			return r.Values[i]
		}
	}
	return DBValue{Type: DBValNull, IsNull: true}
}

// GetInt returns the integer value for a column.
func (r *DBRow) GetInt(colName string) int64 {
	return r.Get(colName).Int()
}

// GetString returns the string value for a column.
func (r *DBRow) GetString(colName string) string {
	return r.Get(colName).String()
}

// DBResult represents a database query result.
type DBResult struct {
	Rows  []*DBRow
	Keys  []DBKey
	nrows int
}

// RowCount returns the number of rows.
func (r *DBResult) RowCount() int {
	return len(r.Rows)
}

// Row returns a row by index.
func (r *DBResult) Row(idx int) *DBRow {
	if idx >= 0 && idx < len(r.Rows) {
		return r.Rows[idx]
	}
	return nil
}

// IsEmpty returns true if the result has no rows.
func (r *DBResult) IsEmpty() bool {
	return len(r.Rows) == 0
}

// DBQuery represents a database query.
type DBQuery struct {
	Table   string
	Keys    []DBKey
	Values  []DBValue
	Op      DBOperation
	Where   []DBCondition
	OrderBy string
	Limit   int
	Offset  int
}

// DBOperation represents the type of database operation.
type DBOperation int

const (
	// DBOpQuery is a SELECT operation.
	DBOpQuery DBOperation = iota
	// DBOpInsert is an INSERT operation.
	DBOpInsert
	// DBOpUpdate is an UPDATE operation.
	DBOpUpdate
	// DBOpDelete is a DELETE operation.
	DBOpDelete
	// DBOpReplace is a REPLACE (INSERT OR UPDATE) operation.
	DBOpReplace
)

// DBCondition represents a WHERE clause condition.
type DBCondition struct {
	Key   string
	Op    string // "=", "!=", "<", ">", "<=", ">=", "LIKE"
	Value DBValue
}

// DBConn represents a database connection.
type DBConn interface {
	// Query executes a SELECT query.
	Query(table string, keys []DBKey, where []DBCondition, orderBy string, limit, offset int) (*DBResult, error)

	// Insert inserts a row.
	Insert(table string, keys []DBKey, values []DBValue) error

	// Update updates rows matching the where clause.
	Update(table string, keys []DBKey, values []DBValue, where []DBCondition) (int64, error)

	// Delete deletes rows matching the where clause.
	Delete(table string, where []DBCondition) (int64, error)

	// Replace inserts or updates a row.
	Replace(table string, keys []DBKey, values []DBValue) error

	// Raw executes a raw SQL/query.
	Raw(query string, args ...interface{}) (*DBResult, error)

	// Close closes the connection.
	Close() error

	// Ping checks if the connection is alive.
	Ping() error
}

// DBDriver creates database connections.
type DBDriver interface {
	// Open creates a new database connection.
	Open(url string) (DBConn, error)

	// Name returns the driver name.
	Name() string
}

// DriverRegistry manages registered database drivers.
type DriverRegistry struct {
	mu      sync.RWMutex
	drivers map[string]DBDriver
}

// Global driver registry.
var globalDrivers = &DriverRegistry{
	drivers: make(map[string]DBDriver),
}

// RegisterDriver registers a database driver.
func RegisterDriver(driver DBDriver) error {
	return globalDrivers.RegisterDriver(driver)
}

// RegisterDriver registers a driver.
func (r *DriverRegistry) RegisterDriver(driver DBDriver) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	name := driver.Name()
	if _, exists := r.drivers[name]; exists {
		return fmt.Errorf("driver %q already registered", name)
	}
	r.drivers[name] = driver
	return nil
}

// Open opens a connection using a registered driver.
func Open(driverName, url string) (DBConn, error) {
	return globalDrivers.Open(driverName, url)
}

// Open opens a connection.
func (r *DriverRegistry) Open(driverName, url string) (DBConn, error) {
	r.mu.RLock()
	driver, ok := r.drivers[driverName]
	r.mu.RUnlock()
	if !ok {
		return nil, fmt.Errorf("driver %q not found", driverName)
	}
	return driver.Open(url)
}

// GetDriver returns a registered driver by name.
func GetDriver(name string) DBDriver {
	return globalDrivers.GetDriver(name)
}

// GetDriver returns a driver.
func (r *DriverRegistry) GetDriver(name string) DBDriver {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.drivers[name]
}

// RegisteredDrivers returns all registered driver names.
func RegisteredDrivers() []string {
	return globalDrivers.RegisteredDrivers()
}

// RegisteredDrivers returns all driver names.
func (r *DriverRegistry) RegisteredDrivers() []string {
	r.mu.RLock()
	defer r.mu.RUnlock()
	names := make([]string, 0, len(r.drivers))
	for name := range r.drivers {
		names = append(names, name)
	}
	return names
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SQLite database backend - matching C db_sqlite
 *
 * Provides a persistent database implementation backed by an SQLite3
 * file. Uses the pure-Go modernc.org/sqlite driver (no cgo required) so
 * it builds cleanly on any platform Go supports.
 */

package db

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"sync"
	"time"

	_ "modernc.org/sqlite"
)

// SQLiteDriver is the SQLite3 driver.
type SQLiteDriver struct{}

// SQLiteConn represents an SQLite3 database connection.
type SQLiteConn struct {
	mu     sync.RWMutex
	db     *sql.DB
	path   string
	opened time.Time
}

// Name returns the driver name.
func (d *SQLiteDriver) Name() string {
	return "sqlite"
}

// Open opens (or creates) an SQLite file at url. Pass ":memory:" for an
// in-memory database that disappears on Close. A trailing fragment such as
// "foo.db?mode=rwc" is passed through to the underlying driver.
func (d *SQLiteDriver) Open(url string) (DBConn, error) {
	if url == "" {
		url = ":memory:"
	}
	raw, err := sql.Open("sqlite", url)
	if err != nil {
		return nil, fmt.Errorf("open sqlite %q: %w", url, err)
	}
	// SQLite is file-based — a small concurrency cap keeps contention sane
	// while still allowing multi-reader access.
	raw.SetMaxOpenConns(4)
	raw.SetMaxIdleConns(2)
	raw.SetConnMaxLifetime(5 * time.Minute)
	if err := raw.Ping(); err != nil {
		raw.Close()
		return nil, fmt.Errorf("ping sqlite %q: %w", url, err)
	}
	return &SQLiteConn{db: raw, path: url, opened: time.Now()}, nil
}

// OpenSQLite is a package-level convenience constructor that bypasses the
// driver registry. Useful for tests and one-off use.
func OpenSQLite(path string) (*SQLiteConn, error) {
	conn, err := (&SQLiteDriver{}).Open(path)
	if err != nil {
		return nil, err
	}
	sc, ok := conn.(*SQLiteConn)
	if !ok {
		return nil, fmt.Errorf("sqlite driver returned unexpected type %T", conn)
	}
	return sc, nil
}

// ensureTable lazily creates a simple columnar schema if it does not yet
// exist. The schema is derived from the keys list passed to the first
// Insert/Replace call; columns use an "ANY" affinity to keep SQLite's
// dynamic typing happy.
func (c *SQLiteConn) ensureTable(table string, keys []DBKey) error {
	if c == nil || c.db == nil {
		return fmt.Errorf("nil sqlite conn")
	}
	if len(keys) == 0 {
		return nil
	}
	// Pick the first column as PRIMARY KEY for idempotent replace semantics.
	var sb strings.Builder
	sb.WriteString("CREATE TABLE IF NOT EXISTS ")
	sb.WriteString(quoteIdent(table))
	sb.WriteString(" (")
	for i, k := range keys {
		if i > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(quoteIdent(k.Name))
		sb.WriteString(" TEXT")
		if i == 0 {
			sb.WriteString(" PRIMARY KEY")
		}
	}
	sb.WriteString(")")
	_, err := c.db.Exec(sb.String())
	return err
}

// tableExists reports whether a table has been created yet.
func (c *SQLiteConn) tableExists(table string) (bool, error) {
	row := c.db.QueryRow(
		"SELECT name FROM sqlite_master WHERE type='table' AND name=?",
		table,
	)
	var name string
	if err := row.Scan(&name); err != nil {
		if err == sql.ErrNoRows {
			return false, nil
		}
		return false, err
	}
	return true, nil
}

// colsFor returns the column names of a table, in the order reported by
// SQLite's PRAGMA table_info. If the table does not yet exist, colsFor
// returns an empty slice without error so callers can fall back to keys.
func (c *SQLiteConn) colsFor(table string) ([]string, error) {
	rows, err := c.db.Query(
		"SELECT name FROM pragma_table_info(?) ORDER BY cid",
		table,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var cols []string
	for rows.Next() {
		var col string
		if err := rows.Scan(&col); err != nil {
			return nil, err
		}
		cols = append(cols, col)
	}
	return cols, rows.Err()
}

// Query executes a SELECT. keys may be nil to select all columns.
func (c *SQLiteConn) Query(table string, keys []DBKey, where []DBCondition, orderBy string, limit, offset int) (*DBResult, error) {
	if c == nil || c.db == nil {
		return nil, fmt.Errorf("nil sqlite conn")
	}
	c.mu.RLock()
	defer c.mu.RUnlock()

	exists, err := c.tableExists(table)
	if err != nil {
		return nil, err
	}
	if !exists {
		return &DBResult{Rows: []*DBRow{}, Keys: keys}, nil
	}

	// Determine selected columns and result keys.
	selCols := make([]string, 0, len(keys))
	selKeys := make([]DBKey, 0, len(keys))
	if len(keys) > 0 {
		for _, k := range keys {
			selCols = append(selCols, quoteIdent(k.Name))
			selKeys = append(selKeys, k)
		}
	} else {
		cols, err := c.colsFor(table)
		if err != nil {
			return nil, err
		}
		for _, col := range cols {
			selCols = append(selCols, quoteIdent(col))
			selKeys = append(selKeys, DBKey{Name: col, Type: DBValString})
		}
	}

	var sb strings.Builder
	sb.WriteString("SELECT ")
	sb.WriteString(strings.Join(selCols, ", "))
	sb.WriteString(" FROM ")
	sb.WriteString(quoteIdent(table))

	var args []interface{}
	if len(where) > 0 {
		clause, wargs, werr := buildWhere(where)
		if werr != nil {
			return nil, werr
		}
		sb.WriteString(" WHERE ")
		sb.WriteString(clause)
		args = append(args, wargs...)
	}
	if orderBy != "" {
		col := strings.TrimPrefix(orderBy, "+")
		desc := false
		if strings.HasPrefix(col, "-") {
			col = col[1:]
			desc = true
		}
		sb.WriteString(" ORDER BY ")
		sb.WriteString(quoteIdent(col))
		if desc {
			sb.WriteString(" DESC")
		} else {
			sb.WriteString(" ASC")
		}
	}
	if limit > 0 {
		sb.WriteString(" LIMIT ")
		fmt.Fprintf(&sb, "%d", limit)
	}
	if offset > 0 {
		if limit <= 0 {
			sb.WriteString(" LIMIT -1")
		}
		sb.WriteString(" OFFSET ")
		fmt.Fprintf(&sb, "%d", offset)
	}

	rows, err := c.db.Query(sb.String(), args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	outRows := make([]*DBRow, 0)
	for rows.Next() {
		// Scan into interface{} then convert to DBValue strings.
		raw := make([]interface{}, len(selKeys))
		ptrs := make([]interface{}, len(selKeys))
		for i := range raw {
			ptrs[i] = &raw[i]
		}
		if err := rows.Scan(ptrs...); err != nil {
			return nil, err
		}
		values := make([]DBValue, len(selKeys))
		for i, v := range raw {
			values[i] = toDBValue(selKeys[i], v)
		}
		outRows = append(outRows, &DBRow{Keys: selKeys, Values: values})
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return &DBResult{Rows: outRows, Keys: selKeys}, nil
}

// Insert inserts a row. If the table does not yet exist it is created from
// keys, with the first column used as the PRIMARY KEY.
func (c *SQLiteConn) Insert(table string, keys []DBKey, values []DBValue) error {
	if c == nil || c.db == nil {
		return fmt.Errorf("nil sqlite conn")
	}
	if len(keys) != len(values) {
		return fmt.Errorf("insert: key/value length mismatch (%d vs %d)", len(keys), len(values))
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if err := c.ensureTable(table, keys); err != nil {
		return err
	}
	var (
		cols []string
		ph   []string
		args []interface{}
	)
	for i, k := range keys {
		cols = append(cols, quoteIdent(k.Name))
		ph = append(ph, "?")
		args = append(args, values[i].String())
	}
	q := fmt.Sprintf("INSERT INTO %s (%s) VALUES (%s)",
		quoteIdent(table), strings.Join(cols, ", "), strings.Join(ph, ", "))
	if _, err := c.db.Exec(q, args...); err != nil {
		return err
	}
	return nil
}

// Replace inserts a row, or updates it if the primary key already exists.
func (c *SQLiteConn) Replace(table string, keys []DBKey, values []DBValue) error {
	if c == nil || c.db == nil {
		return fmt.Errorf("nil sqlite conn")
	}
	if len(keys) != len(values) {
		return fmt.Errorf("replace: key/value length mismatch (%d vs %d)", len(keys), len(values))
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if err := c.ensureTable(table, keys); err != nil {
		return err
	}
	var (
		cols   []string
		ph     []string
		updates []string
		args   []interface{}
	)
	for i, k := range keys {
		cols = append(cols, quoteIdent(k.Name))
		ph = append(ph, "?")
		if i > 0 {
			updates = append(updates, fmt.Sprintf("%s = excluded.%s", quoteIdent(k.Name), quoteIdent(k.Name)))
		}
		args = append(args, values[i].String())
	}
	q := fmt.Sprintf(
		"INSERT INTO %s (%s) VALUES (%s) ON CONFLICT DO UPDATE SET %s",
		quoteIdent(table),
		strings.Join(cols, ", "),
		strings.Join(ph, ", "),
		strings.Join(updates, ", "),
	)
	if _, err := c.db.Exec(q, args...); err != nil {
		return err
	}
	return nil
}

// Update updates rows matching the where clause.
func (c *SQLiteConn) Update(table string, keys []DBKey, values []DBValue, where []DBCondition) (int64, error) {
	if c == nil || c.db == nil {
		return 0, fmt.Errorf("nil sqlite conn")
	}
	if len(keys) != len(values) {
		return 0, fmt.Errorf("update: key/value length mismatch (%d vs %d)", len(keys), len(values))
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	exists, err := c.tableExists(table)
	if err != nil {
		return 0, err
	}
	if !exists {
		return 0, nil
	}
	var sb strings.Builder
	sb.WriteString("UPDATE ")
	sb.WriteString(quoteIdent(table))
	sb.WriteString(" SET ")
	args := make([]interface{}, 0, len(values)+len(where))
	for i, k := range keys {
		if i > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(quoteIdent(k.Name))
		sb.WriteString(" = ?")
		args = append(args, values[i].String())
	}
	if len(where) > 0 {
		clause, wargs, werr := buildWhere(where)
		if werr != nil {
			return 0, werr
		}
		sb.WriteString(" WHERE ")
		sb.WriteString(clause)
		args = append(args, wargs...)
	}
	res, err := c.db.Exec(sb.String(), args...)
	if err != nil {
		return 0, err
	}
	return res.RowsAffected()
}

// Delete deletes rows matching the where clause.
func (c *SQLiteConn) Delete(table string, where []DBCondition) (int64, error) {
	if c == nil || c.db == nil {
		return 0, fmt.Errorf("nil sqlite conn")
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	exists, err := c.tableExists(table)
	if err != nil {
		return 0, err
	}
	if !exists {
		return 0, nil
	}
	var sb strings.Builder
	sb.WriteString("DELETE FROM ")
	sb.WriteString(quoteIdent(table))
	var args []interface{}
	if len(where) > 0 {
		clause, wargs, werr := buildWhere(where)
		if werr != nil {
			return 0, werr
		}
		sb.WriteString(" WHERE ")
		sb.WriteString(clause)
		args = wargs
	}
	res, err := c.db.Exec(sb.String(), args...)
	if err != nil {
		return 0, err
	}
	return res.RowsAffected()
}

// Raw executes a raw SQL query and returns rows as DBResult. For
// simplicity the row values are returned as strings (best-effort).
func (c *SQLiteConn) Raw(query string, args ...interface{}) (*DBResult, error) {
	if c == nil || c.db == nil {
		return nil, fmt.Errorf("nil sqlite conn")
	}
	c.mu.RLock()
	defer c.mu.RUnlock()
	rows, err := c.db.Query(query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	cols, err := rows.Columns()
	if err != nil {
		return nil, err
	}
	keys := make([]DBKey, len(cols))
	for i, col := range cols {
		keys[i] = DBKey{Name: col, Type: DBValString}
	}
	outRows := make([]*DBRow, 0)
	for rows.Next() {
		raw := make([]interface{}, len(cols))
		ptrs := make([]interface{}, len(cols))
		for i := range raw {
			ptrs[i] = &raw[i]
		}
		if err := rows.Scan(ptrs...); err != nil {
			return nil, err
		}
		values := make([]DBValue, len(cols))
		for i, v := range raw {
			values[i] = toDBValue(keys[i], v)
		}
		outRows = append(outRows, &DBRow{Keys: keys, Values: values})
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return &DBResult{Rows: outRows, Keys: keys}, nil
}

// Close closes the underlying SQLite connection. Safe to call on a nil or
// already-closed connection.
func (c *SQLiteConn) Close() error {
	if c == nil || c.db == nil {
		return nil
	}
	err := c.db.Close()
	c.db = nil
	return err
}

// Ping verifies the underlying connection is alive.
func (c *SQLiteConn) Ping() error {
	if c == nil || c.db == nil {
		return fmt.Errorf("nil sqlite conn")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	return c.db.PingContext(ctx)
}

// NewStringValue is a small helper for constructing a DBValue carrying a
// string. Callers such as the auth layer can use it to build query
// conditions without plumbing the DBValue struct fields directly.
func NewStringValue(s string) DBValue {
	return DBValue{Type: DBValString, StrVal: s}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// quoteIdent double-quotes an identifier and escapes any embedded quotes.
func quoteIdent(s string) string {
	return `"` + strings.ReplaceAll(s, `"`, `""`) + `"`
}

// buildWhere renders a slice of DBCondition to a WHERE clause fragment and
// the corresponding positional args. Conditions are joined with AND.
func buildWhere(where []DBCondition) (string, []interface{}, error) {
	var sb strings.Builder
	args := make([]interface{}, 0, len(where))
	for i, cond := range where {
		if i > 0 {
			sb.WriteString(" AND ")
		}
		sb.WriteString(quoteIdent(cond.Key))
		op := strings.ToUpper(strings.TrimSpace(cond.Op))
		switch op {
		case "", "=", "==":
			sb.WriteString(" = ?")
		case "!=", "<>":
			sb.WriteString(" <> ?")
		case "<", ">", "<=", ">=":
			sb.WriteString(" " + op + " ?")
		case "LIKE":
			sb.WriteString(" LIKE ?")
		default:
			return "", nil, fmt.Errorf("unsupported operator %q", cond.Op)
		}
		args = append(args, cond.Value.String())
	}
	return sb.String(), args, nil
}

// toDBValue converts a value scanned from SQLite (which may be nil, []byte,
// int64, float64, or string) into a DBValue aligned with the declared
// column key type.
func toDBValue(key DBKey, v interface{}) DBValue {
	if v == nil {
		return DBValue{Type: DBValNull, IsNull: true}
	}
	switch raw := v.(type) {
	case []byte:
		switch key.Type {
		case DBValInt:
			if n, err := parseIntBytes(raw); err == nil {
				return DBValue{Type: DBValInt, IntVal: n}
			}
		case DBValFloat:
			if f, err := parseFloatBytes(raw); err == nil {
				return DBValue{Type: DBValFloat, FloatVal: f}
			}
		}
		return DBValue{Type: DBValString, StrVal: string(raw)}
	case string:
		switch key.Type {
		case DBValInt:
			if n, err := parseIntBytes([]byte(raw)); err == nil {
				return DBValue{Type: DBValInt, IntVal: n}
			}
		case DBValFloat:
			if f, err := parseFloatBytes([]byte(raw)); err == nil {
				return DBValue{Type: DBValFloat, FloatVal: f}
			}
		}
		return DBValue{Type: DBValString, StrVal: raw}
	case int64:
		if key.Type == DBValFloat {
			return DBValue{Type: DBValFloat, FloatVal: float64(raw)}
		}
		return DBValue{Type: DBValInt, IntVal: raw}
	case float64:
		return DBValue{Type: DBValFloat, FloatVal: raw}
	case bool:
		s := "0"
		if raw {
			s = "1"
		}
		return DBValue{Type: DBValString, StrVal: s}
	default:
		return DBValue{Type: DBValString, StrVal: fmt.Sprintf("%v", raw)}
	}
}

// parseIntBytes is a tiny helper that parses a base-10 int64.
func parseIntBytes(b []byte) (int64, error) {
	var n int64
	if _, err := fmt.Sscanf(string(b), "%d", &n); err != nil {
		return 0, err
	}
	return n, nil
}

// parseFloatBytes parses a float from a byte slice.
func parseFloatBytes(b []byte) (float64, error) {
	var f float64
	if _, err := fmt.Sscanf(string(b), "%f", &f); err != nil {
		return 0, err
	}
	return f, nil
}

func init() {
	RegisterDriver(&SQLiteDriver{})
}

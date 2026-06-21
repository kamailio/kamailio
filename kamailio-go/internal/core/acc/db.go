// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Database backend for CDRs. Writes CDRs to a db.DBConn.
 */

package acc

import (
	"context"
	"fmt"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/db"
)

// DBBackend writes CDRs to a db.DBConn.
type DBBackend struct {
	mu      sync.RWMutex
	conn    db.DBConn
	table   string
}

// NewDBBackend constructs a DBBackend that writes to the provided DBConn.
// If table is empty, "cdr" is used as the default table name.
func NewDBBackend(conn db.DBConn, table string) (*DBBackend, error) {
	if conn == nil {
		return nil, fmt.Errorf("nil database connection")
	}
	if table == "" {
		table = "cdr"
	}
	return &DBBackend{conn: conn, table: table}, nil
}

// Write flushes a single CDR as a row.
func (d *DBBackend) Write(_ context.Context, cdr *CDR) error {
	if d == nil || cdr == nil {
		return nil
	}
	keys := []db.DBKey{
		{Name: "call_id", Type: db.DBValString},
		{Name: "from_user", Type: db.DBValString},
		{Name: "from_domain", Type: db.DBValString},
		{Name: "to_user", Type: db.DBValString},
		{Name: "to_domain", Type: db.DBValString},
		{Name: "request_uri", Type: db.DBValString},
		{Name: "source_ip", Type: db.DBValString},
		{Name: "destination", Type: db.DBValString},
		{Name: "method", Type: db.DBValString},
		{Name: "status_code", Type: db.DBValInt},
		{Name: "reason", Type: db.DBValString},
		{Name: "direction", Type: db.DBValString},
		{Name: "duration_sec", Type: db.DBValInt},
		{Name: "rtp_engine_id", Type: db.DBValString},
	}
	values := []db.DBValue{
		{Type: db.DBValString, StrVal: cdr.CallID},
		{Type: db.DBValString, StrVal: cdr.FromUser},
		{Type: db.DBValString, StrVal: cdr.FromDomain},
		{Type: db.DBValString, StrVal: cdr.ToUser},
		{Type: db.DBValString, StrVal: cdr.ToDomain},
		{Type: db.DBValString, StrVal: cdr.RequestURI},
		{Type: db.DBValString, StrVal: cdr.SourceIP},
		{Type: db.DBValString, StrVal: cdr.Destination},
		{Type: db.DBValString, StrVal: cdr.Method},
		{Type: db.DBValInt, IntVal: int64(cdr.StatusCode)},
		{Type: db.DBValString, StrVal: cdr.Reason},
		{Type: db.DBValString, StrVal: cdr.Direction},
		{Type: db.DBValInt, IntVal: int64(cdr.DurationSec)},
		{Type: db.DBValString, StrVal: cdr.RTPEngineID},
	}
	d.mu.RLock()
	conn := d.conn
	table := d.table
	d.mu.RUnlock()
	return conn.Insert(table, keys, values)
}

// Close releases any backend-side resources; for the DB backend we keep the
// connection open because the caller typically owns its lifecycle.
func (d *DBBackend) Close() error { return nil }

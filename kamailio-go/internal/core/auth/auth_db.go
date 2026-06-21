// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * DB-backed credential store - matching C auth_db / auth_db.so
 *
 * Provides credential lookups over the generic db.DBConn interface, plus
 * a simple in-memory implementation used for tests and small deployments.
 */

package auth

import (
	"context"
	"errors"
	"fmt"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/db"
)

// ErrUserNotFound is returned by Lookup when no row matches the username.
// Callers can distinguish "missing user" from other database errors by
// checking errors.Is(err, ErrUserNotFound).
var ErrUserNotFound = errors.New("user not found")

// Credentials represents a stored user record used by Digest auth.
type Credentials struct {
	// Username is the AOR / subscriber identity.
	Username string
	// Password is the plain-text password (set only when HA1 is empty,
	// e.g. in tests). Production deploys should store HA1.
	Password string
	// HA1 is the pre-computed H(A1) = hash(username:realm:password).
	// Non-empty HA1 is always preferred to Password.
	HA1 string
	// Realm is the auth realm for which this credential is valid.
	Realm string
}

// AuthStore is the minimal interface required for credential lookups.
type AuthStore interface {
	// Lookup returns credentials for the given username, or
	// ErrUserNotFound / a wrapped DB error otherwise.
	Lookup(ctx context.Context, username string) (*Credentials, error)
}

// DBAuthStore implements AuthStore on top of a db.DBConn. Column names and
// the table name are configurable so callers can match their schema.
type DBAuthStore struct {
	mu          sync.RWMutex
	conn        db.DBConn
	table       string
	userColumn  string
	passColumn  string
	ha1Column   string
	realmColumn string
}

// NewDBAuthStore constructs a DBAuthStore. Empty string arguments fall
// back to the Kamailio defaults: subscriber / username / password / ha1
// / realm.
func NewDBAuthStore(conn db.DBConn, table, userCol, passCol, ha1Col, realmCol string) *DBAuthStore {
	if table == "" {
		table = "subscriber"
	}
	if userCol == "" {
		userCol = "username"
	}
	if passCol == "" {
		passCol = "password"
	}
	if ha1Col == "" {
		ha1Col = "ha1"
	}
	if realmCol == "" {
		realmCol = "realm"
	}
	return &DBAuthStore{
		conn:        conn,
		table:       table,
		userColumn:  userCol,
		passColumn:  passCol,
		ha1Column:   ha1Col,
		realmColumn: realmCol,
	}
}

// Lookup queries the backend for username.
func (s *DBAuthStore) Lookup(ctx context.Context, username string) (*Credentials, error) {
	if s == nil {
		return nil, fmt.Errorf("nil auth store")
	}
	if s.conn == nil {
		return nil, fmt.Errorf("nil db conn")
	}
	if username == "" {
		return nil, fmt.Errorf("empty username")
	}
	s.mu.RLock()
	defer s.mu.RUnlock()

	keys := []db.DBKey{
		{Name: s.userColumn, Type: db.DBValString},
		{Name: s.passColumn, Type: db.DBValString},
		{Name: s.ha1Column, Type: db.DBValString},
		{Name: s.realmColumn, Type: db.DBValString},
	}
	where := []db.DBCondition{
		{Key: s.userColumn, Op: "=", Value: db.NewStringValue(username)},
	}

	// The generic db.DBConn.Query signature does not accept a context;
	// ctx is reserved for future middleware/cancellation hooks.
	_ = ctx

	res, err := s.conn.Query(s.table, keys, where, "", 2, 0)
	if err != nil {
		return nil, fmt.Errorf("db query: %w", err)
	}
	if res == nil || res.RowCount() == 0 {
		return nil, ErrUserNotFound
	}
	row := res.Row(0)
	ha1 := row.GetString(s.ha1Column)
	c := &Credentials{
		Username: username,
		HA1:      ha1,
		Password: row.GetString(s.passColumn),
		Realm:    row.GetString(s.realmColumn),
	}
	return c, nil
}

// ---------------------------------------------------------------------------
// In-memory implementation (test helper / small deploys)
// ---------------------------------------------------------------------------

// InMemoryStore is a map-backed AuthStore. Safe for concurrent use.
type InMemoryStore struct {
	mu    sync.RWMutex
	users map[string]*Credentials
}

// NewInMemoryStore returns an empty InMemoryStore.
func NewInMemoryStore() *InMemoryStore {
	return &InMemoryStore{users: map[string]*Credentials{}}
}

// AddUser inserts a user record. Later AddUser calls with the same
// username overwrite earlier ones.
func (s *InMemoryStore) AddUser(username, password, ha1, realm string) {
	if s == nil {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.users[username] = &Credentials{
		Username: username,
		Password: password,
		HA1:      ha1,
		Realm:    realm,
	}
}

// Lookup returns a copy of the stored credentials so callers cannot
// inadvertently mutate the shared map.
func (s *InMemoryStore) Lookup(_ context.Context, username string) (*Credentials, error) {
	if s == nil {
		return nil, fmt.Errorf("nil in-memory store")
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if c, ok := s.users[username]; ok {
		cp := *c
		return &cp, nil
	}
	return nil, ErrUserNotFound
}

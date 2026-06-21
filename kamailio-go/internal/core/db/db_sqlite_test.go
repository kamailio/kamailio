// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for the SQLite backend.
 */

package db

import (
	"testing"
)

// subscriberKeys is a shared fixture shape used by several tests.
var subscriberKeys = []DBKey{
	{Name: "username", Type: DBValString},
	{Name: "password", Type: DBValString},
	{Name: "ha1", Type: DBValString},
	{Name: "realm", Type: DBValString},
}

// TestSQLite_InMemory_InsertAndQuery verifies a round-trip insert/query
// against an ephemeral in-memory SQLite database.
func TestSQLite_InMemory_InsertAndQuery(t *testing.T) {
	conn, err := OpenSQLite(":memory:")
	if err != nil {
		t.Fatalf("OpenSQLite: %v", err)
	}
	defer conn.Close()

	values := []DBValue{
		NewStringValue("alice"),
		NewStringValue("s3cr3t"),
		NewStringValue(""),
		NewStringValue("sip.example.com"),
	}
	if err := conn.Insert("subscriber", subscriberKeys, values); err != nil {
		t.Fatalf("Insert: %v", err)
	}

	res, err := conn.Query("subscriber", subscriberKeys, []DBCondition{
		{Key: "username", Op: "=", Value: NewStringValue("alice")},
	}, "", 0, 0)
	if err != nil {
		t.Fatalf("Query: %v", err)
	}
	if res.RowCount() != 1 {
		t.Fatalf("expected 1 row, got %d", res.RowCount())
	}
	if got := res.Row(0).GetString("username"); got != "alice" {
		t.Errorf("username = %q, want alice", got)
	}
	if got := res.Row(0).GetString("realm"); got != "sip.example.com" {
		t.Errorf("realm = %q, want sip.example.com", got)
	}
}

// TestSQLite_Overwrite verifies that Replace idempotently updates an
// existing row rather than inserting a duplicate.
func TestSQLite_Overwrite(t *testing.T) {
	conn, err := OpenSQLite(":memory:")
	if err != nil {
		t.Fatalf("OpenSQLite: %v", err)
	}
	defer conn.Close()

	insert := func(pw, realm string) error {
		return conn.Replace("subscriber", subscriberKeys, []DBValue{
			NewStringValue("bob"),
			NewStringValue(pw),
			NewStringValue(""),
			NewStringValue(realm),
		})
	}
	if err := insert("first", "one.example.com"); err != nil {
		t.Fatalf("Replace 1: %v", err)
	}
	if err := insert("second", "two.example.com"); err != nil {
		t.Fatalf("Replace 2: %v", err)
	}

	res, err := conn.Query("subscriber", subscriberKeys, []DBCondition{
		{Key: "username", Op: "=", Value: NewStringValue("bob")},
	}, "", 0, 0)
	if err != nil {
		t.Fatalf("Query: %v", err)
	}
	if res.RowCount() != 1 {
		t.Fatalf("expected 1 row after overwrite, got %d", res.RowCount())
	}
	if got := res.Row(0).GetString("password"); got != "second" {
		t.Errorf("password = %q, want second", got)
	}
	if got := res.Row(0).GetString("realm"); got != "two.example.com" {
		t.Errorf("realm = %q, want two.example.com", got)
	}
}

// TestSQLite_Delete verifies Delete removes matching rows and subsequent
// Queries return an empty result.
func TestSQLite_Delete(t *testing.T) {
	conn, err := OpenSQLite(":memory:")
	if err != nil {
		t.Fatalf("OpenSQLite: %v", err)
	}
	defer conn.Close()

	if err := conn.Insert("subscriber", subscriberKeys, []DBValue{
		NewStringValue("carol"),
		NewStringValue("pw"),
		NewStringValue(""),
		NewStringValue("r"),
	}); err != nil {
		t.Fatalf("Insert: %v", err)
	}

	n, err := conn.Delete("subscriber", []DBCondition{
		{Key: "username", Op: "=", Value: NewStringValue("carol")},
	})
	if err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if n != 1 {
		t.Errorf("Delete returned %d, want 1", n)
	}

	res, err := conn.Query("subscriber", subscriberKeys, []DBCondition{
		{Key: "username", Op: "=", Value: NewStringValue("carol")},
	}, "", 0, 0)
	if err != nil {
		t.Fatalf("Query after delete: %v", err)
	}
	if res.RowCount() != 0 {
		t.Errorf("expected 0 rows after delete, got %d", res.RowCount())
	}
}

// TestSQLite_CloseTwiceSafe verifies Close is idempotent.
func TestSQLite_CloseTwiceSafe(t *testing.T) {
	conn, err := OpenSQLite(":memory:")
	if err != nil {
		t.Fatalf("OpenSQLite: %v", err)
	}
	if err := conn.Close(); err != nil {
		t.Fatalf("first Close: %v", err)
	}
	if err := conn.Close(); err != nil {
		t.Fatalf("second Close should be a no-op, got %v", err)
	}
	// Ping after close should fail cleanly, not panic.
	if err := conn.Ping(); err == nil {
		t.Errorf("Ping after close should fail, got nil")
	}
}

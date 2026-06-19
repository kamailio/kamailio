// SPDX-License-Identifier: GPL-2.0-or-later
package db

import (
	"testing"
)

func TestMemoryDriver_Register(t *testing.T) {
	d := GetDriver("memory")
	if d == nil {
		t.Fatal("expected memory driver to be registered")
	}
	if d.Name() != "memory" {
		t.Errorf("driver name = %q, want memory", d.Name())
	}
}

func TestMemoryDriver_OpenAndQuery(t *testing.T) {
	conn, err := Open("memory", "")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer conn.Close()

	keys := []DBKey{
		{Name: "username", Type: DBValString},
		{Name: "domain", Type: DBValString},
		{Name: "ha1", Type: DBValString},
	}

	// Insert
	err = conn.Insert("subscriber", keys, []DBValue{
		{Type: DBValString, StrVal: "alice"},
		{Type: DBValString, StrVal: "example.com"},
		{Type: DBValString, StrVal: "abc123"},
	})
	if err != nil {
		t.Fatalf("Insert: %v", err)
	}

	// Insert second
	err = conn.Insert("subscriber", keys, []DBValue{
		{Type: DBValString, StrVal: "bob"},
		{Type: DBValString, StrVal: "example.com"},
		{Type: DBValString, StrVal: "def456"},
	})
	if err != nil {
		t.Fatalf("Insert: %v", err)
	}

	// Query all
	result, err := conn.Query("subscriber", keys, nil, "", 0, 0)
	if err != nil {
		t.Fatalf("Query: %v", err)
	}
	if result.RowCount() != 2 {
		t.Fatalf("expected 2 rows, got %d", result.RowCount())
	}

	// Query with WHERE
	result, err = conn.Query("subscriber", keys, []DBCondition{
		{Key: "username", Op: "=", Value: DBValue{Type: DBValString, StrVal: "alice"}},
	}, "", 0, 0)
	if err != nil {
		t.Fatalf("Query WHERE: %v", err)
	}
	if result.RowCount() != 1 {
		t.Fatalf("expected 1 row, got %d", result.RowCount())
	}
	if result.Row(0).GetString("username") != "alice" {
		t.Errorf("username = %q", result.Row(0).GetString("username"))
	}
}

func TestMemoryDriver_Update(t *testing.T) {
	conn, err := Open("memory", "")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer conn.Close()

	keys := []DBKey{{Name: "id", Type: DBValInt}, {Name: "value", Type: DBValString}}
	conn.Insert("test", keys, []DBValue{
		{Type: DBValInt, IntVal: 1},
		{Type: DBValString, StrVal: "old"},
	})

	count, err := conn.Update("test", []DBKey{{Name: "value", Type: DBValString}}, []DBValue{
		{Type: DBValString, StrVal: "new"},
	}, []DBCondition{{Key: "id", Op: "=", Value: DBValue{Type: DBValInt, IntVal: 1}}})
	if err != nil {
		t.Fatalf("Update: %v", err)
	}
	if count != 1 {
		t.Errorf("updated %d rows, want 1", count)
	}

	result, _ := conn.Query("test", keys, nil, "", 0, 0)
	if result.Row(0).GetString("value") != "new" {
		t.Errorf("value = %q, want new", result.Row(0).GetString("value"))
	}
}

func TestMemoryDriver_Delete(t *testing.T) {
	conn, _ := Open("memory", "")
	defer conn.Close()

	keys := []DBKey{{Name: "id", Type: DBValInt}}
	conn.Insert("test_del", keys, []DBValue{{Type: DBValInt, IntVal: 1}})
	conn.Insert("test_del", keys, []DBValue{{Type: DBValInt, IntVal: 2}})

	count, err := conn.Delete("test_del", []DBCondition{{Key: "id", Op: "=", Value: DBValue{Type: DBValInt, IntVal: 1}}})
	if err != nil {
		t.Fatalf("Delete: %v", err)
	}
	if count != 1 {
		t.Errorf("deleted %d rows, want 1", count)
	}

	result, _ := conn.Query("test_del", keys, nil, "", 0, 0)
	if result.RowCount() != 1 {
		t.Errorf("expected 1 row after delete, got %d", result.RowCount())
	}
}

func TestMemoryDriver_Replace(t *testing.T) {
	conn, _ := Open("memory", "")
	defer conn.Close()

	keys := []DBKey{{Name: "id", Type: DBValInt}, {Name: "val", Type: DBValString}}
	conn.Insert("test_rep", keys, []DBValue{{Type: DBValInt, IntVal: 1}, {Type: DBValString, StrVal: "first"}})

	// Replace existing
	conn.Replace("test_rep", keys, []DBValue{{Type: DBValInt, IntVal: 1}, {Type: DBValString, StrVal: "updated"}})
	result, _ := conn.Query("test_rep", keys, nil, "", 0, 0)
	if result.RowCount() != 1 {
		t.Fatalf("expected 1 row, got %d", result.RowCount())
	}
	if result.Row(0).GetString("val") != "updated" {
		t.Errorf("val = %q, want updated", result.Row(0).GetString("val"))
	}

	// Replace new (insert)
	conn.Replace("test_rep", keys, []DBValue{{Type: DBValInt, IntVal: 2}, {Type: DBValString, StrVal: "new"}})
	result, _ = conn.Query("test_rep", keys, nil, "", 0, 0)
	if result.RowCount() != 2 {
		t.Errorf("expected 2 rows, got %d", result.RowCount())
	}
}

func TestRedisDriver_Register(t *testing.T) {
	d := GetDriver("redis")
	if d == nil {
		t.Fatal("expected redis driver to be registered")
	}
	if d.Name() != "redis" {
		t.Errorf("driver name = %q, want redis", d.Name())
	}
}

func TestRedisDriver_Open(t *testing.T) {
	conn, err := Open("redis", "redis://localhost:6379/0")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	defer conn.Close()

	if err := conn.Ping(); err != nil {
		t.Fatalf("Ping: %v", err)
	}
}

func TestDBValue(t *testing.T) {
	v := DBValue{Type: DBValString, StrVal: "hello"}
	if v.String() != "hello" {
		t.Errorf("String() = %q", v.String())
	}

	v2 := DBValue{Type: DBValInt, IntVal: 42}
	if v2.Int() != 42 {
		t.Errorf("Int() = %d", v2.Int())
	}

	v3 := DBValue{Type: DBValNull, IsNull: true}
	if v3.String() != "" {
		t.Errorf("null String() should be empty")
	}
}

func TestRegisteredDrivers(t *testing.T) {
	names := RegisteredDrivers()
	found := false
	for _, n := range names {
		if n == "memory" {
			found = true
		}
	}
	if !found {
		t.Error("expected 'memory' in registered drivers")
	}
}

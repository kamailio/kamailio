// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for the DB-backed auth store (auth_db.go).
 */

package auth

import (
	"context"
	"errors"
	"testing"
)

func TestDBAuthStore_Lookup_Found(t *testing.T) {
	store := NewInMemoryStore()
	store.AddUser("alice", "s3cr3t", "", "sip.example.com")

	c, err := store.Lookup(context.Background(), "alice")
	if err != nil {
		t.Fatalf("Lookup: %v", err)
	}
	if c == nil {
		t.Fatalf("got nil credentials")
	}
	if c.Username != "alice" {
		t.Errorf("username = %q, want alice", c.Username)
	}
	if c.Password != "s3cr3t" {
		t.Errorf("password = %q, want s3cr3t", c.Password)
	}
	if c.Realm != "sip.example.com" {
		t.Errorf("realm = %q, want sip.example.com", c.Realm)
	}
}

func TestDBAuthStore_Lookup_NotFound(t *testing.T) {
	store := NewInMemoryStore()
	store.AddUser("alice", "s3cr3t", "", "r")

	c, err := store.Lookup(context.Background(), "bob")
	if !errors.Is(err, ErrUserNotFound) {
		t.Errorf("expected ErrUserNotFound, got err=%v creds=%+v", err, c)
	}
	if c != nil {
		t.Errorf("expected nil credentials on missing user, got %+v", c)
	}
}

func TestDBAuthStore_Lookup_EmptyUsername(t *testing.T) {
	store := NewInMemoryStore()
	store.AddUser("alice", "s3cr3t", "", "r")

	c, err := store.Lookup(context.Background(), "")
	if err == nil {
		t.Errorf("expected error for empty username, got nil (creds=%+v)", c)
	}
	if c != nil {
		t.Errorf("expected nil credentials on empty username, got %+v", c)
	}
}

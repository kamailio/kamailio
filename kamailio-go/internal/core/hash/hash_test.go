// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for core/hash package
 */

package hash

import (
	"testing"
)

func TestHashString(t *testing.T) {
	h1 := HashString("test")
	h2 := HashString("test")
	h3 := HashString("other")

	if h1 != h2 {
		t.Error("same string should produce same hash")
	}
	if h1 == h3 {
		t.Error("different strings should produce different hash")
	}
}

func TestHashBytes(t *testing.T) {
	h1 := HashBytes([]byte("test"))
	h2 := HashBytes([]byte("test"))

	if h1 != h2 {
		t.Error("same bytes should produce same hash")
	}
}

func TestHashStringDeterministic(t *testing.T) {
	// Hash function should be deterministic
	for i := 0; i < 10; i++ {
		h := HashString("deterministic")
		if h == 0 {
			t.Error("hash should not be zero for non-empty input")
		}
	}
}

func TestMemberHash(t *testing.T) {
	h1 := MemberHash([]byte("call-id"), []byte("cseq"), []byte("branch"))
	h2 := MemberHash([]byte("call-id"), []byte("cseq"), []byte("branch"))

	if h1 != h2 {
		t.Error("same inputs should produce same member hash")
	}
}

func TestMemberHashDifferentInputs(t *testing.T) {
	h1 := MemberHash([]byte("call-id-1"), []byte("cseq"), []byte("branch"))
	h2 := MemberHash([]byte("call-id-2"), []byte("cseq"), []byte("branch"))

	if h1 == h2 {
		t.Error("different callids should produce different hashes")
	}
}

func TestMemberHashEmptyBranch(t *testing.T) {
	// Empty branch should still produce valid hash
	h := MemberHash([]byte("call-id"), []byte("cseq"), nil)
	if h == 0 {
		t.Error("member hash should not be zero")
	}
}

func TestNewTable(t *testing.T) {
	ht := NewTable(1024)
	if ht.size != 1024 {
		t.Errorf("expected size 1024, got %d", ht.size)
	}
	if ht.used != 0 {
		t.Error("new table should be empty")
	}
}

func BenchmarkHashString(b *testing.B) {
	data := []byte("this is a moderately long test string for benchmarking")
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		HashBytes(data)
	}
}

func BenchmarkMemberHash(b *testing.B) {
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		MemberHash([]byte("call-id-12345"), []byte("100"), []byte("branch-xyz"))
	}
}

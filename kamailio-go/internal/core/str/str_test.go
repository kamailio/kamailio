// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for core/str package
 */

package str

import (
	"testing"
)

func TestStrBasic(t *testing.T) {
	s := Mk("hello")
	if s.Len != 5 {
		t.Errorf("expected len 5, got %d", s.Len)
	}
	if s.String() != "hello" {
		t.Errorf("expected 'hello', got '%s'", s.String())
	}
}

func TestStrEqual(t *testing.T) {
	s1 := Mk("test")
	s2 := Mk("test")
	s3 := Mk("other")

	if !s1.Equal(s2) {
		t.Error("expected s1 == s2")
	}
	if s1.Equal(s3) {
		t.Error("expected s1 != s3")
	}
}

func TestStrEqualString(t *testing.T) {
	s := Mk("hello")
	if !s.EqualString("hello") {
		t.Error("expected equal to 'hello'")
	}
	if s.EqualString("world") {
		t.Error("expected not equal to 'world'")
	}
}

func TestStrEmpty(t *testing.T) {
	var empty Str
	if !empty.IsEmpty() {
		t.Error("expected empty to be true")
	}

	s := Mk("")
	if !s.IsEmpty() {
		t.Error("expected Mk empty to be empty")
	}
}

func TestStrClone(t *testing.T) {
	original := Mk("original")
	clone := original.Clone()

	if !original.Equal(clone) {
		t.Error("clone should equal original")
	}

	// Modify clone, original should be unaffected
	clone.S[0] = 'X'
	if original.S[0] == 'X' {
		t.Error("clone should be independent")
	}
}

func TestStrTruncate(t *testing.T) {
	s := Mk("hello world")
	trunc := s.Truncate(5)

	if trunc.Len != 5 {
		t.Errorf("expected len 5, got %d", trunc.Len)
	}
	if trunc.String() != "hello" {
		t.Errorf("expected 'hello', got '%s'", trunc.String())
	}
}

func TestStrSkip(t *testing.T) {
	s := Mk("hello world")
	skip := s.Skip(6)

	if skip.Len != 5 {
		t.Errorf("expected len 5, got %d", skip.Len)
	}
	if skip.String() != "world" {
		t.Errorf("expected 'world', got '%s'", skip.String())
	}
}

func TestStrHasPrefix(t *testing.T) {
	s := Mk("hello world")

	if !s.HasPrefixString("hello") {
		t.Error("expected to have prefix 'hello'")
	}
	if s.HasPrefixString("world") {
		t.Error("expected to not have prefix 'world'")
	}
}

func TestStrHasSuffix(t *testing.T) {
	s := Mk("hello world")

	if !s.HasSuffixString("world") {
		t.Error("expected to have suffix 'world'")
	}
	if s.HasSuffixString("hello") {
		t.Error("expected to not have suffix 'hello'")
	}
}

func TestStrIndex(t *testing.T) {
	s := Mk("hello world")

	idx := s.IndexByte('w')
	if idx != 6 {
		t.Errorf("expected index 6, got %d", idx)
	}

	idx = s.IndexByte('X')
	if idx != -1 {
		t.Errorf("expected -1 for 'X', got %d", idx)
	}
}

func TestSplitByte(t *testing.T) {
	s := Mk("a,b,c")
	parts := SplitByte(s, ',')

	if len(parts) != 3 {
		t.Errorf("expected 3 parts, got %d", len(parts))
	}
	if parts[0].String() != "a" || parts[1].String() != "b" || parts[2].String() != "c" {
		t.Error("unexpected split results")
	}
}

func TestSTR_NULL(t *testing.T) {
	if !STR_NULL.IsEmpty() {
		t.Error("STR_NULL should be empty")
	}
	if STR_NULL.Len != 0 {
		t.Error("STR_NULL.Len should be 0")
	}
}

func TestMkBytes(t *testing.T) {
	b := []byte("test bytes")
	s := MkBytes(b)

	if s.Len != len(b) {
		t.Errorf("expected len %d, got %d", len(b), s.Len)
	}
	if !s.EqualString("test bytes") {
		t.Error("expected equal to 'test bytes'")
	}
}

func BenchmarkStrEqual(b *testing.B) {
	s1 := Mk("this is a moderately long test string for benchmarking")
	s2 := Mk("this is a moderately long test string for benchmarking")
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		s1.Equal(s2)
	}
}

func BenchmarkStrString(b *testing.B) {
	s := Mk("this is a moderately long test string for benchmarking")
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = s.String()
	}
}

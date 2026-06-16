// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * String utilities - counted-length strings matching C str_t semantics
 */

package str

import (
	"bytes"
	"strconv"
)

// Str is a counted-length string structure matching C's str_t
// C: struct _str { char *s; int len; }
//
// Zero-copy: the S field holds a reference to the original buffer.
// This mirrors C behavior where str.s points into the original message buffer.
type Str struct {
	S   []byte // pointer to first character of the string
	Len int    // length of the string
}

// String returns a Go string copy of the Str
func (s Str) String() string {
	if s.Len == 0 || s.S == nil {
		return ""
	}
	return string(s.S[:s.Len])
}

// Bytes returns the raw byte slice (zero-copy reference)
func (s Str) Bytes() []byte {
	if s.Len == 0 || s.S == nil {
		return nil
	}
	return s.S[:s.Len]
}

// IsEmpty returns true if the string is nil or zero-length
func (s Str) IsEmpty() bool {
	return s.Len == 0 || s.S == nil
}

// Equal compares two Strs using memcmp-style comparison (C STR_EQ macro)
func (s Str) Equal(other Str) bool {
	if s.Len != other.Len {
		return false
	}
	if s.Len == 0 {
		return true
	}
	return bytes.Equal(s.S[:s.Len], other.S[:other.Len])
}

// EqualString compares Str with a Go string
func (s Str) EqualString(other string) bool {
	if s.Len != len(other) {
		return false
	}
	if s.Len == 0 {
		return true
	}
	return bytes.Equal(s.S[:s.Len], []byte(other))
}

// HasPrefix tests whether the Str starts with the given prefix
func (s Str) HasPrefix(prefix Str) bool {
	if s.Len < prefix.Len {
		return false
	}
	return bytes.HasPrefix(s.S[:s.Len], prefix.S[:prefix.Len])
}

// HasPrefixString tests whether the Str starts with the given string
func (s Str) HasPrefixString(prefix string) bool {
	return s.HasPrefix(Mk(prefix))
}

// HasSuffix tests whether the Str ends with the given suffix
func (s Str) HasSuffix(suffix Str) bool {
	if s.Len < suffix.Len {
		return false
	}
	return bytes.HasSuffix(s.S[:s.Len], suffix.S[:suffix.Len])
}

// HasSuffixString tests whether the Str ends with the given string
func (s Str) HasSuffixString(suffix string) bool {
	return s.HasSuffix(Mk(suffix))
}

// Index returns the index of the first instance of sep in s, or -1 if sep is not present
func (s Str) Index(sep Str) int {
	if sep.Len == 0 {
		return 0
	}
	if s.Len < sep.Len {
		return -1
	}
	for i := 0; i <= s.Len-sep.Len; i++ {
		if bytes.Equal(s.S[i:i+sep.Len], sep.S[:sep.Len]) {
			return i
		}
	}
	return -1
}

// IndexByte returns the index of the first instance of c in s, or -1 if c is not present
func (s Str) IndexByte(c byte) int {
	for i := 0; i < s.Len; i++ {
		if s.S[i] == c {
			return i
		}
	}
	return -1
}

// Clone creates a new independent copy of the Str (allocates new memory)
func (s Str) Clone() Str {
	if s.Len == 0 || s.S == nil {
		return Str{}
	}
	clone := make([]byte, s.Len)
	copy(clone, s.S[:s.Len])
	return Str{S: clone, Len: s.Len}
}

// Truncate truncates the Str to the given length
func (s Str) Truncate(n int) Str {
	if n > s.Len {
		n = s.Len
	}
	return Str{S: s.S, Len: n}
}

// Skip skips the first n bytes and returns the remaining Str
func (s Str) Skip(n int) Str {
	if n >= s.Len {
		return Str{}
	}
	return Str{S: s.S[n:], Len: s.Len - n}
}

// Mk creates a Str from a Go string (allocates)
func Mk(s string) Str {
	if s == "" {
		return Str{}
	}
	b := make([]byte, len(s))
	copy(b, s)
	return Str{S: b, Len: len(s)}
}

// MkBytes creates a Str from a byte slice (shares the underlying array)
func MkBytes(b []byte) Str {
	if len(b) == 0 {
		return Str{}
	}
	return Str{S: b, Len: len(b)}
}

// MkZ creates a Str from a NUL-terminated C string and length
// (points to the original buffer, does not copy)
func MkZ(s []byte, n int) Str {
	if n <= 0 {
		return Str{}
	}
	// Ensure we don't exceed the buffer
	if n > len(s) {
		n = len(s)
	}
	return Str{S: s, Len: n}
}

// STR_STATIC_INIT creates a static Str from a string literal
// C: #define STR_STATIC_INIT(v) {(v), sizeof(v) - 1}
func STR_STATIC_INIT(v string) Str {
	return Mk(v)
}

// STR_NULL is the null/empty Str constant
var STR_NULL = Str{}

// Join joins multiple Strs into a single Str
func Join(strs ...Str) Str {
	var totalLen int
	for _, s := range strs {
		totalLen += s.Len
	}
	if totalLen == 0 {
		return STR_NULL
	}
	result := make([]byte, totalLen)
	var pos int
	for _, s := range strs {
		copy(result[pos:], s.S[:s.Len])
		pos += s.Len
	}
	return Str{S: result, Len: totalLen}
}

// SplitByte splits s into a slice of Strs using the given separator byte
func SplitByte(s Str, sep byte) []Str {
	if s.Len == 0 {
		return nil
	}
	var result []Str
	var start int
	for i := 0; i < s.Len; i++ {
		if s.S[i] == sep {
			if i > start {
				result = append(result, Str{S: s.S[start:i], Len: i - start})
			}
			start = i + 1
		}
	}
	if start < s.Len {
		result = append(result, Str{S: s.S[start:s.Len], Len: s.Len - start})
	}
	return result
}

// ParseUint parses an unsigned integer from the Str
// Returns the value and an error if parsing fails
func ParseUint(s Str, base int) (uint64, error) {
	if s.Len == 0 {
		return 0, nil
	}
	return strconv.ParseUint(s.String(), base, 64)
}

// ParseInt parses a signed integer from the Str
func ParseInt(s Str, base int) (int64, error) {
	if s.Len == 0 {
		return 0, nil
	}
	return strconv.ParseInt(s.String(), base, 64)
}

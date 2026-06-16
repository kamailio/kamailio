// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Generic header parameter parser - matching C parse_param.c / param_parser.h
 *
 * Handles ";name=value;name2=value2" style parameter lists, including
 * quoted values, values with escapes, and bare names without '='.
 */

package parser

import (
	"bytes"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// Param represents a generic header parameter (name=value)
// C: struct param_hdr_field
type Param struct {
	Name  str.Str
	Value str.Str
	Next  *Param
}

// ParseParams parses ";name=value;name2=value2" parameters.
//
// Returns first param in linked list and total count.
// Handles quoted values, escaped characters, and bare names without '='.
// sep is the separator character (typically ';').
//
// The function is careful not to split on separators that appear inside
// quoted strings, so values like ";foo="a;b"" are correctly preserved.
func ParseParams(s string, sep byte) (*Param, int) {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil, 0
	}

	// If the input starts with a leading separator, skip it.
	// E.g. ";foo=bar;baz" should still produce foo and baz.
	if len(s) > 0 && s[0] == sep {
		s = s[1:]
		if s == "" {
			return nil, 0
		}
	}

	var first *Param
	var last *Param
	count := 0

	// Split into segments respecting quoted regions.
	// We do manual splitting to avoid splitting inside "...".
	start := 0
	n := len(s)
	inQuotes := false
	for i := 0; i < n; i++ {
		c := s[i]
		if c == '\\' && inQuotes && i+1 < n {
			// Skip escaped character
			i++
			continue
		}
		if c == '"' {
			inQuotes = !inQuotes
			continue
		}
		if c == sep && !inQuotes {
			seg := s[start:i]
			if p := parseOneParam(seg); p != nil {
				if first == nil {
					first = p
				} else {
					last.Next = p
				}
				last = p
				count++
			}
			start = i + 1
		}
	}
	// Final segment
	if start < n {
		seg := s[start:]
		if p := parseOneParam(seg); p != nil {
			if first == nil {
				first = p
			} else {
				last.Next = p
			}
			last = p
			count++
		}
	}

	return first, count
}

// parseOneParam parses a single "name=value" segment into a Param.
// Returns nil if the segment is empty or whitespace-only.
func parseOneParam(seg string) *Param {
	seg = strings.TrimSpace(seg)
	if seg == "" {
		return nil
	}

	var name, value string

	// Search for '=' outside quoted regions so that values like
	// 'boundary="----=_Part_0_123=456"' are handled correctly.
	inQuotes := false
	eqIdx := -1
	for i := 0; i < len(seg); i++ {
		c := seg[i]
		if c == '\\' && inQuotes && i+1 < len(seg) {
			i++
			continue
		}
		if c == '"' {
			inQuotes = !inQuotes
			continue
		}
		if c == '=' && !inQuotes {
			eqIdx = i
			break
		}
	}

	if eqIdx != -1 {
		name = strings.TrimSpace(seg[:eqIdx])
		value = strings.TrimSpace(seg[eqIdx+1:])
		value = unquoteParam(value)
	} else {
		name = seg
		value = ""
	}

	if name == "" {
		return nil
	}

	return &Param{
		Name:  str.Mk(name),
		Value: str.Mk(value),
	}
}

// ParseParamsToMap parses params to a Go map (convenience, not zero-copy).
// Later values with the same name overwrite earlier ones.
func ParseParamsToMap(s string, sep byte) map[string]string {
	first, _ := ParseParams(s, sep)
	result := make(map[string]string)
	if first == nil {
		return result
	}
	for p := first; p != nil; p = p.Next {
		if p.Name.Len == 0 {
			continue
		}
		result[p.Name.String()] = p.Value.String()
	}
	return result
}

// ParamListGet retrieves a param by name (case-insensitive) from linked list.
// Returns nil if not found.
func ParamListGet(first *Param, name string) *Param {
	if first == nil || name == "" {
		return nil
	}
	for p := first; p != nil; p = p.Next {
		if p.Name.Len == 0 {
			continue
		}
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// ParseParamsStr parses from str.Str instead of string.
// Operates on the underlying bytes directly (zero-copy references).
func ParseParamsStr(s str.Str, sep byte) (*Param, int) {
	if s.Len == 0 || s.S == nil {
		return nil, 0
	}

	// Strip leading/trailing whitespace in the buffer.
	start := 0
	end := s.Len
	for start < end && isSpace(s.S[start]) {
		start++
	}
	for end > start && isSpace(s.S[end-1]) {
		end--
	}
	if start >= end {
		return nil, 0
	}

	// Skip a leading separator if present, e.g. ";foo=bar".
	if s.S[start] == sep {
		start++
		// Skip whitespace again after the separator.
		for start < end && isSpace(s.S[start]) {
			start++
		}
		if start >= end {
			return nil, 0
		}
	}

	buf := s.S
	var first *Param
	var last *Param
	count := 0

	segStart := start
	inQuotes := false
	for i := start; i < end; i++ {
		c := buf[i]
		if c == '\\' && inQuotes && i+1 < end {
			i++
			continue
		}
		if c == '"' {
			inQuotes = !inQuotes
			continue
		}
		if c == sep && !inQuotes {
			if p := parseOneParamStr(buf, segStart, i); p != nil {
				if first == nil {
					first = p
				} else {
					last.Next = p
				}
				last = p
				count++
			}
			segStart = i + 1
		}
	}
	// Final segment
	if segStart < end {
		if p := parseOneParamStr(buf, segStart, end); p != nil {
			if first == nil {
				first = p
			} else {
				last.Next = p
			}
			last = p
			count++
		}
	}

	return first, count
}

// parseOneParamStr parses a single param segment buf[start:end] as zero-copy.
func parseOneParamStr(buf []byte, start, end int) *Param {
	// Trim spaces from segment bounds
	for start < end && isSpace(buf[start]) {
		start++
	}
	for end > start && isSpace(buf[end-1]) {
		end--
	}
	if start >= end {
		return nil
	}

	// Find '=' outside quoted region.
	inQuotes := false
	eqIdx := -1
	for i := start; i < end; i++ {
		c := buf[i]
		if c == '\\' && inQuotes && i+1 < end {
			i++
			continue
		}
		if c == '"' {
			inQuotes = !inQuotes
			continue
		}
		if c == '=' && !inQuotes {
			eqIdx = i
			break
		}
	}

	var nameStart, nameEnd int
	var valStart, valEnd int

	if eqIdx != -1 {
		nameStart = start
		nameEnd = eqIdx
		valStart = eqIdx + 1
		valEnd = end
	} else {
		nameStart = start
		nameEnd = end
		valStart = end
		valEnd = end
	}

	// Trim name
	for nameStart < nameEnd && isSpace(buf[nameStart]) {
		nameStart++
	}
	for nameEnd > nameStart && isSpace(buf[nameEnd-1]) {
		nameEnd--
	}
	if nameStart >= nameEnd {
		return nil
	}

	// Trim and unquote value
	for valStart < valEnd && isSpace(buf[valStart]) {
		valStart++
	}
	for valEnd > valStart && isSpace(buf[valEnd-1]) {
		valEnd--
	}
	valStart, valEnd = unquoteParamBytes(buf, valStart, valEnd)

	return &Param{
		Name:  str.Str{S: buf[nameStart:nameEnd:nameEnd], Len: nameEnd - nameStart},
		Value: str.Str{S: buf[valStart:valEnd:valEnd], Len: valEnd - valStart},
	}
}

// unquoteParamBytes trims surrounding double quotes from buf[start:end].
// Returns updated start/end (unchanged if not quoted).
func unquoteParamBytes(buf []byte, start, end int) (int, int) {
	if end-start >= 2 && buf[start] == '"' && buf[end-1] == '"' {
		return start + 1, end - 1
	}
	return start, end
}

// isSpace reports whether c is ASCII whitespace (space or tab).
func isSpace(c byte) bool {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n'
}

// unquoteParam removes surrounding double quotes from v.
func unquoteParam(v string) string {
	v = strings.TrimSpace(v)
	if len(v) >= 2 && v[0] == '"' && v[len(v)-1] == '"' {
		return v[1 : len(v)-1]
	}
	return v
}

// String renders the linked param list back to ";name=value;..." form.
func (p *Param) String() string {
	if p == nil {
		return ""
	}
	var sb strings.Builder
	for cur := p; cur != nil; cur = cur.Next {
		if sb.Len() > 0 {
			sb.WriteByte(';')
		}
		sb.WriteString(cur.Name.String())
		if cur.Value.Len > 0 {
			sb.WriteByte('=')
			v := cur.Value.String()
			if strings.ContainsAny(v, " ,;=\"") {
				sb.WriteByte('"')
				sb.WriteString(v)
				sb.WriteByte('"')
			} else {
				sb.WriteString(v)
			}
		}
	}
	return sb.String()
}

// Equal reports whether p and other have the same name and value (case sensitive).
func (p *Param) Equal(other *Param) bool {
	if p == nil || other == nil {
		return p == other
	}
	if !bytes.Equal(p.Name.Bytes(), other.Name.Bytes()) {
		return false
	}
	return bytes.Equal(p.Value.Bytes(), other.Value.Bytes())
}

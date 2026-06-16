// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP method table-based lookup - matching C parse_methods.c
 *
 * Replaces the linear if-else chain in defs.go with a table indexed by
 * the first byte of the method name. This keeps the common-case lookup
 * fast and the code small.
 */

package parser

import (
	"bytes"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// MethodEntry is a fast-lookup entry for SIP method names.
type MethodEntry struct {
	Name   string
	Method RequestMethod
}

// methodTable is the master table of known methods - sorted for readability.
// C: static method_desc_t methods[]
var methodTable = []MethodEntry{
	{"INVITE", MethodInvite},
	{"CANCEL", MethodCancel},
	{"ACK", MethodACK},
	{"BYE", MethodBye},
	{"INFO", MethodInfo},
	{"REGISTER", MethodRegister},
	{"SUBSCRIBE", MethodSubscribe},
	{"NOTIFY", MethodNotify},
	{"MESSAGE", MethodMessage},
	{"OPTIONS", MethodOptions},
	{"PRACK", MethodPRACK},
	{"UPDATE", MethodUpdate},
	{"REFER", MethodRefer},
	{"PUBLISH", MethodPublish},
	{"KDMQ", MethodKDMQ},
	{"GET", MethodGet},
	{"POST", MethodPost},
	{"PUT", MethodPut},
	{"DELETE", MethodDelete},
}

// methodIndex groups methodTable entries by their first byte for O(1)
// first-character dispatch.
var methodIndex map[byte][]MethodEntry

func init() {
	methodIndex = make(map[byte][]MethodEntry, 16)
	for _, m := range methodTable {
		first := m.Name[0]
		methodIndex[first] = append(methodIndex[first], m)
	}
}

// ParseMethodBytes parses a method from bytes using the table lookup.
// Returns MethodOther for unrecognised methods; returns MethodUndefined for
// empty input. Comparison is case-insensitive, matching C behaviour.
//
// C: char* parse_method(char* buf, unsigned int len, request_method_t* m)
func ParseMethodBytes(s []byte) RequestMethod {
	if len(s) == 0 {
		return MethodUndefined
	}
	first := s[0]
	if first >= 'a' && first <= 'z' {
		first -= 'a' - 'A'
	}
	entries := methodIndex[first]
	if entries == nil {
		return MethodOther
	}
	for _, m := range entries {
		if len(s) != len(m.Name) {
			continue
		}
		match := true
		for i := 0; i < len(s); i++ {
			c := s[i]
			if c >= 'a' && c <= 'z' {
				c -= 'a' - 'A'
			}
			if c != m.Name[i] {
				match = false
				break
			}
		}
		if match {
			return m.Method
		}
	}
	return MethodOther
}

// ParseMethodStr parses a method from a str.Str.
func ParseMethodStr(s str.Str) RequestMethod {
	return ParseMethodBytes(s.Bytes())
}

// MethodNameFast returns the string name for a method by reverse table lookup.
// If the method is not known, returns "" for MethodUndefined and "OTHER"
// for MethodOther / unknowns - mirroring the style of MethodName in defs.go.
func MethodNameFast(m RequestMethod) string {
	for _, entry := range methodTable {
		if entry.Method == m {
			return entry.Name
		}
	}
	if m == MethodUndefined {
		return ""
	}
	return "OTHER"
}

// IsMethod reports whether the bytes match a specific method (case-insensitive,
// exact length / content match).
func IsMethod(s []byte, m RequestMethod) bool {
	return ParseMethodBytes(s) == m
}

// CompareStrMethod compares a str.Str to a constant method name.
func CompareStrMethod(s str.Str, name string) bool {
	if s.Len != len(name) {
		return false
	}
	return bytes.EqualFold(s.Bytes(), []byte(name))
}

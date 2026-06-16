// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Expires / Min-Expires header body parser - matching C parse_expires.c
 *
 * This module focuses on the RFC 3261 delta-seconds form of Expires:
 *
 *   Expires = "Expires" HCOLON delta-seconds
 *   delta-seconds = 1*DIGIT
 *
 * A sibling parser in parse_date.go handles the (rare) SIP-date form and
 * returns a time.Time; the functions here are named *Body to stay out of
 * the way and make clear they return a parsed integer-seconds struct.
 *
 * Min-Expires shares the same ABNF form and reuses the same logic.
 */

package parser

import (
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ExpiresBody represents a parsed Expires / Min-Expires header.
// C: struct expires_body
type ExpiresBody struct {
	Value uint32 // seconds value
	Error bool   // parse error flag (mirrors C error field)
}

// ParseExpiresBody parses an Expires header body as delta-seconds (uint32).
// Returns nil + error if the body is empty, negative, or non-numeric.
//
// C: char *parse_expires(char *buf, unsigned int len, struct expires_body **eb)
func ParseExpiresBody(body str.Str) (*ExpiresBody, error) {
	eb := &ExpiresBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		eb.Error = true
		return nil, &ExpiresBodyError{Msg: "empty expires body"}
	}

	// Tolerate a leading '+'; reject explicit negative signs so the error
	// message stays helpful instead of a generic strconv failure.
	trimmed := s
	if len(trimmed) > 0 && trimmed[0] == '+' {
		trimmed = trimmed[1:]
	}
	if trimmed == "" {
		eb.Error = true
		return nil, &ExpiresBodyError{Msg: "invalid expires value: " + s}
	}
	if trimmed[0] == '-' {
		eb.Error = true
		return nil, &ExpiresBodyError{Msg: "negative expires value: " + s}
	}

	v, err := strconv.ParseUint(trimmed, 10, 32)
	if err != nil {
		eb.Error = true
		return nil, &ExpiresBodyError{Msg: "invalid expires value: " + s}
	}

	eb.Value = uint32(v)
	return eb, nil
}

// ParseMinExpiresBody parses a Min-Expires header body (same format as Expires).
// C: char *parse_min_expires(char *buf, unsigned int len, struct min_expires_body **meb)
func ParseMinExpiresBody(body str.Str) (*ExpiresBody, error) {
	return ParseExpiresBody(body)
}

// String returns the Expires value as its string representation.
func (eb *ExpiresBody) String() string {
	if eb == nil {
		return ""
	}
	return strconv.FormatUint(uint64(eb.Value), 10)
}

// ParseExpiresBodyFromHeader parses Expires from a HdrField.
func ParseExpiresBodyFromHeader(hdr *HdrField) (*ExpiresBody, error) {
	if hdr == nil {
		return nil, &ExpiresBodyError{Msg: "nil header"}
	}
	return ParseExpiresBody(hdr.Body)
}

// ParseMinExpiresBodyFromHeader parses Min-Expires from a HdrField.
func ParseMinExpiresBodyFromHeader(hdr *HdrField) (*ExpiresBody, error) {
	if hdr == nil {
		return nil, &ExpiresBodyError{Msg: "nil header"}
	}
	return ParseMinExpiresBody(hdr.Body)
}

// ExpiresBodyError represents an Expires / Min-Expires parsing error.
// Named distinctly from parse_date.go's ExpiresError (which reports time
// parsing failures) to avoid a package-level redeclaration.
type ExpiresBodyError struct {
	Msg string
}

func (e *ExpiresBodyError) Error() string {
	return e.Msg
}

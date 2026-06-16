// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * CSeq header parser - matching C parse_cseq.c
 *
 * CSeq = "CSeq" HCOLON 1*DIGIT LWS method
 * method = OPTIONS | INVITE | ACK | CANCEL | BYE | REGISTER | PRACK | UPDATE | MESSAGE | SUBSCRIBE | NOTIFY | REFER | INFO | OPTIONS
 */

package parser

import (
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ParseCSeq parses a CSeq header body
// C: char *parse_cseq(char *buffer, char *end, struct cseq_body *const cb)
func ParseCSeq(body str.Str) (*CSeqBody, error) {
	cb := &CSeqBody{}

	s := string(body.S)
	if body.Len > 0 && len(body.S) > body.Len {
		s = string(body.S[:body.Len])
	}

	// Find the first whitespace (separates number from method)
	s = strings.TrimSpace(s)

	// Parse number
	spaceIdx := strings.IndexFunc(s, func(r rune) bool {
		return r == ' ' || r == '\t'
	})

	var numStr string
	var methodStr string

	if spaceIdx != -1 {
		numStr = s[:spaceIdx]
		methodStr = strings.TrimSpace(s[spaceIdx:])
	} else {
		// No whitespace found, entire string might be just a number
		// or just a method (for some edge cases)
		numStr = s
	}

	// Parse sequence number
	if numStr != "" {
		num, err := strconv.ParseUint(numStr, 10, 32)
		if err != nil {
			return nil, &CSeqError{Msg: "invalid CSeq number: " + numStr}
		}
		cb.Number = uint32(num)
	}

	// Parse method
	if methodStr != "" {
		cb.Method = str.Mk(methodStr)
		cb.MethodValue = ParseMethod([]byte(methodStr))
	}

	return cb, nil
}

// String returns the CSeq as a string
func (cb *CSeqBody) String() string {
	return strconv.FormatUint(uint64(cb.Number), 10) + " " + cb.Method.String()
}

// Compare compares two CSeq bodies
// Returns:
//   0  if equal
//  <0  if cb < other
//  >0  if cb > other
func (cb *CSeqBody) Compare(other *CSeqBody) int {
	if cb.Number != other.Number {
		if cb.Number < other.Number {
			return -1
		}
		return 1
	}
	// Numbers equal, compare methods
	if cb.Method.Len == 0 && other.Method.Len == 0 {
		return 0
	}
	if cb.Method.Len == 0 {
		return -1
	}
	if other.Method.Len == 0 {
		return 1
	}
	return strings.Compare(cb.Method.String(), other.Method.String())
}

// IsMethod checks if the CSeq method matches the given method
func (cb *CSeqBody) IsMethod(m RequestMethod) bool {
	return cb.MethodValue == m
}

// IsInvite checks if this is an INVITE CSeq
func (cb *CSeqBody) IsInvite() bool {
	return cb.IsMethod(MethodInvite)
}

// IsAck checks if this is an ACK CSeq
func (cb *CSeqBody) IsAck() bool {
	return cb.IsMethod(MethodACK)
}

// IsCancel checks if this is a CANCEL CSeq
func (cb *CSeqBody) IsCancel() bool {
	return cb.IsMethod(MethodCancel)
}

// IsBye checks if this is a BYE CSeq
func (cb *CSeqBody) IsBye() bool {
	return cb.IsMethod(MethodBye)
}

// IsError returns true if there was a parsing error
func (cb *CSeqBody) IsError() bool {
	return cb.Method.Len == 0 || cb.Number == 0
}

// CSeqError represents a parsing error
type CSeqError struct {
	Msg string
}

func (e *CSeqError) Error() string {
	return e.Msg
}

// ParseCSeqFromHeader parses CSeq from a header field
func ParseCSeqFromHeader(hdr *HdrField) (*CSeqBody, error) {
	if hdr == nil {
		return nil, &CSeqError{Msg: "nil header"}
	}
	return ParseCSeq(hdr.Body)
}

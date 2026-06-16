// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Retry-After header parser - matching C parse_rr.h and RFC 3261 Section 20.33.
 *
 * Retry-After = "Retry-After" HCOLON delta-seconds [ comment ]
 *                  *( SEMI retry-param )
 * retry-param = ("duration" EQUAL delta-seconds) / generic-param
 *
 * Refer-To header parser - matching C parse_refer.h and RFC 3515.
 *
 * Refer-To = ("Refer-To" / "r") HCOLON name-addr / addr-spec
 * name-addr = [ display-name ] LAQUOT addr-spec RAQUOT *( SEMI generic-param )
 */

package parser

import (
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ================= Retry-After =================

// RetryAfterBody represents a parsed Retry-After header body
// C: struct retry_after_body
type RetryAfterBody struct {
	DeltaSeconds uint32
	Duration     uint32
	Comment      str.Str
	Params       *GenericParam
	LastParam    *GenericParam
}

// GenericParam is a key-value pair used by various parsers
type GenericParam struct {
	Name  str.Str
	Value str.Str
	Next  *GenericParam
}

// ParseRetryAfter parses a Retry-After header body
// C: char *parse_retry_after(char *buf, char *end, struct retry_after_body **rab)
func ParseRetryAfter(body str.Str) (*RetryAfterBody, error) {
	rab := &RetryAfterBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &RetryAfterError{Msg: "empty retry-after body"}
	}

	// Split off optional comment (in parentheses)
	if idx := strings.Index(s, "("); idx != -1 {
		if endIdx := strings.Index(s[idx:], ")"); endIdx != -1 {
			comment := strings.TrimSpace(s[idx+1 : idx+endIdx])
			rab.Comment = str.Mk(comment)
			s = strings.TrimSpace(s[:idx] + s[idx+endIdx+1:])
		}
	}

	// Split on semicolons
	parts := strings.Split(s, ";")
	// First part is delta-seconds
	first := strings.TrimSpace(parts[0])
	val, err := strconv.ParseUint(first, 10, 32)
	if err != nil {
		return nil, &RetryAfterError{Msg: "invalid delta-seconds: " + first}
	}
	rab.DeltaSeconds = uint32(val)

	// Remaining parts are parameters
	for _, p := range parts[1:] {
		p = strings.TrimSpace(p)
		if p == "" {
			continue
		}
		eq := strings.Index(p, "=")
		var name, value string
		if eq == -1 {
			name = p
		} else {
			name = strings.TrimSpace(p[:eq])
			value = stripQuotes(strings.TrimSpace(p[eq+1:]))
		}

		if strings.EqualFold(name, "duration") {
			d, err := strconv.ParseUint(value, 10, 32)
			if err == nil {
				rab.Duration = uint32(d)
			}
		} else {
			param := &GenericParam{
				Name:  str.Mk(name),
				Value: str.Mk(value),
			}
			if rab.Params == nil {
				rab.Params = param
				rab.LastParam = param
			} else {
				rab.LastParam.Next = param
				rab.LastParam = param
			}
		}
	}

	return rab, nil
}

// GetParam returns the param with the given name
func (rab *RetryAfterBody) GetParam(name string) *GenericParam {
	for p := rab.Params; p != nil; p = p.Next {
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// String returns the Retry-After body as string
func (rab *RetryAfterBody) String() string {
	var sb strings.Builder
	sb.WriteString(strconv.FormatUint(uint64(rab.DeltaSeconds), 10))
	if rab.Duration > 0 {
		sb.WriteString("; duration=")
		sb.WriteString(strconv.FormatUint(uint64(rab.Duration), 10))
	}
	if rab.Comment.Len > 0 {
		sb.WriteString(" (")
		sb.WriteString(rab.Comment.String())
		sb.WriteString(")")
	}
	for p := rab.Params; p != nil; p = p.Next {
		sb.WriteString("; ")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// ParseRetryAfterFromHeader parses Retry-After from a header field
func ParseRetryAfterFromHeader(hdr *HdrField) (*RetryAfterBody, error) {
	if hdr == nil {
		return nil, &RetryAfterError{Msg: "nil header"}
	}
	return ParseRetryAfter(hdr.Body)
}

// RetryAfterError represents a Retry-After parsing error
type RetryAfterError struct {
	Msg string
}

func (e *RetryAfterError) Error() string {
	return e.Msg
}

// ================= Refer-To =================

// ReferToBody represents a parsed Refer-To header body
// C: struct refer_to_body
type ReferToBody struct {
	DisplayName str.Str
	URI         *SIPURI
	URIString   str.Str
	Params      *GenericParam
	LastParam   *GenericParam
}

// ParseReferTo parses a Refer-To header body
// C: char *parse_refer_to(char *buf, char *end, struct refer_to_body **rtb)
func ParseReferTo(body str.Str) (*ReferToBody, error) {
	rtb := &ReferToBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &ReferToError{Msg: "empty refer-to body"}
	}

	// Check for name-addr form: "display" <uri>;params
	laquot := strings.Index(s, "<")
	raquot := strings.LastIndex(s, ">")
	rest := ""

	if laquot != -1 && raquot != -1 && raquot > laquot {
		// Extract display name
		display := strings.TrimSpace(s[:laquot])
		display = stripQuotes(display)
		if display != "" {
			rtb.DisplayName = str.Mk(display)
		}
		// Extract URI
		uriStr := strings.TrimSpace(s[laquot+1 : raquot])
		rtb.URIString = str.Mk(uriStr)
		parsed, err := ParseURI(uriStr)
		if err == nil {
			rtb.URI = parsed
		}
		rest = strings.TrimSpace(s[raquot+1:])
	} else {
		// addr-spec form - split off parameters
		semi := strings.Index(s, ";")
		if semi != -1 {
			uriStr := strings.TrimSpace(s[:semi])
			rtb.URIString = str.Mk(uriStr)
			parsed, err := ParseURI(uriStr)
			if err == nil {
				rtb.URI = parsed
			}
			rest = strings.TrimSpace(s[semi+1:])
		} else {
			rtb.URIString = str.Mk(s)
			parsed, err := ParseURI(s)
			if err == nil {
				rtb.URI = parsed
			}
		}
	}

	// Parse parameters
	if strings.HasPrefix(rest, ";") {
		rest = strings.TrimSpace(rest[1:])
	}
	if rest != "" {
		for _, p := range strings.Split(rest, ";") {
			p = strings.TrimSpace(p)
			if p == "" {
				continue
			}
			eq := strings.Index(p, "=")
			var name, value string
			if eq == -1 {
				name = p
			} else {
				name = strings.TrimSpace(p[:eq])
				value = stripQuotes(strings.TrimSpace(p[eq+1:]))
			}
			param := &GenericParam{
				Name:  str.Mk(name),
				Value: str.Mk(value),
			}
			if rtb.Params == nil {
				rtb.Params = param
				rtb.LastParam = param
			} else {
				rtb.LastParam.Next = param
				rtb.LastParam = param
			}
		}
	}

	return rtb, nil
}

// GetParam returns the param with the given name
func (rtb *ReferToBody) GetParam(name string) *GenericParam {
	for p := rtb.Params; p != nil; p = p.Next {
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// String returns the Refer-To body as string
func (rtb *ReferToBody) String() string {
	var sb strings.Builder
	if rtb.DisplayName.Len > 0 {
		sb.WriteString("\"")
		sb.WriteString(rtb.DisplayName.String())
		sb.WriteString("\" ")
	}
	sb.WriteString("<")
	sb.WriteString(rtb.URIString.String())
	sb.WriteString(">")
	for p := rtb.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// ParseReferToFromHeader parses Refer-To from a header field
func ParseReferToFromHeader(hdr *HdrField) (*ReferToBody, error) {
	if hdr == nil {
		return nil, &ReferToError{Msg: "nil header"}
	}
	return ParseReferTo(hdr.Body)
}

// ReferToError represents a Refer-To parsing error
type ReferToError struct {
	Msg string
}

func (e *ReferToError) Error() string {
	return e.Msg
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Content-Type / MIME type parser - matching C parse_content.c
 *
 * Content-Type   = "Content-Type" HCOLON media-type
 * media-type     = type "/" subtype *( ";" parameter )
 * type           = token
 * subtype        = token
 * parameter      = attribute "=" value
 * attribute      = token
 * value          = token / quoted-string
 */

package parser

import (
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ContentBody represents a parsed Content-Type (or Content-Disposition) header.
// C: struct content_body / struct disposition_body
type ContentBody struct {
	Type     str.Str // e.g. "application", "text"
	Subtype  str.Str // e.g. "sdp", "plain"
	Params   *Param  // parameter linked list
	Boundary str.Str // for multipart - convenience
}

// MIME type constants for quick matching.
const (
	MimeApplicationSDP   = "application/sdp"
	MimeApplicationJSON  = "application/json"
	MimeTextPlain        = "text/plain"
	MimeMultipartMixed   = "multipart/mixed"
	MimeMultipartRelated = "multipart/related"
	MimeApplicationXML   = "application/xml"
)

// ParseContentType parses a Content-Type header body.
//
// Examples:
//
//	"application/sdp"
//	"application/sdp; charset=utf-8; handling=required"
//	'multipart/mixed; boundary="----=_Part_0_123"'
func ParseContentType(body str.Str) (*ContentBody, error) {
	s := strings.TrimSpace(body.String())
	if s == "" {
		return nil, &ContentError{Msg: "empty content-type body"}
	}

	cb := &ContentBody{}

	// Find first ';' outside quoted region to split type/subtype from params.
	inQuotes := false
	sepIdx := -1
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '\\' && inQuotes && i+1 < len(s) {
			i++
			continue
		}
		if c == '"' {
			inQuotes = !inQuotes
			continue
		}
		if c == ';' && !inQuotes {
			sepIdx = i
			break
		}
	}

	var mediaType, paramsStr string
	if sepIdx != -1 {
		mediaType = strings.TrimSpace(s[:sepIdx])
		paramsStr = s[sepIdx+1:]
	} else {
		mediaType = s
		paramsStr = ""
	}

	// Split type/subtype on the first '/'.
	slashIdx := strings.Index(mediaType, "/")
	if slashIdx == -1 {
		// No subtype - still a valid token style in some edge cases.
		t := strings.TrimSpace(mediaType)
		if t == "" {
			return nil, &ContentError{Msg: "missing content-type"}
		}
		cb.Type = str.Mk(t)
	} else {
		t := strings.TrimSpace(mediaType[:slashIdx])
		st := strings.TrimSpace(mediaType[slashIdx+1:])
		if t == "" {
			return nil, &ContentError{Msg: "missing content-type type"}
		}
		if st == "" {
			return nil, &ContentError{Msg: "missing content-type subtype"}
		}
		cb.Type = str.Mk(t)
		cb.Subtype = str.Mk(st)
	}

	// Parse parameters, if any.
	if strings.TrimSpace(paramsStr) != "" {
		params, _ := ParseParams(paramsStr, ';')
		cb.Params = params

		// Convenience: extract "boundary" for multipart types.
		if bp := ParamListGet(params, "boundary"); bp != nil {
			cb.Boundary = str.Mk(bp.Value.String())
		}
	}

	return cb, nil
}

// TypeString returns "type/subtype" as a Go string.
// If the receiver or type is nil/empty, returns "".
func (cb *ContentBody) TypeString() string {
	if cb == nil {
		return ""
	}
	if cb.Type.Len == 0 {
		return ""
	}
	result := cb.Type.String()
	if cb.Subtype.Len > 0 {
		result += "/" + cb.Subtype.String()
	}
	return result
}

// Match checks whether the content type matches a given "type/subtype"
// (case-insensitive). Wildcards are supported:
//
//	"*"             -> matches anything
//	"application/*" -> matches any application subtype
//	"*/sdp"         -> matches any type with subtype sdp
//	"application/sdp" -> exact match (case-insensitive)
func (cb *ContentBody) Match(pattern string) bool {
	if cb == nil {
		return false
	}
	pattern = strings.TrimSpace(pattern)
	if pattern == "" {
		return false
	}

	// Support "type/subtype" or just "type" or "*".
	slashIdx := strings.Index(pattern, "/")
	var pType, pSubtype string
	if slashIdx == -1 {
		pType = pattern
		pSubtype = ""
	} else {
		pType = strings.TrimSpace(pattern[:slashIdx])
		pSubtype = strings.TrimSpace(pattern[slashIdx+1:])
	}

	// Type match.
	if pType == "*" {
		// Any type matches.
	} else {
		if !strings.EqualFold(cb.Type.String(), pType) {
			return false
		}
	}

	// Subtype match. If the pattern has no subtype, only match if there is
	// no subtype in this content body either (tolerant check).
	if pSubtype == "" {
		return cb.Subtype.Len == 0 || pType == "*"
	}
	if pSubtype == "*" {
		return true
	}
	return strings.EqualFold(cb.Subtype.String(), pSubtype)
}

// GetParam returns a param by name (case-insensitive).
func (cb *ContentBody) GetParam(name string) *Param {
	if cb == nil {
		return nil
	}
	return ParamListGet(cb.Params, name)
}

// ParseContentLengthValue parses a Content-Length header value
// directly from the raw body bytes (without requiring a HdrField).
//
// The existing ParseContentLength(*HdrField) function in parser.go
// handles parsing from a fully resolved header field.
func ParseContentLengthValue(body str.Str) (uint32, error) {
	s := strings.TrimSpace(body.String())
	if s == "" {
		return 0, &ContentError{Msg: "empty content-length"}
	}
	v, err := strconv.ParseUint(s, 10, 32)
	if err != nil {
		return 0, &ContentError{Msg: "invalid content-length: " + s}
	}
	return uint32(v), nil
}

// String returns the Content-Type body reconstructed as string,
// including parameters. Values containing special characters are re-quoted.
func (cb *ContentBody) String() string {
	if cb == nil {
		return ""
	}
	var sb strings.Builder
	sb.WriteString(cb.TypeString())
	for p := cb.Params; p != nil; p = p.Next {
		sb.WriteString("; ")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			v := p.Value.String()
			if strings.ContainsAny(v, " ,;=\"") {
				sb.WriteString("\"" + v + "\"")
			} else {
				sb.WriteString(v)
			}
		}
	}
	return sb.String()
}

// ParseContentTypeFromHeader parses Content-Type from a HdrField.
func ParseContentTypeFromHeader(hdr *HdrField) (*ContentBody, error) {
	if hdr == nil {
		return nil, &ContentError{Msg: "nil header"}
	}
	return ParseContentType(hdr.Body)
}

// IsMultipart reports whether the type is "multipart" (case-insensitive).
func (cb *ContentBody) IsMultipart() bool {
	if cb == nil {
		return false
	}
	return strings.EqualFold(cb.Type.String(), "multipart")
}

// ContentError represents content-type parsing errors.
type ContentError struct {
	Msg string
}

func (e *ContentError) Error() string {
	return e.Msg
}

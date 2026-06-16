// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP message body parser - matching C parse_body.c
 *
 * Body parsing is driven by Content-Type and Content-Length headers.
 * For well-known types we hand off to specialized parsers (e.g. SDP).
 * Unrecognised bodies are returned as raw bytes so callers can process
 * them (e.g. JSON decoding, XML parsing, multipart dissection).
 */

package parser

import (
	"bytes"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/sdp"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// BodyType represents the high-level parsed body content category.
type BodyType int

const (
	BodyUnknown   BodyType = iota // unrecognised type, raw bytes preserved
	BodySDP                       // application/sdp
	BodyJSON                      // application/json
	BodyXML                       // application/xml or text/xml
	BodyText                      // text/*
	BodyMultipart                 // multipart/*
)

// MessageBody represents a parsed SIP message body.
// C: struct msg_body
type MessageBody struct {
	Type   BodyType    // detected body category
	Raw    str.Str     // raw bytes (slice of original body)
	Parsed interface{} // *sdp.Session for SDP, else nil / string / raw
	Length int         // content-length in bytes
}

// ParseMessageBody parses the message body given a content-type and raw bytes.
//
//   - raw is the raw body (typically the slice of the SIP message buffer after
//     the separating blank line).
//   - contentType is the already-parsed Content-Type header; if nil, the body
//     is treated as BodyUnknown.
//   - contentLength is the explicit Content-Length value. A value of -1 means
//     "use the length of raw". When contentLength >= 0, raw is clipped / padded
//     to that length (clipped only, no padding is added by this function).
func ParseMessageBody(raw []byte, contentType *ContentBody, contentLength int) (*MessageBody, error) {
	mb := &MessageBody{}

	// Compute the effective payload length.
	rawLen := len(raw)
	if contentLength >= 0 {
		// Content-Length wins; if it exceeds rawLen we still return what we
		// have, but record the declared length (matching C's tolerant parsing).
		if contentLength <= rawLen {
			raw = raw[:contentLength]
		}
		mb.Length = contentLength
	} else {
		mb.Length = rawLen
	}

	mb.Raw = str.MkBytes(raw)

	if contentType == nil {
		mb.Type = BodyUnknown
		return mb, nil
	}

	mb.Type = detectBodyType(contentType)

	switch mb.Type {
	case BodySDP:
		parsed, err := sdp.Parse(string(raw))
		if err != nil {
			// Keep the raw bytes so callers can still inspect; do not fail the
			// overall parse just because SDP was malformed.
			mb.Parsed = sdpFallback(raw)
			mb.Type = BodyUnknown
		} else {
			mb.Parsed = parsed
		}
	case BodyText:
		mb.Parsed = strings.TrimRight(string(raw), "\r\n")
	case BodyJSON:
		mb.Parsed = bytes.TrimSpace(raw)
	case BodyXML:
		mb.Parsed = bytes.TrimSpace(raw)
	case BodyMultipart:
		mb.Parsed = raw // left to the caller to split on boundary
	default:
		mb.Parsed = nil
	}

	return mb, nil
}

// detectBodyType maps a parsed Content-Type to a BodyType.
func detectBodyType(ct *ContentBody) BodyType {
	if ct == nil {
		return BodyUnknown
	}
	typeLower := strings.ToLower(ct.Type.String())
	subLower := strings.ToLower(ct.Subtype.String())

	switch typeLower {
	case "application":
		switch subLower {
		case "sdp":
			return BodySDP
		case "json":
			return BodyJSON
		case "xml":
			return BodyXML
		}
	case "text":
		if subLower == "xml" {
			return BodyXML
		}
		return BodyText
	case "multipart":
		return BodyMultipart
	}
	return BodyUnknown
}

// sdpFallback is used when the real SDP parser fails; we return a minimal
// line-based summary so debugging / logging still has useful information.
func sdpFallback(raw []byte) interface{} {
	type simpleSDP struct {
		Lines    []string
		HasAudio bool
		HasVideo bool
		Origin   string
	}
	s := &simpleSDP{}
	for _, line := range bytes.Split(raw, []byte("\n")) {
		line = bytes.TrimSpace(line)
		if len(line) < 2 {
			continue
		}
		s.Lines = append(s.Lines, string(line))
		if bytes.HasPrefix(line, []byte("o=")) {
			s.Origin = string(line[2:])
		}
		if bytes.HasPrefix(line, []byte("m=audio")) {
			s.HasAudio = true
		}
		if bytes.HasPrefix(line, []byte("m=video")) {
			s.HasVideo = true
		}
	}
	return s
}

// IsSDP reports whether the parsed body is (or was) an SDP session.
func (mb *MessageBody) IsSDP() bool {
	if mb == nil {
		return false
	}
	if mb.Type == BodySDP {
		return true
	}
	// Tolerance: if parsing fell back but raw bytes look like SDP, still say yes.
	if mb.Raw.Len >= 4 && bytes.HasPrefix(bytes.TrimSpace(mb.Raw.Bytes()), []byte("v=")) {
		return true
	}
	return false
}

// Session returns the parsed *sdp.Session if available, otherwise nil.
func (mb *MessageBody) Session() *sdp.Session {
	if mb == nil {
		return nil
	}
	if s, ok := mb.Parsed.(*sdp.Session); ok {
		return s
	}
	return nil
}

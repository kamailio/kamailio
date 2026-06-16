// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP first line parser - matching C parse_fline.c
 *
 * Grammar:
 *   request  = method SP uri SP version CRLF
 *   response = version SP status SP reason CRLF
 *   (version = "SIP/2.0")
 */

package parser

import (
	"bytes"
	"errors"
	"fmt"
	"strconv"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// MsgStart represents the parsed first line of a SIP message
// C: struct msg_start
type MsgStart struct {
	Type   MsgType
	Flags  FirstLineFlags
	Len    int // length including delimiter
	Req    *RequestLine
	Reply  *ReplyLine
}

// RequestLine represents a parsed request line
// C: struct msg_start.u.request
type RequestLine struct {
	Method       str.Str
	URI          str.Str
	Version      str.Str
	MethodValue  RequestMethod
}

// ReplyLine represents a parsed reply line
// C: struct msg_start.u.reply
type ReplyLine struct {
	Version    str.Str
	Status     str.Str
	Reason     str.Str
	StatusCode uint16
}

// IsRequest returns true if the message is a SIP request
func (m *MsgStart) IsRequest() bool {
	return m.Type == MsgRequest && (m.Flags&FLINEFlagProtoSIP) != 0
}

// IsReply returns true if the message is a SIP reply
func (m *MsgStart) IsReply() bool {
	return m.Type == MsgReply && (m.Flags&FLINEFlagProtoSIP) != 0
}

// IsSIP returns true if the message uses SIP protocol
func (m *MsgStart) IsSIP() bool {
	return m.Flags&FLINEFlagProtoSIP != 0
}

// ParseFirstLine parses the first line of a SIP message
// C: char *parse_first_line(char *buffer, unsigned int len, struct msg_start *fl)
//
// Returns the remaining buffer after the first line and the parsed MsgStart
func ParseFirstLine(buf []byte) (*MsgStart, []byte, error) {
	if len(buf) <= 16 {
		return nil, nil, errors.New("message too short")
	}

	fl := &MsgStart{}

	// Check if it's a reply (starts with "SIP/2.0" or "HTTP/1.x")
	if len(buf) >= SIPVersionLen+1 &&
		(buf[0] == 'S' || buf[0] == 's') {
		if caseInsensitiveMatch(buf[:SIPVersionLen], []byte(SIPVersion)) &&
			buf[SIPVersionLen] == ' ' {
			fl.Type = MsgReply
			fl.Flags |= FLINEFlagProtoSIP
			return parseReplyLine(buf, fl)
		}
	}

	// Try to parse as request
	return parseRequestLine(buf, fl)
}

// parseRequestLine parses a SIP request line
// Grammar: method SP uri SP version CRLF
func parseRequestLine(buf []byte, fl *MsgStart) (*MsgStart, []byte, error) {

	// Find end of first line
	nl := bytes.Index(buf, []byte("\r\n"))
	if nl == -1 {
		// Try just LF
		nl = bytes.IndexByte(buf, '\n')
		if nl == -1 {
			return nil, nil, errors.New("no CRLF or LF found")
		}
	}
	firstLineEnd := nl

	// Parse method (first token)
	methodEnd := bytes.IndexByte(buf, ' ')
	if methodEnd == -1 || methodEnd >= firstLineEnd {
		return nil, nil, errors.New("no method found")
	}

	method := str.Str{S: buf, Len: methodEnd}
	methodValue := ParseMethod(method.Bytes())

	// Move past method and space
	pos := methodEnd + 1

	// Parse URI (second token)
	uriEnd := bytes.IndexByte(buf[pos:], ' ')
	if uriEnd == -1 || pos+uriEnd >= firstLineEnd {
		return nil, nil, errors.New("no URI found")
	}
	uriEnd += pos

	uri := str.Str{S: buf[pos:], Len: uriEnd - pos}

	// Move past URI and space
	pos = uriEnd + 1

	// Parse version (third token, should be "SIP/2.0")
	versionEnd := firstLineEnd
	if versionEnd <= pos {
		return nil, nil, errors.New("no version found")
	}

	version := str.Str{S: buf[pos:], Len: versionEnd - pos}

	// Validate version
	if !caseInsensitiveMatch(version.Bytes(), []byte(SIPVersion)) {
		return nil, nil, fmt.Errorf("invalid version: %s", version.String())
	}

	fl.Type = MsgRequest
	fl.Flags |= FLINEFlagProtoSIP
	fl.Len = firstLineEnd + 2 // include CRLF
	fl.Req = &RequestLine{
		Method:      method,
		URI:         uri,
		Version:     version,
		MethodValue: methodValue,
	}

	// Return remaining buffer after first line
	remaining := buf[firstLineEnd+2:]
	return fl, remaining, nil
}

// parseReplyLine parses a SIP reply line
// Grammar: version SP status SP reason CRLF
func parseReplyLine(buf []byte, fl *MsgStart) (*MsgStart, []byte, error) {
	// Find end of first line
	nl := bytes.Index(buf, []byte("\r\n"))
	if nl == -1 {
		nl = bytes.IndexByte(buf, '\n')
		if nl == -1 {
			return nil, nil, errors.New("no CRLF or LF found")
		}
	}
	firstLineEnd := nl

	// Version is already known (SIP/2.0)
	version := str.Str{S: buf, Len: SIPVersionLen}
	pos := SIPVersionLen + 1 // skip version and space

	if pos >= firstLineEnd {
		return nil, nil, errors.New("no status code found")
	}

	// Parse status code (3-digit number)
	statusEnd := bytes.IndexByte(buf[pos:], ' ')
	if statusEnd == -1 {
		// Status code extends to end of line (no reason phrase)
		statusEnd = firstLineEnd - pos
	} else {
		statusEnd += pos
	}

	status := str.Str{S: buf[pos:], Len: statusEnd - pos}

	// Parse status code as integer
	statusCode, err := strconv.Atoi(status.String())
	if err != nil || statusCode < 100 || statusCode > 699 {
		return nil, nil, fmt.Errorf("invalid status code: %s", status.String())
	}

	// Parse reason phrase
	var reason str.Str
	if statusEnd < firstLineEnd {
		reasonStart := statusEnd + 1
		if reasonStart < firstLineEnd {
			reason = str.Str{S: buf[reasonStart:], Len: firstLineEnd - reasonStart}
		}
	}

	fl.Type = MsgReply
	fl.Flags |= FLINEFlagProtoSIP
	fl.Len = firstLineEnd + 2 // include CRLF
	fl.Reply = &ReplyLine{
		Version:    version,
		Status:     status,
		Reason:     reason,
		StatusCode: uint16(statusCode),
	}

	remaining := buf[firstLineEnd+2:]
	return fl, remaining, nil
}

// caseInsensitiveMatch performs case-insensitive comparison of two byte slices
func caseInsensitiveMatch(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i]|0x20 != b[i]|0x20 {
			return false
		}
	}
	return true
}

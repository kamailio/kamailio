// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Remote-Party-ID header parser - matching C parse_rpid.c.
 *
 * Remote-Party-ID = "Remote-Party-ID" HCOLON
 *                    rpid-value *( COMMA rpid-value )
 * rpid-value = display-name "<" addr-spec ">"
 *              *( SEMI rpid-param )
 * rpid-param = screen-param / privacy-param / party-param / reason-param
 *            / cid-param / generic-param
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// RPIDBody represents a parsed Remote-Party-ID header
// C: struct rpid_body
// e.g. Remote-Party-ID: "Alice" <sip:alice@example.com>;screen=yes;privacy=full;party=calling
type RPIDBody struct {
	DisplayName str.Str
	URI         str.Str
	URIType     URIType
	Screen      str.Str // "yes" or "no" (screened presentation indicator)
	Privacy     str.Str // privacy value: "full", "off", "id", "name", "uri", "user", "header", "session"
	Party       str.Str // party value: "calling" or "called"
	Reason      str.Str // reason parameter
	CID         str.Str // cid parameter
	Params      *Param
	LastParam   *Param
}

// ParseRPID parses a Remote-Party-ID header body
// C: char *parse_rpid(char *buf, unsigned int len, struct rpid_body **rb)
func ParseRPID(body str.Str) (*RPIDBody, error) {
	rb := &RPIDBody{}
	s := strings.TrimSpace(body.String())
	if s == "" {
		return nil, &RPIDError{Msg: "empty rpid body"}
	}

	laquo := strings.IndexByte(s, '<')
	raquo := strings.IndexByte(s, '>')

	if laquo != -1 && raquo != -1 && raquo > laquo {
		display := strings.TrimSpace(s[:laquo])
		if display != "" {
			display = strings.Trim(display, "\"")
			rb.DisplayName = str.Mk(strings.TrimSpace(display))
		}
		uri := strings.TrimSpace(s[laquo+1 : raquo])
		rb.URI = str.Mk(uri)
		rb.URIType = detectURIType(uri)
		if raquo+1 < len(s) {
			rest := strings.TrimSpace(s[raquo+1:])
			if strings.HasPrefix(rest, ";") {
				rest = strings.TrimSpace(rest[1:])
			}
			if rest != "" {
				rb.parseRPIDParams(rest)
			}
		}
	} else {
		semi := strings.IndexByte(s, ';')
		if semi == -1 {
			rb.URI = str.Mk(s)
			rb.URIType = detectURIType(s)
		} else {
			uri := strings.TrimSpace(s[:semi])
			rb.URI = str.Mk(uri)
			rb.URIType = detectURIType(uri)
			paramsStr := strings.TrimSpace(s[semi+1:])
			if paramsStr != "" {
				rb.parseRPIDParams(paramsStr)
			}
		}
	}
	return rb, nil
}

// parseRPIDParams extracts known RPID params from the given param string
func (rb *RPIDBody) parseRPIDParams(s string) {
	params, _ := ParseParams(s, ';')
	for p := params; p != nil; p = p.Next {
		name := strings.ToLower(p.Name.String())
		switch name {
		case "screen":
			rb.Screen = p.Value
		case "privacy":
			rb.Privacy = p.Value
		case "party":
			rb.Party = p.Value
		case "reason":
			rb.Reason = p.Value
		case "cid":
			rb.CID = p.Value
		default:
			newP := &Param{Name: p.Name, Value: p.Value}
			if rb.Params == nil {
				rb.Params = newP
			} else {
				rb.LastParam.Next = newP
			}
			rb.LastParam = newP
		}
	}
}

// IsScreened returns true if screen=yes
func (rb *RPIDBody) IsScreened() bool {
	return strings.EqualFold(rb.Screen.String(), "yes")
}

// IsCalling returns true if party=calling
func (rb *RPIDBody) IsCalling() bool {
	return strings.EqualFold(rb.Party.String(), "calling")
}

// String returns reconstructed RPID value
func (rb *RPIDBody) String() string {
	var sb strings.Builder
	if rb.DisplayName.Len > 0 {
		sb.WriteString("\"")
		sb.WriteString(rb.DisplayName.String())
		sb.WriteString("\" ")
	}
	sb.WriteString("<")
	sb.WriteString(rb.URI.String())
	sb.WriteString(">")
	if rb.Screen.Len > 0 {
		sb.WriteString(";screen=")
		sb.WriteString(rb.Screen.String())
	}
	if rb.Privacy.Len > 0 {
		sb.WriteString(";privacy=")
		sb.WriteString(rb.Privacy.String())
	}
	if rb.Party.Len > 0 {
		sb.WriteString(";party=")
		sb.WriteString(rb.Party.String())
	}
	if rb.Reason.Len > 0 {
		sb.WriteString(";reason=")
		sb.WriteString(rb.Reason.String())
	}
	if rb.CID.Len > 0 {
		sb.WriteString(";cid=")
		sb.WriteString(rb.CID.String())
	}
	for p := rb.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// ParseRPIDFromHeader parses Remote-Party-ID from HdrField
func ParseRPIDFromHeader(hdr *HdrField) (*RPIDBody, error) {
	if hdr == nil {
		return nil, &RPIDError{Msg: "nil header"}
	}
	return ParseRPID(hdr.Body)
}

// RPIDError represents RPID parsing errors
type RPIDError struct {
	Msg string
}

func (e *RPIDError) Error() string {
	return e.Msg
}

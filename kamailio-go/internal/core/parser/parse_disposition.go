// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Content-Disposition header parser - matching C parse_disposition.c.
 *
 * Content-Disposition = "Content-Disposition" HCOLON
 *                        disposition-type
 *                        *( SEMI disposition-param )
 * disposition-type = "session" / "render" / "stream" / "icon" / token
 * disposition-param = handling-param / rendering-param / generic-param
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// DispositionBody represents a parsed Content-Disposition header
// C: struct disposition_body
// e.g. Content-Disposition: session; handling=required; rendering=session
type DispositionBody struct {
	Disposition str.Str // main disposition value (e.g. "session", "render", "session-timer")
	Handling    str.Str // value of handling= param ("required", "optional")
	Rendering   str.Str // value of rendering= param
	Params      *Param  // other params as linked list
	LastParam   *Param
}

// ParseDisposition parses a Content-Disposition header body
// C: char *parse_disposition(char *buf, unsigned int len, struct disposition_body **db)
func ParseDisposition(body str.Str) (*DispositionBody, error) {
	db := &DispositionBody{}
	s := strings.TrimSpace(body.String())
	if s == "" {
		return nil, &DispositionError{Msg: "empty disposition body"}
	}

	semi := strings.IndexByte(s, ';')
	if semi == -1 {
		db.Disposition = str.Mk(s)
		return db, nil
	}
	db.Disposition = str.Mk(strings.TrimSpace(s[:semi]))
	paramsStr := strings.TrimSpace(s[semi+1:])
	if paramsStr != "" {
		params, _ := ParseParams(paramsStr, ';')
		for p := params; p != nil; p = p.Next {
			name := strings.ToLower(p.Name.String())
			switch name {
			case "handling":
				db.Handling = p.Value
			case "rendering":
				db.Rendering = p.Value
			default:
				newP := &Param{Name: p.Name, Value: p.Value}
				if db.Params == nil {
					db.Params = newP
				} else {
					db.LastParam.Next = newP
				}
				db.LastParam = newP
			}
		}
	}
	return db, nil
}

// IsRequired returns true if handling=required
func (db *DispositionBody) IsRequired() bool {
	return strings.EqualFold(db.Handling.String(), "required")
}

// String returns reconstructed Content-Disposition value
func (db *DispositionBody) String() string {
	var sb strings.Builder
	sb.WriteString(db.Disposition.String())
	if db.Handling.Len > 0 {
		sb.WriteString(";handling=")
		sb.WriteString(db.Handling.String())
	}
	if db.Rendering.Len > 0 {
		sb.WriteString(";rendering=")
		sb.WriteString(db.Rendering.String())
	}
	for p := db.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// ParseDispositionFromHeader parses Content-Disposition from HdrField
func ParseDispositionFromHeader(hdr *HdrField) (*DispositionBody, error) {
	if hdr == nil {
		return nil, &DispositionError{Msg: "nil header"}
	}
	return ParseDisposition(hdr.Body)
}

// DispositionError represents parsing errors
type DispositionError struct {
	Msg string
}

func (e *DispositionError) Error() string {
	return e.Msg
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Identity / Identity-Info header parser - matching C parse_identity.c and
 * parse_identityinfo.c, used by STIR/SHAKEN (RFC 8224/8225/8226).
 *
 * Identity = "Identity" HCOLON base64-value *( SEMI identity-param )
 * identity-param = info-param / alg-param / ppt-param / generic-param
 *
 * Identity-Info = "Identity-Info" HCOLON name-addr
 *                 *( SEMI identity-info-param )
 * identity-info-param = alg-param / domain-param / generic-param
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ========== Identity header ==========

// IdentityBody represents a parsed Identity header
// C: struct identity_body
type IdentityBody struct {
	Error     bool
	Token     str.Str
	Info      str.Str
	Alg       str.Str
	Ppt       str.Str
	Params    *Param
	LastParam *Param
}

// ParseIdentity parses an Identity header body
// C: char *parse_identity(char *buf, unsigned int len, struct identity_body **ib)
func ParseIdentity(body str.Str) (*IdentityBody, error) {
	ib := &IdentityBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		ib.Error = true
		return nil, &IdentityError{Msg: "empty identity body"}
	}

	semi := strings.IndexByte(s, ';')
	var tokenStr, paramsStr string
	if semi == -1 {
		tokenStr = s
	} else {
		tokenStr = strings.TrimSpace(s[:semi])
		paramsStr = strings.TrimSpace(s[semi+1:])
	}

	if tokenStr == "" {
		ib.Error = true
		return nil, &IdentityError{Msg: "missing identity token"}
	}
	ib.Token = str.Mk(tokenStr)

	if paramsStr != "" {
		params, _ := ParseParams(paramsStr, ';')
		for p := params; p != nil; p = p.Next {
			name := strings.ToLower(p.Name.String())
			switch name {
			case "info":
				ib.Info = stripAngleBracketsToStr(p.Value.String())
			case "alg":
				ib.Alg = p.Value
			case "ppt":
				ib.Ppt = p.Value
			default:
				param := &Param{Name: p.Name, Value: p.Value}
				if ib.Params == nil {
					ib.Params = param
					ib.LastParam = param
				} else {
					ib.LastParam.Next = param
					ib.LastParam = param
				}
			}
		}
	}

	return ib, nil
}

// ParseIdentityFromHeader parses Identity from a header field
func ParseIdentityFromHeader(hdr *HdrField) (*IdentityBody, error) {
	if hdr == nil {
		return nil, &IdentityError{Msg: "nil header"}
	}
	return ParseIdentity(hdr.Body)
}

// ========== Identity-Info header ==========

// IdentityInfoBody represents a parsed Identity-Info header
// C: struct identityinfo_body
type IdentityInfoBody struct {
	Error     bool
	URI       str.Str
	Domain    str.Str
	Alg       str.Str
	Params    *Param
	LastParam *Param
}

// ParseIdentityInfo parses an Identity-Info header body
// C: char *parse_identityinfo(char *buf, unsigned int len, struct identityinfo_body **iib)
func ParseIdentityInfo(body str.Str) (*IdentityInfoBody, error) {
	iib := &IdentityInfoBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		iib.Error = true
		return nil, &IdentityError{Msg: "empty identity-info body"}
	}

	laquo := strings.IndexByte(s, '<')
	raquo := strings.LastIndexByte(s, '>')
	var paramsStr string

	if laquo != -1 && raquo != -1 && raquo > laquo {
		uri := strings.TrimSpace(s[laquo+1 : raquo])
		iib.URI = str.Mk(uri)
		if raquo+1 < len(s) {
			paramsStr = strings.TrimSpace(s[raquo+1:])
		}
	} else {
		semi := strings.IndexByte(s, ';')
		if semi == -1 {
			iib.URI = str.Mk(s)
		} else {
			iib.URI = str.Mk(strings.TrimSpace(s[:semi]))
			paramsStr = strings.TrimSpace(s[semi+1:])
		}
	}

	if iib.URI.Len == 0 {
		iib.Error = true
		return nil, &IdentityError{Msg: "missing identity-info uri"}
	}

	if paramsStr != "" {
		params, _ := ParseParams(paramsStr, ';')
		for p := params; p != nil; p = p.Next {
			name := strings.ToLower(p.Name.String())
			switch name {
			case "alg":
				iib.Alg = p.Value
			case "domain":
				iib.Domain = p.Value
			default:
				param := &Param{Name: p.Name, Value: p.Value}
				if iib.Params == nil {
					iib.Params = param
					iib.LastParam = param
				} else {
					iib.LastParam.Next = param
					iib.LastParam = param
				}
			}
		}
	}

	return iib, nil
}

// ParseIdentityInfoFromHeader parses Identity-Info from a header field
func ParseIdentityInfoFromHeader(hdr *HdrField) (*IdentityInfoBody, error) {
	if hdr == nil {
		return nil, &IdentityError{Msg: "nil header"}
	}
	return ParseIdentityInfo(hdr.Body)
}

// ========== helpers ==========

// stripAngleBracketsToStr trims surrounding '<' and '>' from a value and returns a str.Str
func stripAngleBracketsToStr(s string) str.Str {
	s = strings.TrimSpace(s)
	if len(s) >= 2 && s[0] == '<' && s[len(s)-1] == '>' {
		return str.Mk(strings.TrimSpace(s[1 : len(s)-1]))
	}
	return str.Mk(s)
}

// ========== String methods ==========

// String returns the Identity header as a string
func (ib *IdentityBody) String() string {
	var sb strings.Builder
	sb.WriteString(ib.Token.String())

	if ib.Info.Len > 0 {
		sb.WriteString(";info=<")
		sb.WriteString(ib.Info.String())
		sb.WriteString(">")
	}
	if ib.Alg.Len > 0 {
		sb.WriteString(";alg=")
		sb.WriteString(ib.Alg.String())
	}
	if ib.Ppt.Len > 0 {
		sb.WriteString(";ppt=")
		sb.WriteString(ib.Ppt.String())
	}
	for p := ib.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// String returns the Identity-Info header as a string
func (iib *IdentityInfoBody) String() string {
	var sb strings.Builder
	sb.WriteString("<")
	sb.WriteString(iib.URI.String())
	sb.WriteString(">")

	if iib.Alg.Len > 0 {
		sb.WriteString(";alg=")
		sb.WriteString(iib.Alg.String())
	}
	if iib.Domain.Len > 0 {
		sb.WriteString(";domain=")
		sb.WriteString(iib.Domain.String())
	}
	for p := iib.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// IdentityError represents Identity header parsing errors
type IdentityError struct {
	Msg string
}

func (e *IdentityError) Error() string {
	return e.Msg
}

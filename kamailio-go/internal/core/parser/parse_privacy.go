// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Privacy and Diversion header parser - matching C parse_privacy.c and parse_diversion.c
 *
 * Privacy = "Privacy" HCOLON privacy-value *(COMMA privacy-value)
 * privacy-value = "header" / "session" / "user" / "none" / "critical" / extension-privacy
 * extension-privacy = token
 *
 * Diversion = "Diversion" HCOLON diversion-value *(COMMA diversion-value)
 * diversion-value = name-addr *(SEMI diversion-param)
 * diversion-param = ( "reason" EQUAL token ) / ( "counter" EQUAL 1*DIGIT ) / generic-param
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// PrivacyValue represents privacy values
type PrivacyValue int

const (
	PrivacyHeader PrivacyValue = iota
	PrivacySession
	PrivacyUser
	PrivacyNone
	PrivacyCritical
	PrivacyExtension
)

// PrivacyBody represents a parsed Privacy header body
// C: struct privacy_body
type PrivacyBody struct {
	Values []PrivacyValue
	Strs   []str.Str
}

// ParsePrivacy parses a Privacy header body
// C: char *parse_privacy(char *buffer, char *end, struct privacy_body **pb)
func ParsePrivacy(body str.Str) (*PrivacyBody, error) {
	pb := &PrivacyBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &PrivacyError{Msg: "empty privacy body"}
	}

	for _, part := range strings.Split(s, ",") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}

		pb.Strs = append(pb.Strs, str.Mk(part))
		pb.Values = append(pb.Values, parsePrivacyValue(part))
	}

	return pb, nil
}

// parsePrivacyValue parses a single privacy value
func parsePrivacyValue(value string) PrivacyValue {
	switch strings.ToLower(value) {
	case "header":
		return PrivacyHeader
	case "session":
		return PrivacySession
	case "user":
		return PrivacyUser
	case "none":
		return PrivacyNone
	case "critical":
		return PrivacyCritical
	default:
		return PrivacyExtension
	}
}

// HasHeader returns true if header privacy is set
func (pb *PrivacyBody) HasHeader() bool {
	for _, v := range pb.Values {
		if v == PrivacyHeader {
			return true
		}
	}
	return false
}

// HasSession returns true if session privacy is set
func (pb *PrivacyBody) HasSession() bool {
	for _, v := range pb.Values {
		if v == PrivacySession {
			return true
		}
	}
	return false
}

// HasUser returns true if user privacy is set
func (pb *PrivacyBody) HasUser() bool {
	for _, v := range pb.Values {
		if v == PrivacyUser {
			return true
		}
	}
	return false
}

// HasNone returns true if none privacy is set
func (pb *PrivacyBody) HasNone() bool {
	for _, v := range pb.Values {
		if v == PrivacyNone {
			return true
		}
	}
	return false
}

// HasCritical returns true if critical privacy is set
func (pb *PrivacyBody) HasCritical() bool {
	for _, v := range pb.Values {
		if v == PrivacyCritical {
			return true
		}
	}
	return false
}

// String returns the Privacy as a string
func (pb *PrivacyBody) String() string {
	var sb strings.Builder
	for i, s := range pb.Strs {
		if i > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(s.String())
	}
	return sb.String()
}

// ParsePrivacyFromHeader parses Privacy from a header field
func ParsePrivacyFromHeader(hdr *HdrField) (*PrivacyBody, error) {
	if hdr == nil {
		return nil, &PrivacyError{Msg: "nil header"}
	}
	return ParsePrivacy(hdr.Body)
}

// DiversionParam represents a diversion parameter
type DiversionParam struct {
	Name  str.Str
	Value str.Str
	Next  *DiversionParam
}

// DiversionValue represents a single diversion value
type DiversionValue struct {
	DisplayName str.Str
	URI         *SIPURI
	Reason      str.Str
	Counter     int
	Params      *DiversionParam
	LastParam   *DiversionParam
	Next        *DiversionValue
}

// DiversionBody represents a parsed Diversion header body
// C: struct diversion_body
type DiversionBody struct {
	First  *DiversionValue
	Last   *DiversionValue
	Body   str.Str
}

// ParseDiversion parses a Diversion header body
// C: char *parse_diversion(char *buffer, char *end, struct diversion_body **db)
func ParseDiversion(body str.Str) (*DiversionBody, error) {
	db := &DiversionBody{
		Body: body,
	}

	s := strings.TrimSpace(body.String())
	if s == "" {
		return nil, &DiversionError{Msg: "empty diversion body"}
	}

	var first, last *DiversionValue
	inAngle := false
	start := 0

	for i := 0; i <= len(s); i++ {
		if i == len(s) || (s[i] == ',' && !inAngle) {
			if i > start {
				dvStr := strings.TrimSpace(s[start:i])
				if dvStr != "" {
					dv, err := parseDiversionValue(dvStr)
					if err == nil {
						if first == nil {
							first = dv
							last = dv
						} else {
							last.Next = dv
							last = dv
						}
					}
				}
			}
			start = i + 1
		} else if s[i] == '<' {
			inAngle = true
		} else if s[i] == '>' {
			inAngle = false
		}
	}

	db.First = first
	db.Last = last

	return db, nil
}

// parseDiversionValue parses a single diversion value
func parseDiversionValue(dvStr string) (*DiversionValue, error) {
	dv := &DiversionValue{}

	laquot := strings.Index(dvStr, "<")
	raquot := strings.Index(dvStr, ">")

	if laquot != -1 && raquot != -1 && raquot > laquot {
		name := strings.TrimSpace(dvStr[:laquot])
		if name != "" {
			dv.DisplayName = str.Mk(name)
		}

		uri := dvStr[laquot+1 : raquot]
		parsedURI, _ := ParseURI(uri)
		dv.URI = parsedURI

		if raquot+1 < len(dvStr) {
			paramsStr := strings.TrimSpace(dvStr[raquot+1:])
			if strings.HasPrefix(paramsStr, ";") {
				paramsStr = paramsStr[1:]
			}
			parseDiversionParams(dv, paramsStr)
		}
	} else {
		parsedURI, _ := ParseURI(dvStr)
		dv.URI = parsedURI
	}

	return dv, nil
}

// parseDiversionParams parses diversion parameters
func parseDiversionParams(dv *DiversionValue, paramsStr string) {
	for _, paramStr := range strings.Split(paramsStr, ";") {
		paramStr = strings.TrimSpace(paramStr)
		if paramStr == "" {
			continue
		}

		var name, value string
		if eqIdx := strings.Index(paramStr, "="); eqIdx != -1 {
			name = strings.TrimSpace(paramStr[:eqIdx])
			value = strings.TrimSpace(paramStr[eqIdx+1:])
		} else {
			name = paramStr
		}

		switch strings.ToLower(name) {
		case "reason":
			dv.Reason = str.Mk(value)
		case "counter":
			if v, err := strconvParseIntDiversion(value, 10, 64); err == nil {
				dv.Counter = int(v)
			}
		default:
			param := &DiversionParam{
				Name:  str.Mk(name),
				Value: str.Mk(value),
			}
			if dv.Params == nil {
				dv.Params = param
				dv.LastParam = param
			} else {
				dv.LastParam.Next = param
				dv.LastParam = param
			}
		}
	}
}

// strconvParseIntDiversion is a helper for parsing integers
func strconvParseIntDiversion(s string, base int, bitSize int) (int64, error) {
	var result int64
	var sign int64 = 1
	var i int

	if len(s) == 0 {
		return 0, &strconvErrorDiversion{}
	}

	if s[0] == '-' {
		sign = -1
		i = 1
	} else if s[0] == '+' {
		i = 1
	}

	for ; i < len(s); i++ {
		c := s[i]
		if c < '0' || c > '9' {
			return 0, &strconvErrorDiversion{}
		}
		result = result*int64(base) + int64(c-'0')
	}

	return result * sign, nil
}

type strconvErrorDiversion struct{}

func (e *strconvErrorDiversion) Error() string {
	return "invalid integer"
}

// GetParam returns the parameter with the given name
func (dv *DiversionValue) GetParam(name string) *DiversionParam {
	for p := dv.Params; p != nil; p = p.Next {
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// String returns the Diversion value as a string
func (dv *DiversionValue) String() string {
	var sb strings.Builder

	if dv.DisplayName.Len > 0 {
		sb.WriteString(dv.DisplayName.String())
		sb.WriteString(" ")
	}

	sb.WriteString("<")
	if dv.URI != nil {
		if dv.URI.Type == SIPSURIT {
			sb.WriteString("sips:")
		} else {
			sb.WriteString("sip:")
		}
		if dv.URI.User.Len > 0 {
			sb.WriteString(dv.URI.User.String())
			sb.WriteString("@")
		}
		sb.WriteString(dv.URI.Host.String())
		if dv.URI.Port.Len > 0 {
			sb.WriteString(":")
			sb.WriteString(dv.URI.Port.String())
		}
	}
	sb.WriteString(">")

	if dv.Reason.Len > 0 {
		sb.WriteString(";reason=")
		sb.WriteString(dv.Reason.String())
	}

	if dv.Counter > 0 {
		sb.WriteString(";counter=")
		sb.WriteString(strconvItoaDiversion(dv.Counter))
	}

	for p := dv.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}

	return sb.String()
}

// strconvItoaDiversion is a helper to convert int to string
func strconvItoaDiversion(i int) string {
	if i == 0 {
		return "0"
	}

	var sb strings.Builder
	negative := i < 0
	if negative {
		i = -i
	}

	for i > 0 {
		sb.WriteByte(byte('0' + i%10))
		i /= 10
	}

	if negative {
		sb.WriteByte('-')
	}

	runes := []rune(sb.String())
	for j, k := 0, len(runes)-1; j < k; j, k = j+1, k-1 {
		runes[j], runes[k] = runes[k], runes[j]
	}

	return string(runes)
}

// Count returns the number of diversion values
func (db *DiversionBody) Count() int {
	count := 0
	for d := db.First; d != nil; d = d.Next {
		count++
	}
	return count
}

// IsEmpty returns true if there are no diversion values
func (db *DiversionBody) IsEmpty() bool {
	return db.First == nil
}

// String returns the Diversion body as a string
func (db *DiversionBody) String() string {
	var sb strings.Builder
	for d := db.First; d != nil; d = d.Next {
		if sb.Len() > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(d.String())
	}
	return sb.String()
}

// ParseDiversionFromHeader parses Diversion from a header field
func ParseDiversionFromHeader(hdr *HdrField) (*DiversionBody, error) {
	if hdr == nil {
		return nil, &DiversionError{Msg: "nil header"}
	}
	return ParseDiversion(hdr.Body)
}

// PrivacyError represents a privacy parsing error
type PrivacyError struct {
	Msg string
}

func (e *PrivacyError) Error() string {
	return e.Msg
}

// DiversionError represents a diversion parsing error
type DiversionError struct {
	Msg string
}

func (e *DiversionError) Error() string {
	return e.Msg
}
// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Allow header parser - matching C parse_allow.c
 *
 * Allow = "Allow" HCOLON [Method *(COMMA Method)]
 * Method = "INVITE" / "ACK" / "OPTIONS" / "BYE" / "CANCEL" / "REGISTER"
 *        / "SUBSCRIBE" / "NOTIFY" / "REFER" / "INFO" / "MESSAGE" / "UPDATE" / "PRACK"
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// AllowBody represents a parsed Allow header body
// C: struct allow_body
type AllowBody struct {
	Methods []RequestMethod
}

// ParseAllow parses an Allow header body
// C: char *parse_allow(char *buffer, char *end, unsigned int *allow)
func ParseAllow(body str.Str) (*AllowBody, error) {
	ab := &AllowBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return ab, nil // Empty Allow is valid (means no methods allowed)
	}

	for _, part := range strings.Split(s, ",") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		method := ParseMethod([]byte(part))
		ab.Methods = append(ab.Methods, method)
	}

	return ab, nil
}

// HasMethod checks if the Allow header contains a specific method
func (ab *AllowBody) HasMethod(m RequestMethod) bool {
	for _, method := range ab.Methods {
		if method == m {
			return true
		}
	}
	return false
}

// HasInvite returns true if INVITE is allowed
func (ab *AllowBody) HasInvite() bool {
	return ab.HasMethod(MethodInvite)
}

// HasAck returns true if ACK is allowed
func (ab *AllowBody) HasAck() bool {
	return ab.HasMethod(MethodACK)
}

// HasBye returns true if BYE is allowed
func (ab *AllowBody) HasBye() bool {
	return ab.HasMethod(MethodBye)
}

// HasCancel returns true if CANCEL is allowed
func (ab *AllowBody) HasCancel() bool {
	return ab.HasMethod(MethodCancel)
}

// HasRegister returns true if REGISTER is allowed
func (ab *AllowBody) HasRegister() bool {
	return ab.HasMethod(MethodRegister)
}

// HasOptions returns true if OPTIONS is allowed
func (ab *AllowBody) HasOptions() bool {
	return ab.HasMethod(MethodOptions)
}

// HasSubscribe returns true if SUBSCRIBE is allowed
func (ab *AllowBody) HasSubscribe() bool {
	return ab.HasMethod(MethodSubscribe)
}

// HasNotify returns true if NOTIFY is allowed
func (ab *AllowBody) HasNotify() bool {
	return ab.HasMethod(MethodNotify)
}

// HasRefer returns true if REFER is allowed
func (ab *AllowBody) HasRefer() bool {
	return ab.HasMethod(MethodRefer)
}

// HasInfo returns true if INFO is allowed
func (ab *AllowBody) HasInfo() bool {
	return ab.HasMethod(MethodInfo)
}

// HasMessage returns true if MESSAGE is allowed
func (ab *AllowBody) HasMessage() bool {
	return ab.HasMethod(MethodMessage)
}

// HasUpdate returns true if UPDATE is allowed
func (ab *AllowBody) HasUpdate() bool {
	return ab.HasMethod(MethodUpdate)
}

// HasPrack returns true if PRACK is allowed
func (ab *AllowBody) HasPrack() bool {
	return ab.HasMethod(MethodPRACK)
}

// String returns the Allow body as a string
func (ab *AllowBody) String() string {
	var parts []string
	for _, m := range ab.Methods {
		parts = append(parts, MethodName(m))
	}
	return strings.Join(parts, ", ")
}

// ParseAllowFromHeader parses Allow from a header field
func ParseAllowFromHeader(hdr *HdrField) (*AllowBody, error) {
	if hdr == nil {
		return nil, &AllowError{Msg: "nil header"}
	}
	return ParseAllow(hdr.Body)
}

// AllowError represents an Allow parsing error
type AllowError struct {
	Msg string
}

func (e *AllowError) Error() string {
	return e.Msg
}

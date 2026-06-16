// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP-If-Match header parser - matching C parse_sipifmatch.c (RFC 3903).
 *
 * SIP-If-Match = "SIP-If-Match" HCOLON ( "*" / entity-tag )
 *
 * Used with PUBLISH/Subscribe event state - value is an opaque etag.
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// SIPIfMatchBody represents a parsed SIP-If-Match header
// C: struct sipifmatch_body
// e.g. SIP-If-Match: 3z8h0nd29387421
type SIPIfMatchBody struct {
	Tag      str.Str // the opaque etag / cseq reference value
	Wildcard bool    // true if value is "*"
}

// ParseSIPIfMatch parses a SIP-If-Match header body
// C: char *parse_sipifmatch(char *buf, unsigned int len, struct sipifmatch_body **sb)
func ParseSIPIfMatch(body str.Str) (*SIPIfMatchBody, error) {
	sb := &SIPIfMatchBody{}
	s := strings.TrimSpace(body.String())
	if s == "" {
		return nil, &SIPIfMatchError{Msg: "empty sip-if-match body"}
	}
	if s == "*" {
		sb.Wildcard = true
		return sb, nil
	}
	s = strings.Trim(s, "\"")
	sb.Tag = str.Mk(s)
	return sb, nil
}

// String returns SIP-If-Match body as string
func (sb *SIPIfMatchBody) String() string {
	if sb.Wildcard {
		return "*"
	}
	return sb.Tag.String()
}

// IsWildcard returns true if this matches any etag (value "*")
func (sb *SIPIfMatchBody) IsWildcard() bool {
	return sb.Wildcard
}

// ParseSIPIfMatchFromHeader parses SIP-If-Match from HdrField
func ParseSIPIfMatchFromHeader(hdr *HdrField) (*SIPIfMatchBody, error) {
	if hdr == nil {
		return nil, &SIPIfMatchError{Msg: "nil header"}
	}
	return ParseSIPIfMatch(hdr.Body)
}

// SIPIfMatchError represents SIP-If-Match parsing errors
type SIPIfMatchError struct {
	Msg string
}

func (e *SIPIfMatchError) Error() string {
	return e.Msg
}

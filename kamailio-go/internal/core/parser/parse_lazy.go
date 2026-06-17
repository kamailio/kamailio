// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Lazy-parsing support for SIP header bodies - matching C parse_headers(msg, flag, next)
 *
 * Core idea: SIPMsg only parses header name + raw body string up-front; the
 * structured parsing of the body (ContactBody, ViaBody, ToBody, ...) is deferred
 * until a caller explicitly asks for it. This mirrors Kamailio's design where
 * parse_headers() walks headers on demand based on a flag mask.
 */

package parser

import (
	"fmt"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ParseMode controls lazy header body parsing behavior.
type ParseMode int

const (
	// ParseModeLazy parses only name + raw body; body parsing is deferred
	// until a caller invokes ParseHeaderBody / GetParsedXxx helpers.
	ParseModeLazy ParseMode = iota
	// ParseModeFull eagerly parses the body. Reserved for callers that want
	// the legacy "parse-everything-up-front" behavior.
	ParseModeFull
)

// ParseHeaderBody parses a single header body according to its type and caches
// the result in h.Parsed. If the header has already been parsed, the cached
// value is returned immediately.
//
// This is the central dispatcher for lazy-parsing and corresponds roughly to
// the C helper parse_header(msg, flag) which dispatches to per-header parsers.
func ParseHeaderBody(h *HdrField) (interface{}, error) {
	if h == nil {
		return nil, fmt.Errorf("nil header")
	}
	if h.Parsed != nil {
		return h.Parsed, nil
	}

	switch h.Type {
	case HdrVia:
		vb, _, err := ParseMultiVia(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = vb
		return vb, nil

	case HdrFrom, HdrTo:
		as, err := ParseNameAddr(h.Body.String(), true)
		if err != nil {
			return nil, err
		}
		tb := &ToBody{
			DisplayName: as.DisplayName,
			URI:         as.URI,
			ParsedURI:   as.URI,
		}
		tagParam := ParamListGet(as.Params, "tag")
		if tagParam != nil {
			tb.Tag = tagParam.Value
		}
		tb.Params = str.Mk(paramsToString(as.Params))
		h.Parsed = tb
		return tb, nil

	case HdrCallID:
		h.Parsed = h.Body.String()
		return h.Parsed, nil

	case HdrCSeq:
		cb, err := ParseCSeq(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = cb
		return cb, nil

	case HdrContact:
		cb, err := ParseContact(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = cb
		return cb, nil

	case HdrRoute, HdrRecordRoute, HdrPath:
		asList, err := ParseNameAddrList(h.Body.String(), true)
		if err != nil {
			return nil, err
		}
		h.Parsed = asList
		return asList, nil

	case HdrContentType:
		ctb, err := ParseContentType(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = ctb
		return ctb, nil

	case HdrContentLength:
		val, err := ParseContentLengthValue(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = val
		return val, nil

	case HdrContentDisposition:
		db, err := ParseDisposition(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = db
		return db, nil

	case HdrExpires:
		eb, err := ParseExpiresBody(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = eb
		return eb, nil

	case HdrMinExpires:
		eb, err := ParseMinExpiresBody(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = eb
		return eb, nil

	case HdrAllow:
		ab, err := ParseAllow(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = ab
		return ab, nil

	case HdrRequire, HdrProxyRequire, HdrSupported:
		rb, err := ParseRequire(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = rb
		return rb, nil

	case HdrEvent:
		eb, err := ParseEvent(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = eb
		return eb, nil

	case HdrSubscriptionState:
		ssb, err := ParseSubscriptionState(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = ssb
		return ssb, nil

	case HdrDate:
		d, err := ParseDate(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = d
		return d, nil

	case HdrReferTo:
		rtb, err := ParseReferTo(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = rtb
		return rtb, nil

	case HdrDiversion:
		entries, err := ParseNameAddrList(h.Body.String(), true)
		if err != nil {
			return nil, err
		}
		h.Parsed = entries
		return entries, nil

	case HdrPrivacy:
		pb, err := ParsePrivacy(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = pb
		return pb, nil

	case HdrRPID:
		rb, err := ParseRPID(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = rb
		return rb, nil

	case HdrIdentity:
		ib, err := ParseIdentity(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = ib
		return ib, nil

	case HdrIdentityInfo:
		iib, err := ParseIdentityInfo(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = iib
		return iib, nil

	case HdrPAI:
		aib, err := ParseAssertedIdentity(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = aib
		return aib, nil

	case HdrPPI:
		pib, err := ParsePreferredIdentity(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = pib
		return pib, nil

	case HdrSIPIfMatch:
		sb, err := ParseSIPIfMatch(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = sb
		return sb, nil

	case HdrAuthorization, HdrProxyAuth:
		ab, err := ParseAuthenticate(h.Body)
		if err != nil {
			return nil, err
		}
		h.Parsed = ab
		return ab, nil

	default:
		// Unknown / unsupported header - expose the raw body string.
		h.Parsed = h.Body.String()
		return h.Parsed, nil
	}
}

// ParseHeadersWithFlag parses the bodies for all headers whose type matches one
// of the provided types. This is the Go counterpart of C's
//   int parse_headers(struct sip_msg *msg, hdr_flags_t flags, int next);
//
// Each parsed header has its structured body cached in HdrField.Parsed, and
// msg.ParsedFlag is updated to reflect which header types are parsed.
func (m *SIPMsg) ParseHeadersWithFlag(types ...HdrType) error {
	target := make(map[HdrType]bool, len(types))
	for _, t := range types {
		target[t] = true
	}
	for _, h := range m.Headers {
		if target[h.Type] {
			if _, err := ParseHeaderBody(h); err != nil {
				return err
			}
			m.ParsedFlag |= hdrFlagForType(h.Type)
		}
	}
	return nil
}

// ParseAllHeaders parses bodies for ALL headers currently in the message.
// This is the equivalent of the legacy "eager parse everything" mode.
func (m *SIPMsg) ParseAllHeaders() error {
	for _, h := range m.Headers {
		if _, err := ParseHeaderBody(h); err != nil {
			return err
		}
		m.ParsedFlag |= hdrFlagForType(h.Type)
	}
	return nil
}

// IsParsed returns true if the header type has already been parsed for this
// message (i.e., the corresponding flag in m.ParsedFlag is set).
func (m *SIPMsg) IsParsed(ht HdrType) bool {
	f := hdrFlagForType(ht)
	if f == 0 {
		return false
	}
	return (m.ParsedFlag & f) != 0
}

// GetParsedVia returns the parsed top Via body, parsing it on demand.
func (m *SIPMsg) GetParsedVia() (*ViaBody, error) {
	if m.HdrVia1 == nil {
		return nil, nil
	}
	if m.HdrVia1.Parsed == nil {
		if _, err := ParseHeaderBody(m.HdrVia1); err != nil {
			return nil, err
		}
		m.ParsedFlag |= HdrViaF
	}
	if vb, ok := m.HdrVia1.Parsed.(*ViaBody); ok {
		return vb, nil
	}
	return nil, nil
}

// GetParsedContact returns the parsed Contact body, parsing on demand.
func (m *SIPMsg) GetParsedContact() (*ContactBody, error) {
	if m.Contact == nil {
		return nil, nil
	}
	if m.Contact.Parsed == nil {
		if _, err := ParseHeaderBody(m.Contact); err != nil {
			return nil, err
		}
		m.ParsedFlag |= HdrContactF
	}
	if cb, ok := m.Contact.Parsed.(*ContactBody); ok {
		return cb, nil
	}
	return nil, nil
}

// GetParsedTo returns the parsed To body.
func (m *SIPMsg) GetParsedTo() (*ToBody, error) {
	if m.To == nil {
		return nil, nil
	}
	if m.To.Parsed == nil {
		if _, err := ParseHeaderBody(m.To); err != nil {
			return nil, err
		}
		m.ParsedFlag |= HdrToF
	}
	if tb, ok := m.To.Parsed.(*ToBody); ok {
		return tb, nil
	}
	return nil, nil
}

// GetParsedFrom returns the parsed From body.
func (m *SIPMsg) GetParsedFrom() (*ToBody, error) {
	if m.From == nil {
		return nil, nil
	}
	if m.From.Parsed == nil {
		if _, err := ParseHeaderBody(m.From); err != nil {
			return nil, err
		}
		m.ParsedFlag |= HdrFromF
	}
	if tb, ok := m.From.Parsed.(*ToBody); ok {
		return tb, nil
	}
	return nil, nil
}

// paramsToString serializes a linked *Param list back into its "name=value"
// semicolon-separated string form (used for ToBody.Params / FromBody.Params).
func paramsToString(p *Param) string {
	var sb strings.Builder
	for p != nil {
		if sb.Len() > 0 {
			sb.WriteString(";")
		}
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
		p = p.Next
	}
	return sb.String()
}

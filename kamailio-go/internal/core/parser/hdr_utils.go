// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * HdrField / SIPMsg helper utilities:
 *   - header iteration and removal helpers,
 *   - Subscription-State convenience wrappers,
 *   - Contact and expires helpers,
 *   - R-URI / destination helpers,
 *   - message rebuild / re-serialization.
 */

package parser

import (
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ---------------------------------------------------------------------------
// Header iteration and counting
// ---------------------------------------------------------------------------

// ForEachHeader iterates all headers whose type matches ht. The callback can
// return false to abort iteration early.
func (m *SIPMsg) ForEachHeader(ht HdrType, callback func(h *HdrField) bool) {
	for _, h := range m.Headers {
		if h.Type == ht {
			if !callback(h) {
				break
			}
		}
	}
}

// CountHeadersByType returns the number of headers of the given type currently
// present in the message.
func (m *SIPMsg) CountHeadersByType(ht HdrType) int {
	count := 0
	for _, h := range m.Headers {
		if h.Type == ht {
			count++
		}
	}
	return count
}

// HeaderCount returns the total number of parsed headers.
func (m *SIPMsg) HeaderCount() int {
	return len(m.Headers)
}

// FirstHeader returns the first header in the message (nil if empty).
func (m *SIPMsg) FirstHeader() *HdrField {
	if len(m.Headers) == 0 {
		return nil
	}
	return m.Headers[0]
}

// NextHeader returns the sibling of h that immediately follows it in the
// parsed header list. Returns nil when there is no next header.
//
// This replaces the linked-list sibling walking model of the C code with a
// direct index lookup into the headers slice, which is more efficient in Go.
func (m *SIPMsg) NextHeader(h *HdrField) *HdrField {
	for i, hd := range m.Headers {
		if hd == h && i+1 < len(m.Headers) {
			return m.Headers[i+1]
		}
	}
	return nil
}

// ---------------------------------------------------------------------------
// Header removal
// ---------------------------------------------------------------------------

// RemoveHeader removes a specific header by reference, clearing the matching
// quick-access pointer (if any) and resetting the corresponding parsed flag.
func (m *SIPMsg) RemoveHeader(h *HdrField) {
	for i, hd := range m.Headers {
		if hd == h {
			m.Headers = append(m.Headers[:i], m.Headers[i+1:]...)
			m.resetQuickRef(h)
			return
		}
	}
}

// RemoveHeadersByType removes all headers of the given type.
func (m *SIPMsg) RemoveHeadersByType(ht HdrType) {
	kept := m.Headers[:0]
	for _, h := range m.Headers {
		if h.Type == ht {
			m.resetQuickRef(h)
			continue
		}
		kept = append(kept, h)
	}
	m.Headers = kept
}

// resetQuickRef clears any quick-access pointer that points to the given
// header, and clears its parsed flag.
func (m *SIPMsg) resetQuickRef(h *HdrField) {
	switch h.Type {
	case HdrVia:
		if m.HdrVia1 == h {
			m.HdrVia1 = nil
		} else if m.HdrVia2 == h {
			m.HdrVia2 = nil
		}
	case HdrFrom:
		m.From = nil
	case HdrTo:
		m.To = nil
	case HdrCallID:
		m.CallID = nil
	case HdrCSeq:
		m.CSeq = nil
	case HdrContact:
		m.Contact = nil
	case HdrMaxForwards:
		m.MaxForwards = nil
	case HdrRoute:
		m.Route = nil
	case HdrRecordRoute:
		m.RecordRoute = nil
	case HdrContentType:
		m.ContentType = nil
	case HdrContentLength:
		m.ContentLength = nil
	case HdrExpires:
		m.Expires = nil
	case HdrProxyAuth:
		m.ProxyAuth = nil
	case HdrSupported:
		m.Supported = nil
	case HdrRequire:
		m.Require = nil
	case HdrProxyRequire:
		m.ProxyRequire = nil
	case HdrAllow:
		m.Allow = nil
	case HdrEvent:
		m.Event = nil
	case HdrAccept:
		m.Accept = nil
	case HdrAcceptLanguage:
		m.AcceptLanguage = nil
	case HdrOrganization:
		m.Organization = nil
	case HdrPriority:
		m.Priority = nil
	case HdrSubject:
		m.Subject = nil
	case HdrUserAgent:
		m.UserAgent = nil
	case HdrServer:
		m.Server = nil
	case HdrContentDisposition:
		m.ContentDisposition = nil
	case HdrDiversion:
		m.Diversion = nil
	case HdrRPID:
		m.RPID = nil
	case HdrReferTo:
		m.ReferTo = nil
	case HdrSessionExpires:
		m.SessionExpires = nil
	case HdrMinSE:
		m.MinSE = nil
	case HdrSIPIfMatch:
		m.SIPIfMatch = nil
	case HdrSubscriptionState:
		m.SubscriptionState = nil
	case HdrDate:
		m.Date = nil
	case HdrIdentity:
		m.Identity = nil
	case HdrIdentityInfo:
		m.IdentityInfo = nil
	case HdrPAI:
		m.PAI = nil
	case HdrPPI:
		m.PPI = nil
	case HdrPath:
		m.Path = nil
	case HdrPrivacy:
		m.Privacy = nil
	case HdrMinExpires:
		m.MinExpires = nil
	case HdrPAccessNetworkInfo:
		m.PAccessNetworkInfo = nil
	case HdrPVisitedNetworkID:
		m.PVisitedNetworkID = nil
	case HdrAuthorization:
		m.Authorization = nil
	}
	flag := hdrFlagForType(h.Type)
	m.ParsedFlag &^= flag
}

// ---------------------------------------------------------------------------
// Subscription-State convenience wrappers
// ---------------------------------------------------------------------------

// GetParsedSubscriptionState returns the parsed Subscription-State body,
// parsing it on demand.
func (m *SIPMsg) GetParsedSubscriptionState() (*SubscriptionStateBody, error) {
	if m.SubscriptionState == nil {
		return nil, nil
	}
	if m.SubscriptionState.Parsed == nil {
		if _, err := ParseHeaderBody(m.SubscriptionState); err != nil {
			return nil, err
		}
		m.ParsedFlag |= HdrSubscriptionStateF
	}
	if ssb, ok := m.SubscriptionState.Parsed.(*SubscriptionStateBody); ok {
		return ssb, nil
	}
	return nil, nil
}

// IsSubscriptionActive returns true when Subscription-State is "active".
func (m *SIPMsg) IsSubscriptionActive() bool {
	ssb, err := m.GetParsedSubscriptionState()
	if err != nil || ssb == nil {
		return false
	}
	return ssb.State == SubscriptionActive
}

// IsSubscriptionTerminated returns true when Subscription-State is "terminated".
func (m *SIPMsg) IsSubscriptionTerminated() bool {
	ssb, err := m.GetParsedSubscriptionState()
	if err != nil || ssb == nil {
		return false
	}
	return ssb.State == SubscriptionTerminated
}

// GetSubscriptionExpires returns the expires value from Subscription-State,
// or 0 if not present / not parseable.
func (m *SIPMsg) GetSubscriptionExpires() int {
	ssb, err := m.GetParsedSubscriptionState()
	if err != nil || ssb == nil {
		return 0
	}
	return ssb.Expires
}

// GetSubscriptionReason returns the reason string from a terminated
// Subscription-State, or the empty string if absent.
func (m *SIPMsg) GetSubscriptionReason() string {
	ssb, err := m.GetParsedSubscriptionState()
	if err != nil || ssb == nil {
		return ""
	}
	return ssb.ReasonStr.String()
}

// ---------------------------------------------------------------------------
// Contact helpers on SIPMsg
// ---------------------------------------------------------------------------

// GetAllParsedContacts returns every Contact entry from the Contact header as
// a slice of parsed ContactBody values.
func (m *SIPMsg) GetAllParsedContacts() ([]*ContactBody, error) {
	if m.Contact == nil {
		return nil, nil
	}
	return ParseContactList(m.Contact.Body)
}

// GetContactExpires returns the minimum expires value across all Contact
// entries; if no Contact carries an expires parameter, the value from the
// Expires header is used instead. Returns 0 if nothing is available.
func (m *SIPMsg) GetContactExpires() int {
	contacts, err := m.GetAllParsedContacts()
	if err == nil && len(contacts) > 0 {
		minExp := -1
		for _, c := range contacts {
			if c.HasExpires() {
				e := c.GetExpires()
				if minExp < 0 || e < minExp {
					minExp = e
				}
			}
		}
		if minExp >= 0 {
			return minExp
		}
	}
	if m.Expires != nil {
		if eb, ok := m.Expires.Parsed.(*ExpiresBody); ok {
			return int(eb.Value)
		}
	}
	return 0
}

// ---------------------------------------------------------------------------
// R-URI helpers
// ---------------------------------------------------------------------------

// SetRURI updates the request URI - this is the "real" Request URI
// (used in the request line) as well as m.NewURI for routing decisions.
func (m *SIPMsg) SetRURI(uri string) {
	m.NewURI = str.Mk(uri)
	if m.FirstLine != nil && m.FirstLine.Req != nil {
		m.FirstLine.Req.URI = str.Mk(uri)
	}
}

// SetDestinationURI updates the destination / next-hop URI.
func (m *SIPMsg) SetDestinationURI(uri string) {
	m.DstURI = str.Mk(uri)
}

// ---------------------------------------------------------------------------
// Message re-serialization
// ---------------------------------------------------------------------------

// RebuildMessage re-serializes the SIP message into a single string. It is
// primarily useful after header modifications when a caller wants to obtain
// the "new" on-wire form of the message.
//
// The format is:
//   - request or status line
//   - each header on its own line as "Name: Body"
//   - blank line terminator
//   - raw body bytes (if any)
func (m *SIPMsg) RebuildMessage() string {
	var sb strings.Builder

	if m.FirstLine != nil {
		if m.FirstLine.Reply != nil {
			sb.WriteString("SIP/2.0 ")
			sb.WriteString(strconv.Itoa(int(m.FirstLine.Reply.StatusCode)))
			sb.WriteString(" ")
			sb.WriteString(m.FirstLine.Reply.Reason.String())
		} else if m.FirstLine.Req != nil {
			sb.WriteString(m.FirstLine.Req.Method.String())
			sb.WriteString(" ")
			sb.WriteString(m.FirstLine.Req.URI.String())
			sb.WriteString(" SIP/2.0")
		}
	}
	sb.WriteString("\r\n")

	for _, h := range m.Headers {
		sb.WriteString(h.Name.String())
		sb.WriteString(": ")
		sb.WriteString(h.Body.String())
		sb.WriteString("\r\n")
	}
	sb.WriteString("\r\n")

	if bodyBytes, ok := m.Body.([]byte); ok && len(bodyBytes) > 0 {
		sb.Write(bodyBytes)
	}
	return sb.String()
}

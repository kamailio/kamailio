// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Main SIP message parser - matching C msg_parser.c
 */

package parser

import (
	"errors"
	"fmt"
	"strconv"
	"time"
)

// ParseMsg parses a complete SIP message from a byte buffer
// C: int parse_msg(char *const buf, const unsigned int len, struct sip_msg *const msg)
func ParseMsg(buf []byte) (*SIPMsg, error) {
	if len(buf) <= 16 {
		return nil, errors.New("message too short")
	}

	msg := &SIPMsg{
		Buf:        buf,
		Len:        len(buf),
		BufSize:    len(buf),
		ReceivedAt: time.Now(),
	}

	// Parse first line
	fl, remaining, err := ParseFirstLine(buf)
	if err != nil {
		return nil, fmt.Errorf("first line parse error: %w", err)
	}
	msg.FirstLine = fl

	// Parse headers
	headers, bodyOffset, err := ParseHeaders(remaining)
	if err != nil {
		return nil, fmt.Errorf("header parse error: %w", err)
	}
	msg.Headers = headers

	// Set up quick references
	for _, h := range msg.Headers {
		msg.setHeaderRef(h)
	}

	// Parse body if present
	if bodyOffset < len(remaining) {
		msg.Body = remaining[bodyOffset:]
	}

	return msg, nil
}

// setHeaderRef sets the quick reference for a header field.
// For Via headers it also populates the legacy Via1/Via2 *ViaBody pointers
// so callers can use msg.Via1 directly instead of msg.HdrVia1.Parsed.
func (m *SIPMsg) setHeaderRef(h *HdrField) {
	switch h.Type {
	case HdrVia:
		if m.HdrVia1 == nil {
			m.HdrVia1 = h
			if vb, ok := h.Parsed.(*ViaBody); ok {
				m.Via1 = vb
			}
		} else if m.HdrVia2 == nil {
			m.HdrVia2 = h
			if vb, ok := h.Parsed.(*ViaBody); ok {
				m.Via2 = vb
			}
		}
	case HdrFrom:
		m.From = h
	case HdrTo:
		m.To = h
	case HdrCallID:
		m.CallID = h
	case HdrCSeq:
		m.CSeq = h
	case HdrContact:
		m.Contact = h
	case HdrMaxForwards:
		m.MaxForwards = h
	case HdrRoute:
		m.Route = h
	case HdrRecordRoute:
		m.RecordRoute = h
	case HdrContentType:
		m.ContentType = h
	case HdrContentLength:
		m.ContentLength = h
	case HdrAuthorization:
		m.Authorization = h
	case HdrExpires:
		m.Expires = h
	case HdrProxyAuth:
		m.ProxyAuth = h
	case HdrSupported:
		m.Supported = h
	case HdrRequire:
		m.Require = h
	case HdrProxyRequire:
		m.ProxyRequire = h
	case HdrAllow:
		m.Allow = h
	case HdrEvent:
		m.Event = h
	case HdrAccept:
		m.Accept = h
	case HdrAcceptLanguage:
		m.AcceptLanguage = h
	case HdrOrganization:
		m.Organization = h
	case HdrPriority:
		m.Priority = h
	case HdrSubject:
		m.Subject = h
	case HdrUserAgent:
		m.UserAgent = h
	case HdrServer:
		m.Server = h
	case HdrContentDisposition:
		m.ContentDisposition = h
	case HdrDiversion:
		m.Diversion = h
	case HdrRPID:
		m.RPID = h
	case HdrReferTo:
		m.ReferTo = h
	case HdrSessionExpires:
		m.SessionExpires = h
	case HdrMinSE:
		m.MinSE = h
	case HdrSIPIfMatch:
		m.SIPIfMatch = h
	case HdrSubscriptionState:
		m.SubscriptionState = h
	case HdrDate:
		m.Date = h
	case HdrIdentity:
		m.Identity = h
	case HdrIdentityInfo:
		m.IdentityInfo = h
	case HdrPAI:
		m.PAI = h
	case HdrPPI:
		m.PPI = h
	case HdrPath:
		m.Path = h
	case HdrPrivacy:
		m.Privacy = h
	case HdrMinExpires:
		m.MinExpires = h
	case HdrPAccessNetworkInfo:
		m.PAccessNetworkInfo = h
	case HdrPVisitedNetworkID:
		m.PVisitedNetworkID = h
	}

	// Update parsed flag
	m.ParsedFlag |= hdrFlagForType(h.Type)
}

// hdrFlagForType returns the flag for a header type
func hdrFlagForType(ht HdrType) HdrFlag {
	switch ht {
	case HdrVia:
		return HdrViaF
	case HdrFrom:
		return HdrFromF
	case HdrTo:
		return HdrToF
	case HdrCallID:
		return HdrCallIDF
	case HdrCSeq:
		return HdrCSeqF
	case HdrContact:
		return HdrContactF
	case HdrMaxForwards:
		return HdrMaxForwardsF
	case HdrRoute:
		return HdrRouteF
	case HdrRecordRoute:
		return HdrRecordRouteF
	case HdrContentType:
		return HdrContentTypeF
	case HdrContentLength:
		return HdrContentLengthF
	case HdrAuthorization:
		return HdrAuthorizationF
	case HdrExpires:
		return HdrExpiresF
	case HdrProxyAuth:
		return HdrProxyAuthF
	case HdrSupported:
		return HdrSupportedF
	case HdrRequire:
		return HdrRequireF
	case HdrProxyRequire:
		return HdrProxyRequireF
	case HdrAllow:
		return HdrAllowF
	case HdrEvent:
		return HdrEventF
	case HdrAccept:
		return HdrAcceptF
	case HdrAcceptLanguage:
		return HdrAcceptLanguageF
	case HdrOrganization:
		return HdrOrganizationF
	case HdrPriority:
		return HdrPriorityF
	case HdrSubject:
		return HdrSubjectF
	case HdrUserAgent:
		return HdrUserAgentF
	case HdrServer:
		return HdrServerF
	case HdrContentDisposition:
		return HdrContentDispositionF
	case HdrDiversion:
		return HdrDiversionF
	case HdrRPID:
		return HdrRPIDF
	case HdrReferTo:
		return HdrReferToF
	case HdrSessionExpires:
		return HdrSessionExpiresF
	case HdrMinSE:
		return HdrMinSEF
	case HdrSIPIfMatch:
		return HdrSIPIfMatchF
	case HdrSubscriptionState:
		return HdrSubscriptionStateF
	case HdrDate:
		return HdrDateF
	case HdrIdentity:
		return HdrIdentityF
	case HdrIdentityInfo:
		return HdrIdentityInfoF
	case HdrPAI:
		return HdrPAIF
	case HdrPPI:
		return HdrPPIF
	case HdrPath:
		return HdrPathF
	case HdrPrivacy:
		return HdrPrivacyF
	case HdrMinExpires:
		return HdrMinExpiresF
	case HdrPAccessNetworkInfo:
		return HdrPAccessNetworkInfoF
	case HdrPVisitedNetworkID:
		return HdrPVisitedNetworkIDF
	default:
		return 0
	}
}

// ParseContentLength parses the Content-Length header value
func ParseContentLength(h *HdrField) (int, error) {
	if h == nil || h.Type != HdrContentLength {
		return 0, errors.New("not a Content-Length header")
	}
	val, err := strconv.Atoi(h.Body.String())
	if err != nil {
		return 0, err
	}
	return val, nil
}

// ParseCSeqHeader parses the CSeq from a HdrField - delegates to ParseCSeq in parse_cseq.go
func ParseCSeqHeader(h *HdrField) (*CSeqBody, error) {
	if h == nil || h.Type != HdrCSeq {
		return nil, errors.New("not a CSeq header")
	}
	return ParseCSeq(h.Body)
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP header field name parser - matching C parse_hname2.c
 *
 * Fast header field name identification using first-letter indexing.
 */

package parser

import (
	"bytes"
	"errors"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// HdrType represents SIP header field types
// C: enum hdr_types_t
type HdrType int

const (
	HdrError HdrType = -1
	HdrOther HdrType = 0

	// Standard headers
	HdrVia           HdrType = 1
	HdrFrom          HdrType = 2
	HdrTo            HdrType = 3
	HdrCallID        HdrType = 4
	HdrCSeq          HdrType = 5
	HdrContact       HdrType = 6
	HdrMaxForwards   HdrType = 7
	HdrRoute         HdrType = 8
	HdrRecordRoute   HdrType = 9
	HdrContentType   HdrType = 10
	HdrContentLength HdrType = 11
	HdrAuthorization HdrType = 12
	HdrExpires       HdrType = 13
	HdrProxyAuth     HdrType = 14
	HdrSupported     HdrType = 15
	HdrRequire       HdrType = 16
	HdrProxyRequire  HdrType = 17
	HdrAllow         HdrType = 18
	HdrEvent         HdrType = 19
	HdrAccept        HdrType = 20
	HdrAcceptLanguage HdrType = 21
	HdrOrganization  HdrType = 22
	HdrPriority      HdrType = 23
	HdrSubject       HdrType = 24
	HdrUserAgent     HdrType = 25
	HdrServer        HdrType = 26
	HdrContentDisposition HdrType = 27
	HdrDiversion     HdrType = 28
	HdrRPID          HdrType = 29
	HdrReferTo       HdrType = 30
	HdrSessionExpires HdrType = 31
	HdrMinSE         HdrType = 32
	HdrSIPIfMatch    HdrType = 33
	HdrSubscriptionState HdrType = 34
	HdrDate          HdrType = 35
	HdrIdentity      HdrType = 36
	HdrIdentityInfo  HdrType = 37
	HdrPAI           HdrType = 38
	HdrPPI           HdrType = 39
	HdrPath          HdrType = 40
	HdrPrivacy       HdrType = 41
	HdrMinExpires    HdrType = 42
	HdrAcceptContact HdrType = 43
	HdrAllowEvents   HdrType = 44
	HdrCallInfo      HdrType = 45
	HdrContentEncoding HdrType = 46
	HdrRequestDisposition HdrType = 47
	HdrServiceRoute  HdrType = 48
	HdrPChargingVector HdrType = 49
	HdrPChargingFunctionAddresses HdrType = 50
	HdrPAccessNetworkInfo HdrType = 51
	HdrPVisitedNetworkID HdrType = 52
	HdrSecurityClient HdrType = 53
	HdrSecurityServer HdrType = 54
	HdrSecurityVerify HdrType = 55
)

// HdrFlag represents header field flags (bitmask)
// C: hdr_flags_t
type HdrFlag uint64

const (
	HdrViaF           HdrFlag = 1 << 0
	HdrFromF          HdrFlag = 1 << 1
	HdrToF            HdrFlag = 1 << 2
	HdrCallIDF        HdrFlag = 1 << 3
	HdrCSeqF          HdrFlag = 1 << 4
	HdrContactF       HdrFlag = 1 << 5
	HdrMaxForwardsF   HdrFlag = 1 << 6
	HdrRouteF         HdrFlag = 1 << 7
	HdrRecordRouteF   HdrFlag = 1 << 8
	HdrContentTypeF   HdrFlag = 1 << 9
	HdrContentLengthF HdrFlag = 1 << 10
	HdrAuthorizationF HdrFlag = 1 << 11
	HdrExpiresF       HdrFlag = 1 << 12
	HdrProxyAuthF     HdrFlag = 1 << 13
	HdrSupportedF     HdrFlag = 1 << 14
	HdrRequireF       HdrFlag = 1 << 15
	HdrProxyRequireF  HdrFlag = 1 << 16
	HdrAllowF         HdrFlag = 1 << 17
	HdrEventF         HdrFlag = 1 << 18
	HdrAcceptF        HdrFlag = 1 << 19
	HdrAcceptLanguageF HdrFlag = 1 << 20
	HdrOrganizationF  HdrFlag = 1 << 21
	HdrPriorityF      HdrFlag = 1 << 22
	HdrSubjectF       HdrFlag = 1 << 23
	HdrUserAgentF     HdrFlag = 1 << 24
	HdrServerF        HdrFlag = 1 << 25
	HdrContentDispositionF HdrFlag = 1 << 26
	HdrDiversionF     HdrFlag = 1 << 27
	HdrRPIDF          HdrFlag = 1 << 28
	HdrReferToF       HdrFlag = 1 << 29
	HdrSessionExpiresF HdrFlag = 1 << 30
	HdrMinSEF         HdrFlag = 1 << 31
	HdrSIPIfMatchF    HdrFlag = 1 << 32
	HdrSubscriptionStateF HdrFlag = 1 << 33
	HdrDateF          HdrFlag = 1 << 34
	HdrIdentityF      HdrFlag = 1 << 35
	HdrIdentityInfoF  HdrFlag = 1 << 36
	HdrPAIF           HdrFlag = 1 << 37
	HdrPPIF           HdrFlag = 1 << 38
	HdrPathF          HdrFlag = 1 << 39
	HdrPrivacyF       HdrFlag = 1 << 40
	HdrMinExpiresF    HdrFlag = 1 << 41
	HdrAcceptContactF HdrFlag = 1 << 42
	HdrAllowEventsF   HdrFlag = 1 << 43
	HdrCallInfoF      HdrFlag = 1 << 44
	HdrContentEncodingF HdrFlag = 1 << 45
	HdrRequestDispositionF HdrFlag = 1 << 46
	HdrServiceRouteF  HdrFlag = 1 << 47
	HdrPChargingVectorF HdrFlag = 1 << 48
	HdrPChargingFunctionAddressesF HdrFlag = 1 << 49
	HdrPAccessNetworkInfoF HdrFlag = 1 << 50
	HdrPVisitedNetworkIDF HdrFlag = 1 << 51
	HdrSecurityClientF HdrFlag = 1 << 52
	HdrSecurityServerF HdrFlag = 1 << 53
	HdrSecurityVerifyF HdrFlag = 1 << 54
)

// hdrMapEntry maps header name to type and flag
type hdrMapEntry struct {
	Name  string
	Type  HdrType
	Flag  HdrFlag
}

// Header name map - grouped by first letter for fast lookup
// C: static ksr_hdr_map_t _ksr_hdr_map[]
var hdrMap = []hdrMapEntry{
	{"a", HdrAcceptContact, HdrAcceptContactF},
	{"Accept", HdrAccept, HdrAcceptF},
	{"Accept-Contact", HdrAcceptContact, HdrAcceptContactF},
	{"Accept-Language", HdrAcceptLanguage, HdrAcceptLanguageF},
	{"Allow", HdrAllow, HdrAllowF},
	{"Allow-Events", HdrAllowEvents, HdrAllowEventsF},
	{"Authorization", HdrAuthorization, HdrAuthorizationF},

	{"b", HdrReferTo, HdrReferToF}, // compact form for Refer-To

	{"c", HdrContentType, HdrContentTypeF}, // compact form
	{"Call-Id", HdrCallID, HdrCallIDF},
	{"Call-Info", HdrCallInfo, HdrCallInfoF},
	{"Contact", HdrContact, HdrContactF},
	{"Content-Disposition", HdrContentDisposition, HdrContentDispositionF},
	{"Content-Encoding", HdrContentEncoding, HdrContentEncodingF},
	{"Content-Length", HdrContentLength, HdrContentLengthF},
	{"Content-Type", HdrContentType, HdrContentTypeF},
	{"CSeq", HdrCSeq, HdrCSeqF},

	{"d", HdrRequestDisposition, HdrRequestDispositionF},
	{"Date", HdrDate, HdrDateF},
	{"Diversion", HdrDiversion, HdrDiversionF},

	{"e", HdrContentEncoding, HdrContentEncodingF}, // compact form
	{"Event", HdrEvent, HdrEventF},
	{"Expires", HdrExpires, HdrExpiresF},

	{"f", HdrFrom, HdrFromF}, // compact form
	{"From", HdrFrom, HdrFromF},

	{"i", HdrCallID, HdrCallIDF}, // compact form
	{"Identity", HdrIdentity, HdrIdentityF},
	{"Identity-Info", HdrIdentityInfo, HdrIdentityInfoF},

	{"k", HdrSupported, HdrSupportedF}, // compact form

	{"l", HdrContentLength, HdrContentLengthF}, // compact form

	{"m", HdrContact, HdrContactF}, // compact form
	{"Max-Forwards", HdrMaxForwards, HdrMaxForwardsF},
	{"Min-SE", HdrMinSE, HdrMinSEF},
	{"Min-Expires", HdrMinExpires, HdrMinExpiresF},

	{"o", HdrEvent, HdrEventF}, // compact form
	{"Organization", HdrOrganization, HdrOrganizationF},

	{"P-Access-Network-Info", HdrPAccessNetworkInfo, HdrPAccessNetworkInfoF},
	{"P-Asserted-Identity", HdrPAI, HdrPAIF},
	{"P-Charging-Function-Addresses", HdrPChargingFunctionAddresses, HdrPChargingFunctionAddressesF},
	{"P-Charging-Vector", HdrPChargingVector, HdrPChargingVectorF},
	{"P-Preferred-Identity", HdrPPI, HdrPPIF},
	{"P-Visited-Network-ID", HdrPVisitedNetworkID, HdrPVisitedNetworkIDF},
	{"Path", HdrPath, HdrPathF},
	{"Priority", HdrPriority, HdrPriorityF},
	{"Privacy", HdrPrivacy, HdrPrivacyF},
	{"Proxy-Authenticate", HdrProxyAuth, HdrProxyAuthF},
	{"Proxy-Authorization", HdrProxyAuth, HdrProxyAuthF},
	{"Proxy-Require", HdrProxyRequire, HdrProxyRequireF},

	{"r", HdrReferTo, HdrReferToF}, // compact form
	{"Record-Route", HdrRecordRoute, HdrRecordRouteF},
	{"Refer-To", HdrReferTo, HdrReferToF},
	{"Require", HdrRequire, HdrRequireF},
	{"Route", HdrRoute, HdrRouteF},
	{"RPID", HdrRPID, HdrRPIDF},

	{"s", HdrSubject, HdrSubjectF}, // compact form
	{"Security-Client", HdrSecurityClient, HdrSecurityClientF},
	{"Security-Server", HdrSecurityServer, HdrSecurityServerF},
	{"Security-Verify", HdrSecurityVerify, HdrSecurityVerifyF},
	{"Server", HdrServer, HdrServerF},
	{"Service-Route", HdrServiceRoute, HdrServiceRouteF},
	{"Session-Expires", HdrSessionExpires, HdrSessionExpiresF},
	{"Subject", HdrSubject, HdrSubjectF},
	{"Subscription-State", HdrSubscriptionState, HdrSubscriptionStateF},
	{"Supported", HdrSupported, HdrSupportedF},

	{"t", HdrTo, HdrToF}, // compact form
	{"To", HdrTo, HdrToF},

	{"u", HdrAllow, HdrAllowF}, // compact form
	{"User-Agent", HdrUserAgent, HdrUserAgentF},

	{"v", HdrVia, HdrViaF}, // compact form
	{"Via", HdrVia, HdrViaF},
	{"VIA", HdrVia, HdrViaF},

	{"x", HdrSessionExpires, HdrSessionExpiresF}, // compact form
}

// ParseHeaderName parses a header field name and returns its type and flag
// C: char *parse_hname2(char *const begin, char *const end, struct hdr_field *const hdr)
func ParseHeaderName(buf []byte) (HdrType, HdrFlag, int) {
	// Find colon separator
	colon := bytes.IndexByte(buf, ':')
	if colon == -1 {
		return HdrError, 0, 0
	}

	// Extract header name (trim whitespace)
	name := buf[:colon]
	name = bytes.TrimSpace(name)

	// Look up header name in map
	nameStr := string(name)
	for _, entry := range hdrMap {
		if strings.EqualFold(entry.Name, nameStr) {
			return entry.Type, entry.Flag, colon + 1
		}
	}

	// Unknown header - return HdrOther with generic flag
	return HdrOther, 0, colon + 1
}

// HdrField represents a parsed SIP header field
// C: struct hdr_field
type HdrField struct {
	Name       str.Str
	Body       str.Str
	Type       HdrType
	Parsed     interface{} // type-specific parsed body
	Next       *HdrField   // next header of same type
	Siblings   *HdrField   // next sibling header
	Offset     int         // offset in original buffer
	Len        int         // total header length including CRLF
}

// ParseHeaders parses all headers from the given buffer
// Returns headers list, body offset, and error
func ParseHeaders(buf []byte) ([]*HdrField, int, error) {
	var headers []*HdrField
	var lastHeader *HdrField

	pos := 0
	for pos < len(buf) {
		// Check for end of headers (empty line)
		if pos+1 < len(buf) && buf[pos] == '\r' && buf[pos+1] == '\n' {
			// Empty line - end of headers
			return headers, pos + 2, nil
		}
		if buf[pos] == '\n' {
			return headers, pos + 1, nil
		}

		// Find end of this header line
		lineEnd := bytes.Index(buf[pos:], []byte("\r\n"))
		if lineEnd == -1 {
			lineEnd = bytes.IndexByte(buf[pos:], '\n')
			if lineEnd == -1 {
				return nil, 0, errors.New("header line without terminator")
			}
			lineEnd += pos
		} else {
			lineEnd += pos
		}

		// Check for folded headers (line starts with whitespace)
		// In Go, we handle this by checking if the next line starts with space/tab
		headerLine := buf[pos:lineEnd]

		// Parse header name and body
		hdrType, _, bodyOffset := ParseHeaderName(headerLine)
		if hdrType == HdrError {
			return nil, 0, errors.New("invalid header name")
		}

		// Extract body (after colon)
		body := headerLine[bodyOffset:]

		// Calculate name length (trim trailing whitespace before colon)
		nameLen := bodyOffset - 1
		for nameLen > 0 && (headerLine[nameLen-1] == ' ' || headerLine[nameLen-1] == '\t') {
			nameLen--
		}

		// Trim leading whitespace from body
		bodyTrimmed := bytes.TrimLeft(body, " \t")
		bodyOffsetTrimmed := bodyOffset + (len(body) - len(bodyTrimmed))

		hdr := &HdrField{
			Name: str.Str{S: buf[pos:], Len: nameLen},
			Body: str.Str{S: buf[pos+bodyOffsetTrimmed:], Len: len(bodyTrimmed)},
			Type: hdrType,
		}

		// For Via headers, eagerly parse the body and cache it in h.Parsed.
		// This mirrors Kamailio's parse_headers() behaviour for HdrViaF and
		// eliminates the need for every caller to lazily parse later.
		if hdrType == HdrVia {
			if vb, _, err := ParseMultiVia(hdr.Body); err == nil && vb != nil {
				hdr.Parsed = vb
			}
		}

		headers = append(headers, hdr)
		if lastHeader != nil {
			lastHeader.Siblings = hdr
		}
		lastHeader = hdr

		pos = lineEnd + 2 // skip CRLF
	}

	return headers, pos, nil
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * IMS-specific header field handling
 *
 * 3GPP TS 24.229 defines IMS-specific SIP headers:
 * - P-Asserted-Identity (PAI)
 * - P-Preferred-Identity (PPI)
 * - Privacy
 * - Path
 * - Service-Route
 * - P-Access-Network-Info
 * - P-Visited-Network-ID
 * - P-Charging-Vector
 * - Security-Client/Server/Verify
 */

package ims

import (
	"fmt"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// PAIInfo represents P-Asserted-Identity header content
// RFC 3325 / TS 24.229
type PAIInfo struct {
	DisplayName string
	URI         string
	IsSIP       bool
	IsTel       bool
}

// ParsePAI parses P-Asserted-Identity header
func ParsePAI(hdr *parser.HdrField) (*PAIInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPAI {
		return nil, fmt.Errorf("not a PAI header")
	}

	body := hdr.Body.String()

	info := &PAIInfo{}

	// Check if it's a tel: URI or sip: URI
	if strings.HasPrefix(body, "<") {
		// Angle-bracket format: <sip:user@host> or <tel:+1234>
		end := strings.Index(body, ">")
		if end > 0 {
			uri := body[1:end]
			info.URI = uri
			info.IsSIP = strings.HasPrefix(strings.ToLower(uri), "sip:") || strings.HasPrefix(strings.ToLower(uri), "sips:")
			info.IsTel = strings.HasPrefix(strings.ToLower(uri), "tel:")
		}
		// Check for display name before <
		if idx := strings.LastIndex(body[:strings.Index(body, "<")], "\""); idx >= 0 {
			info.DisplayName = strings.Trim(body[:strings.Index(body, "<")], " \"")
		}
	} else {
		// Plain URI format
		info.URI = strings.TrimSpace(body)
		info.IsSIP = strings.HasPrefix(strings.ToLower(info.URI), "sip:") || strings.HasPrefix(strings.ToLower(info.URI), "sips:")
		info.IsTel = strings.HasPrefix(strings.ToLower(info.URI), "tel:")
	}

	return info, nil
}

// PPIInfo represents P-Preferred-Identity header content
// RFC 3325 / TS 24.229
type PPIInfo struct {
	DisplayName string
	URI         string
	IsSIP       bool
	IsTel       bool
}

// ParsePPI parses P-Preferred-Identity header
func ParsePPI(hdr *parser.HdrField) (*PPIInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPPI {
		return nil, fmt.Errorf("not a PPI header")
	}

	body := hdr.Body.String()

	info := &PPIInfo{}

	if strings.HasPrefix(body, "<") {
		end := strings.Index(body, ">")
		if end > 0 {
			uri := body[1:end]
			info.URI = uri
			info.IsSIP = strings.HasPrefix(strings.ToLower(uri), "sip:") || strings.HasPrefix(strings.ToLower(uri), "sips:")
			info.IsTel = strings.HasPrefix(strings.ToLower(uri), "tel:")
		}
	} else {
		info.URI = strings.TrimSpace(body)
		info.IsSIP = strings.HasPrefix(strings.ToLower(info.URI), "sip:") || strings.HasPrefix(strings.ToLower(info.URI), "sips:")
		info.IsTel = strings.HasPrefix(strings.ToLower(info.URI), "tel:")
	}

	return info, nil
}

// PrivacyLevel represents Privacy header values
// RFC 3323 / TS 24.229
type PrivacyLevel int

const (
	PrivacyNone PrivacyLevel = iota
	PrivacyHeader
	PrivacySession
	PrivacyUser
	PrivacyID
	PrivacyCritical
	PrivacyDefault
	PrivacyNone2
)

// PrivacyInfo represents Privacy header content
type PrivacyInfo struct {
	Levels []PrivacyLevel
	Value  string
}

// ParsePrivacy parses Privacy header
func ParsePrivacy(hdr *parser.HdrField) (*PrivacyInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPrivacy {
		return nil, fmt.Errorf("not a Privacy header")
	}

	body := hdr.Body.String()
	info := &PrivacyInfo{Value: body}

	parts := strings.Split(body, ";")
	for _, part := range parts {
		part = strings.TrimSpace(strings.ToLower(part))
		switch part {
		case "header":
			info.Levels = append(info.Levels, PrivacyHeader)
		case "session":
			info.Levels = append(info.Levels, PrivacySession)
		case "user":
			info.Levels = append(info.Levels, PrivacyUser)
		case "id":
			info.Levels = append(info.Levels, PrivacyID)
		case "critical":
			info.Levels = append(info.Levels, PrivacyCritical)
		case "default":
			info.Levels = append(info.Levels, PrivacyDefault)
		case "none":
			info.Levels = append(info.Levels, PrivacyNone2)
		}
	}

	return info, nil
}

// HasPrivacyID returns true if Privacy: id is set
func (p *PrivacyInfo) HasPrivacyID() bool {
	for _, level := range p.Levels {
		if level == PrivacyID {
			return true
		}
	}
	return false
}

// PANIInfo represents P-Access-Network-Info header content
// TS 24.229 - used by P-CSCF to convey access network info
type PANIInfo struct {
	AccessType    string // e.g., "3GPP-UTRAN-TDD", "3GPP-E-UTRAN-FDD"
	CI3GPP        string // Cell ID
	UtranCellID   string
	LocationArea  string
	RoutingArea   string
	PLMN          string // MCC-MNC
	Raw           string
}

// ParsePANI parses P-Access-Network-Info header
func ParsePANI(hdr *parser.HdrField) (*PANIInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPAccessNetworkInfo {
		return nil, fmt.Errorf("not a P-Access-Network-Info header")
	}

	body := hdr.Body.String()
	info := &PANIInfo{Raw: body}

	// Parse access type and parameters
	// Format: "3GPP-UTRAN-TDD;utran-cell-id-3gpp=4600001234ABCD"
	parts := strings.Split(body, ";")
	if len(parts) > 0 {
		info.AccessType = strings.TrimSpace(parts[0])
	}

	for _, part := range parts[1:] {
		part = strings.TrimSpace(part)
		kv := strings.SplitN(part, "=", 2)
		if len(kv) == 2 {
			key := strings.TrimSpace(strings.ToLower(kv[0]))
			value := strings.TrimSpace(kv[1])
			switch key {
			case "utran-cell-id-3gpp":
				info.UtranCellID = value
				info.CI3GPP = value
			case "ci-3gpp":
				info.CI3GPP = value
			case "cgi-3gpp":
				info.CI3GPP = value
			case "location-area-3gpp":
				info.LocationArea = value
			case "routing-area-3gpp":
				info.RoutingArea = value
			case "plmn":
				info.PLMN = value
			}
		}
	}

	return info, nil
}

// PVNIInfo represents P-Visited-Network-ID header content
// TS 24.229 - used by P-CSCF to identify visited network
type PVNIInfo struct {
	NetworkID string
	Raw       string
}

// ParsePVNI parses P-Visited-Network-ID header
func ParsePVNI(hdr *parser.HdrField) (*PVNIInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPVisitedNetworkID {
		return nil, fmt.Errorf("not a P-Visited-Network-ID header")
	}

	body := hdr.Body.String()
	info := &PVNIInfo{Raw: body}

	// Remove quotes if present
	info.NetworkID = strings.Trim(body, "\"")

	return info, nil
}

// PathInfo represents Path header content
// RFC 3327 / TS 24.229
type PathInfo struct {
	URIs []string
	Raw  string
}

// ParsePath parses Path header
func ParsePath(hdr *parser.HdrField) (*PathInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPath {
		return nil, fmt.Errorf("not a Path header")
	}

	body := hdr.Body.String()
	info := &PathInfo{Raw: body}

	// Parse comma-separated URI list
	// Format: <sip:pcscf@host;lr>, <sip:scscf@host;lr>
	parts := strings.Split(body, ",")
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if strings.HasPrefix(part, "<") && strings.Contains(part, ">") {
			uri := part[1:strings.Index(part, ">")]
			info.URIs = append(info.URIs, uri)
		} else {
			info.URIs = append(info.URIs, part)
		}
	}

	return info, nil
}

// ServiceRouteInfo represents Service-Route header content
// RFC 3608 / TS 24.229
type ServiceRouteInfo struct {
	URIs []string
	Raw  string
}

// ParseServiceRoute parses Service-Route header
func ParseServiceRoute(hdr *parser.HdrField) (*ServiceRouteInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrServiceRoute {
		return nil, fmt.Errorf("not a Service-Route header")
	}

	body := hdr.Body.String()
	info := &ServiceRouteInfo{Raw: body}

	parts := strings.Split(body, ",")
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if strings.HasPrefix(part, "<") && strings.Contains(part, ">") {
			uri := part[1:strings.Index(part, ">")]
			info.URIs = append(info.URIs, uri)
		} else {
			info.URIs = append(info.URIs, part)
		}
	}

	return info, nil
}

// ChargingVectorInfo represents P-Charging-Vector header content
// TS 24.229
type ChargingVectorInfo struct {
	ICIDValue string // IMS Charging Identifier
	ICIDGenAddr string // ICID generating entity address
	OrigIOI   string // Originating Inter-Operator Identifier
	TermIOI   string // Terminating Inter-Operator Identifier
	Raw       string
}

// ParseChargingVector parses P-Charging-Vector header
func ParseChargingVector(hdr *parser.HdrField) (*ChargingVectorInfo, error) {
	if hdr == nil || hdr.Type != parser.HdrPChargingVector {
		return nil, fmt.Errorf("not a P-Charging-Vector header")
	}

	body := hdr.Body.String()
	info := &ChargingVectorInfo{Raw: body}

	parts := strings.Split(body, ";")
	for _, part := range parts {
		part = strings.TrimSpace(part)
		kv := strings.SplitN(part, "=", 2)
		if len(kv) == 2 {
			key := strings.TrimSpace(strings.ToLower(kv[0]))
			value := strings.TrimSpace(kv[1])
			switch key {
			case "icid-value":
				info.ICIDValue = value
			case "icid-generated-at":
				info.ICIDGenAddr = value
			case "orig-ioi":
				info.OrigIOI = value
			case "term-ioi":
				info.TermIOI = value
			}
		}
	}

	return info, nil
}

// BuildPAI builds a P-Asserted-Identity header value
func BuildPAI(uri string, displayName string) str.Str {
	if displayName != "" {
		return str.Mk(fmt.Sprintf("\"%s\" <%s>", displayName, uri))
	}
	return str.Mk(fmt.Sprintf("<%s>", uri))
}

// BuildPANI builds a P-Access-Network-Info header value
func BuildPANI(accessType string, cellID string) str.Str {
	if cellID != "" {
		return str.Mk(fmt.Sprintf("%s;utran-cell-id-3gpp=%s", accessType, cellID))
	}
	return str.Mk(accessType)
}

// BuildPath builds a Path header value from URI list
func BuildPath(uris []string) str.Str {
	if len(uris) == 0 {
		return str.Str{}
	}
	var parts []string
	for _, uri := range uris {
		parts = append(parts, fmt.Sprintf("<%s;lr>", uri))
	}
	return str.Mk(strings.Join(parts, ", "))
}

// BuildServiceRoute builds a Service-Route header value from URI list
func BuildServiceRoute(uris []string) str.Str {
	if len(uris) == 0 {
		return str.Str{}
	}
	var parts []string
	for _, uri := range uris {
		parts = append(parts, fmt.Sprintf("<%s;lr>", uri))
	}
	return str.Mk(strings.Join(parts, ", "))
}

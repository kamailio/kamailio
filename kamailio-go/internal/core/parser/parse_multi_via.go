// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Multi-Via header body parser + branch parameter RFC 3261 validation.
 *
 * Matches the combined functionality of:
 *   - C: char *parse_via_list(char *buf, char *end, struct via_body **first)
 *   - C: int check_via_branch(struct via_body *vb)
 *
 * A Via header can appear as a single value, as a comma-separated list on
 * one line, or as multiple Via header lines (in which case the caller is
 * responsible for concatenating the bodies with "," before passing them in).
 *
 * Grammar:
 *   Via-body   =  ( "Via" / "v" ) HCOLON via-parm *(COMMA via-parm)
 *   via-parm   =  sent-protocol LWS sent-by *( SEMI via-params )
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// BranchMagicCookie is the RFC 3261 branch parameter magic cookie prefix.
// C: #define RFC3261_BRANCH "z9hG4bK"
const BranchMagicCookie = "z9hG4bK"

// ParseMultiVia parses a comma-separated list of Via header bodies.
//
// Each comma-separated segment is parsed as an independent ViaBody.
// Segments inside quoted-strings are preserved (commas in quoted parameter
// values are not treated as separators). The result is returned as a linked
// list of ViaBody structures (chained through the Next pointer), together
// with the total count.
//
// C: char *parse_via_list(char *buf, char *end, struct via_body **first)
func ParseMultiVia(body str.Str) (*ViaBody, int, error) {
	s := strings.TrimSpace(body.String())
	if s == "" {
		return nil, 0, &MultiViaError{Msg: "empty via body"}
	}

	// Split on commas respecting quoted substrings (rare in Via but allowed
	// e.g. inside unknown parameter values).
	var parts []string
	inQuote := false
	start := 0
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '"' {
			inQuote = !inQuote
		} else if c == ',' && !inQuote {
			parts = append(parts, s[start:i])
			start = i + 1
		}
	}
	if start < len(s) {
		parts = append(parts, s[start:])
	}

	var first *ViaBody
	var last *ViaBody
	count := 0
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p == "" {
			continue
		}
		vb, err := ParseVia(str.Mk(p))
		if err != nil {
			return nil, count, err
		}
		if first == nil {
			first = vb
			last = vb
		} else {
			last.Next = vb
			last = vb
		}
		count++
	}
	if count == 0 {
		return nil, 0, &MultiViaError{Msg: "no valid via entries"}
	}
	return first, count, nil
}

// CountViaBodies returns the number of linked Via bodies reachable from first.
func CountViaBodies(first *ViaBody) int {
	count := 0
	for v := first; v != nil; v = v.Next {
		count++
	}
	return count
}

// ValidateBranch inspects the top Via's branch parameter and returns:
//
//   - hasBranch: whether a branch parameter exists
//   - hasMagicCookie: whether the branch value starts with "z9hG4bK"
//     (RFC 3261 compliant requests / responses between strict proxies)
//   - value: the raw branch value (including the magic cookie prefix, if any)
//
// C: int check_via_branch(struct via_body *vb)
func ValidateBranch(vb *ViaBody) (hasBranch bool, hasMagicCookie bool, value string) {
	if vb == nil || vb.Branch == nil {
		return false, false, ""
	}
	val := vb.Branch.Value.String()
	if val == "" {
		return true, false, val
	}
	return true, strings.HasPrefix(val, BranchMagicCookie), val
}

// HasValidBranch reports whether the Via has a branch parameter whose value
// begins with the RFC 3261 "z9hG4bK" magic cookie.
func HasValidBranch(vb *ViaBody) bool {
	_, m, _ := ValidateBranch(vb)
	return m
}

// GetBranchValue returns the branch value WITHOUT the "z9hG4bK" prefix.
// If the branch does not start with the magic cookie the full value is
// returned. Returns "" if there is no branch parameter.
func GetBranchValue(vb *ViaBody) string {
	if vb == nil || vb.Branch == nil {
		return ""
	}
	val := vb.Branch.Value.String()
	if strings.HasPrefix(val, BranchMagicCookie) {
		return val[len(BranchMagicCookie):]
	}
	return val
}

// GetSentBy returns the sent-by string as "host[:port]".
func (vb *ViaBody) GetSentBy() string {
	if vb == nil {
		return ""
	}
	if vb.PortStr.Len > 0 {
		return vb.Host.String() + ":" + vb.PortStr.String()
	}
	return vb.Host.String()
}

// GetTransport returns the transport protocol string (uppercase).
// Typical values: "UDP", "TCP", "TLS", "SCTP", "WS", "WSS".
func (vb *ViaBody) GetTransport() string {
	if vb == nil {
		return ""
	}
	return strings.ToUpper(vb.Transport.String())
}

// GetReceivedAddress returns the effective received address. If the Via
// carries a "received" parameter its value is returned (reflecting a
// possible NAT rewrite). Otherwise the host field is returned.
func (vb *ViaBody) GetReceivedAddress() string {
	if vb != nil && vb.Received != nil && vb.Received.Value.Len > 0 {
		return vb.Received.Value.String()
	}
	if vb != nil {
		return vb.Host.String()
	}
	return ""
}

// GetRPortValue returns the numeric value of the "rport" parameter as uint16.
// Returns 0 if the parameter is missing, empty, or not purely numeric.
func (vb *ViaBody) GetRPortValue() uint16 {
	if vb == nil || vb.RPort == nil || vb.RPort.Value.Len == 0 {
		return 0
	}
	var v uint16
	for _, c := range vb.RPort.Value.String() {
		if c < '0' || c > '9' {
			return 0
		}
		v = v*10 + uint16(c-'0')
	}
	return v
}

// HasAlias reports whether the "alias" Via parameter is present.
func (vb *ViaBody) HasAlias() bool {
	return vb != nil && vb.Alias != nil
}

// GetParam returns the named Via parameter (case-insensitive) from the
// linked ParamList. Prefer the typed fields (Branch, Received, RPort, ...)
// when available; this is for unknown/extension parameters.
func (vb *ViaBody) GetParam(name string) *ViaParam {
	if vb == nil {
		return nil
	}
	for p := vb.ParamList; p != nil; p = p.Next {
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// HasParam reports whether a Via parameter with the given name exists
// (case-insensitive).
func (vb *ViaBody) HasParam(name string) bool {
	return vb.GetParam(name) != nil
}

// ParamCount returns the total number of Via parameters attached.
func (vb *ViaBody) ParamCount() int {
	n := 0
	for p := vb.ParamList; p != nil; p = p.Next {
		n++
	}
	return n
}

// String reconstructs the Via header body as "SIP/2.0/UDP host[:port];params".
func (vb *ViaBody) String() string {
	if vb == nil {
		return ""
	}
	var sb strings.Builder
	sb.WriteString(vb.Name.String())
	sb.WriteString("/")
	sb.WriteString(vb.Version.String())
	sb.WriteString("/")
	sb.WriteString(vb.Transport.String())
	sb.WriteString(" ")
	sb.WriteString(vb.Host.String())
	if vb.PortStr.Len > 0 {
		sb.WriteString(":")
		sb.WriteString(vb.PortStr.String())
	}
	for p := vb.ParamList; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// ParseViaFromHeader parses a HdrField containing one or more comma-separated
// Via bodies. Returns a linked list of ViaBody structures, the total count,
// or an error if the header is not a Via header or parsing fails.
func ParseViaFromHeader(h *HdrField) (*ViaBody, int, error) {
	if h == nil || h.Type != HdrVia {
		return nil, 0, &MultiViaError{Msg: "nil or non-Via header"}
	}
	return ParseMultiVia(h.Body)
}

// MultiViaError represents an error that occurred while parsing multiple Via
// header bodies.
type MultiViaError struct {
	Msg string
}

func (e *MultiViaError) Error() string { return e.Msg }

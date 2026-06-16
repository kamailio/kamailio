// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Via header parser - matching C parse_via.c
 *
 * Via = "Via" HCOLON via-parm *(COMMA via-parm)
 * via-parm = sent-protocol LWS sent-by *( SEMI via-params )
 * sent-protocol = protocol-name SLASH protocol-version SLASH transport
 * sent-by = host [ COLON port ]
 * via-params = via-ttl / via-maddr / via-received / via-branch / via-rport / via-i / via-extension
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ViaParamType represents Via parameter types
type ViaParamType int

const (
	ViaParamError ViaParamType = -1
	ViaParamOther ViaParamType = 0
	ViaParamBranch ViaParamType = 1
	ViaParamReceived ViaParamType = 2
	ViaParamRPort ViaParamType = 3
	ViaParamTTL ViaParamType = 4
	ViaParamMAddr ViaParamType = 5
	ViaParamI ViaParamType = 6
	ViaParamAlias ViaParamType = 7
)

// ParseVia parses a Via header body
// C: char *parse_via(char *buffer, char *end, struct via_body *const vb)
func ParseVia(body str.Str) (*ViaBody, error) {
	vb := &ViaBody{
		Hdr: body,
	}

	s := string(body.S)
	if body.Len > 0 && len(body.S) > body.Len {
		s = string(body.S[:body.Len])
	}

	// Parse sent-protocol: SIP/2.0/UDP or SIP/2.0/TCP
	// Format: protocol-name/version/transport
	protoEnd := strings.Index(s, " ")
	if protoEnd == -1 {
		return nil, ErrInvalidVia
	}

	proto := s[:protoEnd]
	parts := strings.Split(proto, "/")
	if len(parts) != 3 {
		return nil, ErrInvalidVia
	}

	vb.Name = str.Mk(parts[0])     // SIP
	vb.Version = str.Mk(parts[1])  // 2.0
	vb.Transport = str.Mk(parts[2]) // UDP, TCP, TLS, SCTP, WS, WSS

	// Set protocol number
	switch strings.ToUpper(parts[2]) {
	case "UDP":
		vb.Proto = ProtoUDP
	case "TCP":
		vb.Proto = ProtoTCP
	case "TLS":
		vb.Proto = ProtoTLS
	case "SCTP":
		vb.Proto = ProtoSCTP
	case "WS":
		vb.Proto = ProtoWS
	case "WSS":
		vb.Proto = ProtoWSS
	default:
		vb.Proto = ProtoUDP
	}

	// Skip whitespace after protocol
	pos := protoEnd
	for pos < len(s) && (s[pos] == ' ' || s[pos] == '\t') {
		pos++
	}

	// Parse sent-by: host[:port]
	hostEnd := len(s)

	// Find end of host (semicolon for params, or end)
	semicolon := strings.Index(s[pos:], ";")
	if semicolon != -1 {
		hostEnd = pos + semicolon
	}

	// Also check for comma (multiple Via headers)
	comma := strings.Index(s[pos:], ",")
	if comma != -1 && pos+comma < hostEnd {
		hostEnd = pos + comma
	}

	hostPort := s[pos:hostEnd]

	// Check for port
	if colon := strings.LastIndex(hostPort, ":"); colon != -1 {
		// Check if it's IPv6 (contains [ or ])
		if !strings.Contains(hostPort[colon:], "]") {
			vb.Host = str.Mk(hostPort[:colon])
			vb.PortStr = str.Mk(hostPort[colon+1:])
			// Parse port number
			port := 0
			for _, c := range hostPort[colon+1:] {
				if c >= '0' && c <= '9' {
					port = port*10 + int(c-'0')
				} else {
					break
				}
			}
			vb.Port = uint16(port)
		} else {
			// IPv6 address without port
			vb.Host = str.Mk(hostPort)
		}
	} else {
		vb.Host = str.Mk(hostPort)
	}

	// Parse parameters
	if hostEnd < len(s) && s[hostEnd] == ';' {
		params := s[hostEnd+1:]
		vb.Params = str.Mk(params)
		vb.parseViaParams(params)
	}

	return vb, nil
}

// parseViaParams parses Via parameters
func (vb *ViaBody) parseViaParams(params string) {
	// Split by semicolon
	for _, param := range strings.Split(params, ";") {
		if param == "" {
			continue
		}

		// Parse parameter name and value
		var name, value string
		if eq := strings.Index(param, "="); eq != -1 {
			name = strings.TrimSpace(param[:eq])
			value = strings.TrimSpace(param[eq+1:])
		} else {
			name = strings.TrimSpace(param)
		}

		// Create ViaParam
		vp := &ViaParam{
			Name: str.Mk(name),
			Value: str.Mk(value),
		}

		// Determine parameter type
		switch strings.ToLower(name) {
		case "branch":
			vp.Type = int(ViaParamBranch)
			vb.Branch = vp
		case "received":
			vp.Type = int(ViaParamReceived)
			vb.Received = vp
		case "rport":
			vp.Type = int(ViaParamRPort)
			vb.RPort = vp
		case "ttl":
			vp.Type = int(ViaParamTTL)
		case "maddr":
			vp.Type = int(ViaParamMAddr)
		case "i", "interim":
			vp.Type = int(ViaParamI)
			vb.I = vp
		case "alias":
			vp.Type = int(ViaParamAlias)
			vb.Alias = vp
		default:
			vp.Type = int(ViaParamOther)
		}

		// Add to list
		if vb.ParamList == nil {
			vb.ParamList = vp
			vb.LastParam = vp
		} else {
			vb.LastParam.Next = vp
			vb.LastParam = vp
		}
	}
}

// Protocol constants
const (
	ProtoUDP  int16 = 1
	ProtoTCP  int16 = 2
	ProtoTLS  int16 = 3
	ProtoSCTP int16 = 4
	ProtoWS   int16 = 5
	ProtoWSS  int16 = 6
)

// ErrInvalidVia represents a Via parsing error
var ErrInvalidVia = &ViaParseError{Msg: "invalid Via header"}

// ViaParseError represents a Via parsing error
type ViaParseError struct {
	Msg string
	Pos int
}

func (e *ViaParseError) Error() string {
	return e.Msg
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Route/Record-Route header parser - matching C parse_rr.c
 *
 * Route = "Route" HCOLON route-param *(COMMA route-param)
 * Record-Route = "Record-Route" HCOLON rec-route *(COMMA rec-route)
 * rec-route = name-addr *(SEMI rr-param)
 * route-param = rr-param *(SEMI rr-param)
 * rr-param =  ( "lr" | "lr=" way | received | rport | rs | tag )
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// RRParam represents a Route parameter
type RRParam struct {
	Type  int // parameter type (RR_PARAM_LR, RR_PARAM_RPORT, etc.)
	Name  str.Str
	Value str.Str
	Next  *RRParam
}

// RRParamType represents Route parameter types
type RRParamType int

const (
	RRParamOther RRParamType = iota
	RRParamLR
	RRParamRPort
	RRParamReceived
	RRParamRS
	RRParamTag
)

// RRBody represents a parsed Route/Record-Route body
// C: struct rr_body
type RRBody struct {
	FirstURL  *RRUrl  // first route URL
	LastURL   *RRUrl  // last route URL
	Body      str.Str // raw body
}

// RRUrl represents a single Route URL
// C: struct rr
type RRUrl struct {
	// Inherited from SIPURI
	User       str.Str
	Host       str.Str
	Port       str.Str
	PortNo     uint16
	Proto      int16
	Type       URIType

	// Route-specific
	Name       str.Str
	Params     str.Str
	ParamList  *RRParam
	LastParam  *RRParam
	Next       *RRUrl
}

// HasLR returns true if the loose routing parameter is present
func (rr *RRUrl) HasLR() bool {
	for p := rr.ParamList; p != nil; p = p.Next {
		if p.Type == int(RRParamLR) || (p.Name.Len > 0 && strings.EqualFold(p.Name.String(), "lr")) {
			return true
		}
	}
	return false
}

// HasRPort returns true if the rport parameter is present
func (rr *RRUrl) HasRPort() bool {
	for p := rr.ParamList; p != nil; p = p.Next {
		if p.Type == int(RRParamRPort) || (p.Name.Len > 0 && strings.EqualFold(p.Name.String(), "rport")) {
			return true
		}
	}
	return false
}

// HasReceived returns true if the received parameter is present
func (rr *RRUrl) HasReceived() bool {
	for p := rr.ParamList; p != nil; p = p.Next {
		if p.Name.Len > 0 && strings.EqualFold(p.Name.String(), "received") {
			return true
		}
	}
	return false
}

// GetReceived returns the received parameter value
func (rr *RRUrl) GetReceived() string {
	for p := rr.ParamList; p != nil; p = p.Next {
		if p.Name.Len > 0 && strings.EqualFold(p.Name.String(), "received") {
			return p.Value.String()
		}
	}
	return ""
}

// String returns the Route URL as a string
func (rr *RRUrl) String() string {
	var sb strings.Builder

	if rr.Name.Len > 0 {
		sb.WriteString(rr.Name.String())
		sb.WriteString(" ")
	}

	sb.WriteString("<")

	if rr.Type == SIPSURIT {
		sb.WriteString("sips:")
	} else {
		sb.WriteString("sip:")
	}

	if rr.User.Len > 0 {
		sb.WriteString(rr.User.String())
		sb.WriteString("@")
	}

	// Handle IPv6
	host := rr.Host.String()
	if strings.Contains(host, ":") {
		sb.WriteString("[")
		sb.WriteString(host)
		sb.WriteString("]")
	} else {
		sb.WriteString(host)
	}

	if rr.Port.Len > 0 {
		sb.WriteString(":")
		sb.WriteString(rr.Port.String())
	}

	sb.WriteString(">")

	// Add parameters
	if rr.Params.Len > 0 {
		sb.WriteString(";")
		sb.WriteString(rr.Params.String())
	}

	return sb.String()
}

// ParseRoute parses a Route header body
// C: char *parse_rr(char *buffer, char *end, struct rr **rr_parsed)
func ParseRoute(body str.Str) (*RRBody, error) {
	return parseRRBody(body, false)
}

// ParseRecordRoute parses a Record-Route header body
// C: char *parse_rr(char *buffer, char *end, struct rr **rr_parsed)
func ParseRecordRoute(body str.Str) (*RRBody, error) {
	return parseRRBody(body, true)
}

// parseRRBody is the internal parser for Route/Record-Route
func parseRRBody(body str.Str, isRecordRoute bool) (*RRBody, error) {
	rrb := &RRBody{
		Body: body,
	}

	s := string(body.S)
	if body.Len > 0 && len(body.S) > body.Len {
		s = string(body.S[:body.Len])
	}

	// Split by comma (but not inside angle brackets)
	var routes []string
	inAngle := false
	start := 0

	for i := 0; i <= len(s); i++ {
		if i == len(s) || (s[i] == ',' && !inAngle) {
			if i > start {
				route := strings.TrimSpace(s[start:i])
				if route != "" {
					routes = append(routes, route)
				}
			}
			start = i + 1
		} else if s[i] == '<' {
			inAngle = true
		} else if s[i] == '>' {
			inAngle = false
		}
	}

	// Parse each route
	var first, last *RRUrl
	for _, route := range routes {
		rrUrl, err := parseSingleRoute(route)
		if err != nil {
			continue // Skip invalid routes
		}

		if first == nil {
			first = rrUrl
			last = rrUrl
		} else {
			last.Next = rrUrl
			last = rrUrl
		}
	}

	rrb.FirstURL = first
	rrb.LastURL = last

	return rrb, nil
}

// parseSingleRoute parses a single Route URL
func parseSingleRoute(route string) (*RRUrl, error) {
	rr := &RRUrl{}

	// Check for name-addr format: <uri> or "name" <uri>
	laquot := strings.Index(route, "<")
	raquot := strings.Index(route, ">")

	if laquot != -1 && raquot != -1 && raquot > laquot {
		// name-addr format
		name := strings.TrimSpace(route[:laquot])
		if name != "" {
			rr.Name = str.Mk(name)
		}

		// Parse URI
		uri := route[laquot+1 : raquot]
		uri = strings.TrimPrefix(uri, "sips:")
		uri = strings.TrimPrefix(uri, "sip:")

		if strings.HasPrefix(route, "sips:") || strings.HasPrefix(route, "SIPS:") {
			rr.Type = SIPSURIT
		} else {
			rr.Type = SIPURIT
		}

		// Parse the URI parts
		parseRRUri(rr, uri)

		// Parse parameters after >
		if raquot+1 < len(route) {
			params := strings.TrimSpace(route[raquot+1:])
			if strings.HasPrefix(params, ";") {
				params = params[1:]
			}
			rr.Params = str.Mk(params)
			parseRRParams(rr, params)
		}
	} else {
		// Just URI
		uri := strings.TrimSpace(route)
		uri = strings.TrimPrefix(uri, "sips:")
		uri = strings.TrimPrefix(uri, "sip:")

		if strings.HasPrefix(route, "sips:") || strings.HasPrefix(route, "SIPS:") {
			rr.Type = SIPSURIT
		} else {
			rr.Type = SIPURIT
		}

		parseRRUri(rr, uri)
	}

	return rr, nil
}

// parseRRUri parses the URI part of a Route URL
func parseRRUri(rr *RRUrl, uri string) {
	// Check for user@host
	if at := strings.Index(uri, "@"); at != -1 {
		rr.User = str.Mk(uri[:at])
		uri = uri[at+1:]
	}

	// Check for port
	if colon := strings.LastIndex(uri, ":"); colon != -1 {
		// Make sure colon is not part of IPv6
		if !strings.Contains(uri[colon:], "]") {
			rr.Host = str.Mk(uri[:colon])
			rr.Port = str.Mk(uri[colon+1:])
			// Parse port number
			port := 0
			for _, c := range uri[colon+1:] {
				if c >= '0' && c <= '9' {
					port = port*10 + int(c-'0')
				} else {
					break
				}
			}
			rr.PortNo = uint16(port)
		} else {
			rr.Host = str.Mk(uri)
		}
	} else {
		rr.Host = str.Mk(uri)
	}

	// Default port
	if rr.PortNo == 0 {
		rr.PortNo = 5060
	}
}

// parseRRParams parses Route parameters
func parseRRParams(rr *RRUrl, params string) {
	// Split by semicolon
	for _, param := range strings.Split(params, ";") {
		if param == "" {
			continue
		}

		param = strings.TrimSpace(param)

		var name, value string
		if eq := strings.Index(param, "="); eq != -1 {
			name = strings.TrimSpace(param[:eq])
			value = strings.TrimSpace(param[eq+1:])
		} else {
			name = param
		}

		p := &RRParam{
			Name:  str.Mk(name),
			Value: str.Mk(value),
		}

		// Determine parameter type
		switch strings.ToLower(name) {
		case "lr":
			p.Type = int(RRParamLR)
		case "rport":
			p.Type = int(RRParamRPort)
		case "received":
			p.Type = int(RRParamReceived)
		case "rs":
			p.Type = int(RRParamRS)
		case "tag":
			p.Type = int(RRParamTag)
		default:
			p.Type = int(RRParamOther)
		}

		if rr.ParamList == nil {
			rr.ParamList = p
			rr.LastParam = p
		} else {
			rr.LastParam.Next = p
			rr.LastParam = p
		}
	}
}

// RRCount returns the number of routes in the body
func (rrb *RRBody) RRCount() int {
	count := 0
	for r := rrb.FirstURL; r != nil; r = r.Next {
		count++
	}
	return count
}

// IsEmpty returns true if there are no routes
func (rrb *RRBody) IsEmpty() bool {
	return rrb.FirstURL == nil
}

// First returns the first route URL
func (rrb *RRBody) First() *RRUrl {
	return rrb.FirstURL
}

// Last returns the last route URL
func (rrb *RRBody) Last() *RRUrl {
	return rrb.LastURL
}

// GetAll returns all route URLs
func (rrb *RRBody) GetAll() []*RRUrl {
	var routes []*RRUrl
	for r := rrb.FirstURL; r != nil; r = r.Next {
		routes = append(routes, r)
	}
	return routes
}

// String returns the Route body as a string
func (rrb *RRBody) String() string {
	var sb strings.Builder
	for r := rrb.FirstURL; r != nil; r = r.Next {
		if sb.Len() > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(r.String())
	}
	return sb.String()
}

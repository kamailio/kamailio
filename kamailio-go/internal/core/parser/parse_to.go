// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * From/To header parser - matching C parse_from.c / parse_to.c
 *
 * From = "From" HCOLON from-spec
 * from-spec = ( name-addr / addr-spec ) *( SEMI from-param )
 * To = "To" HCOLON ( name-addr / addr-spec ) *( SEMI to-param )
 * name-addr = [ display-name ] LAQUOT addr-spec RAQUOT
 * addr-spec = SIP-URI / SIPS-URI / absoluteURI
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ParseToBody parses a To/From header body
// C: char *parse_to(char *buffer, char *end, struct to_body *const to_b)
func ParseToBody(body str.Str) (*ToBody, error) {
	tb := &ToBody{}

	s := string(body.S)
	if body.Len > 0 && len(body.S) > body.Len {
		s = string(body.S[:body.Len])
	}

	// Find angle brackets
	laquot := strings.Index(s, "<")
	raquot := strings.Index(s, ">")

	if laquot != -1 && raquot != -1 && raquot > laquot {
		// name-addr form: display-name <uri>
		tb.DisplayName = str.Mk(strings.TrimSpace(s[:laquot]))

		// Parse URI
		uriStr := s[laquot+1 : raquot]
		uri, err := ParseURI(uriStr)
		if err != nil {
			return nil, err
		}
		tb.URI = uri
		tb.ParsedURI = uri

		// Parse parameters after >
		if raquot+1 < len(s) {
			params := s[raquot+1:]
			tb.parseToParams(params)
		}
	} else {
		// addr-spec form: just URI
		// Find parameters (semicolon)
		semi := strings.Index(s, ";")
		var uriStr string
		if semi != -1 {
			uriStr = s[:semi]
			tb.parseToParams(s[semi:])
		} else {
			uriStr = s
		}

		uri, err := ParseURI(strings.TrimSpace(uriStr))
		if err != nil {
			return nil, err
		}
		tb.URI = uri
		tb.ParsedURI = uri
	}

	return tb, nil
}

// parseToParams parses To/From parameters
func (tb *ToBody) parseToParams(params string) {
	if len(params) == 0 {
		return
	}

	// Remove leading semicolon if present
	if params[0] == ';' {
		params = params[1:]
	}

	tb.Params = str.Mk(params)

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

		// Check for tag parameter
		if strings.ToLower(name) == "tag" {
			tb.Tag = str.Mk(value)
		}
	}
}

// ParseFromBody parses a From header body (same format as To)
func ParseFromBody(body str.Str) (*ToBody, error) {
	return ParseToBody(body)
}

// GetName returns the display name or user@host if no display name
func (tb *ToBody) GetName() string {
	if tb.DisplayName.Len > 0 {
		return string(tb.DisplayName.S[:tb.DisplayName.Len])
	}
	if tb.URI != nil {
		if tb.URI.User.Len > 0 && tb.URI.Host.Len > 0 {
			user := string(tb.URI.User.S[:tb.URI.User.Len])
			host := string(tb.URI.Host.S[:tb.URI.Host.Len])
			return user + "@" + host
		}
		if tb.URI.Host.Len > 0 {
			return string(tb.URI.Host.S[:tb.URI.Host.Len])
		}
	}
	return ""
}

// GetURI returns the URI as a string
func (tb *ToBody) GetURI() string {
	if tb.URI == nil {
		return ""
	}
	return tb.URI.String()
}

// GetTag returns the tag parameter
func (tb *ToBody) GetTag() string {
	if tb.Tag.Len > 0 {
		return string(tb.Tag.S[:tb.Tag.Len])
	}
	return ""
}

// HasTag returns true if tag parameter exists
func (tb *ToBody) HasTag() bool {
	return tb.Tag.Len > 0
}

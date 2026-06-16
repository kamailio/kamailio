// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Contact header parser - matching C parse_contact.c
 *
 * Contact = "Contact" HCOLON ( STAR / ( contact-param *( COMMA contact-param ) ) )
 * contact-param = ( name-addr | addr-spec ) *( SEMI contact-params )
 * contact-params = c-p-q / c-p-expires / contact-extension
 */

package parser

import (
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ParseContact parses a Contact header body
// C: int parse_contact(struct hdr_field *_h, void *_c)
func ParseContact(body str.Str) (*ContactBody, error) {
	cb := &ContactBody{}

	s := string(body.S)
	if body.Len > 0 && len(body.S) > body.Len {
		s = string(body.S[:body.Len])
	}

	// Check for STAR
	if strings.TrimSpace(s) == "*" {
		return cb, nil
	}

	// Find angle brackets
	laquot := strings.Index(s, "<")
	raquot := strings.Index(s, ">")

	if laquot != -1 && raquot != -1 && raquot > laquot {
		// name-addr form: display-name <uri>
		cb.DisplayName = str.Mk(strings.TrimSpace(s[:laquot]))

		// Parse URI
		uriStr := s[laquot+1 : raquot]
		uri, err := ParseURI(uriStr)
		if err != nil {
			return nil, err
		}
		cb.URI = uri

		// Parse parameters after >
		if raquot+1 < len(s) {
			cb.parseContactParams(s[raquot+1:])
		}
	} else {
		// addr-spec form: just URI
		semi := strings.Index(s, ";")
		var uriStr string
		if semi != -1 {
			uriStr = s[:semi]
			cb.parseContactParams(s[semi:])
		} else {
			uriStr = s
		}

		uri, err := ParseURI(strings.TrimSpace(uriStr))
		if err != nil {
			return nil, err
		}
		cb.URI = uri
	}

	return cb, nil
}

// parseContactParams parses Contact parameters
func (cb *ContactBody) parseContactParams(params string) {
	if len(params) == 0 {
		return
	}

	// Remove leading semicolon if present
	if params[0] == ';' {
		params = params[1:]
	}

	cb.Params = str.Mk(params)

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

		// Check for known parameters
		switch strings.ToLower(name) {
		case "q":
			// Parse q-value
			if q, err := strconv.ParseFloat(value, 32); err == nil {
				cb.QValue = float32(q)
			}
		case "expires":
			// Parse expires
			if exp, err := strconv.Atoi(value); err == nil {
				cb.Expires = exp
			}
		case "+sip.instance":
			cb.Instance = str.Mk(value)
		case "reg-id":
			if regID, err := strconv.ParseUint(value, 10, 32); err == nil {
				cb.RegID = uint32(regID)
			}
		}
	}
}

// GetName returns the display name or user@host if no display name
func (cb *ContactBody) GetName() string {
	if cb.DisplayName.Len > 0 {
		return string(cb.DisplayName.S[:cb.DisplayName.Len])
	}
	if cb.URI != nil {
		if cb.URI.User.Len > 0 && cb.URI.Host.Len > 0 {
			user := string(cb.URI.User.S[:cb.URI.User.Len])
			host := string(cb.URI.Host.S[:cb.URI.Host.Len])
			return user + "@" + host
		}
		if cb.URI.Host.Len > 0 {
			return string(cb.URI.Host.S[:cb.URI.Host.Len])
		}
	}
	return ""
}

// GetURI returns the URI as a string
func (cb *ContactBody) GetURI() string {
	if cb.URI == nil {
		return ""
	}
	return cb.URI.String()
}

// HasExpires returns true if expires parameter exists
func (cb *ContactBody) HasExpires() bool {
	return cb.Expires != 0
}

// GetExpires returns the expires value
func (cb *ContactBody) GetExpires() int {
	return cb.Expires
}

// HasQValue returns true if q parameter exists
func (cb *ContactBody) HasQValue() bool {
	return cb.QValue != 0
}

// GetQValue returns the q-value
func (cb *ContactBody) GetQValue() float32 {
	return cb.QValue
}

// ParseContactList parses multiple Contact headers (comma-separated)
func ParseContactList(body str.Str) ([]*ContactBody, error) {
	s := string(body.S)
	if body.Len > 0 && len(body.S) > body.Len {
		s = string(body.S[:body.Len])
	}

	// Check for STAR
	if strings.TrimSpace(s) == "*" {
		return []*ContactBody{{}}, nil
	}

	var contacts []*ContactBody

	// Split by comma, but be careful with URIs containing commas
	// We need to track angle brackets
	inAngle := false
	start := 0

	for i := 0; i < len(s); i++ {
		switch s[i] {
		case '<':
			inAngle = true
		case '>':
			inAngle = false
		case ',':
			if !inAngle {
				// Parse this contact
				cb, err := ParseContact(str.Mk(s[start:i]))
				if err != nil {
					return nil, err
				}
				contacts = append(contacts, cb)
				start = i + 1
			}
		}
	}

	// Parse last contact
	if start < len(s) {
		cb, err := ParseContact(str.Mk(s[start:]))
		if err != nil {
			return nil, err
		}
		contacts = append(contacts, cb)
	}

	return contacts, nil
}

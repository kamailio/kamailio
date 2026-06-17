// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Generic name-addr / addr-spec parser - matching the common parts of
 * parse_to.c / parse_contact.c / parse_rpid.c / parse_ppi_pai.c.
 *
 * This is the "one true parser" for headers of the form:
 *
 *   [display-name] <uri> ;param=value;...
 *   uri ;param=value;...
 *
 * Used for To / From / Contact / Route / Record-Route / Refer-To
 *          / P-Asserted-Identity / P-Preferred-Identity / Remote-Party-ID.
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// AddrSpec represents a parsed name-addr or addr-spec entry.
// C: struct to_body concept, generalized for all name-addr headers.
type AddrSpec struct {
	DisplayName str.Str   // display-name (if present, unquoted)
	URI         *SIPURI   // parsed SIP URI (only set when parseURI=true)
	URIString   str.Str   // raw URI string (always set)
	URIType     URIType   // URI scheme type (SIPURIT, SIPSURIT, TELURIT, ...)
	Params      *Param    // generic parameters as linked list
	LastParam   *Param    // tail pointer for efficient append
}

// ParseNameAddr parses a generic "[display] <uri>;params" or "uri;params" entry.
//
//   - If parseURI is true the URI string is additionally parsed into an *SIPURI
//     (accessible via AddrSpec.URI). If URI parsing fails the URI field stays nil
//     but URIString is still populated.
//
// C: conceptually the shared body of parse_to / parse_contact / parse_rpid /
// parse_ppi / parse_pai for the name-addr / addr-spec part.
func ParseNameAddr(s string, parseURI bool) (*AddrSpec, error) {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil, &AddrSpecError{Msg: "empty name-addr"}
	}

	as := &AddrSpec{}

	// Find '<' that is not inside a quoted display-name.
	inQuote := false
	laquo := -1
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '"' {
			inQuote = !inQuote
		} else if c == '<' && !inQuote {
			laquo = i
			break
		}
	}

	raquo := -1
	if laquo != -1 {
		for i := laquo + 1; i < len(s); i++ {
			if s[i] == '>' {
				raquo = i
				break
			}
		}
	}

	if laquo != -1 && raquo != -1 {
		// name-addr form: [display] <uri>;params
		display := strings.TrimSpace(s[:laquo])
		if display != "" {
			display = stripQuotes(display)
			display = strings.TrimSpace(display)
			if display != "" {
				as.DisplayName = str.Mk(display)
			}
		}

		uriStr := strings.TrimSpace(s[laquo+1 : raquo])
		as.URIString = str.Mk(uriStr)
		as.URIType = detectURIType(uriStr)

		if parseURI {
			if uri, err := ParseURI(uriStr); err == nil {
				as.URI = uri
			}
		}

		// parameters after '>'
		if raquo+1 < len(s) {
			rest := strings.TrimSpace(s[raquo+1:])
			if strings.HasPrefix(rest, ";") {
				rest = strings.TrimSpace(rest[1:])
			}
			if rest != "" {
				params, _ := ParseParams(rest, ';')
				as.Params = params
				for p := params; p != nil; p = p.Next {
					as.LastParam = p
				}
			}
		}
	} else {
		// addr-spec form: just uri;params
		semi := strings.IndexByte(s, ';')
		var uriStr string
		if semi == -1 {
			uriStr = s
		} else {
			uriStr = strings.TrimSpace(s[:semi])
			rest := strings.TrimSpace(s[semi+1:])
			if rest != "" {
				params, _ := ParseParams(rest, ';')
				as.Params = params
				for p := params; p != nil; p = p.Next {
					as.LastParam = p
				}
			}
		}
		as.URIString = str.Mk(uriStr)
		as.URIType = detectURIType(uriStr)

		if parseURI {
			if uri, err := ParseURI(uriStr); err == nil {
				as.URI = uri
			}
		}
	}

	return as, nil
}

// GetParam returns a named parameter (case-insensitive) from this entry.
// Returns nil if not present.
func (as *AddrSpec) GetParam(name string) *Param {
	return ParamListGet(as.Params, name)
}

// HasParam reports whether this entry has a parameter with the given name
// (case-insensitive lookup).
func (as *AddrSpec) HasParam(name string) bool {
	return as.GetParam(name) != nil
}

// ParamCount returns the number of parameters attached to this entry.
func (as *AddrSpec) ParamCount() int {
	n := 0
	for p := as.Params; p != nil; p = p.Next {
		n++
	}
	return n
}

// String reconstructs the name-addr/addr-spec as "display" <uri>;params.
func (as *AddrSpec) String() string {
	var sb strings.Builder
	if as.DisplayName.Len > 0 {
		sb.WriteString("\"")
		sb.WriteString(as.DisplayName.String())
		sb.WriteString("\" ")
	}
	sb.WriteString("<")
	sb.WriteString(as.URIString.String())
	sb.WriteString(">")
	for p := as.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// AddrSpecList represents a comma-separated list of name-addr / addr-spec entries
// (e.g. Route, Record-Route, Contact, P-Asserted-Identity, ...).
// Entries are linked by their Next pointer and also indexed in Entries.
type AddrSpecList struct {
	First   *AddrSpec
	Last    *AddrSpec
	Count   int
	Entries []*AddrSpec
}

// ParseNameAddrList parses a comma-separated list of name-addr / addr-spec entries.
//
// Commas inside angle-brackets (< uri-with-, >) or quoted display names are not
// treated as separators. Each entry is parsed by ParseNameAddr.
//
//   - If parseURI is true every URI is parsed into *SIPURI (see ParseNameAddr).
//
// C: analogous to the comma-splitting loops in parse_contact.c (contact list)
// and parse_ppi_pai.c (multiple identity entries).
func ParseNameAddrList(s string, parseURI bool) (*AddrSpecList, error) {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil, &AddrSpecError{Msg: "empty name-addr list"}
	}

	var entries []string
	var current strings.Builder
	inAngle := false
	inQuote := false

	for i := 0; i < len(s); i++ {
		c := s[i]
		switch c {
		case '"':
			inQuote = !inQuote
			current.WriteByte(c)
		case '<':
			inAngle = true
			current.WriteByte(c)
		case '>':
			inAngle = false
			current.WriteByte(c)
		case ',':
			if !inAngle && !inQuote {
				entry := strings.TrimSpace(current.String())
				if entry != "" {
					entries = append(entries, entry)
				}
				current.Reset()
			} else {
				current.WriteByte(c)
			}
		default:
			current.WriteByte(c)
		}
	}

	remaining := strings.TrimSpace(current.String())
	if remaining != "" {
		entries = append(entries, remaining)
	}

	list := &AddrSpecList{}
	for _, e := range entries {
		as, err := ParseNameAddr(e, parseURI)
		if err != nil {
			return nil, err
		}
		if list.First == nil {
			list.First = as
			list.Last = as
		} else {
			list.Last = as
		}
		list.Entries = append(list.Entries, as)
		list.Count++
	}

	if list.Count == 0 {
		return nil, &AddrSpecError{Msg: "no valid name-addr entries"}
	}
	return list, nil
}

// String reconstructs the full comma-separated list.
func (l *AddrSpecList) String() string {
	if l == nil {
		return ""
	}
	var sb strings.Builder
	for i, e := range l.Entries {
		if i > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(e.String())
	}
	return sb.String()
}

// GetByURIHost returns the first entry whose URI string contains the given host
// substring (case-insensitive). Useful for Route/Record-Route lookups.
func (l *AddrSpecList) GetByURIHost(host string) *AddrSpec {
	if l == nil || host == "" {
		return nil
	}
	lower := strings.ToLower(host)
	for _, e := range l.Entries {
		if strings.Contains(strings.ToLower(e.URIString.String()), lower) {
			return e
		}
	}
	return nil
}

// AddrSpecError represents a name-addr / addr-spec parsing error.
type AddrSpecError struct {
	Msg string
}

func (e *AddrSpecError) Error() string { return e.Msg }

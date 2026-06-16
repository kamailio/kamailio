// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * P-Preferred-Identity / P-Asserted-Identity header parser - matching C
 * parse_ppi_pai.c (RFC 3325 / RFC 7315).
 *
 * P-Preferred-Identity = "P-Preferred-Identity" HCOLON
 *                          ( name-addr / addr-spec )
 *                          *( COMMA ( name-addr / addr-spec ) )
 * P-Asserted-Identity  = "P-Asserted-Identity" HCOLON
 *                          ( name-addr / addr-spec )
 *                          *( COMMA ( name-addr / addr-spec ) )
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// IdentityEntry represents a single PPI/PAI identity entry
// C: struct to_body (reused) / struct ppi_body entry
type IdentityEntry struct {
	DisplayName str.Str
	URI         str.Str
	URIType     URIType
	Params      *Param
	LastParam   *Param
	Next        *IdentityEntry
}

// ========== P-Preferred-Identity ==========

// PreferredIdentityBody represents a parsed P-Preferred-Identity header
// C: struct ppi_body
type PreferredIdentityBody struct {
	First *IdentityEntry
	Last  *IdentityEntry
	Count int
}

// ParsePreferredIdentity parses P-Preferred-Identity header body
// C: char *parse_ppi(char *buf, unsigned int len, struct ppi_body **pb)
func ParsePreferredIdentity(body str.Str) (*PreferredIdentityBody, error) {
	pb := &PreferredIdentityBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &PPIError{Msg: "empty preferred-identity body"}
	}

	entries := parseIdentityEntries(s)
	for _, e := range entries {
		if pb.First == nil {
			pb.First = e
			pb.Last = e
		} else {
			pb.Last.Next = e
			pb.Last = e
		}
		pb.Count++
	}

	if pb.Count == 0 {
		return nil, &PPIError{Msg: "no valid identity entries"}
	}
	return pb, nil
}

// ParsePreferredIdentityFromHeader parses P-Preferred-Identity from a header field
func ParsePreferredIdentityFromHeader(hdr *HdrField) (*PreferredIdentityBody, error) {
	if hdr == nil {
		return nil, &PPIError{Msg: "nil header"}
	}
	return ParsePreferredIdentity(hdr.Body)
}

// ========== P-Asserted-Identity ==========

// AssertedIdentityBody represents a parsed P-Asserted-Identity header
// C: struct pai_body
type AssertedIdentityBody struct {
	First *IdentityEntry
	Last  *IdentityEntry
	Count int
}

// ParseAssertedIdentity parses P-Asserted-Identity header body
// C: char *parse_pai(char *buf, unsigned int len, struct pai_body **pab)
func ParseAssertedIdentity(body str.Str) (*AssertedIdentityBody, error) {
	pab := &AssertedIdentityBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &PPIError{Msg: "empty asserted-identity body"}
	}

	entries := parseIdentityEntries(s)
	for _, e := range entries {
		if pab.First == nil {
			pab.First = e
			pab.Last = e
		} else {
			pab.Last.Next = e
			pab.Last = e
		}
		pab.Count++
	}

	if pab.Count == 0 {
		return nil, &PPIError{Msg: "no valid identity entries"}
	}
	return pab, nil
}

// ParseAssertedIdentityFromHeader parses P-Asserted-Identity from a header field
func ParseAssertedIdentityFromHeader(hdr *HdrField) (*AssertedIdentityBody, error) {
	if hdr == nil {
		return nil, &PPIError{Msg: "nil header"}
	}
	return ParseAssertedIdentity(hdr.Body)
}

// ========== shared helpers ==========

// parseIdentityEntries parses comma-separated name-addr/addr-spec entries
// respecting angle brackets and quoted strings.
func parseIdentityEntries(s string) []*IdentityEntry {
	var entries []*IdentityEntry
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
				entry := parseIdentityEntry(strings.TrimSpace(current.String()))
				if entry != nil {
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
		entry := parseIdentityEntry(remaining)
		if entry != nil {
			entries = append(entries, entry)
		}
	}
	return entries
}

// parseIdentityEntry parses a single "display-name" <uri>;params entry
func parseIdentityEntry(s string) *IdentityEntry {
	if s == "" {
		return nil
	}
	entry := &IdentityEntry{}

	laquo := strings.IndexByte(s, '<')
	raquo := strings.IndexByte(s, '>')

	if laquo != -1 && raquo != -1 && raquo > laquo {
		display := strings.TrimSpace(s[:laquo])
		if display != "" {
			display = stripQuotes(display)
			display = strings.TrimSpace(display)
			if display != "" {
				entry.DisplayName = str.Mk(display)
			}
		}
		uri := strings.TrimSpace(s[laquo+1 : raquo])
		entry.URI = str.Mk(uri)
		entry.URIType = detectURIType(uri)

		if raquo+1 < len(s) {
			rest := strings.TrimSpace(s[raquo+1:])
			if strings.HasPrefix(rest, ";") {
				rest = strings.TrimSpace(rest[1:])
			}
			if rest != "" {
				params, _ := ParseParams(rest, ';')
				entry.Params = params
				for p := params; p != nil; p = p.Next {
					entry.LastParam = p
				}
			}
		}
	} else {
		semi := strings.IndexByte(s, ';')
		if semi == -1 {
			uri := strings.TrimSpace(s)
			entry.URI = str.Mk(uri)
			entry.URIType = detectURIType(uri)
		} else {
			uri := strings.TrimSpace(s[:semi])
			entry.URI = str.Mk(uri)
			entry.URIType = detectURIType(uri)
			paramsStr := strings.TrimSpace(s[semi+1:])
			if paramsStr != "" {
				params, _ := ParseParams(paramsStr, ';')
				entry.Params = params
				for p := params; p != nil; p = p.Next {
					entry.LastParam = p
				}
			}
		}
	}
	return entry
}

// detectURIType returns URI type based on scheme prefix
func detectURIType(uri string) URIType {
	lower := strings.ToLower(strings.TrimSpace(uri))
	switch {
	case strings.HasPrefix(lower, "sips:"):
		return SIPSURIT
	case strings.HasPrefix(lower, "sip:"):
		return SIPURIT
	case strings.HasPrefix(lower, "tel:"):
		return TELURIT
	default:
		return ErrorURIT
	}
}

// identityEntryString reconstructs a single entry as "display" <uri>;params
func identityEntryString(e *IdentityEntry) string {
	var sb strings.Builder
	if e.DisplayName.Len > 0 {
		sb.WriteString("\"")
		sb.WriteString(e.DisplayName.String())
		sb.WriteString("\" ")
	}
	sb.WriteString("<")
	sb.WriteString(e.URI.String())
	sb.WriteString(">")
	for p := e.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}
	return sb.String()
}

// ========== String methods ==========

// String returns PPI as a reconstructed string (comma-separated entries)
func (pb *PreferredIdentityBody) String() string {
	var sb strings.Builder
	for e := pb.First; e != nil; e = e.Next {
		if sb.Len() > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(identityEntryString(e))
	}
	return sb.String()
}

// String returns PAI as a reconstructed string (comma-separated entries)
func (pab *AssertedIdentityBody) String() string {
	var sb strings.Builder
	for e := pab.First; e != nil; e = e.Next {
		if sb.Len() > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(identityEntryString(e))
	}
	return sb.String()
}

// PPIError represents PPI/PAI parsing errors
type PPIError struct {
	Msg string
}

func (e *PPIError) Error() string {
	return e.Msg
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * P-CSCF / route-from-registration
 *
 * Derives the forwarding target for an incoming SIP request by looking
 * up the To user's registration in the usrloc registrar. Contacts are
 * returned in priority order and are suitable for parallel forking.
 */

package pcscf

import (
	"fmt"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/usrloc"
)

// defaultDomain is used when a specific domain isn't provided.
const defaultDomain = "ims.local"

// RouteFromRegistration derives forwarding contacts for msg by looking
// up the To user's registration in reg. It returns a list of contact
// URIs sorted in registration priority order (usrloc sorts by q-value
// descending with expired entries filtered).
//
// If no registration is found the returned slice is empty and a
// non-nil error is returned.
func RouteFromRegistration(reg *usrloc.Registrar, msg *parser.SIPMsg) ([]string, error) {
	if reg == nil {
		return nil, fmt.Errorf("nil registrar")
	}
	if msg == nil {
		return nil, fmt.Errorf("nil message")
	}
	user, domain := extractUserAndDomain(msg)
	if user == "" {
		return nil, fmt.Errorf("unable to extract target user from To header")
	}
	if domain == "" {
		domain = defaultDomain
	}

	contacts := reg.Query(domain, user)
	if len(contacts) == 0 {
		return nil, fmt.Errorf("no registration for user %q in domain %q", user, domain)
	}

	out := make([]string, 0, len(contacts))
	for _, c := range contacts {
		if !c.IsExpired() {
			out = append(out, c.URI)
		}
	}
	if len(out) == 0 {
		return nil, fmt.Errorf("no non-expired registration for user %q", user)
	}
	return out, nil
}

// extractUserAndDomain returns the user and domain portions of the To
// header URI. If the URI lacks a domain, domain is returned empty and
// callers can fall back to a default.
func extractUserAndDomain(msg *parser.SIPMsg) (string, string) {
	if msg == nil || msg.To == nil {
		return "", ""
	}
	body := msg.To.Body.String()
	// Strip surrounding "< >" brackets.
	inner := body
	if idx := strings.Index(inner, "<"); idx >= 0 {
		end := strings.Index(inner[idx:], ">")
		if end >= 0 {
			inner = inner[idx+1 : idx+end]
		} else {
			inner = inner[idx+1:]
		}
	}
	// Strip "sip:" / "sips:" scheme.
	schemeIdx := strings.Index(inner, ":")
	rest := inner
	if schemeIdx >= 0 {
		rest = inner[schemeIdx+1:]
	}
	atIdx := strings.Index(rest, "@")
	if atIdx < 0 {
		// No "@" - strip trailing ";params" if any.
		semiIdx := strings.Index(rest, ";")
		if semiIdx >= 0 {
			rest = rest[:semiIdx]
		}
		return strings.TrimSpace(rest), ""
	}
	user := strings.TrimSpace(rest[:atIdx])
	domain := strings.TrimSpace(rest[atIdx+1:])
	// Strip parameters/ports from the domain.
	for i, ch := range domain {
		if ch == ';' || ch == '>' || ch == ' ' {
			domain = domain[:i]
			break
		}
	}
	return user, domain
}

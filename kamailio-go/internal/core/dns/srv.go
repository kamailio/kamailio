// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * DNS SRV resolver - RFC 2782 weighted ordering and host:port to targets.
 *
 * This module extends the base resolver (internal/core/dns/resolver.go)
 * with helpers for:
 *   - Resolving _sip._udp.domain / _sip._tcp.domain style records.
 *   - Sorting the results by priority, and then weighted shuffle within
 *     a priority group (RFC 2782 §3).
 *   - Resolving a SIP destination host[:port] to a list of targets that
 *     can be fed straight into the fork coordinator.
 */

package dns

import (
	"net"
	"sort"
	"strings"
	"time"
)

// SRVTarget is a resolved SRV record ready for forwarding.
type SRVTarget struct {
	Target   string
	Port     int
	Priority uint16
	Weight   uint16
}

// SRVResolver extends the base resolver with SRV support. It can resolve
// `_sip._udp.domain.com` style queries and return a weighted list of
// `SRVTarget` entries sorted by priority/weight (RFC 2782).
type SRVResolver struct {
	Timeout time.Duration
}

// NewSRVResolver constructs a resolver with sensible defaults.
func NewSRVResolver() *SRVResolver {
	return &SRVResolver{Timeout: 3 * time.Second}
}

// ResolveSRV resolves a DNS SRV name like `_sip._udp.example.com` and returns
// the list of SRV targets in the RFC 2782 preferred order: lowest priority
// first, and within a priority the entries are ordered by weight using a
// deterministic weighted selection.
func (r *SRVResolver) ResolveSRV(serviceProtoDomain string) ([]SRVTarget, error) {
	if r == nil {
		r = NewSRVResolver()
	}
	if serviceProtoDomain == "" {
		return nil, nil
	}
	_, addrs, err := net.LookupSRV("", "", serviceProtoDomain)
	if err != nil {
		// Fall through — on error we return empty list + error so callers
		// can try A/AAAA as a fallback.
		return nil, err
	}
	result := make([]SRVTarget, 0, len(addrs))
	for _, a := range addrs {
		host := a.Target
		if strings.HasSuffix(host, ".") {
			host = host[:len(host)-1]
		}
		result = append(result, SRVTarget{
			Target:   host,
			Port:     int(a.Port),
			Priority: a.Priority,
			Weight:   a.Weight,
		})
	}
	return SortByRFC2782(result), nil
}

// ResolveSIPHost resolves a destination "host[:port]" to a list of targets
// suitable for forwarding. If the input is an IP:port, it returns it
// directly; if it's a domain, it first attempts to resolve `_sip._udp.<domain>`
// and `_sip._tcp.<domain>` then falls back to A/AAAA records.
//
// The returned list is already ordered — iterate and try each one.
func (r *SRVResolver) ResolveSIPHost(hostport string) []string {
	if r == nil {
		r = NewSRVResolver()
	}
	if hostport == "" {
		return nil
	}
	// 1. If the input already has an explicit port, return it directly.
	//    e.g. "sip.example.com:5060" → ["sip.example.com:5060"].
	if strings.Count(hostport, ":") >= 1 && !strings.HasPrefix(hostport, "[") {
		// plain host:port
		return []string{hostport}
	}

	// 2. Try SRV for UDP then TCP.
	domain := strings.TrimPrefix(hostport, "sip:")
	domain = strings.TrimPrefix(domain, "sips:")
	if strings.Contains(domain, "@") {
		domain = domain[strings.Index(domain, "@")+1:]
	}
	for _, proto := range []string{"udp", "tcp"} {
		if targets, err := r.ResolveSRV("_sip._" + proto + "." + domain); err == nil && len(targets) > 0 {
			out := make([]string, 0, len(targets))
			for _, t := range targets {
				out = append(out, net.JoinHostPort(t.Target, itoa(t.Port)))
			}
			return out
		}
	}

	// 3. Fall back to plain A/AAAA on default SIP port 5060.
	return []string{domain + ":5060"}
}

// SortByRFC2782 reorders a slice of SRVTargets into the canonical RFC 2782
// priority/weight order. The algorithm:
//   - Order by Priority (ascending — low first).
//   - Within a priority group, use a stable weighted shuffle (pseudo-random
//     but deterministic using the sum of (Target + Port) bytes) so results
//     are reproducible yet well-balanced.
func SortByRFC2782(targets []SRVTarget) []SRVTarget {
	if len(targets) == 0 {
		return nil
	}
	// Group by priority.
	groups := map[uint16][]SRVTarget{}
	var order []uint16
	for _, t := range targets {
		if _, exists := groups[t.Priority]; !exists {
			order = append(order, t.Priority)
		}
		groups[t.Priority] = append(groups[t.Priority], t)
	}
	sort.Slice(order, func(i, j int) bool { return order[i] < order[j] })

	out := make([]SRVTarget, 0, len(targets))
	for _, prio := range order {
		group := groups[prio]
		// Weighted shuffle within group.
		sorted := weightedShuffle(group)
		out = append(out, sorted...)
	}
	return out
}

// weightedShuffle picks entries proportionally to weight using a deterministic
// seed derived from the group's Target+Port bytes. The same input always
// yields the same output order (good for reproducibility / tests).
func weightedShuffle(group []SRVTarget) []SRVTarget {
	if len(group) <= 1 {
		return group
	}
	// seed
	seed := 0
	for _, g := range group {
		for _, c := range g.Target {
			seed += int(c)
		}
		seed += int(g.Port)
	}
	result := make([]SRVTarget, 0, len(group))
	pool := make([]SRVTarget, len(group))
	copy(pool, group)
	for len(pool) > 1 {
		totalW := 0
		for _, p := range pool {
			totalW += int(p.Weight) + 1 // avoid zero-weight div-by-zero
		}
		if totalW == 0 {
			// degenerate — just append remaining in order.
			result = append(result, pool...)
			return result
		}
		// deterministic pseudo-random pick based on seed.
		seed = (seed*1103515245 + 12345) & 0x7fffffff
		r := seed % totalW
		for i, p := range pool {
			w := int(p.Weight) + 1
			if r < w {
				result = append(result, p)
				// remove pool[i]
				pool = append(pool[:i], pool[i+1:]...)
				break
			}
			r -= w
		}
	}
	if len(pool) == 1 {
		result = append(result, pool[0])
	}
	return result
}

// itoa converts int to string without importing strconv (keeps this module
// self-contained for the small port formatting case).
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	buf := make([]byte, 0, 8)
	for n > 0 {
		buf = append([]byte{byte('0' + n%10)}, buf...)
		n /= 10
	}
	if neg {
		buf = append([]byte{'-'}, buf...)
	}
	return string(buf)
}

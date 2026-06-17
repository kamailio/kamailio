// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Router with DNS integration - Phase 6-3
 *
 * Combines the core Router with the DNS resolver so request destinations
 * can be automatically resolved through NAPTR -> SRV -> A record fallbacks.
 */

package router

import (
	"errors"
	"fmt"
	"net"

	"github.com/kamailio/kamailio-go/internal/core/dns"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// RouterWithDNS extends the core Router with next-hop DNS resolution.
type RouterWithDNS struct {
	Router
	resolver *dns.Resolver
}

// NewRouterWithDNS constructs a RouterWithDNS with default DNS resolution.
func NewRouterWithDNS() *RouterWithDNS {
	return &RouterWithDNS{
		Router:   *NewRouter(),
		resolver: dns.NewResolver(),
	}
}

// NewRouterWithDNSWithResolver builds a RouterWithDNS using an existing resolver.
func NewRouterWithDNSWithResolver(r *dns.Resolver) *RouterWithDNS {
	if r == nil {
		r = dns.NewResolver()
	}
	return &RouterWithDNS{
		Router:   *NewRouter(),
		resolver: r,
	}
}

// Resolver exposes the underlying DNS resolver.
func (r *RouterWithDNS) Resolver() *dns.Resolver {
	return r.resolver
}

// ResolveNextHop resolves forwarding destinations for the given message.
// If forceHost is provided, that host/port are used directly and msg.RURI is
// ignored. Otherwise the request-URI of msg is parsed and resolved through
// DNS (NAPTR -> SRV -> A record).
func (r *RouterWithDNS) ResolveNextHop(msg *parser.SIPMsg, forceHost string, forcePort uint16) ([]*ForwardDestination, error) {
	// Override path: use explicit host/port
	if forceHost != "" {
		if net.ParseIP(forceHost) != nil {
			port := int(forcePort)
			if port == 0 {
				port = 5060
			}
			return []*ForwardDestination{{
				URI:         fmt.Sprintf("sip:%s:%d", forceHost, port),
				Host:        forceHost,
				Port:        port,
				Proto:       "udp",
				DNSResolved: true,
				Order:       0,
			}}, nil
		}
		// Hostname: resolve via DNS
		port := forcePort
		if port == 0 {
			port = 5060
		}
		addrs, err := r.resolver.Resolve(forceHost, dns.ProtoUDP, port)
		if err != nil {
			// Best effort: keep hostname as-is for deferred resolution
			return []*ForwardDestination{{
				URI:         fmt.Sprintf("sip:%s:%d", forceHost, port),
				Host:        forceHost,
				Port:        int(port),
				Proto:       "udp",
				DNSResolved: false,
				Order:       0,
			}}, nil
		}
		dests := make([]*ForwardDestination, 0, len(addrs))
		for i, a := range addrs {
			dests = append(dests, &ForwardDestination{
				URI:         fmt.Sprintf("sip:%s:%d", a.IP.String(), a.Port),
				Host:        a.IP.String(),
				Port:        int(a.Port),
				Proto:       a.Proto.String(),
				DNSResolved: true,
				Order:       i,
			})
		}
		return dests, nil
	}

	// Parse R-URI of the message
	if msg == nil || msg.FirstLine == nil {
		return nil, errors.New("nil message or first line")
	}
	uriStr := msg.FirstLine.Req.URI.String()
	if uriStr == "" {
		return nil, errors.New("empty request URI")
	}

	// Try SIP URI DNS resolution first (includes NAPTR/SRV/A)
	if addrs, err := r.resolver.ResolveSIPURI(uriStr); err == nil && len(addrs) > 0 {
		dests := make([]*ForwardDestination, 0, len(addrs))
		for i, a := range addrs {
			dests = append(dests, &ForwardDestination{
				URI:         uriStr,
				Host:        a.IP.String(),
				Port:        int(a.Port),
				Proto:       a.Proto.String(),
				DNSResolved: true,
				Order:       i,
			})
		}
		return dests, nil
	}

	// Fallback: parse URI and use host/port directly
	uri, err := parser.ParseURI(uriStr)
	if err != nil {
		return nil, fmt.Errorf("failed to parse R-URI: %w", err)
	}
	host, port, _ := parser.ExtractHostPortFromURI(uri)
	if host == "" {
		return nil, errors.New("R-URI host empty")
	}
	if net.ParseIP(host) != nil {
		return []*ForwardDestination{{
			URI:         uriStr,
			Host:        host,
			Port:        int(port),
			Proto:       "udp",
			DNSResolved: true,
			Order:       0,
		}}, nil
	}
	// Last resort: direct DNS A lookup
	ips, err := net.LookupIP(host)
	if err == nil && len(ips) > 0 {
		dests := make([]*ForwardDestination, 0, len(ips))
		for i, ip := range ips {
			dests = append(dests, &ForwardDestination{
				URI:         uriStr,
				Host:        ip.String(),
				Port:        int(port),
				Proto:       "udp",
				DNSResolved: true,
				Order:       i,
			})
		}
		return dests, nil
	}
	// Last, last resort: keep host string for deferred resolution
	return []*ForwardDestination{{
		URI:         uriStr,
		Host:        host,
		Port:        int(port),
		Proto:       "udp",
		DNSResolved: false,
		Order:       0,
	}}, nil
}

// PickBestDestination selects the best destination from a list.
// Current policy: pick the one with the lowest Order (first from the DNS
// resolver output, which corresponds to the highest SRV priority / first
// returned A record). Returns nil if dests is empty or nil.
func (r *RouterWithDNS) PickBestDestination(dests []*ForwardDestination) *ForwardDestination {
	if len(dests) == 0 {
		return nil
	}
	best := dests[0]
	for _, d := range dests[1:] {
		if d.Order < best.Order {
			best = d
		}
	}
	return best
}

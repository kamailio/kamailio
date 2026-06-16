// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * DNS resolution - matching C resolve.h / dset.c
 *
 * Provides NAPTR, SRV, and A/AAAA DNS lookups for SIP routing.
 * RFC 3263 - SIP: Locating SIP Servers
 */

package dns

import (
	"context"
	"net"
	"sort"
	"strings"
	"sync"
	"time"
)

// Proto represents transport protocol
type Proto int

const (
	ProtoUDP Proto = iota
	ProtoTCP
	ProtoTLS
	ProtoSCTP
	ProtoWS
	ProtoWSS
)

// ProtoFromString converts string to Proto
func ProtoFromString(s string) Proto {
	switch strings.ToLower(s) {
	case "udp":
		return ProtoUDP
	case "tcp":
		return ProtoTCP
	case "tls":
		return ProtoTLS
	case "sctp":
		return ProtoSCTP
	case "ws":
		return ProtoWS
	case "wss":
		return ProtoWSS
	default:
		return ProtoUDP
	}
}

// String returns protocol string
func (p Proto) String() string {
	switch p {
	case ProtoUDP:
		return "udp"
	case ProtoTCP:
		return "tcp"
	case ProtoTLS:
		return "tls"
	case ProtoSCTP:
		return "sctp"
	case ProtoWS:
		return "ws"
	case ProtoWSS:
		return "wss"
	default:
		return "udp"
	}
}

// Service returns the DNS service name
func (p Proto) Service() string {
	switch p {
	case ProtoUDP:
		return "SIP+D2U"
	case ProtoTCP:
		return "SIP+D2T"
	case ProtoTLS:
		return "SIPS+D2T"
	case ProtoSCTP:
		return "SIP+D2S"
	case ProtoWS:
		return "SIP+D2W"
	case ProtoWSS:
		return "SIPS+D2W"
	default:
		return "SIP+D2U"
	}
}

// NAPTRRecord represents a NAPTR DNS record
// RFC 3403
type NAPTRRecord struct {
	Order       uint16
	Preference  uint16
	Flags       string
	Service     string
	Regexp      string
	Replacement string
}

// SRVRecord represents an SRV DNS record
// RFC 2782
type SRVRecord struct {
	Priority uint16
	Weight   uint16
	Port     uint16
	Target   string
}

// Addr represents a resolved address
type Addr struct {
	IP   net.IP
	Port uint16
	Proto Proto
}

// String returns address string
func (a *Addr) String() string {
	if a.Port == 0 {
		return a.IP.String()
	}
	return net.JoinHostPort(a.IP.String(), string(rune(a.Port)))
}

// Resolver provides DNS resolution with caching
type Resolver struct {
	resolver *net.Resolver
	cache    *Cache
	timeout  time.Duration
	mu       sync.RWMutex
}

// NewResolver creates a new DNS resolver
func NewResolver() *Resolver {
	return &Resolver{
		resolver: net.DefaultResolver,
		cache:    NewCache(5 * time.Minute),
		timeout:  5 * time.Second,
	}
}

// SetTimeout sets the DNS query timeout
func (r *Resolver) SetTimeout(d time.Duration) {
	r.mu.Lock()
	r.timeout = d
	r.mu.Unlock()
}

// SetCache sets the DNS cache
func (r *Resolver) SetCache(cache *Cache) {
	r.mu.Lock()
	r.cache = cache
	r.mu.Unlock()
}

// Resolve resolves a SIP domain to addresses
// RFC 3263 Section 4
func (r *Resolver) Resolve(domain string, proto Proto, port uint16) ([]*Addr, error) {
	// Check if domain is already an IP address
	if ip := net.ParseIP(domain); ip != nil {
		return []*Addr{{
			IP:    ip,
			Port:  port,
			Proto: proto,
		}}, nil
	}

	// Check cache first
	cacheKey := domain + ":" + proto.String()
	if r.cache != nil {
		if addrs := r.cache.Get(cacheKey); len(addrs) > 0 {
			return addrs, nil
		}
	}

	// Try NAPTR lookup first
	addrs, err := r.resolveNAPTR(domain, proto)
	if err == nil && len(addrs) > 0 {
		if r.cache != nil {
			r.cache.Set(cacheKey, addrs)
		}
		return addrs, nil
	}

	// Fall back to SRV lookup
	srvName := "_sip._" + proto.String() + "." + domain
	addrs, err = r.resolveSRV(srvName, proto)
	if err == nil && len(addrs) > 0 {
		if r.cache != nil {
			r.cache.Set(cacheKey, addrs)
		}
		return addrs, nil
	}

	// Fall back to A/AAAA lookup
	return r.resolveA(domain, proto, port)
}

// resolveNAPTR performs NAPTR lookup
func (r *Resolver) resolveNAPTR(domain string, proto Proto) ([]*Addr, error) {
	_ = context.Background() // For future NAPTR implementation

	// Look up NAPTR records
	// Note: Go's net.Resolver doesn't support NAPTR directly
	// We'll use a simplified approach here
	// In production, you'd use a DNS library like miekg/dns

	// For now, return nil to fall back to SRV
	return nil, ErrNoNAPTR
}

// resolveSRV performs SRV lookup
func (r *Resolver) resolveSRV(srvName string, proto Proto) ([]*Addr, error) {
	ctx, cancel := context.WithTimeout(context.Background(), r.timeout)
	defer cancel()

	// Look up SRV records
	_, addrs, err := r.resolver.LookupSRV(ctx, "", "", srvName)
	if err != nil {
		return nil, err
	}

	var results []*Addr

	for _, srv := range addrs {
		// Resolve target to IP addresses
		ips, err := r.resolveHost(srv.Target)
		if err != nil {
			continue
		}

		for _, ip := range ips {
			results = append(results, &Addr{
				IP:    ip,
				Port:  srv.Port,
				Proto: proto,
			})
		}
	}

	// Sort by priority and weight
	sort.Slice(results, func(i, j int) bool {
		// SRV records are already sorted by priority/weight
		return false
	})

	return results, nil
}

// resolveA performs A/AAAA lookup
func (r *Resolver) resolveA(domain string, proto Proto, port uint16) ([]*Addr, error) {
	ips, err := r.resolveHost(domain)
	if err != nil {
		return nil, err
	}

	var addrs []*Addr
	for _, ip := range ips {
		addrs = append(addrs, &Addr{
			IP:    ip,
			Port:  port,
			Proto: proto,
		})
	}

	return addrs, nil
}

// resolveHost resolves hostname to IP addresses
func (r *Resolver) resolveHost(host string) ([]net.IP, error) {
	ctx, cancel := context.WithTimeout(context.Background(), r.timeout)
	defer cancel()

	// Try IPv6 first
	ips, err := r.resolver.LookupIPAddr(ctx, host)
	if err != nil {
		return nil, err
	}

	var results []net.IP
	for _, ip := range ips {
		results = append(results, ip.IP)
	}

	return results, nil
}

// LookupIP looks up IP addresses for a hostname
func (r *Resolver) LookupIP(host string) ([]net.IP, error) {
	return r.resolveHost(host)
}

// LookupSRV looks up SRV records
func (r *Resolver) LookupSRV(service, proto, domain string) ([]*SRVRecord, error) {
	srvName := "_" + service + "._" + proto + "." + domain

	ctx, cancel := context.WithTimeout(context.Background(), r.timeout)
	defer cancel()

	_, addrs, err := r.resolver.LookupSRV(ctx, "", "", srvName)
	if err != nil {
		return nil, err
	}

	var records []*SRVRecord
	for _, srv := range addrs {
		records = append(records, &SRVRecord{
			Priority: srv.Priority,
			Weight:   srv.Weight,
			Port:     srv.Port,
			Target:   srv.Target,
		})
	}

	return records, nil
}

// ResolveSIPURI resolves a SIP URI to addresses
// RFC 3263
func (r *Resolver) ResolveSIPURI(uri string) ([]*Addr, error) {
	// Parse URI to extract host, port, and transport
	// This is a simplified implementation

	// Remove sip: prefix
	if strings.HasPrefix(strings.ToLower(uri), "sips:") {
		// SIPS URI - use TLS
		uri = uri[5:]
		return r.Resolve(extractHost(uri), ProtoTLS, 5061)
	} else if strings.HasPrefix(strings.ToLower(uri), "sip:") {
		uri = uri[4:]
	}

	// Extract host and port
	host, port := extractHostPort(uri)
	if port == 0 {
		port = 5060
	}

	return r.Resolve(host, ProtoUDP, port)
}

// extractHost extracts host from URI
func extractHost(uri string) string {
	// Remove user part if present
	if at := strings.Index(uri, "@"); at != -1 {
		uri = uri[at+1:]
	}

	// Remove parameters
	if semi := strings.Index(uri, ";"); semi != -1 {
		uri = uri[:semi]
	}

	// Remove headers
	if quest := strings.Index(uri, "?"); quest != -1 {
		uri = uri[:quest]
	}

	// Extract host:port
	host, _ := extractHostPort(uri)
	return host
}

// extractHostPort extracts host and port from host:port string
func extractHostPort(s string) (string, uint16) {
	// Handle IPv6 addresses
	if strings.HasPrefix(s, "[") {
		end := strings.Index(s, "]")
		if end == -1 {
			return s, 0
		}
		host := s[1:end]
		if end+1 < len(s) && s[end+1] == ':' {
			port := parsePort(s[end+2:])
			return host, port
		}
		return host, 0
	}

	// Handle host:port
	if colon := strings.LastIndex(s, ":"); colon != -1 {
		host := s[:colon]
		port := parsePort(s[colon+1:])
		return host, port
	}

	return s, 0
}

// parsePort parses port number
func parsePort(s string) uint16 {
	var port uint16
	for _, c := range s {
		if c >= '0' && c <= '9' {
			port = port*10 + uint16(c-'0')
		} else {
			break
		}
	}
	return port
}

// Errors
var (
	ErrNoNAPTR = &DNSError{Msg: "no NAPTR records found"}
	ErrNoSRV   = &DNSError{Msg: "no SRV records found"}
	ErrNoAddr  = &DNSError{Msg: "no address records found"}
)

// DNSError represents a DNS error
type DNSError struct {
	Msg string
}

func (e *DNSError) Error() string {
	return e.Msg
}

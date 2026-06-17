// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Request-URI routing operations (matching C route.c core utilities
 * like rewrite_uri, rewrite_host, rewrite_port, t_relay_to_udp etc.)
 *
 * Provides helpers for:
 *   - Parsing and modifying the R-URI (Request URI) of a SIP message
 *   - Building forwarding destinations from R-URI or explicit overrides
 *   - Stripping / appending / rewriting URI components
 *   - Simple load-balancing across static gateways
 */

package router

import (
	"context"
	"errors"
	"fmt"
	"net"
	"strconv"
	"strings"
	"sync/atomic"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// DefaultSessionTimers defaults for RFC 4028 (Session-Expires / Min-SE)
//
//	C: default values in Kamailio's cfg
const (
	DefaultSessionExpires = 1800 // seconds
	DefaultMinSE          = 90   // RFC 4028 minimum
)

// RewriteMode controls how R-URI modifications affect message rebuild
type RewriteMode int

const (
	RewriteInPlace     RewriteMode = iota // modify msg.FirstLine only
	RewriteWithRaw                         // also update msg.Raw (best effort)
)

// ForwardDestination captures a computed outbound destination
//
//	C: struct proxy_l (partial)
type ForwardDestination struct {
	URI         string  // the full SIP URI used for forwarding
	Host        string  // resolved host (or IP)
	Port        int     // resolved port (or default)
	Proto       string  // transport: "udp", "tcp", "tls", "ws", "wss"
	DNSResolved bool    // whether host was resolved from DNS
	Order       int     // index when multiple destinations are returned
}

// Gateway represents a static forwarding target for load balancing
type Gateway struct {
	Address    string  // "host:port"
	Proto      string  // transport
	Weight     int     // for weighted round-robin
	Active     bool    // administratively up/down
}

// GatewayGroup manages a group of gateways with simple round-robin
type GatewayGroup struct {
	Name     string
	Gateways []*Gateway
	// Current round-robin index (atomic)
	rrIndex uint64
}

// NewGatewayGroup creates a new gateway group
func NewGatewayGroup(name string, gateways ...*Gateway) *GatewayGroup {
	return &GatewayGroup{
		Name:     name,
		Gateways: gateways,
	}
}

// PickNext returns the next gateway using simple round-robin, skipping inactive ones.
//
//	C: loosely maps to `dispatcher` module's next ds
func (g *GatewayGroup) PickNext() *Gateway {
	if len(g.Gateways) == 0 {
		return nil
	}
	// Advance atomically; retry up to N to find an active one
	n := uint64(len(g.Gateways))
	for i := uint64(0); i < n; i++ {
		idx := atomic.AddUint64(&g.rrIndex, 1) % n
		if gw := g.Gateways[idx]; gw != nil && gw.Active {
			return gw
		}
	}
	return nil
}

// PickAllActive returns all currently-active gateways (for parallel forking)
func (g *GatewayGroup) PickAllActive() []*Gateway {
	active := make([]*Gateway, 0, len(g.Gateways))
	for _, gw := range g.Gateways {
		if gw != nil && gw.Active {
			active = append(active, gw)
		}
	}
	return active
}

// -------------------------------------------------------------
// R-URI parsing helpers
// -------------------------------------------------------------

// ParseRURI returns the parsed R-URI of a message
func ParseRURI(msg *parser.SIPMsg) (*parser.SIPURI, error) {
	if msg == nil || msg.FirstLine == nil {
		return nil, errors.New("nil message or first line")
	}
	ruri := msg.FirstLine.Req.URI.String()
	if ruri == "" {
		return nil, errors.New("empty R-URI")
	}
	return parser.ParseURI(ruri)
}

// RewriteRURI replaces the whole R-URI with `newRURI`
//
//	C: int rewrite_uri(struct sip_msg *msg, char *uri)
func RewriteRURI(msg *parser.SIPMsg, newRURI string) error {
	if msg == nil || msg.FirstLine == nil {
		return errors.New("nil message or first line")
	}
	if newRURI == "" {
		return errors.New("empty new R-URI")
	}
	// Validate
	if _, err := parser.ParseURI(newRURI); err != nil {
		return fmt.Errorf("invalid R-URI: %w", err)
	}
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return nil
}

// RewriteHost rewrites just the host portion of the R-URI
//
//	C: int rewrite_host(struct sip_msg *msg, char *host)
func RewriteHost(msg *parser.SIPMsg, newHost string) error {
	if msg == nil || msg.FirstLine == nil {
		return errors.New("nil message or first line")
	}
	if newHost == "" {
		return errors.New("empty host")
	}
	uri, err := ParseRURI(msg)
	if err != nil {
		return err
	}
	// Rebuild using the parsed URI, replacing host
	newRURI := rebuildURI(uri, uri.User.String(), newHost, portStringFor(uri))
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return nil
}

// RewritePort rewrites just the port portion of the R-URI
//
//	C: int rewrite_port(struct sip_msg *msg, char *port)
func RewritePort(msg *parser.SIPMsg, newPort int) error {
	if msg == nil || msg.FirstLine == nil {
		return errors.New("nil message or first line")
	}
	if newPort <= 0 || newPort > 65535 {
		return errors.New("invalid port")
	}
	uri, err := ParseRURI(msg)
	if err != nil {
		return err
	}
	newRURI := rebuildURI(uri, uri.User.String(), uri.Host.String(), strconv.Itoa(newPort))
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return nil
}

// RewriteUser rewrites just the user portion of the R-URI
//
//	C: int rewrite_user(struct sip_msg *msg, char *user)
func RewriteUser(msg *parser.SIPMsg, newUser string) error {
	if msg == nil || msg.FirstLine == nil {
		return errors.New("nil message or first line")
	}
	uri, err := ParseRURI(msg)
	if err != nil {
		return err
	}
	newRURI := rebuildURI(uri, newUser, uri.Host.String(), portStringFor(uri))
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return nil
}

// Strip strips `n` leading characters from the R-URI user
//
//	C: int strip(struct sip_msg *msg, int n)
func Strip(msg *parser.SIPMsg, n int) error {
	if msg == nil || msg.FirstLine == nil {
		return errors.New("nil message or first line")
	}
	uri, err := ParseRURI(msg)
	if err != nil {
		return err
	}
	user := uri.User.String()
	if n < 0 {
		return errors.New("negative strip")
	}
	if n >= len(user) {
		user = ""
	} else {
		user = user[n:]
	}
	newRURI := rebuildURI(uri, user, uri.Host.String(), portStringFor(uri))
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return nil
}

// StripPrefix strips the exact `prefix` if the user matches; returns true if stripped
func StripPrefix(msg *parser.SIPMsg, prefix string) (bool, error) {
	if msg == nil || msg.FirstLine == nil {
		return false, errors.New("nil message or first line")
	}
	uri, err := ParseRURI(msg)
	if err != nil {
		return false, err
	}
	user := uri.User.String()
	if !strings.HasPrefix(user, prefix) {
		return false, nil
	}
	newRURI := rebuildURI(uri, user[len(prefix):], uri.Host.String(), portStringFor(uri))
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return true, nil
}

// AppendPrefix prepends `prefix` to the R-URI user
func AppendPrefix(msg *parser.SIPMsg, prefix string) error {
	if msg == nil || msg.FirstLine == nil {
		return errors.New("nil message or first line")
	}
	uri, err := ParseRURI(msg)
	if err != nil {
		return err
	}
	newRURI := rebuildURI(uri, prefix+uri.User.String(), uri.Host.String(), portStringFor(uri))
	msg.FirstLine.Req.URI = str.Mk(newRURI)
	return nil
}

// IsLocalDomain checks whether the R-URI host matches one of our local domains
//
//	C: int is_myself(struct sip_msg *msg, char *host) (roughly)
func IsLocalDomain(msg *parser.SIPMsg, localDomains []string) bool {
	uri, err := ParseRURI(msg)
	if err != nil {
		return false
	}
	host := strings.ToLower(uri.Host.String())
	for _, d := range localDomains {
		if host == strings.ToLower(d) {
			return true
		}
	}
	return false
}

// -------------------------------------------------------------
// Forward destination computation
// -------------------------------------------------------------

// ComputeForwardDestinations resolves the R-URI (or explicit override)
// into an ordered list of ForwardDestination.
//
//	C: similar in spirit to `getbypath` / `proxy2str`
func ComputeForwardDestinations(msg *parser.SIPMsg, overrideURI string) ([]*ForwardDestination, error) {
	var targetURI string
	if overrideURI != "" {
		targetURI = overrideURI
	} else {
		if msg == nil || msg.FirstLine == nil {
			return nil, errors.New("nil message")
		}
		targetURI = msg.FirstLine.Req.URI.String()
	}
	if targetURI == "" {
		return nil, errors.New("no target URI")
	}

	uri, err := parser.ParseURI(targetURI)
	if err != nil {
		return nil, err
	}

	host := uri.Host.String()
	port := int(uri.PortNo)
	if port == 0 {
		switch strings.ToLower(uri.TransportVal.String()) {
		case "tls":
			port = 5061
		case "tcp":
			port = 5060
		default:
			// RFC 3261 default UDP / generic
			port = 5060
		}
	}
	proto := strings.ToLower(uri.TransportVal.String())
	if proto == "" {
		proto = "udp"
	}

	// If host is a domain, try to resolve (best-effort; on failure, keep host)
	var resolvedHost = host
	dnsOK := false
	if ip := net.ParseIP(host); ip == nil {
		if ips, err := net.LookupIP(host); err == nil && len(ips) > 0 {
			dests := make([]*ForwardDestination, 0, len(ips))
			for i, ip := range ips {
				dests = append(dests, &ForwardDestination{
					URI:         targetURI,
					Host:        ip.String(),
					Port:        port,
					Proto:       proto,
					DNSResolved: true,
					Order:       i,
				})
			}
			return dests, nil
		}
	} else {
		dnsOK = true
		resolvedHost = ip.String()
	}
	return []*ForwardDestination{{
		URI:         targetURI,
		Host:        resolvedHost,
		Port:        port,
		Proto:       proto,
		DNSResolved: dnsOK,
		Order:       0,
	}}, nil
}

// ForwardViaGateway returns a destination computed from a static gateway
// (host:port/proto), keeping the original R-URI as the target URI
// (for request routing via a proxy).
func ForwardViaGateway(msg *parser.SIPMsg, gw *Gateway) (*ForwardDestination, error) {
	if gw == nil {
		return nil, errors.New("nil gateway")
	}
	parts := strings.SplitN(gw.Address, ":", 2)
	host := parts[0]
	port := 5060
	if len(parts) == 2 {
		if p, err := strconv.Atoi(parts[1]); err == nil {
			port = p
		}
	}
	proto := strings.ToLower(gw.Proto)
	if proto == "" {
		proto = "udp"
	}

	origURI := ""
	if msg != nil && msg.FirstLine != nil {
		origURI = msg.FirstLine.Req.URI.String()
	}

	return &ForwardDestination{
		URI:         origURI,
		Host:        host,
		Port:        port,
		Proto:       proto,
		DNSResolved: net.ParseIP(host) != nil,
		Order:       0,
	}, nil
}

// String formats a destination for logging/debugging
func (d *ForwardDestination) String() string {
	if d == nil {
		return "<nil>"
	}
	return fmt.Sprintf("%s:%d/%s", d.Host, d.Port, d.Proto)
}

// -------------------------------------------------------------
// Session timers helpers on SIPMsg
// -------------------------------------------------------------

// ApplySessionTimers ensures Session-Expires: and Min-SE: are present on
// messages that need them (INVITE, re-INVITE, UPDATE). Returns a list of
// header mutations (for diagnostics).
//
//	C: loosely matches `nathelper`/`nath` logic for session-timer insertion
func ApplySessionTimers(msg *parser.SIPMsg, sessionExpires, minSE int) ([]string, error) {
	if msg == nil {
		return nil, errors.New("nil message")
	}
	if sessionExpires <= 0 {
		sessionExpires = DefaultSessionExpires
	}
	if minSE <= 0 {
		minSE = DefaultMinSE
	}
	if sessionExpires < minSE {
		return nil, fmt.Errorf("session-expires %d < min-se %d", sessionExpires, minSE)
	}

	changes := make([]string, 0, 2)

	// Scan existing headers (using the parser header list)
	hasSE := false
	hasMin := false
	for _, hdr := range msg.Headers {
		if hdr.Type == parser.HdrSessionExpires {
			hasSE = true
		}
		if hdr.Type == parser.HdrMinSE {
			hasMin = true
		}
	}

	if !hasSE {
		changes = append(changes, "added Session-Expires: "+parser.BuildSessionExpires(sessionExpires, parser.RefresherUnknown))
	}
	if !hasMin {
		changes = append(changes, "added Min-SE: "+parser.BuildMinSE(minSE))
	}

	return changes, nil
}

// -------------------------------------------------------------
// Router integration: expose R-URI actions as a RouteBlock helper
// -------------------------------------------------------------

// RURIAction represents a R-URI modification step for high-level routing plans
type RURIAction struct {
	Type  RURIActionType
	Param string
	Param2 string
	ParamInt int
}

// RURIActionType enumerates R-URI modification types
type RURIActionType int

const (
	RURIActionNone RURIActionType = iota
	RURIActionRewriteURI
	RURIActionRewriteHost
	RURIActionRewritePort
	RURIActionRewriteUser
	RURIActionStrip
	RURIActionStripPrefix
	RURIActionAppendPrefix
	RURIActionLog
)

// ExecuteRURIActions applies a list of RURIAction steps to a message
func ExecuteRURIActions(ctx context.Context, msg *parser.SIPMsg, actions []RURIAction) ([]string, error) {
	log := make([]string, 0, len(actions))
	for _, a := range actions {
		switch a.Type {
		case RURIActionRewriteURI:
			if err := RewriteRURI(msg, a.Param); err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("rewrite_ruri(%s)", a.Param))
		case RURIActionRewriteHost:
			if err := RewriteHost(msg, a.Param); err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("rewrite_host(%s)", a.Param))
		case RURIActionRewritePort:
			if err := RewritePort(msg, a.ParamInt); err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("rewrite_port(%d)", a.ParamInt))
		case RURIActionRewriteUser:
			if err := RewriteUser(msg, a.Param); err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("rewrite_user(%s)", a.Param))
		case RURIActionStrip:
			if err := Strip(msg, a.ParamInt); err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("strip(%d)", a.ParamInt))
		case RURIActionStripPrefix:
			stripped, err := StripPrefix(msg, a.Param)
			if err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("strip_prefix(%s)=%v", a.Param, stripped))
		case RURIActionAppendPrefix:
			if err := AppendPrefix(msg, a.Param); err != nil {
				return log, err
			}
			log = append(log, fmt.Sprintf("append_prefix(%s)", a.Param))
		case RURIActionLog:
			log = append(log, fmt.Sprintf("log: %s", a.Param))
		}
	}
	return log, nil
}

// -------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------

func schemeFor(uri *parser.SIPURI) string {
	if uri == nil {
		return "sip"
	}
	switch uri.Type {
	case parser.SIPSURIT:
		return "sips"
	case parser.TELURIT:
		return "tel"
	default:
		return "sip"
	}
}

func portStringFor(uri *parser.SIPURI) string {
	if uri == nil {
		return ""
	}
	if uri.PortNo == 0 {
		return uri.Port.String()
	}
	return strconv.Itoa(int(uri.PortNo))
}

// rebuildURI reconstructs a SIP URI string replacing user/host/port.
// Preserves userinfo, uses host:port and scheme from original uri.
func rebuildURI(uri *parser.SIPURI, user, host, port string) string {
	scheme := schemeFor(uri)
	var b strings.Builder
	b.WriteString(scheme)
	b.WriteByte(':')
	if user != "" {
		b.WriteString(user)
		if uri.Passwd.Len > 0 {
			b.WriteByte(':')
			b.WriteString(uri.Passwd.String())
		}
		b.WriteByte('@')
	}
	b.WriteString(host)
	if port != "" {
		b.WriteByte(':')
		b.WriteString(port)
	}
	return b.String()
}

// ParseForwardAddress parses "host:port" / "host:port/proto" strings
func ParseForwardAddress(addr string) (*ForwardDestination, error) {
	if addr == "" {
		return nil, errors.New("empty address")
	}
	proto := "udp"
	if idx := strings.LastIndex(addr, "/"); idx != -1 {
		proto = strings.ToLower(addr[idx+1:])
		addr = addr[:idx]
	}
	host := addr
	port := 5060
	if idx := strings.LastIndex(addr, ":"); idx != -1 {
		// handle IPv6 literals like [::1]:5060
		if strings.HasPrefix(addr, "[") && strings.Contains(addr, "]") {
			if end := strings.Index(addr, "]"); end != -1 && end+1 < len(addr) && addr[end+1] == ':' {
				host = addr[:end+1]
				if p, err := strconv.Atoi(addr[end+2:]); err == nil {
					port = p
				}
			} else {
				host = addr[:idx]
				if p, err := strconv.Atoi(addr[idx+1:]); err == nil {
					port = p
				}
			}
		} else {
			host = addr[:idx]
			if p, err := strconv.Atoi(addr[idx+1:]); err == nil {
				port = p
			}
		}
	}
	return &ForwardDestination{
		Host: host,
		Port: port,
		Proto: proto,
	}, nil
}

// String renders a gateway
func (g *Gateway) String() string {
	if g == nil {
		return "<nil>"
	}
	return fmt.Sprintf("%s/%s (weight=%d active=%v)", g.Address, g.Proto, g.Weight, g.Active)
}

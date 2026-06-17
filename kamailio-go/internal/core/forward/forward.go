// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Stateless message forwarding - matching C forward.c
 *
 * Phase 6-3: integrated with parser.BuildMessage / BuildForwardRequest,
 * transport.UDPListener, and DNS-based next-hop resolution.
 */

package forward

import (
	"errors"
	"fmt"
	"net"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// Forwarder handles message forwarding
type Forwarder struct {
	udpListeners map[string]*transport.UDPListener
	tcpListeners map[string]*transport.TCPListener
	// sendSocket is the default outgoing UDP socket address (host:port).
	// When set, it lets forwarded requests appear from a well-known
	// proxy address in their Via header.
	sendHost string
	sendPort int
}

// NewForwarder creates a new forwarder
func NewForwarder() *Forwarder {
	return &Forwarder{
		udpListeners: make(map[string]*transport.UDPListener),
		tcpListeners: make(map[string]*transport.TCPListener),
	}
}

// SetSendAddress configures the default "from" address for forwarded requests.
func (f *Forwarder) SetSendAddress(host string, port int) {
	f.sendHost = host
	f.sendPort = port
}

// RegisterUDPListener registers a UDP listener for forwarding
func (f *Forwarder) RegisterUDPListener(si *transport.SocketInfo, listener *transport.UDPListener) {
	f.udpListeners[si.String()] = listener
}

// RegisterTCPListener registers a TCP listener for forwarding
func (f *Forwarder) RegisterTCPListener(si *transport.SocketInfo, listener *transport.TCPListener) {
	f.tcpListeners[si.String()] = listener
}

// ---------------------------------------------------------------------------
// Request forwarding - new Phase 6-3 signature
// ---------------------------------------------------------------------------

// ForwardRequest builds a forwarded copy of the request, decrements Max-Forwards,
// prepends a new Via header, and sends it to the next hop resolved from
// nextHopURI using UDP. srcSocket is used to determine the local Via address.
func (f *Forwarder) ForwardRequest(msg *parser.SIPMsg, nextHopURI string, srcSocket *transport.SocketInfo) error {
	if msg == nil {
		return errors.New("nil message")
	}

	// Resolve the next-hop destination
	dstHost, dstPort, err := f.resolveNextHop(nextHopURI)
	if err != nil {
		return fmt.Errorf("failed to resolve next hop %q: %w", nextHopURI, err)
	}

	// Determine proxy host/port for Via header
	proxyHost := f.sendHost
	proxyPort := f.sendPort
	if srcSocket != nil {
		if proxyHost == "" {
			proxyHost = srcSocket.Address.String()
		}
		if proxyPort == 0 {
			proxyPort = int(srcSocket.Port)
		}
	}
	if proxyHost == "" {
		proxyHost = "127.0.0.1"
	}
	if proxyPort == 0 {
		proxyPort = 5060
	}

	// Build the forwarded request
	fwd, err := parser.BuildForwardRequest(msg, "udp", proxyHost, proxyPort, nextHopURI)
	if err != nil {
		return fmt.Errorf("failed to build forwarded request: %w", err)
	}

	// Serialize to raw bytes using BuildMessage
	raw, err := parser.BuildMessage(fwd)
	if err != nil {
		return fmt.Errorf("failed to serialize forwarded request: %w", err)
	}

	// Dispatch via UDP
	return f.SendToUDP(dstHost, uint16(dstPort), raw)
}

// ForwardReply sends a reply upstream based on the topmost Via header in
// the reply message. It extracts host:port from Via (transport/UDP by default),
// serializes the message via BuildMessage, and forwards the bytes over UDP.
//
// If `via` is non-nil, it is used as the destination hint; otherwise the
// topmost Via header from msg.Headers is used.
func (f *Forwarder) ForwardReply(reply *parser.SIPMsg, via *parser.HdrField, srcSocket *transport.SocketInfo) error {
	if reply == nil {
		return errors.New("nil reply")
	}

	// Resolve destination from the topmost Via header
	host, port, err := extractViaDestination(reply, via)
	if err != nil {
		return fmt.Errorf("failed to parse Via for reply forwarding: %w", err)
	}

	// Serialize reply
	raw, err := parser.BuildMessage(reply)
	if err != nil {
		return fmt.Errorf("failed to serialize reply: %w", err)
	}

	return f.SendToUDP(host, port, raw)
}

// SendToUDP sends raw bytes to host:port over UDP using a registered UDP
// listener (or the first available listener).
func (f *Forwarder) SendToUDP(dstHost string, dstPort uint16, data []byte) error {
	if dstHost == "" {
		return errors.New("empty destination host")
	}
	if dstPort == 0 {
		dstPort = 5060
	}
	if len(data) == 0 {
		return errors.New("empty payload")
	}

	// Resolve destination UDP address
	udpAddr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("%s:%d", dstHost, dstPort))
	if err != nil {
		return fmt.Errorf("failed to resolve UDP address: %w", err)
	}

	// Select a sending UDP listener
	listener := f.selectUDPListener()
	if listener == nil {
		// Fallback: create an ephemeral UDP sender if no listener is registered.
		sender, err := transport.NewUDPSender()
		if err != nil {
			return fmt.Errorf("failed to create UDP sender: %w", err)
		}
		defer sender.Close()
		return sender.Send(udpAddr, data)
	}
	return listener.Send(udpAddr, data)
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// resolveNextHop parses a next hop string (either a SIP URI or host:port) and
// returns a reachable host:port. Supports:
//   - "sip:user@domain.com" -> DNS resolution
//   - "sip:user@host:port"
//   - "host:port"
func (f *Forwarder) resolveNextHop(uri string) (host string, port int, err error) {
	if uri == "" {
		return "", 0, errors.New("empty next hop URI")
	}

	// Check if it contains a SIP scheme
	lower := strings.ToLower(uri)
	if strings.HasPrefix(lower, "sip:") || strings.HasPrefix(lower, "sips:") {
		parsed, perr := parser.ParseURI(uri)
		if perr == nil {
			h, p, _ := parser.ExtractHostPortFromURI(parsed)
			if h == "" {
				return "", 0, errors.New("failed to extract host from next hop URI")
			}
			// If host is already an IP, use it directly
			if net.ParseIP(h) != nil {
				return h, int(p), nil
			}
			// Otherwise try DNS resolution
			ips, rerr := net.LookupIP(h)
			if rerr == nil && len(ips) > 0 {
				return ips[0].String(), int(p), nil
			}
			// Fall back to the hostname for defer resolution by net.ResolveUDPAddr
			return h, int(p), nil
		}
	}

	// Plain host:port or just host
	host, port = parseHostPort(uri)
	if host == "" {
		return "", 0, fmt.Errorf("invalid next hop: %q", uri)
	}
	return host, port, nil
}

// parseHostPort parses "host" or "host:port" or "[ipv6]:port"
func parseHostPort(s string) (string, int) {
	s = strings.TrimSpace(s)
	if strings.HasPrefix(s, "[") {
		end := strings.Index(s, "]")
		if end == -1 {
			return s, 5060
		}
		host := s[1:end]
		rest := s[end+1:]
		if strings.HasPrefix(rest, ":") {
			if p, err := strconv.Atoi(rest[1:]); err == nil {
				return host, p
			}
		}
		return host, 5060
	}
	if colon := strings.LastIndex(s, ":"); colon != -1 {
		host := s[:colon]
		portStr := s[colon+1:]
		if p, err := strconv.Atoi(portStr); err == nil {
			return host, p
		}
	}
	return s, 5060
}

// selectUDPListener returns a registered UDP listener, or nil if none registered.
func (f *Forwarder) selectUDPListener() *transport.UDPListener {
	for _, l := range f.udpListeners {
		return l
	}
	return nil
}

// getSendSocket selects the appropriate sending socket (kept for backward compatibility)
func (f *Forwarder) getSendSocket(proto transport.Protocol, dst net.Addr) *transport.SocketInfo {
	switch proto {
	case transport.ProtoUDP:
		for _, listener := range f.udpListeners {
			if listener.LocalAddr() != nil {
				ua, ok := listener.LocalAddr().(*net.UDPAddr)
				if ok {
					return &transport.SocketInfo{
						Address:  ua.IP,
						Port:     uint16(ua.Port),
						Protocol: proto,
					}
				}
			}
		}
	case transport.ProtoTCP:
		for _, listener := range f.tcpListeners {
			if listener.LocalAddr() != nil {
				ta, ok := listener.LocalAddr().(*net.TCPAddr)
				if ok {
					return &transport.SocketInfo{
						Address:  ta.IP,
						Port:     uint16(ta.Port),
						Protocol: proto,
					}
				}
			}
		}
	}
	return nil
}

// extractViaDestination returns host:port from the topmost Via header.
// Via format: "SIP/2.0/UDP host:port;branch=z9hG4bK...;received=...;rport=..."
//
// Priority:
//  1. received= parameter (public-reflexive address) overrides host
//  2. rport= parameter overrides port
//  3. fall back to host:port in the header body
func extractViaDestination(msg *parser.SIPMsg, via *parser.HdrField) (host string, port uint16, err error) {
	if via == nil && msg != nil {
		// Use the first Via header from the parsed message
		for _, h := range msg.Headers {
			if strings.ToLower(h.Name.String()) == "via" {
				via = h
				break
			}
		}
		if via == nil && msg.Via1 != nil {
			// Fall back to Via1 parsed field
			return parseViaBody(msg.Via1)
		}
	}
	if via == nil {
		return "", 0, errors.New("no Via header present")
	}
	return parseViaField(via)
}

// parseViaField parses a Via HdrField body, handling received= / rport= overrides.
func parseViaField(h *parser.HdrField) (host string, port uint16, err error) {
	body := h.Body.String()
	if body == "" {
		return "", 0, errors.New("empty Via body")
	}
	// Remove leading transport prefix, e.g. "SIP/2.0/UDP "
	slashIdx := strings.Index(body, " ")
	afterProto := body
	if slashIdx != -1 {
		afterProto = strings.TrimSpace(body[slashIdx:])
	}
	// Separate parameters from sent-by
	parts := strings.SplitN(afterProto, ";", 2)
	sentBy := strings.TrimSpace(parts[0])
	// Parse sent-by as host[:port]
	host, port = extractSentBy(sentBy)
	// Apply received= and rport= parameter overrides
	if len(parts) > 1 {
		params := parts[1]
		for _, p := range strings.Split(params, ";") {
			kv := strings.SplitN(strings.TrimSpace(p), "=", 2)
			if len(kv) == 2 {
				k := strings.ToLower(kv[0])
				v := strings.TrimSpace(kv[1])
				switch k {
				case "received":
					if v != "" {
						host = v
					}
				case "rport":
					if v != "" {
						if n, perr := strconv.Atoi(v); perr == nil {
							port = uint16(n)
						}
					}
				}
			}
		}
	}
	if host == "" {
		return "", 0, errors.New("Via host empty")
	}
	if port == 0 {
		port = 5060
	}
	return host, port, nil
}

// parseViaBody extracts destination from a parsed *parser.ViaBody structure.
func parseViaBody(v *parser.ViaBody) (host string, port uint16, err error) {
	host = strings.TrimSpace(v.Host.String())
	port = v.Port
	// Look at parameters for received= / rport=
	params := strings.TrimSpace(v.Params.String())
	if params != "" {
		for _, p := range strings.Split(params, ";") {
			kv := strings.SplitN(strings.TrimSpace(p), "=", 2)
			if len(kv) == 2 {
				k := strings.ToLower(kv[0])
				vv := strings.TrimSpace(kv[1])
				switch k {
				case "received":
					if vv != "" {
						host = vv
					}
				case "rport":
					if vv != "" {
						if n, perr := strconv.Atoi(vv); perr == nil {
							port = uint16(n)
						}
					}
				}
			}
		}
	}
	if host == "" {
		return "", 0, errors.New("empty Via host")
	}
	if port == 0 {
		port = 5060
	}
	return host, port, nil
}

// extractSentBy parses "host" or "host:port", handling bracketed IPv6.
func extractSentBy(s string) (string, uint16) {
	s = strings.TrimSpace(s)
	if strings.HasPrefix(s, "[") {
		end := strings.Index(s, "]")
		if end == -1 {
			return s, 5060
		}
		host := s[1:end]
		rest := s[end+1:]
		if strings.HasPrefix(rest, ":") {
			if p, err := strconv.Atoi(rest[1:]); err == nil {
				return host, uint16(p)
			}
		}
		return host, 5060
	}
	if colon := strings.LastIndex(s, ":"); colon != -1 {
		host := s[:colon]
		if p, err := strconv.Atoi(s[colon+1:]); err == nil {
			return host, uint16(p)
		}
	}
	return s, 5060
}

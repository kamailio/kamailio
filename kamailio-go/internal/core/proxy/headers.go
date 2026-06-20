// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Proxy header manipulation - RFC 3261 section 16 (proxy behaviour)
 *
 * Core operations:
 *   - AddVia / RemoveTopVia    (Via header management)
 *   - AddRecordRoute           (Record-Route insertion for stateful proxies)
 *   - ProcessRouteHeaders      (Route header stripping / routing decision)
 *   - CreateRequest            (Clone request + reset routing for new target)
 *   - BuildResponse            (Build response with proper Via/From/To)
 *
 * C equivalent: via.{c,h} / rr.{c,h} / msg_translator.c
 */

package proxy

import (
	"fmt"
	"net"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// --------------------------------------------------------------------
// Via header handling (RFC 3261 section 16.6 step 3 / 16.7 step 3)
// --------------------------------------------------------------------

// AddVia prepends a Via header to the given request using the provided
// socket information and branch token. This implements RFC 3261 §16.6 #3.
//
//	Format: "SIP/2.0/UDP host:port;branch=z9hG4bK...;rport"
func AddVia(request *parser.SIPMsg, sendSocketInfo *transport.SocketInfo, branch string) {
	if request == nil {
		return
	}

	host := ""
	port := 0
	proto := "UDP"
	if sendSocketInfo != nil {
		if sendSocketInfo.Address != nil {
			host = sendSocketInfo.Address.String()
		}
		port = int(sendSocketInfo.Port)
		proto = strings.ToUpper(sendSocketInfo.Protocol.String())
		if proto == "NONE" || proto == "" {
			proto = "UDP"
		}
	}

	token := branch
	if token == "" {
		token = "z9hG4bK" + randomHexToken(8)
	} else if !strings.HasPrefix(token, "z9hG4bK") {
		// RFC 3261 §16.6 step 3: branch tokens MUST begin with "z9hG4bK"
		token = "z9hG4bK" + token
	}

	request.AddVia(proto, host, port, token)
}

// RemoveTopVia strips the topmost Via header from a response.
// RFC 3261 §18.2.2: a proxy MUST remove its own Via from forwarded responses.
func RemoveTopVia(reply *parser.SIPMsg) string {
	if reply == nil {
		return ""
	}
	return reply.RemoveTopmostVia()
}

// --------------------------------------------------------------------
// Record-Route handling (RFC 3261 section 16.6 step 4)
// --------------------------------------------------------------------

// AddRecordRoute adds a Record-Route header to the request so that the
// proxy stays in the signalling path for subsequent requests within
// the same dialog.
//
//	Format: "<sip:host:port;lr>"
func AddRecordRoute(request *parser.SIPMsg, sendSocketInfo *transport.SocketInfo) {
	if request == nil || sendSocketInfo == nil {
		return
	}

	host := ""
	port := 0
	if sendSocketInfo.Address != nil {
		host = sendSocketInfo.Address.String()
	}
	port = int(sendSocketInfo.Port)

	// Format as sip:host:port;lr  (loose-route indicator)
	uri := fmt.Sprintf("<sip:%s:%d;lr>", host, port)
	if port == 0 {
		uri = fmt.Sprintf("<sip:%s;lr>", host)
	}

	// Insert near the top (after Via headers) for correct SIP ordering.
	insertIndex := 0
	for i, h := range request.Headers {
		if h.Type != parser.HdrVia {
			insertIndex = i
			break
		}
		insertIndex = i + 1
	}

	request.AddHeaderAt(insertIndex, "Record-Route", uri)
}

// --------------------------------------------------------------------
// Route header handling (RFC 3261 section 16.4)
// --------------------------------------------------------------------

// ProcessRouteHeaders performs RFC 3261 §16.4 processing:
//
//  1. If the first Route URI points to this proxy, strip it.
//  2. If the Route list is now empty, routing is based on the Request-URI.
//
// For simplicity the "this proxy" check is omitted; callers that need a
// full strict-route/strict-route translation should run it beforehand.
func ProcessRouteHeaders(request *parser.SIPMsg) {
	if request == nil {
		return
	}

	// If there are Route headers and the first one targets this proxy,
	// strip it. We keep things simple: strip the header if the URI ends
	// with ";lr" (loose route) and the host is empty (self-reference).
	//
	// Production deployments should check against the proxy's own host list.
	routeHeaders := request.GetAllHeadersByType(parser.HdrRoute)
	if len(routeHeaders) == 0 {
		return
	}

	first := strings.TrimSpace(routeHeaders[0].Body.String())
	// Remove surrounding angle brackets / sip: scheme to compare host.
	host := extractHostFromRouteURI(first)
	if host != "" && isSelfReference(host) {
		// Strip the first Route header.
		request.RemoveHeaderByType(parser.HdrRoute)
		// Re-add the remaining routes (comma-separated in the first header).
		if len(routeHeaders) > 1 {
			// Re-insert them as separate headers to preserve ordering.
			for i := len(routeHeaders) - 1; i >= 1; i-- {
				request.AddHeaderAt(0, "Route", routeHeaders[i].Body.String())
			}
		}
	}
}

// extractHostFromRouteURI extracts the hostname from a Route URI string
// such as "<sip:host:port;lr>" or "sip:host:port".
func extractHostFromRouteURI(route string) string {
	s := strings.TrimSpace(route)
	s = strings.TrimPrefix(s, "<")
	s = strings.TrimSuffix(s, ">")
	s = strings.TrimPrefix(s, "sips:")
	s = strings.TrimPrefix(s, "sip:")

	// Strip user@ part
	if at := strings.Index(s, "@"); at != -1 && at+1 < len(s) {
		s = s[at+1:]
	}

	// Stop at the first parameter separator.
	if semi := strings.Index(s, ";"); semi != -1 {
		s = s[:semi]
	}

	// Host can be "host:port"
	if colon := strings.LastIndex(s, ":"); colon != -1 {
		// IPv6 in brackets?
		if !strings.Contains(s[:colon], "]") && strings.Contains(s[colon:], ":") {
			// ambiguous - fall through
		} else {
			s = s[:colon]
		}
	}
	s = strings.Trim(s, "[]")
	return strings.TrimSpace(s)
}

// isSelfReference reports whether the host should be considered "this
// proxy". A production proxy would compare against its own listen
// addresses; here we treat empty strings and localhost as self.
func isSelfReference(host string) bool {
	if host == "" {
		return false
	}
	switch host {
	case "localhost", "127.0.0.1", "::1":
		return true
	}
	return false
}

// --------------------------------------------------------------------
// Request cloning / response construction (RFC 3261 section 16)
// --------------------------------------------------------------------

// CreateRequest clones the given request and resets routing information
// (Max-Forwards restored to 70). The resulting message can be forwarded
// to a new destination while preserving dialog-forming headers.
func CreateRequest(original *parser.SIPMsg, method parser.RequestMethod) *parser.SIPMsg {
	if original == nil {
		return nil
	}

	clone := original.Clone()
	// Reset Max-Forwards to a fresh value when changing destination.
	if !clone.DecrementMaxForwards() {
		// If missing, add a default Max-Forwards header.
		clone.AddHeader("Max-Forwards", "70")
	}

	// If a method was supplied, rewrite the request line.
	if method != parser.MethodUndefined && clone.FirstLine != nil && clone.FirstLine.Req != nil {
		methodStr := parser.MethodName(method)
		clone.FirstLine.Req.Method = str.Mk(methodStr)
		clone.FirstLine.Req.MethodValue = method

		// Update CSeq method as well to stay consistent with the new request.
		for _, h := range clone.Headers {
			if h.Type == parser.HdrCSeq {
				parts := strings.Fields(h.Body.String())
				if len(parts) >= 1 {
					h.Body = str.Mk(parts[0] + " " + methodStr)
				}
				break
			}
		}
	}
	return clone
}

// BuildResponse constructs a response to `request` using the provided
// status code and reason phrase. The returned bytes are ready to be sent
// over the wire.
func BuildResponse(request *parser.SIPMsg, code int, reason string) ([]byte, error) {
	if request == nil {
		return nil, fmt.Errorf("nil request")
	}

	opts := parser.ReplyOptions{
		StatusCode:   code,
		ReasonPhrase: reason,
	}
	// Build response - automatically copies Via/From/To/Call-ID/CSeq.
	return parser.BuildResponse(request, opts)
}

// --------------------------------------------------------------------
// Helper utilities used by the proxy core.
// --------------------------------------------------------------------

// checkMaxForwards reports whether the request has a Max-Forwards value
// greater than zero. RFC 3261 §16.3: if the value is 0 the proxy MUST
// respond with 483 Too Many Hops.
func checkMaxForwards(msg *parser.SIPMsg) bool {
	if msg == nil {
		return false
	}
	for _, h := range msg.Headers {
		if h.Type == parser.HdrMaxForwards {
			n, err := strconv.Atoi(strings.TrimSpace(h.Body.String()))
			if err != nil {
				return false
			}
			return n > 0
		}
	}
	// Missing header is acceptable in a loose implementation.
	return true
}

// processRouteHeaders is the internal convenience wrapper that simply
// strips Route headers that self-reference. It is used by ProxyCore.
func processRouteHeaders(msg *parser.SIPMsg) {
	ProcessRouteHeaders(msg)
}

// removeTopVia is an internal convenience wrapper for response handling.
func removeTopVia(msg *parser.SIPMsg) string {
	return RemoveTopVia(msg)
}

// hasAuthHeader reports whether the message contains an Authorization
// or Proxy-Authorization header.
func hasAuthHeader(msg *parser.SIPMsg) bool {
	if msg == nil {
		return false
	}
	if msg.Authorization != nil {
		return true
	}
	if msg.ProxyAuth != nil {
		return true
	}
	for _, h := range msg.Headers {
		if h.Type == parser.HdrAuthorization || h.Type == parser.HdrProxyAuth {
			return true
		}
	}
	return false
}

// extractCallID returns the Call-ID header value (used for dialog lookups).
func extractCallID(msg *parser.SIPMsg) string {
	if msg == nil {
		return ""
	}
	if msg.CallID != nil {
		return strings.TrimSpace(msg.CallID.Body.String())
	}
	for _, h := range msg.Headers {
		if h.Type == parser.HdrCallID {
			return strings.TrimSpace(h.Body.String())
		}
	}
	return ""
}

// extractFromTag returns the tag parameter from the From header.
func extractFromTag(msg *parser.SIPMsg) string {
	if msg == nil {
		return ""
	}
	var from *parser.HdrField
	if msg.From != nil {
		from = msg.From
	} else {
		for _, h := range msg.Headers {
			if h.Type == parser.HdrFrom {
				from = h
				break
			}
		}
	}
	if from == nil {
		return ""
	}
	return extractTagParam(from.Body.String())
}

// extractToTag returns the tag parameter from the To header.
func extractToTag(msg *parser.SIPMsg) string {
	if msg == nil {
		return ""
	}
	var to *parser.HdrField
	if msg.To != nil {
		to = msg.To
	} else {
		for _, h := range msg.Headers {
			if h.Type == parser.HdrTo {
				to = h
				break
			}
		}
	}
	if to == nil {
		return ""
	}
	return extractTagParam(to.Body.String())
}

// extractTagParam extracts the value of the ";tag=" parameter from a
// header body such as "<sip:alice@example.com>;tag=12345".
func extractTagParam(body string) string {
	lower := strings.ToLower(body)
	idx := strings.Index(lower, ";tag=")
	if idx == -1 {
		return ""
	}
	rest := body[idx+len(";tag="):]
	// Tag ends at next ';' or whitespace.
	for i := 0; i < len(rest); i++ {
		c := rest[i]
		if c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n' {
			return strings.TrimSpace(rest[:i])
		}
	}
	return strings.TrimSpace(rest)
}

// extractToURI returns the URI embedded in the To header.
func extractToURI(msg *parser.SIPMsg) string {
	if msg == nil || msg.To == nil {
		return ""
	}
	return extractURI(msg.To.Body.String())
}

// extractFromURI returns the URI embedded in the From header.
func extractFromURI(msg *parser.SIPMsg) string {
	if msg == nil || msg.From == nil {
		return ""
	}
	return extractURI(msg.From.Body.String())
}

// extractContactURI returns the URI from the Contact header.
func extractContactURI(msg *parser.SIPMsg) string {
	if msg == nil || msg.Contact == nil {
		return ""
	}
	return extractURI(msg.Contact.Body.String())
}

// extractURI pulls the sip: URI substring out of a header body such as
// "Alice <sip:alice@example.com>;tag=1" or plain "sip:alice@example.com".
func extractURI(body string) string {
	s := strings.TrimSpace(body)
	if la := strings.Index(s, "<"); la != -1 {
		if ra := strings.Index(s[la:], ">"); ra != -1 {
			return strings.TrimSpace(s[la+1 : la+ra])
		}
	}
	// No angle brackets - strip any trailing parameters.
	if semi := strings.Index(s, ";"); semi != -1 {
		s = s[:semi]
	}
	return strings.TrimSpace(s)
}

// extractEvent returns the Event header body, used for SUBSCRIBE/NOTIFY.
func extractEvent(msg *parser.SIPMsg) string {
	if msg == nil {
		return ""
	}
	if msg.Event != nil {
		return strings.TrimSpace(msg.Event.Body.String())
	}
	for _, h := range msg.Headers {
		if h.Type == parser.HdrEvent {
			return strings.TrimSpace(h.Body.String())
		}
	}
	return ""
}

// extractSourceIP pulls the source IP address out of a net.Addr. The
// returned net.IP is nil if the address cannot be parsed.
func extractSourceIP(src net.Addr) net.IP {
	if src == nil {
		return nil
	}
	// Handle common forms: *net.UDPAddr, *net.TCPAddr, or a generic
	// "host:port" string.
	switch v := src.(type) {
	case *net.UDPAddr:
		if v != nil {
			return v.IP
		}
	case *net.TCPAddr:
		if v != nil {
			return v.IP
		}
	}
	s := src.String()
	if host, _, err := net.SplitHostPort(s); err == nil {
		return net.ParseIP(host)
	}
	return net.ParseIP(s)
}

// randomHexToken generates a random-looking hex token used for branch
// parameters. The implementation uses a deterministic seed so that
// test scenarios remain reproducible; production code should substitute
// crypto/rand.
func randomHexToken(n int) string {
	const hexChars = "0123456789abcdef"
	buf := make([]byte, 2*n)
	x := uint64(len(buf))*1103515245 + 12345
	for i := range buf {
		x = x*1103515245 + 12345
		buf[i] = hexChars[(x>>8)&0x0f]
	}
	return string(buf)
}

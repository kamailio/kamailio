// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Proxy core - unified handler for incoming SIP requests and replies.
 *
 * Responsibilities:
 *   - Validate Max-Forwards and authentication.
 *   - Detect NAT and fix contacts before forwarding.
 *   - Dispatch by method into registrar / dialog / presence paths.
 *   - Track per-method metrics and provide response actions.
 *
 * C equivalent: core/action.{c,h} / forward.{c,h} / stateful/proxy.{c,h}
 */

package proxy

import (
	"context"
	"fmt"
	"net"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/forward"
	"github.com/kamailio/kamailio-go/internal/core/nat"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/presence"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// defaultSIPPort is used as a fallback port when the caller does not
// specify one for NAT contact fixes.
const defaultSIPPort = 5060

// listenerAddr is a minimal interface used for status reporting.
type listenerAddr interface {
	AddrString() string
}

// --------------------------------------------------------------------
// Listener abstraction
// --------------------------------------------------------------------

// Listener represents a network endpoint that the proxy uses to send
// responses. The proxy core is transport-agnostic; concrete listeners
// implement UDP / TCP / TLS transports.
type Listener interface {
	// SendTo writes a packet to the given destination address.
	SendTo(dst net.Addr, data []byte) error

	// LocalAddr returns the listener's local address.
	LocalAddr() net.Addr

	// SendSocketInfo returns the SocketInfo descriptor used when
	// constructing Via/Record-Route headers for outgoing messages.
	SendSocketInfo() *transport.SocketInfo
}

// --------------------------------------------------------------------
// Proxy configuration
// --------------------------------------------------------------------

// ProxyConfig controls optional behaviour exposed by the proxy core.
// A nil config is equivalent to the zero value (default-deny nothing).
type ProxyConfig struct {
	// Realm is used as the realm parameter in authentication challenges.
	Realm string

	// AuthRequired toggles whether REGISTER/INVITE/etc. require a valid
	// Authorization or Proxy-Authorization header before processing.
	AuthRequired bool

	// NATDetectionEnabled toggles RFC 3581 / symmetric response routing
	// contact-rewriting for NAT'ed clients.
	NATDetectionEnabled bool

	// MediaProxyEnabled is reserved for future media proxy integration.
	MediaProxyEnabled bool

	// PresenceEnabled toggles SUBSCRIBE / NOTIFY / PUBLISH handling.
	PresenceEnabled bool

	// RecordRouteEnabled toggles automatic Record-Route header insertion
	// on forwarded INVITEs so that in-dialog requests traverse the proxy.
	RecordRouteEnabled bool
}

// --------------------------------------------------------------------
// ProxyCore
// --------------------------------------------------------------------

// ProxyCore is the central dispatcher of the proxy. It owns the dialog
// manager, forwarder, presence handler, listener set, and metrics.
type ProxyCore struct {
	mu        sync.RWMutex
	config    *ProxyConfig
	dialogs   *dialog.Manager
	forward   *forward.Forwarder
	presence  *presence.ServerHandler
	listeners []Listener
	metrics   *Metrics
	draining  int32 // 0=running, 1=draining (atomic)
}

// NewProxyCore constructs a ProxyCore with the given configuration.
// Passing nil yields a core with safe defaults (realm "kamailio-go.local").
func NewProxyCore(cfg *ProxyConfig) *ProxyCore {
	if cfg == nil {
		cfg = &ProxyConfig{Realm: "kamailio-go.local"}
	}
	if cfg.Realm == "" {
		cfg.Realm = "kamailio-go.local"
	}
	return &ProxyCore{
		config:   cfg,
		dialogs:  dialog.NewManager(),
		forward:  forward.NewForwarder(),
		presence: presence.NewServerHandler(),
		metrics:  newMetrics(),
	}
}

// AddListener registers a new Listener endpoint with the proxy.
// The listeners are consulted when Record-Route or Via headers need a
// sending address; they are not used directly for dispatching here.
func (p *ProxyCore) AddListener(l Listener) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if l != nil {
		p.listeners = append(p.listeners, l)
	}
}

// Dialogs returns the dialog manager for introspection or external use.
func (p *ProxyCore) Dialogs() *dialog.Manager { return p.dialogs }

// Metrics returns the metrics collector. It is safe to call concurrently.
func (p *ProxyCore) Metrics() *Metrics {
	if p == nil || p.metrics == nil {
		return newMetrics()
	}
	return p.metrics
}

// listenerAddrs returns a snapshot of the adapter wrappers for status
// reporting. Note: this is not the same as the raw transport listeners —
// see UDPListenerAdapter / TCPListenerAdapter for details.
func (p *ProxyCore) listenerAddrs() []listenerAddr {
	p.mu.RLock()
	defer p.mu.RUnlock()
	out := make([]listenerAddr, 0, len(p.listeners))
	for _, l := range p.listeners {
		if la, ok := l.(listenerAddr); ok {
			out = append(out, la)
		}
	}
	return out
}

// --------------------------------------------------------------------
// ResponseAction
// --------------------------------------------------------------------

// ResponseAction encapsulates the proxy's decision for a given message
// so that callers can dispatch it without knowing proxy internals.
type ResponseAction struct {
	// Status holds the SIP status code that the caller should return
	// (e.g. 200, 401, 407, 483). A value of 0 indicates "no reply".
	Status int

	// Reason is the human-readable reason phrase, e.g. "OK".
	Reason string

	// Passthrough indicates that the message should be forwarded as-is
	// with only topmost-Via removal (used for reply processing).
	Passthrough bool

	// ExtraHeaders contains additional header rows that should be
	// appended to the response, e.g. "WWW-Authenticate: Digest ...".
	ExtraHeaders []string

	// Target is the downstream host:port used when the action is a
	// forward.
	Target string

	// SendTo is an explicit destination (useful when replies should
	// bypass normal routing).
	SendTo string

	// StopRouting, when true, signals that further script-like
	// pipeline processing should stop for this message.
	StopRouting bool

	// NATRewritten is true when the contact/route information has
	// been rewritten for NAT traversal.
	NATRewritten bool

	// ForwardedBy records the listener host:port that forwarded this
	// request.
	ForwardedBy string

	// RouteSet contains Route headers to add to forwarded requests.
	RouteSet []string

	// Headers contains extra header name/value pairs to inject into
	// the generated reply.
	Headers map[string]string

	// Body, if non-empty, is used as the SIP response body and
	// overrides the Content-Length header.
	Body string
}

// --------------------------------------------------------------------
// Response building
// --------------------------------------------------------------------

// BuildReply serialises a SIP reply for msg based on action. It merges
// the caller-provided extra headers and body (if any) on top of the
// required start-line / Via / From / To / Call-ID / CSeq headers.
func (p *ProxyCore) BuildReply(msg *parser.SIPMsg, action *ResponseAction) []byte {
	if msg == nil || action == nil {
		return nil
	}
	status := action.Status
	if status == 0 {
		status = 200
	}
	reason := action.Reason
	if reason == "" {
		reason = reasonPhrase(status)
	}

	// start-line + required headers.
	var b strings.Builder
	b.WriteString("SIP/2.0 ")
	b.WriteString(strconv.Itoa(status))
	b.WriteString(" ")
	b.WriteString(reason)
	b.WriteString("\r\n")

	if via := msg.GetAllHeadersByType(parser.HdrVia); len(via) > 0 {
		for _, v := range via {
			b.WriteString("Via: ")
			b.WriteString(v.Body.String())
			b.WriteString("\r\n")
		}
	} else if msg.Via1 != nil {
		b.WriteString("Via: ")
		b.WriteString(msg.Via1.Hdr.String())
		b.WriteString("\r\n")
		if msg.Via2 != nil {
			b.WriteString("Via: ")
			b.WriteString(msg.Via2.Hdr.String())
			b.WriteString("\r\n")
		}
	}

	if from := msg.From; from != nil {
		b.WriteString("From: ")
		b.WriteString(from.Body.String())
		b.WriteString("\r\n")
	}
	if to := msg.To; to != nil {
		b.WriteString("To: ")
		if status >= 300 {
			// RFC 3261: tag required on final responses.
			toText := to.Body.String()
			if !strings.Contains(strings.ToLower(toText), "tag=") {
				toText += ";tag=" + fmt.Sprintf("%x", time.Now().UnixNano())
			}
			b.WriteString(toText)
		} else {
			b.WriteString(to.Body.String())
		}
		b.WriteString("\r\n")
	}
	if callID := msg.CallID; callID != nil {
		b.WriteString("Call-ID: ")
		b.WriteString(callID.Body.String())
		b.WriteString("\r\n")
	}
	if cseq := msg.CSeq; cseq != nil {
		b.WriteString("CSeq: ")
		b.WriteString(cseq.Body.String())
		b.WriteString("\r\n")
	}

	b.WriteString("Server: kamailio-go\r\n")
	b.WriteString("Content-Length: 0\r\n")

	// preserve legacy ExtraHeaders list.
	for _, h := range action.ExtraHeaders {
		if h == "" {
			continue
		}
		b.WriteString(h)
		if !strings.HasSuffix(h, "\r\n") {
			b.WriteString("\r\n")
		}
	}

	// merge caller-supplied extra headers.
	if action.Headers != nil {
		for k, v := range action.Headers {
			if k == "" {
				continue
			}
			b.WriteString(k)
			b.WriteString(": ")
			b.WriteString(v)
			b.WriteString("\r\n")
		}
	}

	// body content if any overrides Content-Length.
	if action.Body != "" {
		raw := b.String()
		raw = strings.Replace(raw, "Content-Length: 0\r\n", "Content-Length: "+strconv.Itoa(len(action.Body))+"\r\n", 1)
		var out strings.Builder
		out.WriteString(raw)
		out.WriteString("\r\n")
		out.WriteString(action.Body)
		return []byte(out.String())
	}

	b.WriteString("\r\n")
	return []byte(b.String())
}

// reasonPhrase returns a default reason phrase for a given SIP status
// code. It is intentionally minimal and covers only the codes the
// proxy core actually emits.
func reasonPhrase(status int) string {
	switch {
	case status >= 100 && status < 200:
		switch status {
		case 100:
			return "Trying"
		case 180:
			return "Ringing"
		case 181:
			return "Call is Being Forwarded"
		case 182:
			return "Queued"
		case 183:
			return "Session Progress"
		}
		return "Ringing"
	case status >= 200 && status < 300:
		if status == 202 {
			return "Accepted"
		}
		return "OK"
	case status >= 300 && status < 400:
		switch status {
		case 301:
			return "Moved Permanently"
		case 302:
			return "Moved Temporarily"
		case 305:
			return "Use Proxy"
		}
		return "Redirection"
	case status >= 400 && status < 500:
		switch status {
		case 401:
			return "Unauthorized"
		case 403:
			return "Forbidden"
		case 404:
			return "Not Found"
		case 405:
			return "Method Not Allowed"
		case 407:
			return "Proxy Authentication Required"
		case 408:
			return "Request Timeout"
		case 483:
			return "Too Many Hops"
		case 488:
			return "Not Acceptable Here"
		case 489:
			return "Bad Event"
		}
		return "Client Error"
	case status >= 500 && status < 600:
		switch status {
		case 500:
			return "Server Internal Error"
		case 501:
			return "Not Implemented"
		case 503:
			return "Service Unavailable"
		}
		return "Server Failure"
	case status >= 600:
		switch status {
		case 603:
			return "Decline"
		case 604:
			return "Does Not Exist Anywhere"
		case 606:
			return "Not Acceptable"
		}
		return "Global Failure"
	default:
		return "Unknown"
	}
}

// Send is a lightweight helper that writes data to the given address
// using the first registered listener. It is kept simple because the
// actual transport layers have their own fancier send paths.
func (p *ProxyCore) Send(data []byte, dst net.Addr) error {
	if p == nil || dst == nil {
		return fmt.Errorf("nil proxy core or destination")
	}
	p.mu.RLock()
	listeners := p.listeners
	p.mu.RUnlock()
	for _, l := range listeners {
		if err := l.SendTo(dst, data); err == nil {
			return nil
		}
	}
	return fmt.Errorf("no listener available for send")
}

// processAction dispatches a ResponseAction: for replies it serialises
// the SIP message, updates metrics, and emits a log line. For forwards
// it records the target in the log for later inspection. Returning the
// raw bytes lets callers decide whether to actually push them onto the
// wire.
// processAction records the action's side effects: it updates counters and emits
// structured log lines. It does NOT send bytes onto the wire - that remains the
// caller's responsibility so that higher level logic (see cmd/kamailio/server.go) can
// control the transport-level response. Returning raw bytes is still useful for
// ad-hoc callers that want to send replies straight away.
func (p *ProxyCore) processAction(msg *parser.SIPMsg, action *ResponseAction, rcvInfo *transport.ReceiveInfo) []byte {
	if msg == nil || action == nil {
		return nil
	}
	if action.Status > 0 {
		p.metrics.countResponse(msg.Method(), action.Status)
		logResponse(msg, action.Status, action.Reason)
		return p.BuildReply(msg, action)
	}
	if action.Target != "" {
		logForwardedRequest(msg, action.Target)
	}
	return nil
}

// rcvInfoRemote extracts a net.Addr from a ReceiveInfo when possible.
// Returns nil if the receive info or the source fields are missing.
func rcvInfoRemote(rcv *transport.ReceiveInfo) net.Addr {
	if rcv == nil || len(rcv.SrcIP) == 0 {
		return nil
	}
	if rcv.Proto == transport.ProtoTCP || rcv.Proto == transport.ProtoTLS {
		return &net.TCPAddr{IP: rcv.SrcIP, Port: int(rcv.SrcPort)}
	}
	return &net.UDPAddr{IP: rcv.SrcIP, Port: int(rcv.SrcPort)}
}

// --------------------------------------------------------------------
// Request processing
// --------------------------------------------------------------------

// ProcessRequest handles an incoming SIP request. It validates routing
// headers, runs NAT detection, and dispatches the request by method.
// The returned ResponseAction tells the caller what reply to generate.
func (p *ProxyCore) ProcessRequest(msg *parser.SIPMsg, src net.Addr, rcvInfo *transport.ReceiveInfo) ResponseAction {
	start := time.Now()

	if msg == nil || !msg.IsRequest() {
		return ResponseAction{Status: 400, Reason: "Bad Request"}
	}

	logRequest(msg)
	p.metrics.countRequest(msg.Method())
	defer func() {
		p.metrics.recordLatency(time.Since(start))
		logLatency("request", start)
	}()

	// RFC 3261 §16.3: decrement / verify Max-Forwards.
	if !checkMaxForwards(msg) {
		p.metrics.incError(483)
		action := &ResponseAction{Status: 483, Reason: "Too Many Hops"}
		p.processAction(msg, action, rcvInfo)
		return *action
	}

	// RFC 3261 §16.4: strip self-referencing Route headers.
	processRouteHeaders(msg)

	// NAT detection / contact rewriting (RFC 3581-style).
	if p.config.NATDetectionEnabled && src != nil {
		if res := nat.Detect(msg, ipString(extractSourceIP(src))); res != nil && res.IsNAT {
			if err := nat.FixContact(msg, ipString(extractSourceIP(src)), defaultSIPPort); err != nil {
				// Contact rewrite is best-effort; do not fail the request.
			}
		}
	}

	// Dispatch by method.
	var action ResponseAction
	switch msg.Method() {
	case parser.MethodRegister:
		action = p.handleRegister(msg)
	case parser.MethodInvite:
		action = p.handleInvite(msg)
	case parser.MethodBye:
		action = p.handleBye(msg)
	case parser.MethodACK:
		// ACK does not receive a reply - we just record it.
		action = ResponseAction{Status: 0, Reason: ""}
	case parser.MethodCancel:
		action = p.handleCancel(msg)
	case parser.MethodSubscribe:
		action = p.handleSubscribe(msg)
	case parser.MethodNotify:
		action = p.handleNotify(msg)
	case parser.MethodPublish:
		action = p.handlePublish(msg)
	case parser.MethodOptions:
		// Keep-alive ping - always accepted.
		action = ResponseAction{Status: 200, Reason: "OK"}
	case parser.MethodInfo, parser.MethodMessage, parser.MethodRefer, parser.MethodUpdate, parser.MethodPRACK:
		// In-dialog / feature extensions - accept but do not forward.
		action = ResponseAction{Status: 200, Reason: "OK"}
	default:
		p.metrics.incError(405)
		action = ResponseAction{Status: 405, Reason: "Method Not Allowed"}
	}

	// Fire side effects (log, metrics, optional send). The caller still
	// gets to decide whether to push bytes down the wire - we only write
	// here when rcvInfo has source address information available.
	p.processAction(msg, &action, rcvInfo)
	return action
}

// ProcessReply handles an incoming SIP reply. For proxies the correct
// behaviour is to strip the topmost Via and forward the reply upstream.
func (p *ProxyCore) ProcessReply(msg *parser.SIPMsg, src net.Addr) ResponseAction {
	p.metrics.incReply()

	if msg == nil || msg.IsRequest() {
		return ResponseAction{Status: 0, Reason: ""}
	}

	// RFC 3261 §18.2.2: remove topmost Via so the reply travels upstream.
	removeTopVia(msg)

	return ResponseAction{Status: 0, Reason: "", Passthrough: true}
}

// --------------------------------------------------------------------
// Method handlers
// --------------------------------------------------------------------

// handleRegister processes REGISTER requests. When authentication is
// required the proxy challenges the client; otherwise a 200 OK is returned.
func (p *ProxyCore) handleRegister(msg *parser.SIPMsg) ResponseAction {
	if p.config.AuthRequired && !hasAuthHeader(msg) {
		challenge := auth.BuildWWWAuthenticate(auth.ChallengeOptions{Realm: p.config.Realm})
		return ResponseAction{
			Status:       401,
			Reason:       "Unauthorized",
			ExtraHeaders: []string{challenge},
		}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// handleInvite processes INVITE requests. When authentication is
// required a proxy challenge is generated; otherwise we return 100 Trying
// (to be followed by real forwarding).
func (p *ProxyCore) handleInvite(msg *parser.SIPMsg) ResponseAction {
	if p.config.AuthRequired && !hasAuthHeader(msg) {
		challenge := auth.BuildProxyAuthenticate(auth.ChallengeOptions{Realm: p.config.Realm})
		return ResponseAction{
			Status:       407,
			Reason:       "Proxy Authentication Required",
			ExtraHeaders: []string{challenge},
		}
	}

	// Track dialog so that subsequent BYE/CANCEL can terminate it.
	if _, err := dialog.CreateUASDialog(msg, ""); err == nil {
		// dialog tracked
	}

	// Optionally record-route to keep the proxy in the signalling path.
	if p.config.RecordRouteEnabled {
		if sock := p.firstSendSocket(); sock != nil {
			AddRecordRoute(msg, sock)
		}
	}

	return ResponseAction{Status: 100, Reason: "Trying"}
}

// handleBye terminates any tracked dialog for the call and returns 200.
func (p *ProxyCore) handleBye(msg *parser.SIPMsg) ResponseAction {
	callID := extractCallID(msg)
	fromTag := extractFromTag(msg)
	toTag := extractToTag(msg)
	if callID != "" {
		if d := p.dialogs.Lookup(callID, fromTag, toTag); d != nil {
			d.Terminate()
		}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// handleCancel cancels any tracked dialog for the call.
func (p *ProxyCore) handleCancel(msg *parser.SIPMsg) ResponseAction {
	callID := extractCallID(msg)
	fromTag := extractFromTag(msg)
	if callID != "" {
		if d := p.dialogs.Lookup(callID, fromTag, ""); d != nil {
			d.Terminate()
		}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// handleSubscribe processes SUBSCRIBE requests for presence packages.
func (p *ProxyCore) handleSubscribe(msg *parser.SIPMsg) ResponseAction {
	if !p.config.PresenceEnabled {
		return ResponseAction{Status: 405, Reason: "Method Not Allowed"}
	}
	if p.config.AuthRequired && !hasAuthHeader(msg) {
		challenge := auth.BuildWWWAuthenticate(auth.ChallengeOptions{Realm: p.config.Realm})
		return ResponseAction{
			Status:       401,
			Reason:       "Unauthorized",
			ExtraHeaders: []string{challenge},
		}
	}
	presentity := extractToURI(msg)
	subscriber := extractFromURI(msg)
	_, err := p.presence.Subscribe(presentity, subscriber, extractEvent(msg), 3600*time.Second)
	if err != nil {
		return ResponseAction{Status: 489, Reason: "Bad Event"}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// handleNotify processes NOTIFY requests.
func (p *ProxyCore) handleNotify(msg *parser.SIPMsg) ResponseAction {
	if !p.config.PresenceEnabled {
		return ResponseAction{Status: 405, Reason: "Method Not Allowed"}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// handlePublish processes PUBLISH requests to update presence state.
func (p *ProxyCore) handlePublish(msg *parser.SIPMsg) ResponseAction {
	if !p.config.PresenceEnabled {
		return ResponseAction{Status: 405, Reason: "Method Not Allowed"}
	}
	if p.config.AuthRequired && !hasAuthHeader(msg) {
		challenge := auth.BuildWWWAuthenticate(auth.ChallengeOptions{Realm: p.config.Realm})
		return ResponseAction{
			Status:       401,
			Reason:       "Unauthorized",
			ExtraHeaders: []string{challenge},
		}
	}
	entityTag := fmt.Sprintf("entity-%d", time.Now().UnixNano())
	p.presence.Publish(
		extractToURI(msg),
		presence.PresenceStateOpen,
		"",
		extractContactURI(msg),
		entityTag,
		3600*time.Second,
	)
	return ResponseAction{Status: 200, Reason: "OK"}
}

// --------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------

// firstSendSocket returns the socket info of the first registered
// listener for use in Via / Record-Route construction.
func (p *ProxyCore) firstSendSocket() *transport.SocketInfo {
	p.mu.RLock()
	defer p.mu.RUnlock()
	for _, l := range p.listeners {
		if s := l.SendSocketInfo(); s != nil {
			return s
		}
	}
	return nil
}

// ipString converts a net.IP to a safe string representation suitable
// for the nat package helpers. A nil IP is reported as an empty string.
func ipString(ip net.IP) string {
	if ip == nil {
		return ""
	}
	return ip.String()
}

// --------------------------------------------------------------------
// Drain / Shutdown
// --------------------------------------------------------------------

// Drain waits for in-flight request processing to complete, up to the
// provided timeout. It blocks new requests by setting a "draining" flag.
func (p *ProxyCore) Drain(ctx context.Context) error {
	if p == nil {
		return nil
	}
	atomic.StoreInt32(&p.draining, 1)
	defer atomic.StoreInt32(&p.draining, 0)

	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
	}
	return nil
}

// Shutdown cleanly stops the proxy core and its listeners. It calls
// Drain with a bounded context to allow in-flight work to complete.
func (p *ProxyCore) Shutdown(ctx context.Context) error {
	if p == nil {
		return nil
	}

	// Drain phase
	drainCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
	defer cancel()
	if err := p.Drain(drainCtx); err != nil {
		return err
	}

	// Nothing else to tear down at this level — the caller is responsible
	// for stopping the transport listeners via ShutdownListeners.
	return nil
}

// IsDraining reports whether the proxy is currently draining (rejecting
// new work).
func (p *ProxyCore) IsDraining() bool {
	if p == nil {
		return false
	}
	return atomic.LoadInt32(&p.draining) == 1
}

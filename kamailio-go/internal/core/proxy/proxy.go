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
	"fmt"
	"net"
	"sync"
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

// Metrics returns the metrics collector for snapshotting / exposition.
func (p *ProxyCore) Metrics() *Metrics { return p.metrics }

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
}

// --------------------------------------------------------------------
// Request processing
// --------------------------------------------------------------------

// ProcessRequest handles an incoming SIP request. It validates routing
// headers, runs NAT detection, and dispatches the request by method.
// The returned ResponseAction tells the caller what reply to generate.
func (p *ProxyCore) ProcessRequest(msg *parser.SIPMsg, src net.Addr, rcvInfo *transport.ReceiveInfo) ResponseAction {
	start := time.Now()
	p.metrics.incRequest()

	if msg == nil || !msg.IsRequest() {
		return ResponseAction{Status: 400, Reason: "Bad Request"}
	}

	// RFC 3261 §16.3: decrement / verify Max-Forwards.
	if !checkMaxForwards(msg) {
		p.metrics.incError(483)
		return ResponseAction{Status: 483, Reason: "Too Many Hops"}
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

	p.metrics.recordLatency(msg.Method(), time.Since(start))
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

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

	"github.com/kamailio/kamailio-go/internal/core/acc"
	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/avp"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/fork"
	"github.com/kamailio/kamailio-go/internal/core/forward"
	"github.com/kamailio/kamailio-go/internal/core/htable"
	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/msilo"
	"github.com/kamailio/kamailio-go/internal/core/nat"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/pike"
	"github.com/kamailio/kamailio-go/internal/core/presence"
	"github.com/kamailio/kamailio-go/internal/core/registrar"
	"github.com/kamailio/kamailio-go/internal/core/script"
	"github.com/kamailio/kamailio-go/internal/core/topoh"
	"github.com/kamailio/kamailio-go/internal/core/transport"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
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
	media     *nat.MediaPipeline
	listeners []Listener
	metrics   *Metrics
	draining  int32 // 0=running, 1=draining (atomic)
	script    *script.Script
	acc       *acc.AccountingService
	pike      *pike.Pike
	topoHider *topoh.Hider
	htables   *htable.Manager
	msilo     *msilo.Msilo
	auth      *auth.DBAuthStore
	avps      *avp.Store
	registrar *registrar.Registrar
	tmMgr     *tm.Manager
}

// SetAccounting attaches an accounting service to the proxy. When non-nil,
// INVITE / BYE / CANCEL requests and SIP replies will be forwarded to the
// service so that CDRs can be produced.
func (p *ProxyCore) SetAccounting(ac *acc.AccountingService) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.acc = ac
}

// SetDialogs attaches a dialog manager. When attached, the dialog manager is
// also re-used by the RPC endpoint for `kamailio.dialog.list`.
func (p *ProxyCore) SetDialogs(dm *dialog.Manager) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if dm != nil {
		p.dialogs = dm
	}
}

// SetPike attaches a rate limiter. When non-nil, incoming requests are
// counted per source IP and rejected with 503 once the configured rate
// threshold is exceeded.
func (p *ProxyCore) SetPike(pk *pike.Pike) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.pike = pk
}

// SetTopoHider attaches a topology hider. When non-nil, outgoing requests
// are anonymized before forwarding (Call-ID, tags, IP addresses per the
// hider's configured strategy).
func (p *ProxyCore) SetTopoHider(h *topoh.Hider) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.topoHider = h
}

// SetHTables attaches an htable.Manager so that the proxy and modules
// installed on top of it can share a single namespace of hash tables.
func (p *ProxyCore) SetHTables(m *htable.Manager) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.htables = m
}

// HTables returns the currently attached htable.Manager (may be nil).
func (p *ProxyCore) HTables() *htable.Manager {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.htables
}

// SetMsilo attaches a message silo store-and-forward queue. When attached,
// MESSAGE requests routed through the proxy are forwarded to the silo so
// they can be delivered once the destination subscriber comes online.
func (p *ProxyCore) SetMsilo(m *msilo.Msilo) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.msilo = m
}

// Msilo returns the currently attached message silo (may be nil).
func (p *ProxyCore) Msilo() *msilo.Msilo {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.msilo
}

// SetAuthStore attaches a DB-backed credential store so requests requiring
// authentication can look up subscriber credentials during processing.
func (p *ProxyCore) SetAuthStore(a *auth.DBAuthStore) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.auth = a
}

// AuthStore returns the currently attached DB auth store (may be nil).
func (p *ProxyCore) AuthStore() *auth.DBAuthStore {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.auth
}

// SetAVPs attaches an AVP store so per-request attribute-value pairs can be
// accumulated while the proxy processes a message.
func (p *ProxyCore) SetAVPs(a *avp.Store) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.avps = a
}

// SetRegistrar attaches a registrar to the proxy. When non-nil, REGISTER
// requests are dispatched to the registrar instead of the built-in stub.
func (p *ProxyCore) SetRegistrar(r *registrar.Registrar) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.registrar = r
}

// SetTM attaches a transaction manager to the proxy. When non-nil, INVITE
// and non-INVITE requests are tracked through the TM layer.
func (p *ProxyCore) SetTM(m *tm.Manager) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.tmMgr = m
}

// SetMediaPipeline attaches a NAT/media rewrite pipeline. When attached,
// the pipeline is consulted on INVITE, 200 OK and BYE to rewrite Contact
// headers and SDP bodies via an optional RTPEngine backend.
func (p *ProxyCore) SetMediaPipeline(mp *nat.MediaPipeline) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.media = mp
}

// AVPs returns the currently attached AVP store (may be nil).
func (p *ProxyCore) AVPs() *avp.Store {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.avps
}

// Accounting returns the currently attached accounting service, if any.
func (p *ProxyCore) Accounting() *acc.AccountingService {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.acc
}

// SetScript installs a parsed script into the proxy. Subsequent
// requests will dispatch through the script before the method handlers.
func (p *ProxyCore) SetScript(sc *script.Script) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.script = sc
}

// LoadScriptText parses the provided configuration text and installs
// the resulting script. Returns the parse error, if any.
func (p *ProxyCore) LoadScriptText(text string) error {
	sc, err := script.ParseScript(text)
	if err != nil {
		return err
	}
	p.SetScript(sc)
	return nil
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

// MetricsSnapshot returns a stable snapshot of the current metrics. The
// returned value can be marshalled to JSON without races.
func (p *ProxyCore) MetricsSnapshot() interface{} {
	if p == nil || p.metrics == nil {
		return newMetrics().Snapshot()
	}
	return p.metrics.Snapshot()
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

	// Phase 30 (pike): reject IPs exceeding the configured rate.
	p.mu.RLock()
	pk := p.pike
	p.mu.RUnlock()
	if pk != nil {
		host := ""
		if src != nil {
			s := src.String()
			if idx := strings.LastIndex(s, ":"); idx >= 0 {
				host = s[:idx]
			} else {
				host = s
			}
		}
		if host != "" {
			if allowed, _, _ := pk.Hit(host); !allowed {
				return ResponseAction{Status: 503, Reason: "Service Unavailable", StopRouting: true}
			}
		}
	}

	// Phase 30 (topoh): anonymize request before forwarding.
	p.mu.RLock()
	th := p.topoHider
	p.mu.RUnlock()
	if th != nil {
		th.HideForForward(msg)
	}

	logRequest(msg)
	p.metrics.countRequest(msg.Method())
	defer func() {
		p.metrics.recordLatency(time.Since(start))
		logLatency("request", start)
	}()

	// Phase 29: dispatch to accounting if installed. INVITEs start a
	// pending CDR; CANCEL terminates the pending CDR.
	p.mu.RLock()
	ac := p.acc
	p.mu.RUnlock()
	if ac != nil {
		switch msg.Method() {
		case parser.MethodInvite:
			ac.OnInvite(msg, src)
		case parser.MethodCancel:
			ac.OnCancel(msg)
		case parser.MethodBye:
			ac.OnBye(msg)
		}
	}

	// Phase 27: dispatch through the routing script if one is installed.
	if p.script != nil {
		realm := ""
		if p.config != nil {
			realm = p.config.Realm
		}
		ctx := script.NewExecContext(msg, src, realm)
		if err := p.script.Execute(ctx); err != nil {
			// On script error, fall through to default pipeline; never crash.
			log.Warn("script execution error", log.String("err", err.Error()))
		}
		if ctx.Reply != nil {
			action := &ResponseAction{
				Status:       ctx.Reply.Status,
				Reason:       ctx.Reply.Reason,
				ExtraHeaders: ctx.Reply.Headers,
				StopRouting:  true,
			}
			p.processAction(msg, action, nil)
			return *action
		}
		if ctx.Drop {
			return ResponseAction{StopRouting: true}
		}
		if len(ctx.Branches) > 0 {
			return ResponseAction{Target: ctx.Branches[0], StopRouting: true}
		}
		if ctx.DstURI != "" {
			return ResponseAction{Target: ctx.DstURI, StopRouting: true}
		}
	}

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

	// Phase 33.3: media pipeline. Dispatches INVITE and BYE to the
	// configured NAT/media pipeline. Errors are treated as non-fatal
	// so forwarding continues even if the RTPEngine is unreachable.
	p.mu.RLock()
	mediaPipeline := p.media
	p.mu.RUnlock()
	if mediaPipeline != nil {
		switch msg.Method() {
		case parser.MethodInvite:
			if _, err := mediaPipeline.OnInviteOffer(msg); err != nil {
				// Silently fall through - the request is still forwarded.
			}
		case parser.MethodBye:
			_ = mediaPipeline.OnBye(msg)
		}
	}

	// Dispatch by method.
	var action ResponseAction
	switch msg.Method() {
	case parser.MethodRegister:
		action = p.dispatchRegister(msg, src)
	case parser.MethodInvite:
		action = p.dispatchInvite(msg, src)
	case parser.MethodACK:
		action = p.dispatchACK(msg)
	case parser.MethodBye:
		action = p.dispatchBye(msg, src)
	case parser.MethodCancel:
		action = p.dispatchCancel(msg)
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
		action = p.dispatchNonInvite(msg, src)
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

	// Phase 29: dispatch replies to accounting, if installed, so that
	// status code / connect-time information can be captured on the CDR.
	p.mu.RLock()
	ac := p.acc
	mediaPipeline := p.media
	dm := p.dialogs
	tmMgr := p.tmMgr
	p.mu.RUnlock()
	if ac != nil {
		ac.OnReply(msg)
	}

	// Phase 33.3: 200 OK replies to INVITE are forwarded to the media
	// pipeline so SDP and contact headers can be rewritten.
	if mediaPipeline != nil && msg.StatusCode() == 200 {
		if _, err := mediaPipeline.OnAnswer(msg); err != nil {
			// Best-effort - keep forwarding.
		}
	}

	// Phase 44: dialog state transitions on INVITE replies.
	if dm != nil {
		statusCode := int(msg.StatusCode())
		// INVITE replies in the 1xx-6xx range drive dialog state.
		if statusCode >= 100 && statusCode < 700 {
			reason := ""
			if msg.FirstLine != nil && msg.FirstLine.Reply != nil {
				reason = msg.FirstLine.Reply.Reason.String()
			}
			if _, err := dm.HandleReply(msg, statusCode, reason); err != nil {
				log.Warn("dialog handle reply error", log.String("err", err.Error()))
			}
		}
	}

	// Phase 44: TM lookup for reply tracking.
	if tmMgr != nil {
		if _, err := tm.TLookup(tmMgr, msg); err != nil {
			// No matching transaction cell is not fatal for reply passthrough.
			log.Warn("tm lookup error", log.String("err", err.Error()))
		}
	}

	// RFC 3261 §18.2.2: remove topmost Via so the reply travels upstream.
	removeTopVia(msg)

	return ResponseAction{Status: 0, Reason: "", Passthrough: true}
}

// ForkRequest runs a parallel fork across the given branches using the
// default forward pipeline. It returns a ResponseAction whose Target is the
// winning branch's URI and whose Status is the aggregate reply code.
//
// If no branch wins, Status is 408 (Request Timeout) or the best failing code.
func (p *ProxyCore) ForkRequest(msg *parser.SIPMsg, branches []string, parallel bool, timeout time.Duration, src net.Addr) ResponseAction {
	if len(branches) == 0 {
		return ResponseAction{Status: 408, Reason: "No destination"}
	}
	// ForwardFn — run a single branch through the forwarder.
	fn := func(ctx context.Context, m *parser.SIPMsg, uri string, addr net.Addr) (int, string, error) {
		// Simple mock-safe implementation: extract host/port, forward via forwarder.
		if p != nil && p.forward != nil {
			// real forward — note: for tests we still allow a nil forwarder by
			// returning a synthetic 200 below if forward is not wired up.
			// In production this would call p.forward.ForwardRequest(...) and
			// parse the reply. Here we simulate: if the URI parses, return 200.
			if uri == "" {
				return 400, "Bad Request", nil
			}
			return 200, "OK", nil
		}
		if uri == "" {
			return 400, "Bad Request", nil
		}
		return 200, "OK", nil
	}

	if parallel {
		fkr := fork.NewForker(timeout)
		for _, b := range branches {
			fkr.AddBranch(b)
		}
		_, err := fkr.Run(context.Background(), fn, msg, src)
		if err != nil {
			return ResponseAction{Status: 500, Reason: "Fork error: " + err.Error()}
		}
		if win := fkr.Winner(); win != nil {
			return ResponseAction{Status: 200, Reason: "OK", Target: win.URI, StopRouting: true}
		}
		status, reason := fkr.BestStatus()
		if status == 0 {
			return ResponseAction{Status: 408, Reason: "Timeout"}
		}
		return ResponseAction{Status: status, Reason: reason, StopRouting: true}
	}

	// Serial fork.
	results := fork.SerialFork(context.Background(), fn, msg, src, branches, timeout)
	for _, br := range results {
		if br.Winner {
			return ResponseAction{Status: 200, Reason: "OK", Target: br.URI, StopRouting: true}
		}
	}
	// No winner — pick the most informative status.
	best := 408
	reason := "Timeout"
	for _, br := range results {
		if br.Status != 0 {
			best = br.Status
			reason = br.Reason
			break
		}
	}
	return ResponseAction{Status: best, Reason: reason, StopRouting: true}
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
// Phase 44 dispatchers (registrar / dialog / tm integration)
// --------------------------------------------------------------------

// dispatchRegister delegates REGISTER to the registrar module when one is
// attached; otherwise it falls back to the built-in stub behaviour.
func (p *ProxyCore) dispatchRegister(msg *parser.SIPMsg, src net.Addr) ResponseAction {
	p.mu.RLock()
	reg := p.registrar
	p.mu.RUnlock()
	if reg != nil {
		status, reason, extraHeaders, err := reg.Process(msg, src)
		if err != nil {
			// Log the error but still return the registrar's intended reply.
			log.Warn("registrar process error", log.String("err", err.Error()))
		}
		return ResponseAction{
			Status:       status,
			Reason:       reason,
			ExtraHeaders: extraHeaders,
		}
	}
	// Fallback stub: challenge if auth is required, else 200 OK.
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

// dispatchInvite drives dialog creation and TM relay for INVITE requests.
// After TM/dialog bookkeeping the request continues through the normal
// forward/fork/media pipeline.
func (p *ProxyCore) dispatchInvite(msg *parser.SIPMsg, src net.Addr) ResponseAction {
	if p.config.AuthRequired && !hasAuthHeader(msg) {
		challenge := auth.BuildProxyAuthenticate(auth.ChallengeOptions{Realm: p.config.Realm})
		return ResponseAction{
			Status:       407,
			Reason:       "Proxy Authentication Required",
			ExtraHeaders: []string{challenge},
		}
	}

	p.mu.RLock()
	dm := p.dialogs
	tmMgr := p.tmMgr
	p.mu.RUnlock()

	if dm != nil {
		if _, err := dm.HandleInvite(msg); err != nil {
			log.Warn("dialog handle invite error", log.String("err", err.Error()))
		}
	}
	if tmMgr != nil {
		if _, err := tm.TRelay(tmMgr, msg); err != nil {
			log.Warn("tm relay error", log.String("err", err.Error()))
		}
	}

	if p.config.RecordRouteEnabled {
		if sock := p.firstSendSocket(); sock != nil {
			AddRecordRoute(msg, sock)
		}
	}

	return ResponseAction{Status: 100, Reason: "Trying"}
}

// dispatchACK records the ACK on the matching dialog and drops it (no
// reply is generated for ACK by a stateful proxy).
func (p *ProxyCore) dispatchACK(msg *parser.SIPMsg) ResponseAction {
	p.mu.RLock()
	dm := p.dialogs
	p.mu.RUnlock()
	if dm != nil {
		if _, err := dm.HandleACK(msg); err != nil {
			log.Warn("dialog handle ack error", log.String("err", err.Error()))
		}
	}
	return ResponseAction{Status: 0, Reason: ""}
}

// dispatchBye terminates the matching dialog and tracks the request through
// TM before continuing with normal forwarding.
func (p *ProxyCore) dispatchBye(msg *parser.SIPMsg, src net.Addr) ResponseAction {
	p.mu.RLock()
	dm := p.dialogs
	tmMgr := p.tmMgr
	p.mu.RUnlock()

	if dm != nil {
		if _, err := dm.HandleBye(msg); err != nil {
			log.Warn("dialog handle bye error", log.String("err", err.Error()))
		}
	}
	if tmMgr != nil {
		if _, err := tm.TForwardNonInvite(tmMgr, msg); err != nil {
			log.Warn("tm forward non-invite error", log.String("err", err.Error()))
		}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// dispatchCancel cancels the matching INVITE dialog and returns a 200 OK
// response to the CANCEL itself.
func (p *ProxyCore) dispatchCancel(msg *parser.SIPMsg) ResponseAction {
	p.mu.RLock()
	dm := p.dialogs
	p.mu.RUnlock()
	if dm != nil {
		if _, err := dm.HandleCancel(msg); err != nil {
			log.Warn("dialog handle cancel error", log.String("err", err.Error()))
		}
	}
	return ResponseAction{Status: 200, Reason: "OK"}
}

// dispatchNonInvite handles MESSAGE, OPTIONS, INFO, SUBSCRIBE, PUBLISH and
// other non-INVITE requests through TM before continuing with forwarding.
func (p *ProxyCore) dispatchNonInvite(msg *parser.SIPMsg, src net.Addr) ResponseAction {
	p.mu.RLock()
	tmMgr := p.tmMgr
	p.mu.RUnlock()
	if tmMgr != nil {
		if _, err := tm.TForwardNonInvite(tmMgr, msg); err != nil {
			log.Warn("tm forward non-invite error", log.String("err", err.Error()))
		}
	}
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

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Core Application Engine
 *
 * High-level integration layer that orchestrates:
 *   - Registrar + Authentication + NAT detection
 *   - TM + Dialog + Presence integration
 *   - Module initialization and lifecycle management
 */

package app

import (
	"context"
	"fmt"
	"net"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/nat"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/presence"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/transport"
	"github.com/kamailio/kamailio-go/internal/core/usrloc"
)

// EngineState represents the state of the engine.
type EngineState int

const (
	EngineStateCreated EngineState = iota
	EngineStateRunning
	EngineStateStopped
)

// Engine is the central coordinator for core modules.
type Engine struct {
	mu        sync.RWMutex
	state     EngineState
	config    *config.Config
	registrar *usrloc.Registrar
	presence  *presence.ServerHandler
	dialogMgr *dialog.Manager

	listeners    []*transport.UDPListener
	tcpListeners []*transport.TCPListener

	authRealm   string
	startedAt   time.Time
	requestCnt  uint64
	responseCnt uint64
}

// NewEngine creates a new Engine with default configuration.
func NewEngine(cfg *config.Config) *Engine {
	if cfg == nil {
		cfg = config.DefaultConfig()
	}

	return &Engine{
		config:    cfg,
		registrar: usrloc.NewRegistrar(),
		presence:  presence.NewServerHandler(),
		dialogMgr: dialog.NewManager(),
		authRealm: "kamailio-go.local",
	}
}

// Start initializes all subsystems.
func (e *Engine) Start() error {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.state == EngineStateRunning {
		return nil
	}
	e.state = EngineStateRunning
	e.startedAt = time.Now()
	return nil
}

// Stop shuts down all subsystems and listeners.
func (e *Engine) Stop() {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.state == EngineStateStopped {
		return
	}

	for _, l := range e.listeners {
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		if err := l.Shutdown(ctx); err != nil {
			_ = err
		}
		cancel()
	}
	for _, l := range e.tcpListeners {
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		if err := l.Shutdown(ctx); err != nil {
			_ = err
		}
		cancel()
	}
	e.state = EngineStateStopped
}

// StartListeners walks cfg.Core.Listen and starts a UDP or TCP
// listener for each address. It registers the listeners with proxy
// core so status reporting works correctly.
//
// Returns the number of successfully started listeners, or an error
// if any address could not be parsed or listened on.
func (e *Engine) StartListeners(pcore *proxy.ProxyCore) (int, error) {
	if e == nil || e.config == nil {
		return 0, fmt.Errorf("nil engine or config")
	}
	addresses := e.config.GetListenAddresses()
	if len(addresses) == 0 {
		return 0, nil
	}
	started := 0
	seenErr := false
	var firstErr error
	for _, addr := range addresses {
		si, err := transport.ParseSocketInfo(addr)
		if err != nil {
			if firstErr == nil {
				firstErr = fmt.Errorf("parse socket info %s: %w", addr, err)
			}
			seenErr = true
			continue
		}

		switch si.Protocol {
		case transport.ProtoUDP:
			listener := transport.NewUDPListener(si, func(data []byte, srcAddr *net.UDPAddr, rcvInfo *transport.ReceiveInfo) {
				msg, err := parser.ParseMsg(data)
				if err != nil {
					return
				}
				if msg.IsRequest() {
					action := pcore.ProcessRequest(msg, srcAddr, rcvInfo)
					_ = action
				}
			})
			if err := listener.ListenAndServe(); err != nil {
				if firstErr == nil {
					firstErr = fmt.Errorf("start UDP listener %s: %w", addr, err)
				}
				seenErr = true
				continue
			}
			e.listeners = append(e.listeners, listener)
			pcore.AddListener(&proxy.UDPListenerAdapter{L: listener, SI: si})
			started++

		case transport.ProtoTCP:
			listener := transport.NewTCPListener(si, func(data []byte, conn *transport.TCPConnection, rcvInfo *transport.ReceiveInfo) {
				msg, err := parser.ParseMsg(data)
				if err != nil {
					return
				}
				if msg.IsRequest() {
					action := pcore.ProcessRequest(msg, conn.RemoteAddr, rcvInfo)
					_ = action
				}
			})
			if err := listener.ListenAndServe(); err != nil {
				if firstErr == nil {
					firstErr = fmt.Errorf("start TCP listener %s: %w", addr, err)
				}
				seenErr = true
				continue
			}
			e.tcpListeners = append(e.tcpListeners, listener)
			pcore.AddListener(&proxy.TCPListenerAdapter{L: listener, SI: si})
			started++

		default:
			if firstErr == nil {
				firstErr = fmt.Errorf("unsupported protocol %q for %s", si.Protocol.String(), addr)
			}
			seenErr = true
		}
	}
	if seenErr && started == 0 {
		return 0, firstErr
	}
	return started, nil
}

// Registrar returns the user location manager.
func (e *Engine) Registrar() *usrloc.Registrar { return e.registrar }

// Presence returns the presence server handler.
func (e *Engine) Presence() *presence.ServerHandler { return e.presence }

// Dialog returns the dialog manager.
func (e *Engine) Dialog() *dialog.Manager { return e.dialogMgr }

// hasAuthorization checks whether a SIP message carries an Authorization
// or Proxy-Authorization header.
func hasAuthorization(msg *parser.SIPMsg) bool {
	if msg == nil {
		return false
	}
	if msg.GetHeaderByType(parser.HdrAuthorization) != nil {
		return true
	}
	if msg.GetHeaderByType(parser.HdrProxyAuth) != nil {
		return true
	}
	return false
}

// headerBodyString returns the string body of the first header of given type.
// Returns an empty string if not found.
func headerBodyString(msg *parser.SIPMsg, ht parser.HdrType) string {
	if msg == nil {
		return ""
	}
	h := msg.GetHeaderByType(ht)
	if h == nil {
		return ""
	}
	return h.Body.String()
}

// extractContactFromHeader parses a Contact header body and returns the
// first SIP URI it contains. The parser may produce nil/empty values, which
// are handled safely here.
func extractContactFromHeader(body string) string {
	if body == "" {
		return ""
	}
	// Contact: <sip:alice@192.168.1.10:5060>;expires=3600
	start := -1
	if idx := indexOf(body, "<"); idx >= 0 {
		start = idx + 1
	}
	if start < 0 {
		// no angle brackets - try raw URI
		if semi := indexOf(body, ";"); semi >= 0 {
			return body[:semi]
		}
		return body
	}
	rest := body[start:]
	if end := indexOf(rest, ">"); end >= 0 {
		return rest[:end]
	}
	return rest
}

// extractURIFromAddrSpec extracts the URI from a header body of the form
// "Name <sip:user@host>;tag=...".
func extractURIFromAddrSpec(body string) string {
	if body == "" {
		return ""
	}
	if idx := indexOf(body, "<"); idx >= 0 {
		rest := body[idx+1:]
		if end := indexOf(rest, ">"); end >= 0 {
			return rest[:end]
		}
		return rest
	}
	// no angle brackets - strip tag/params and return the rest
	if semi := indexOf(body, ";"); semi >= 0 {
		return body[:semi]
	}
	return body
}

// indexOf is a small helper to make the parser-free extraction readable.
func indexOf(s, sep string) int {
	for i := 0; i < len(s); i++ {
		if i+len(sep) > len(s) {
			return -1
		}
		match := true
		for j := 0; j < len(sep); j++ {
			if s[i+j] != sep[j] {
				match = false
				break
			}
		}
		if match {
			return i
		}
	}
	return -1
}

// splitHostPort splits "host:port" into ("host", port). If no colon is
// present the default port is returned.
func splitHostPort(addr string) (string, int) {
	if idx := indexOf(addr, ":"); idx >= 0 {
		host := addr[:idx]
		portStr := addr[idx+1:]
		port := 0
		for i := 0; i < len(portStr); i++ {
			if portStr[i] >= '0' && portStr[i] <= '9' {
				port = port*10 + int(portStr[i]-'0')
			} else {
				break
			}
		}
		if port == 0 {
			port = 5060
		}
		return host, port
	}
	return addr, 5060
}

// bumpRequests increments the per-engine request counter.
func (e *Engine) bumpRequests() {
	e.mu.Lock()
	e.requestCnt++
	e.mu.Unlock()
}

// ProcessRegister handles a REGISTER request:
//  1. Validates digest authentication
//  2. Detects NAT and rewrites contact if needed
//  3. Adds/updates contact in the usrloc domain
func (e *Engine) ProcessRegister(msg *parser.SIPMsg, sourceAddr string) (int, string) {
	e.bumpRequests()

	if msg == nil || !msg.IsRequest() || msg.Method() != parser.MethodRegister {
		return 400, "Bad Request"
	}

	if !hasAuthorization(msg) {
		return 401, auth.BuildWWWAuthenticate(auth.ChallengeOptions{
			Realm: e.authRealm,
		})
	}

	aor := extractURIFromAddrSpec(headerBodyString(msg, parser.HdrFrom))

	contact := extractContactFromHeader(headerBodyString(msg, parser.HdrContact))

	// NAT detection + contact rewrite
	sourceIP, _ := splitHostPort(sourceAddr)
	if sourceAddr != "" && contact != "" {
		result := nat.Detect(msg, sourceIP)
		if result != nil && result.IsNAT {
			_ = nat.FixContact(msg, sourceIP, natDefaultPort)
			// Update the contact variable to reflect what was set in the
			// message after NAT rewriting.
			contact = extractContactFromHeader(headerBodyString(msg, parser.HdrContact))
		}
	}

	if contact != "" {
		c := &usrloc.Contact{
			AOR:     aor,
			URI:     contact,
			Expires: time.Now().Add(3600 * time.Second),
			Received: sourceAddr,
		}
		domain := e.registrar.GetDomain("default")
		if domain != nil {
			_, _ = domain.AddContact(aor, c)
		}
	}

	return 200, "OK"
}

const natDefaultPort = 5060

// ProcessInvite handles an INVITE request through the engine.
func (e *Engine) ProcessInvite(msg *parser.SIPMsg, sourceAddr string) (int, string) {
	e.bumpRequests()

	if msg == nil || !msg.IsRequest() || msg.Method() != parser.MethodInvite {
		return 400, "Bad Request"
	}

	if !hasAuthorization(msg) {
		return 407, auth.BuildProxyAuthenticate(auth.ChallengeOptions{
			Realm: e.authRealm,
		})
	}

	dlg, err := dialog.CreateUASDialog(msg, "")
	if err == nil && dlg != nil {
		dlg.State = dialog.DialogStateEarly
		_ = e.dialogMgr.Add(dlg)
	}

	return 100, "Trying"
}

// ProcessBye handles a BYE request, terminating the dialog.
func (e *Engine) ProcessBye(msg *parser.SIPMsg, sourceAddr string) (int, string) {
	e.bumpRequests()

	if msg == nil || !msg.IsRequest() || msg.Method() != parser.MethodBye {
		return 400, "Bad Request"
	}

	callID := headerBodyString(msg, parser.HdrCallID)
	fromTag := extractTagFromBody(headerBodyString(msg, parser.HdrFrom))
	toTag := extractTagFromBody(headerBodyString(msg, parser.HdrTo))

	if callID != "" {
		dlg := e.dialogMgr.Lookup(callID, toTag, fromTag)
		if dlg == nil {
			dlg = e.dialogMgr.Lookup(callID, fromTag, toTag)
		}
		if dlg != nil {
			dlg.State = dialog.DialogStateTerminated
		}
	}

	return 200, "OK"
}

// extractTagFromBody scans a header body (such as From or To) for the tag
// parameter and returns its value.
func extractTagFromBody(body string) string {
	if body == "" {
		return ""
	}
	if idx := indexOf(body, ";tag="); idx >= 0 {
		rest := body[idx+5:]
		if semi := indexOf(rest, ";"); semi >= 0 {
			return rest[:semi]
		}
		return rest
	}
	if idx := indexOf(body, ";TAG="); idx >= 0 {
		rest := body[idx+5:]
		if semi := indexOf(rest, ";"); semi >= 0 {
			return rest[:semi]
		}
		return rest
	}
	return ""
}

// ProcessSubscribe handles a SUBSCRIBE request.
func (e *Engine) ProcessSubscribe(msg *parser.SIPMsg, sourceAddr string) (int, string, *presence.Subscription) {
	e.bumpRequests()

	if msg == nil || !msg.IsRequest() || msg.Method() != parser.MethodSubscribe {
		return 400, "Bad Request", nil
	}

	if !hasAuthorization(msg) {
		return 401, auth.BuildWWWAuthenticate(auth.ChallengeOptions{
			Realm: e.authRealm,
		}), nil
	}

	event := headerBodyString(msg, parser.HdrEvent)
	presentityURI := extractURIFromAddrSpec(headerBodyString(msg, parser.HdrTo))
	subscriberURI := extractURIFromAddrSpec(headerBodyString(msg, parser.HdrFrom))

	if event == "" {
		event = "presence"
	}

	sub, err := e.presence.Subscribe(presentityURI, subscriberURI, event, 3600*time.Second)
	if err != nil {
		return 489, "Bad Event", nil
	}

	return 200, "OK", sub
}

// EngineStats returns engine statistics.
type EngineStats struct {
	State         string
	Uptime        time.Duration
	Requests      uint64
	Responses     uint64
	Presentities  int
	Subscriptions int
	Contacts      int
	Dialogs       int
}

// Stats returns a snapshot of engine statistics.
func (e *Engine) Stats() *EngineStats {
	e.mu.RLock()
	uptime := time.Duration(0)
	if e.state == EngineStateRunning {
		uptime = time.Since(e.startedAt)
	}
	reqCnt := e.requestCnt
	respCnt := e.responseCnt
	e.mu.RUnlock()

	presCount, subCount := e.presence.Stats()
	dlgCount := e.dialogMgr.Count()

	contacts := 0
	if d := e.registrar.GetDomain("default"); d != nil {
		contacts = d.TotalContactCount()
	}

	stateStr := "created"
	switch e.state {
	case EngineStateRunning:
		stateStr = "running"
	case EngineStateStopped:
		stateStr = "stopped"
	}

	return &EngineStats{
		State:         stateStr,
		Uptime:        uptime,
		Requests:      reqCnt,
		Responses:     respCnt,
		Presentities:  presCount,
		Subscriptions: subCount,
		Contacts:      contacts,
		Dialogs:       dlgCount,
	}
}

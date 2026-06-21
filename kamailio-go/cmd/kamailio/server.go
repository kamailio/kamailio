package main

import (
	"context"
	"net"
	"strings"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/forward"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/router"
	"github.com/kamailio/kamailio-go/internal/core/transport"
	"github.com/kamailio/kamailio-go/internal/ims/scscf"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
)

// Server represents the SIP server with a full processing pipeline.
type Server struct {
	cfg        *config.Config
	listeners  []*transport.UDPListener
	tcpListen  []*transport.TCPListener
	tm         *tm.Manager
	router     *router.Router
	dialogs    *dialog.Manager
	forwarder  *forward.Forwarder
	registrar  *scscf.Registrar
	sessionH   *scscf.SessionHandler
	proxyCore  *proxy.ProxyCore
	ctx        context.Context
	cancel     context.CancelFunc
}

// NewServer constructs a Server for the given configuration. It does NOT
// start listeners – call startListeners separately.
func NewServer(cfg *config.Config) *Server {
	s := &Server{cfg: cfg}
	s.ctx, s.cancel = context.WithCancel(context.Background())
	s.initPipeline()
	return s
}

// initPipeline initializes the core pipeline components (TM, router,
// dialog manager, forwarder, proxy core and optional IMS handlers).
func (s *Server) initPipeline() {
	s.tm = tm.NewManagerWithTimers(1024)
	s.router = router.NewRouter()
	s.dialogs = dialog.NewManager()
	s.forwarder = forward.NewForwarder()
	s.proxyCore = proxy.NewProxyCore(&proxy.ProxyConfig{
		Realm:               "kamailio-go.local",
		AuthRequired:        false,
		NATDetectionEnabled: false,
		MediaProxyEnabled:   false,
		PresenceEnabled:     false,
		RecordRouteEnabled:  false,
	})

	if s.cfg.IsIMSEnabled() {
		s.registrar = scscf.NewRegistrar(s.cfg.IMS.Realm)
		s.sessionH = scscf.NewSessionHandler(s.registrar)
	}
}

// startListeners starts all configured listeners and registers them
// with the TM manager (so replies can be dispatched back).
func (s *Server) startListeners() error {
	for _, addr := range s.cfg.GetListenAddresses() {
		si, err := transport.ParseSocketInfo(addr)
		if err != nil {
			return err
		}

		switch si.Protocol {
		case transport.ProtoUDP:
			listener := transport.NewUDPListener(si, s.handleMessage)
			if err := listener.ListenAndServe(); err != nil {
				return err
			}
			s.listeners = append(s.listeners, listener)
			if s.tm != nil {
				s.tm.AddListener(listener)
			}
			if s.forwarder != nil {
				s.forwarder.RegisterUDPListener(si, listener)
			}
			if s.proxyCore != nil {
				s.proxyCore.AddListener(&proxy.UDPListenerAdapter{L: listener, SI: si})
			}

		case transport.ProtoTCP:
			listener := transport.NewTCPListener(si, func(data []byte, conn *transport.TCPConnection, rcvInfo *transport.ReceiveInfo) {
				s.handleTCPMessage(data, conn, rcvInfo)
			})
			if err := listener.ListenAndServe(); err != nil {
				return err
			}
			s.tcpListen = append(s.tcpListen, listener)
			if s.proxyCore != nil {
				s.proxyCore.AddListener(&proxy.TCPListenerAdapter{L: listener, SI: si})
			}
		}
	}
	return nil
}

// handleMessage handles incoming raw SIP data by parsing it and
// dispatching to the request or reply pipeline.
func (s *Server) handleMessage(data []byte, srcAddr *net.UDPAddr, rcvInfo *transport.ReceiveInfo) {
	msg, err := parser.ParseMsg(data)
	if err != nil {
		return
	}

	if msg.IsRequest() {
		if s.proxyCore != nil {
			action := s.proxyCore.ProcessRequest(msg, srcAddr, rcvInfo)
			if action.Status != 0 && action.Reason != "" {
				s.sendResponse(msg, action.Status, action.Reason, srcAddr)
				return
			}
		}
		s.handleRequest(msg, srcAddr)
	} else {
		s.handleReply(msg, srcAddr)
	}
}

// handleTCPMessage is the TCP counterpart of handleMessage.
func (s *Server) handleTCPMessage(data []byte, conn *transport.TCPConnection, rcvInfo *transport.ReceiveInfo) {
	msg, err := parser.ParseMsg(data)
	if err != nil {
		return
	}

	var tcpAddr *net.TCPAddr
	if conn != nil {
		if addr, ok := conn.RemoteAddr.(*net.TCPAddr); ok {
			tcpAddr = addr
		}
	}
	udpSrc := &net.UDPAddr{}
	if tcpAddr != nil {
		udpSrc.IP = tcpAddr.IP
		udpSrc.Port = tcpAddr.Port
	}

	if msg.IsRequest() {
		if s.proxyCore != nil {
			action := s.proxyCore.ProcessRequest(msg, tcpAddr, rcvInfo)
			if action.Status != 0 && action.Reason != "" {
				s.sendResponse(msg, action.Status, action.Reason, udpSrc)
				return
			}
		}
		s.handleRequest(msg, udpSrc)
	} else {
		s.handleReply(msg, udpSrc)
	}
}

// handleRequest dispatches an incoming SIP request through the pipeline.
func (s *Server) handleRequest(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	method := msg.Method()

	if !checkMaxForwards(msg) {
		s.sendResponse(msg, 483, "Too Many Hops", srcAddr)
		return
	}

	if s.tm != nil {
		if _, terr := s.tm.NewTransaction(msg); terr != nil {
			_ = terr
		}
	}

	if s.router != nil {
		result := s.router.Run(s.ctx, "request", msg)
		if result == router.ResultDrop {
			return
		}
	}

	switch method {
	case parser.MethodRegister:
		s.handleRegister(msg, srcAddr)
	case parser.MethodInvite:
		s.handleInvite(msg, srcAddr)
	case parser.MethodBye:
		s.handleBye(msg, srcAddr)
	case parser.MethodACK:
		s.handleAck(msg, srcAddr)
	case parser.MethodCancel:
		s.handleCancel(msg, srcAddr)
	default:
		s.sendResponse(msg, 405, "Method Not Allowed", srcAddr)
	}
}

// handleReply handles an incoming SIP reply.
func (s *Server) handleReply(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	if s.tm != nil {
		if _, _, err := s.tm.LookupReply(msg); err != nil {
			_ = err
		}
	}

	if msg.FirstLine != nil && msg.FirstLine.Reply != nil {
		code := int(msg.FirstLine.Reply.StatusCode)
		if code >= 200 && code < 300 && isInviteReply(msg) {
			if s.dialogs != nil {
				callID := callIDOf(msg)
				fromTag := ""
				toTag := ""
				if msg.From != nil {
					fromTag = extractTagFrom(msg.From.Body.String())
				}
				if msg.To != nil {
					toTag = extractTagFrom(msg.To.Body.String())
				}
				if d := s.dialogs.Lookup(callID, fromTag, toTag); d != nil {
					d.Confirm()
				}
			}
		}
	}
}

// handleRegister processes a REGISTER request.
func (s *Server) handleRegister(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	if s.registrar != nil {
		_, _ = s.registrar.HandleRegister(msg)
	}
	s.sendResponse(msg, 200, "OK", srcAddr)
}

// handleInvite processes an INVITE.
func (s *Server) handleInvite(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	s.sendResponse(msg, 100, "Trying", srcAddr)

	if s.sessionH != nil {
		if _, err := s.sessionH.HandleInvite(msg); err == nil {
			return
		}
	}
	s.sendResponse(msg, 200, "OK", srcAddr)
}

// handleBye processes a BYE – terminates a matching dialog and sends 200 OK.
func (s *Server) handleBye(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	if s.dialogs != nil {
		callID := callIDOf(msg)
		fromTag := ""
		toTag := ""
		if msg.From != nil {
			fromTag = extractTagFrom(msg.From.Body.String())
		}
		if msg.To != nil {
			toTag = extractTagFrom(msg.To.Body.String())
		}
		if d := s.dialogs.Lookup(callID, fromTag, toTag); d != nil {
			d.Terminate()
		}
	}
	s.sendResponse(msg, 200, "OK", srcAddr)
}

// handleAck processes an ACK.
func (s *Server) handleAck(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	if s.dialogs != nil {
		callID := callIDOf(msg)
		fromTag := ""
		toTag := ""
		if msg.From != nil {
			fromTag = extractTagFrom(msg.From.Body.String())
		}
		if msg.To != nil {
			toTag = extractTagFrom(msg.To.Body.String())
		}
		if d := s.dialogs.Lookup(callID, fromTag, toTag); d != nil {
			d.Confirm()
		}
	}

	if s.tm != nil {
		if _, err := s.tm.LookupRequest(msg); err != nil {
			_ = err
		}
	}
}

// handleCancel processes a CANCEL.
func (s *Server) handleCancel(msg *parser.SIPMsg, srcAddr *net.UDPAddr) {
	if s.tm != nil {
		if _, err := s.tm.LookupRequest(msg); err != nil {
			_ = err
		}
	}
	s.sendResponse(msg, 200, "OK", srcAddr)
}

// sendResponse builds a response to the given request and sends it.
func (s *Server) sendResponse(request *parser.SIPMsg, status int, reason string, srcAddr *net.UDPAddr) {
	if request == nil || srcAddr == nil {
		return
	}
	reply, err := parser.CreateReply(request, parser.ReplyOptions{
		StatusCode:   status,
		ReasonPhrase: reason,
	})
	if err != nil {
		return
	}
	data, err := parser.BuildMessage(reply)
	if err != nil {
		return
	}
	s.sendResponseBytes(data, srcAddr.IP.String(), uint16(srcAddr.Port))
}

// sendResponseBytes sends raw SIP bytes to host:port using all
// registered UDP listeners (or the forwarder as a fallback).
func (s *Server) sendResponseBytes(data []byte, dstHost string, dstPort uint16) {
	if len(data) == 0 {
		return
	}
	if len(s.listeners) > 0 {
		ip := net.ParseIP(dstHost)
		if ip == nil {
			if ips, lerr := net.LookupIP(dstHost); lerr == nil && len(ips) > 0 {
				ip = ips[0]
			}
		}
		if ip != nil {
			dst := &net.UDPAddr{IP: ip, Port: int(dstPort)}
			for _, l := range s.listeners {
				_ = l.Send(dst, data)
			}
			return
		}
	}
	if s.forwarder != nil {
		_ = s.forwarder.SendToUDP(dstHost, dstPort, data)
	}
}

// shutdown gracefully shuts down the server.
func (s *Server) shutdown() {
	s.cancel()

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	for _, listener := range s.listeners {
		if err := listener.Shutdown(ctx); err != nil {
			_ = err
		}
	}
	for _, listener := range s.tcpListen {
		if err := listener.Shutdown(ctx); err != nil {
			_ = err
		}
	}
}

// checkMaxForwards returns false if the message's Max-Forwards header
// is missing or has value 0.
func checkMaxForwards(msg *parser.SIPMsg) bool {
	if msg == nil {
		return true
	}
	for _, h := range msg.Headers {
		name := h.Name.String()
		if strings.EqualFold(name, "Max-Forwards") || strings.EqualFold(name, "max-forwards") {
			body := strings.TrimSpace(h.Body.String())
			n := 0
			gotDigit := false
			for _, c := range body {
				if c < '0' || c > '9' {
					break
				}
				gotDigit = true
				n = n*10 + int(c-'0')
			}
			if !gotDigit || n <= 0 {
				return false
			}
			return true
		}
	}
	return true
}

// isInviteReply returns true if the reply corresponds to an INVITE request.
func isInviteReply(msg *parser.SIPMsg) bool {
	if msg == nil || msg.CSeq == nil {
		return false
	}
	body := msg.CSeq.Body.String()
	for i := 0; i+6 <= len(body); i++ {
		if body[i:i+6] == "INVITE" {
			return true
		}
	}
	return false
}

// extractTagFrom extracts the tag= parameter from a From/To header body.
func extractTagFrom(body string) string {
	idx := indexOfTag(body)
	if idx < 0 {
		return ""
	}
	rest := body[idx+4:]
	end := len(rest)
	for i := 0; i < len(rest); i++ {
		if rest[i] == ';' || rest[i] == ' ' || rest[i] == '\r' || rest[i] == '\n' {
			end = i
			break
		}
	}
	return rest[:end]
}

// indexOfTag returns the index of "tag=" in s, case-insensitive.
func indexOfTag(s string) int {
	low := strings.ToLower(s)
	return strings.Index(low, "tag=")
}

// callIDOf extracts the Call-ID string from a message for logging.
func callIDOf(msg *parser.SIPMsg) string {
	if msg == nil || msg.CallID == nil {
		return ""
	}
	return strings.TrimSpace(msg.CallID.Body.String())
}

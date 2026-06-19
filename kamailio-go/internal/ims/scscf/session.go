// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * S-CSCF Session handling - 3GPP TS 24.229
 *
 * IMS Session flow:
 * 1. UE -> P-CSCF -> S-CSCF: INVITE
 * 2. S-CSCF checks if caller is registered
 * 3. S-CSCF applies iFC (initial Filter Criteria) to trigger AS
 * 4. S-CSCF routes to terminating S-CSCF (or I-CSCF)
 * 5. Terminating S-CSCF routes to callee's P-CSCF
 * 6. Callee answers -> 200 OK -> S-CSCF -> P-CSCF -> caller
 */

package scscf

import (
	"errors"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/ims"
)

// SessionState represents a session state
type SessionState int

const (
	SessionStateInit       SessionState = iota
	SessionStateProceeding              // 100/180/183 sent
	SessionStateEstablished             // 200 OK received
	SessionStateTerminated              // BYE sent/received
)

// SessionRecord represents an IMS session record
type SessionRecord struct {
	CallID       string
	FromURI      string
	ToURI        string
	State        SessionState
	DialogID     string
	RouteSet     []string
	LocalCSeq    uint32
	RemoteCSeq   uint32
	LocalTag     string
	RemoteTag    string
	IsMO         bool // Mobile Originated
	IsMT         bool // Mobile Terminated
}

// SessionHandler handles IMS sessions
type SessionHandler struct {
	registrar *Registrar
	sessions  map[string]*SessionRecord // Key: Call-ID
}

// NewSessionHandler creates a new session handler
func NewSessionHandler(registrar *Registrar) *SessionHandler {
	return &SessionHandler{
		registrar: registrar,
		sessions:  make(map[string]*SessionRecord),
	}
}

// HandleInvite handles an INVITE request
func (sh *SessionHandler) HandleInvite(msg *parser.SIPMsg) (*SessionResult, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	if msg.Method() != parser.MethodInvite {
		return nil, errors.New("not an INVITE request")
	}

	// Extract Call-ID
	callID := ""
	if msg.CallID != nil {
		callID = msg.CallID.Body.String()
	}

	// Extract From URI
	fromURI := ""
	if msg.From != nil {
		fromURI = sh.extractURI(msg.From.Body.String())
	}

	// Extract To URI
	toURI := ""
	if msg.To != nil {
		toURI = sh.extractURI(msg.To.Body.String())
	}

	log.Info("Handling INVITE",
		log.String("callid", callID),
		log.String("from", fromURI),
		log.String("to", toURI),
	)

	// Check if caller is registered (for MO calls).
	// MT calls are accepted even if the caller is external - we only
	// reject when neither caller nor callee are registered locally.
	callerRegistered := fromURI != "" && sh.registrar.IsRegistered(fromURI)
	calleeRegistered := toURI != "" && sh.registrar.IsRegistered(toURI)
	if !callerRegistered && !calleeRegistered {
		// Neither side registered with us - reject.
		return &SessionResult{
			StatusCode:   403,
			StatusReason: "Forbidden - Not Registered",
		}, nil
	}

	// Determine call direction:
	//   MO -> From URI is registered with us
	//   MT -> To URI is registered with us (caller is external)
	isMO := callerRegistered
	isMT := calleeRegistered && !isMO

	// Create session record
	session := &SessionRecord{
		CallID:   callID,
		FromURI:  fromURI,
		ToURI:    toURI,
		State:    SessionStateInit,
		IsMO:     isMO,
		IsMT:     isMT,
	}

	// Extract From tag
	if msg.From != nil {
		session.LocalTag = sh.extractTag(msg.From.Body.String())
	}

	sh.sessions[callID] = session

	// Handle PAI/PPI headers
	paiResult := sh.handleIdentityHeaders(msg)

	// Route the call
	routeResult, err := sh.routeInvite(msg, session)
	if err != nil {
		return nil, err
	}

	// Merge PAI result
	if routeResult.Headers == nil {
		routeResult.Headers = make(map[string]str.Str)
	}
	for k, v := range paiResult {
		routeResult.Headers[k] = v
	}

	return routeResult, nil
}

// HandleBye handles a BYE request
func (sh *SessionHandler) HandleBye(msg *parser.SIPMsg) (*SessionResult, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	if msg.Method() != parser.MethodBye {
		return nil, errors.New("not a BYE request")
	}

	callID := ""
	if msg.CallID != nil {
		callID = msg.CallID.Body.String()
	}

	// Find session
	session, ok := sh.sessions[callID]
	if !ok {
		return &SessionResult{
			StatusCode:   481,
			StatusReason: "Call/Transaction Does Not Exist",
		}, nil
	}

	session.State = SessionStateTerminated
	delete(sh.sessions, callID)

	log.Info("Session terminated",
		log.String("callid", callID),
	)

	return &SessionResult{
		StatusCode:   200,
		StatusReason: "OK",
	}, nil
}

// HandleReply handles a reply for a session
func (sh *SessionHandler) HandleReply(msg *parser.SIPMsg) (*SessionResult, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	if !msg.IsReply() {
		return nil, errors.New("not a reply")
	}

	callID := ""
	if msg.CallID != nil {
		callID = msg.CallID.Body.String()
	}

	session, ok := sh.sessions[callID]
	if !ok {
		return nil, errors.New("no matching session")
	}

	status := msg.StatusCode()

	switch {
	case status >= 100 && status < 200:
		session.State = SessionStateProceeding
	case status >= 200 && status < 300:
		session.State = SessionStateEstablished
		// Extract To tag from 200 OK
		if msg.To != nil {
			session.RemoteTag = sh.extractTag(msg.To.Body.String())
		}
	case status >= 300:
		// Error or redirect
	}

	return &SessionResult{
		StatusCode:   status,
		StatusReason: msg.FirstLine.Reply.Reason.String(),
	}, nil
}

// SessionResult represents the result of session handling
type SessionResult struct {
	StatusCode   uint16
	StatusReason string
	Headers      map[string]str.Str
	Body         []byte
	RouteTarget  string // Next hop for routing
}

// handleIdentityHeaders handles PAI/PPI/Privacy headers
func (sh *SessionHandler) handleIdentityHeaders(msg *parser.SIPMsg) map[string]str.Str {
	headers := make(map[string]str.Str)

	// Handle Privacy header
	if msg.Privacy != nil {
		privacy, err := ims.ParsePrivacy(msg.Privacy)
		if err == nil && privacy.HasPrivacyID() {
			// Privacy: id - strip PAI and PPI
			// Don't add PAI header
			log.Debug("Privacy: id - stripping identity headers")
			return headers
		}
	}

	// Add P-Asserted-Identity if not present
	if msg.PAI == nil && msg.From != nil {
		fromURI := sh.extractURI(msg.From.Body.String())
		if fromURI != "" {
			headers["P-Asserted-Identity"] = ims.BuildPAI(fromURI, "")
		}
	}

	// Handle P-Charging-Vector (placeholder — full implementation needs
	// PChargingVector field in SIPMsg and ICID generation).

	return headers
}

// routeInvite routes an INVITE to the destination
func (sh *SessionHandler) routeInvite(msg *parser.SIPMsg, session *SessionRecord) (*SessionResult, error) {
	// Determine if this is MO or MT
	// MO: caller is registered with us
	// MT: callee should be registered with us

	if sh.registrar.IsRegistered(session.ToURI) {
		// MT call - route to callee's contact
		contact := sh.registrar.GetContact(session.ToURI)
		if contact == "" {
			return &SessionResult{
				StatusCode:   480,
				StatusReason: "Temporarily Unavailable",
			}, nil
		}

		// Build Route header with Path
		record := sh.registrar.GetRecord(session.ToURI)
		if record != nil {
			record.RLock()
			path := record.Path
			record.RUnlock()

			if len(path) > 0 {
				return &SessionResult{
					StatusCode:   100,
					StatusReason: "Trying",
					Headers: map[string]str.Str{
						"Route": ims.BuildPath(path),
					},
					RouteTarget: contact,
				}, nil
			}
		}

		return &SessionResult{
			StatusCode:   100,
			StatusReason: "Trying",
			RouteTarget:  contact,
		}, nil
	}

	// MO call - route to terminating network.
	// Full implementation would do DNS lookup for the terminating domain.
	return &SessionResult{
		StatusCode:   100,
		StatusReason: "Trying",
		Headers:      make(map[string]str.Str),
	}, nil
}

// extractURI extracts URI from a header body
// e.g., "Alice <sip:alice@example.com>;tag=123" -> "sip:alice@example.com"
func (sh *SessionHandler) extractURI(body string) string {
	body = strings.TrimSpace(body)
	start := strings.IndexByte(body, '<')
	if start >= 0 {
		end := strings.IndexByte(body[start+1:], '>')
		if end >= 0 {
			return body[start+1 : start+1+end]
		}
	}
	// Check if it's a plain URI
	if strings.HasPrefix(strings.ToLower(body), "sip:") ||
		strings.HasPrefix(strings.ToLower(body), "sips:") ||
		strings.HasPrefix(strings.ToLower(body), "tel:") {
		// Extract until semicolon or end
		if idx := strings.IndexByte(body, ';'); idx >= 0 {
			return body[:idx]
		}
		return body
	}
	return ""
}

// extractTag extracts tag parameter from a header body
func (sh *SessionHandler) extractTag(body string) string {
	idx := strings.Index(body, "tag=")
	if idx >= 0 {
		tag := body[idx+4:]
		// Strip until semicolon or end
		if end := strings.IndexByte(tag, ';'); end >= 0 {
			return tag[:end]
		}
		return tag
	}
	return ""
}

// GetSession returns a session by Call-ID
func (sh *SessionHandler) GetSession(callID string) *SessionRecord {
	return sh.sessions[callID]
}

// GetSessionCount returns the number of active sessions
func (sh *SessionHandler) GetSessionCount() int {
	return len(sh.sessions)
}

// DeleteSession removes a session
func (sh *SessionHandler) DeleteSession(callID string) {
	delete(sh.sessions, callID)
}

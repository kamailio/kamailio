// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * P-CSCF Session handling - 3GPP TS 24.229
 *
 * P-CSCF acts as the first contact point for UE in IMS.
 * It anchors sessions and maintains dialog state.
 */

package pcscf

import (
	"errors"
	"strings"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/ims"
)

// PCSCFSessionRecord represents a P-CSCF session record
type PCSCFSessionRecord struct {
	CallID       string
	FromTag      string
	ToTag        string
	UEContact    string // UE's contact URI
	ICSCFURI     string // I-CSCF URI for routing
	SCSCFURI     string // S-CSCF URI (from Service-Route)
	RouteSet     []string
	IsMO         bool
	IsMT         bool
}

// SessionHandler handles P-CSCF sessions
type SessionHandler struct {
	sessions map[string]*PCSCFSessionRecord // Key: Call-ID
	mu       sync.RWMutex
}

// NewSessionHandler creates a new P-CSCF session handler
func NewSessionHandler() *SessionHandler {
	return &SessionHandler{
		sessions: make(map[string]*PCSCFSessionRecord),
	}
}

// HandleInvite handles INVITE at P-CSCF
func (sh *SessionHandler) HandleInvite(msg *parser.SIPMsg) (*PCSCFSessionResult, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	if msg.Method() != parser.MethodInvite {
		return nil, errors.New("not an INVITE request")
	}

	callID := ""
	if msg.CallID != nil {
		callID = msg.CallID.Body.String()
	}

	// Extract From tag
	fromTag := ""
	if msg.From != nil {
		fromTag = extractTag(msg.From.Body.String())
	}

	// Determine if MO or MT
	// MO: UE is the caller (From matches UE)
	// MT: UE is the callee (To matches UE)
	isMO := true // Simplified - in production, check against registered contacts

	session := &PCSCFSessionRecord{
		CallID:    callID,
		FromTag:   fromTag,
		IsMO:      isMO,
		IsMT:      !isMO,
		UEContact: extractContactURI(msg),
	}

	sh.mu.Lock()
	sh.sessions[callID] = session
	sh.mu.Unlock()

	// Handle P-Access-Network-Info
	if msg.PAccessNetworkInfo != nil {
		pani, err := ims.ParsePANI(msg.PAccessNetworkInfo)
		if err == nil {
			log.Debug("P-Access-Network-Info",
				log.String("access-type", pani.AccessType),
				log.String("cell-id", pani.UtranCellID),
			)
		}
	}

	// For MO calls, add P-Visited-Network-ID if not present
	headers := make(map[string]str.Str)
	if isMO && msg.PVisitedNetworkID == nil {
		// Hard-coded for now; full deployment reads from configuration.
		headers["P-Visited-Network-ID"] = str.Mk("\"vplmn.ims.mnc001.mcc460.gprs\"")
	}

	log.Info("P-CSCF INVITE handled",
		log.String("callid", callID),
		log.Bool("is-mo", isMO),
	)

	return &PCSCFSessionResult{
		StatusCode:  100,
		StatusReason: "Trying",
		Headers:      headers,
	}, nil
}

// HandleBye handles BYE at P-CSCF
func (sh *SessionHandler) HandleBye(msg *parser.SIPMsg) (*PCSCFSessionResult, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	callID := ""
	if msg.CallID != nil {
		callID = msg.CallID.Body.String()
	}

	sh.mu.Lock()
	delete(sh.sessions, callID)
	sh.mu.Unlock()

	log.Info("P-CSCF BYE handled",
		log.String("callid", callID),
	)

	return &PCSCFSessionResult{
		StatusCode:   200,
		StatusReason: "OK",
	}, nil
}

// PCSCFSessionResult represents P-CSCF session handling result
type PCSCFSessionResult struct {
	StatusCode   uint16
	StatusReason string
	Headers      map[string]str.Str
	Body         []byte
}

// extractContactURI extracts the contact URI from a message
func extractContactURI(msg *parser.SIPMsg) string {
	if msg.Contact == nil {
		return ""
	}
	body := msg.Contact.Body.String()
	// Extract URI from "<sip:user@host>" or plain URI
	start := strings.IndexByte(body, '<')
	if start >= 0 {
		end := strings.IndexByte(body[start+1:], '>')
		if end >= 0 {
			return body[start+1 : start+1+end]
		}
	}
	// Plain URI
	if idx := strings.IndexByte(body, ';'); idx >= 0 {
		return strings.TrimSpace(body[:idx])
	}
	return strings.TrimSpace(body)
}

// extractTag extracts tag parameter from header body
func extractTag(body string) string {
	idx := strings.Index(body, "tag=")
	if idx >= 0 {
		tag := body[idx+4:]
		if end := strings.IndexByte(tag, ';'); end >= 0 {
			return tag[:end]
		}
		return tag
	}
	return ""
}

// GetSession returns a session by Call-ID
func (sh *SessionHandler) GetSession(callID string) *PCSCFSessionRecord {
	sh.mu.RLock()
	defer sh.mu.RUnlock()
	return sh.sessions[callID]
}

// GetSessionCount returns the number of active sessions
func (sh *SessionHandler) GetSessionCount() int {
	sh.mu.RLock()
	defer sh.mu.RUnlock()
	return len(sh.sessions)
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * S-CSCF REGISTER handling - 3GPP TS 24.229
 *
 * IMS Registration flow:
 * 1. UE -> P-CSCF -> I-CSCF -> S-CSCF: REGISTER (no auth)
 * 2. S-CSCF -> HSS: MAR ( Multimedia-Auth-Request )
 * 3. S-CSCF <- HSS: MAA ( Multimedia-Auth-Answer ) with AV
 * 4. S-CSCF -> UE: 401 Unauthorized + WWW-Authenticate (AKA challenge)
 * 5. UE -> S-CSCF: REGISTER + Authorization (AKA response)
 * 6. S-CSCF verifies RES == XRES
 * 7. S-CSCF -> HSS: SAR ( Server-Assignment-Request )
 * 8. S-CSCF <- HSS: SAA ( Server-Assignment-Answer )
 * 9. S-CSCF -> UE: 200 OK + Service-Route + Path
 */

package scscf

import (
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/ims"
	"github.com/kamailio/kamailio-go/internal/ims/auth"
)

// RegistrationState represents the registration state
type RegistrationState int

const (
	RegStateUnregistered RegistrationState = iota
	RegStatePending      // 401 sent, waiting for challenge response
	RegStateRegistered   // Successfully registered
	RegStateDeregistered // Explicitly deregistered
)

// RegistrationRecord represents a user registration record
type RegistrationRecord struct {
	sync.RWMutex

	// User identity
	IMPI       string // Private Identity (username)
	IMPU       string // Public Identity (URI)
	Realm      string // Home domain

	// Contact binding
	ContactURI string
	ContactParams string
	Expires    time.Time

	// Registration state
	State      RegistrationState
	AuthState  *AuthState

	// Routing
	Path       []string // Path header URIs
	ServiceRoute []string // Service-Route for outgoing requests

	// Security
	Security   *SecurityContext

	// Timestamps
	CreatedAt  time.Time
	UpdatedAt  time.Time
}

// AuthState holds authentication state for pending registration
type AuthState struct {
	AuthVector   *auth.AuthVector
	Challenge    *auth.AKAChallenge
	Opaque       string
	Nonce        string
	Attempts     int
}

// SecurityContext holds security context after successful auth
type SecurityContext struct {
	CK         []byte
	IK         []byte
	Algorithm  string
}

// Registrar manages user registrations
type Registrar struct {
	sync.RWMutex

	// Registration database
	// Key: IMPU (Public Identity)
	records    map[string]*RegistrationRecord

	// Realm configuration
	realm      string
	akaConfig  *auth.AKAConfig
}

// NewRegistrar creates a new registrar
func NewRegistrar(realm string) *Registrar {
	return &Registrar{
		records:   make(map[string]*RegistrationRecord),
		realm:     realm,
		akaConfig: auth.NewAKAConfig(realm),
	}
}

// HandleRegister handles a REGISTER request
func (r *Registrar) HandleRegister(msg *parser.SIPMsg) (*RegisterResult, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	if msg.Method() != parser.MethodRegister {
		return nil, errors.New("not a REGISTER request")
	}

	// Extract user identity from To header
	impu := r.extractIMPU(msg)
	if impu == "" {
		return nil, errors.New("no IMPU found")
	}

	// Check if this is an initial or re-registration
	record := r.GetRecord(impu)

	// Check for Authorization header
	if msg.Authorization != nil {
		// This is a response to a challenge
		return r.handleAuthResponse(msg, record)
	}

	// Initial registration - send challenge
	return r.sendChallenge(msg, impu)
}

// RegisterResult represents the result of register handling
type RegisterResult struct {
	StatusCode   uint16
	StatusReason string
	Headers      map[string]str.Str
	Body         []byte
}

// sendChallenge sends a 401 challenge
func (r *Registrar) sendChallenge(msg *parser.SIPMsg, impu string) (*RegisterResult, error) {
	// Generate authentication vector
	av, err := auth.GenerateAuthVector()
	if err != nil {
		return nil, fmt.Errorf("failed to generate auth vector: %w", err)
	}

	// Generate opaque
	var callID, cseq string
	if msg.CallID != nil {
		callID = msg.CallID.Body.String()
	}
	if msg.CSeq != nil {
		cseqBody, _ := parser.ParseCSeqHeader(msg.CSeq)
		if cseqBody != nil {
			cseq = fmt.Sprintf("%d", cseqBody.Number)
		}
	}
	opaque := auth.GenerateOpaque(callID, cseq)

	// Build challenge
	challenge := auth.BuildChallenge(av, r.realm, opaque)

	// Store auth state
	record := &RegistrationRecord{
		IMPU:      impu,
		State:     RegStatePending,
		AuthState: &AuthState{
			AuthVector: av,
			Challenge:  challenge,
			Opaque:     opaque,
			Nonce:      challenge.Nonce,
		},
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}

	r.setRecord(impu, record)

	// Build 401 response
	wwwAuth := auth.BuildWWWAuthenticate(challenge)

	result := &RegisterResult{
		StatusCode:   401,
		StatusReason: "Unauthorized",
		Headers: map[string]str.Str{
			"WWW-Authenticate": wwwAuth,
		},
	}

	log.Info("Sent 401 challenge",
		log.String("impu", impu),
		log.String("realm", r.realm),
	)

	return result, nil
}

// handleAuthResponse handles REGISTER with Authorization header
func (r *Registrar) handleAuthResponse(msg *parser.SIPMsg, record *RegistrationRecord) (*RegisterResult, error) {
	if record == nil || record.AuthState == nil {
		return nil, errors.New("no pending auth state")
	}

	// Parse Authorization header
	authValue := msg.Authorization.Body.String()
	akaResp, err := auth.ParseAuthorization(authValue)
	if err != nil {
		return nil, fmt.Errorf("failed to parse authorization: %w", err)
	}

	// Verify opaque matches
	if akaResp.Opaque != record.AuthState.Opaque {
		return nil, errors.New("opaque mismatch")
	}

	// Verify response
	if !auth.VerifyResponse(record.AuthState.AuthVector, akaResp) {
		record.AuthState.Attempts++
		if record.AuthState.Attempts >= 3 {
			// Too many attempts
			return &RegisterResult{
				StatusCode:   403,
				StatusReason: "Forbidden",
			}, nil
		}
		// Send new challenge
		return r.sendChallenge(msg, record.IMPU)
	}

	// Authentication successful - complete registration
	return r.completeRegistration(msg, record)
}

// completeRegistration completes the registration after successful auth
func (r *Registrar) completeRegistration(msg *parser.SIPMsg, record *RegistrationRecord) (*RegisterResult, error) {
	record.Lock()
	defer record.Unlock()

	record.State = RegStateRegistered
	record.UpdatedAt = time.Now()

	// Extract contact
	if msg.Contact != nil {
		record.ContactURI = msg.Contact.Body.String()
	}

	// Extract expires
	if msg.Expires != nil {
		// Parse expires value
		var expires int
		fmt.Sscanf(msg.Expires.Body.String(), "%d", &expires)
		if expires > 0 {
			record.Expires = time.Now().Add(time.Duration(expires) * time.Second)
		} else {
			record.Expires = time.Now().Add(3600 * time.Second) // Default 1 hour
		}
	}

	// Extract Path headers
	if msg.Path != nil {
		pathInfo, _ := ims.ParsePath(msg.Path)
		if pathInfo != nil {
			record.Path = pathInfo.URIs
		}
	}

	// Build Service-Route
	// Service-Route is hard-coded to the originating route for now.
	// In a full deployment this comes from configuration or iFC.
	record.ServiceRoute = []string{
		fmt.Sprintf("sip:orig@%s", r.realm),
	}

	// Store security context
	record.Security = &SecurityContext{
		CK:        record.AuthState.AuthVector.CK,
		IK:        record.AuthState.AuthVector.IK,
		Algorithm: record.AuthState.Challenge.Algorithm,
	}

	// Clear auth state
	record.AuthState = nil

	// Build 200 OK response
	serviceRoute := ims.BuildServiceRoute(record.ServiceRoute)

	result := &RegisterResult{
		StatusCode:   200,
		StatusReason: "OK",
		Headers: map[string]str.Str{
			"Service-Route": serviceRoute,
		},
	}

	// Add Path if present
	if len(record.Path) > 0 {
		result.Headers["Path"] = ims.BuildPath(record.Path)
	}

	// Add P-Associated-URI. In a full deployment this comes from HSS.
	result.Headers["P-Associated-URI"] = str.Mk(fmt.Sprintf("<%s>", record.IMPU))

	log.Info("Registration successful",
		log.String("impu", record.IMPU),
		log.String("contact", record.ContactURI),
		log.Time("expires", record.Expires),
	)

	return result, nil
}

// extractIMPU extracts the IMPU from the message
func (r *Registrar) extractIMPU(msg *parser.SIPMsg) string {
	// Try To header first
	if msg.To != nil {
		body := msg.To.Body.String()
		// Extract URI from "To: <sip:user@domain>" or "To: User <sip:user@domain>"
		start := strings.IndexByte(body, '<')
		if start >= 0 {
			end := strings.IndexByte(body[start+1:], '>')
			if end >= 0 {
				return body[start+1 : start+1+end]
			}
		}
		// Plain URI format
		return body
	}
	return ""
}

// GetRecord gets a registration record by IMPU.
func (r *Registrar) GetRecord(impu string) *RegistrationRecord {
	r.RLock()
	defer r.RUnlock()
	return r.records[impu]
}

// GetRealm returns the configured realm.
func (r *Registrar) GetRealm() string {
	r.RLock()
	defer r.RUnlock()
	return r.realm
}

// setRecord sets a registration record
func (r *Registrar) setRecord(impu string, record *RegistrationRecord) {
	r.Lock()
	defer r.Unlock()
	r.records[impu] = record
}

// DeleteRecord removes a registration record
func (r *Registrar) DeleteRecord(impu string) {
	r.Lock()
	defer r.Unlock()
	delete(r.records, impu)
}

// GetRecordCount returns the number of registered users
func (r *Registrar) GetRecordCount() int {
	r.RLock()
	defer r.RUnlock()
	return len(r.records)
}

// IsRegistered checks if a user is registered
func (r *Registrar) IsRegistered(impu string) bool {
	record := r.GetRecord(impu)
	if record == nil {
		return false
	}

	record.RLock()
	defer record.RUnlock()

	if record.State != RegStateRegistered {
		return false
	}

	// Check if expired
	if !record.Expires.IsZero() && time.Now().After(record.Expires) {
		return false
	}

	return true
}

// GetContact returns the registered contact for a user
func (r *Registrar) GetContact(impu string) string {
	record := r.GetRecord(impu)
	if record == nil {
		return ""
	}

	record.RLock()
	defer record.RUnlock()

	return record.ContactURI
}

// SetRecordForTest creates a registration record bypassing the auth handshake.
// Intended for unit/integration tests that need to seed registered users
// without going through the 401 challenge flow.
func (r *Registrar) SetRecordForTest(impu, contactURI string) {
	r.setRecord(impu, &RegistrationRecord{
		IMPU:       impu,
		ContactURI: contactURI,
		State:      RegStateRegistered,
	})
}

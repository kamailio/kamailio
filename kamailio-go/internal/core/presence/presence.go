// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Presence core - matching C presence.h / pvar.h
 *
 * Implements SIP event publication and subscription framework:
 *   - Presence state management (RFC 3856/3857)
 *   - PIDF (RFC 3863) and XPIDF document parsing/generation
 *   - Subscription state machine (active/pending/terminated)
 */

package presence

import (
	"fmt"
	"strings"
	"sync"
	"time"
)

// PresenceState represents the presence status.
type PresenceState int

const (
	PresenceStateClosed PresenceState = iota
	PresenceStateOpen
	PresenceStateBusy
	PresenceStateAway
	PresenceStateXAway
	PresenceStateDND
	PresenceStateUnknown
)

func PresenceStatusString(state PresenceState) string {
	switch state {
	case PresenceStateClosed:
		return "closed"
	case PresenceStateOpen:
		return "open"
	case PresenceStateBusy:
		return "busy"
	case PresenceStateAway:
		return "away"
	case PresenceStateXAway:
		return "xa"
	case PresenceStateDND:
		return "dnd"
	default:
		return "unknown"
	}
}

func ParsePresenceState(s string) PresenceState {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "open", "online", "available":
		return PresenceStateOpen
	case "closed", "offline", "unavailable":
		return PresenceStateClosed
	case "busy":
		return PresenceStateBusy
	case "away":
		return PresenceStateAway
	case "xa", "extended-away":
		return PresenceStateXAway
	case "dnd", "do-not-disturb":
		return PresenceStateDND
	default:
		return PresenceStateUnknown
	}
}

// SubscriptionState represents a subscription's state (RFC 3265).
type SubscriptionState int

const (
	SubscriptionStateActive SubscriptionState = iota
	SubscriptionStatePending
	SubscriptionStateTerminated
)

func SubscriptionStateString(state SubscriptionState) string {
	switch state {
	case SubscriptionStateActive:
		return "active"
	case SubscriptionStatePending:
		return "pending"
	case SubscriptionStateTerminated:
		return "terminated"
	default:
		return "unknown"
	}
}

// SubscriptionReasonCode represents a termination reason.
type SubscriptionReasonCode int

const (
	ReasonUnknown SubscriptionReasonCode = iota
	ReasonDeactivated
	ReasonProbation
	ReasonRejected
	ReasonTimeout
	ReasonGiveup
	ReasonNoresource
)

func ReasonString(code SubscriptionReasonCode) string {
	switch code {
	case ReasonDeactivated:
		return "deactivated"
	case ReasonProbation:
		return "probation"
	case ReasonRejected:
		return "rejected"
	case ReasonTimeout:
		return "timeout"
	case ReasonGiveup:
		return "giveup"
	case ReasonNoresource:
		return "noresource"
	default:
		return "unknown"
	}
}

// PresenceRecord represents a single published presence document.
type PresenceRecord struct {
	URI         string
	State       PresenceState
	Note        string
	Timestamp   time.Time
	Expires     time.Duration
	Entity      string
	Priority    float64
	ContactAddr string
	RawBody     string
}

// Subscription represents a watcher subscription to a presentity.
type Subscription struct {
	ID             string
	PresentityURI  string
	SubscriberURI  string
	Event          string
	State          SubscriptionState
	Reason         SubscriptionReasonCode
	Expires        time.Time
	UpdatedAt      time.Time
	Version        int
	ContactAddr    string
	FromTag        string
	ToTag          string
	CallID         string
}

// Presentity represents a presentity (entity that can be watched).
type Presentity struct {
	URI           string
	Records       map[string]*PresenceRecord
	Subscriptions map[string]*Subscription
	mu            sync.RWMutex
}

func NewPresentity(uri string) *Presentity {
	return &Presentity{
		URI:           uri,
		Records:       make(map[string]*PresenceRecord),
		Subscriptions: make(map[string]*Subscription),
	}
}

func (p *Presentity) Publish(rec *PresenceRecord) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.Records[rec.Entity] = rec
}

func (p *Presentity) RemovePublish(entity string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	delete(p.Records, entity)
}

func (p *Presentity) CurrentState() PresenceState {
	p.mu.RLock()
	defer p.mu.RUnlock()

	best := PresenceStateClosed
	bestPriority := -1.0
	for _, rec := range p.Records {
		if rec.Priority > bestPriority {
			best = rec.State
			bestPriority = rec.Priority
		}
	}
	return best
}

func (p *Presentity) AddSubscription(sub *Subscription) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.Subscriptions[sub.ID] = sub
}

func (p *Presentity) RemoveSubscription(id string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	delete(p.Subscriptions, id)
}

func (p *Presentity) GetSubscription(id string) *Subscription {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.Subscriptions[id]
}

func (p *Presentity) Subscribers() []*Subscription {
	p.mu.RLock()
	defer p.mu.RUnlock()
	result := make([]*Subscription, 0, len(p.Subscriptions))
	for _, sub := range p.Subscriptions {
		if sub.State == SubscriptionStateActive {
			result = append(result, sub)
		}
	}
	return result
}

// Server manages all presentities and subscriptions.
type Server struct {
	mu             sync.RWMutex
	presentities   map[string]*Presentity
	defaultExpires time.Duration
	maxExpires     time.Duration
}

func NewServer() *Server {
	return &Server{
		presentities:   make(map[string]*Presentity),
		defaultExpires: 1 * time.Hour,
		maxExpires:     24 * time.Hour,
	}
}

func (s *Server) GetOrCreatePresentity(uri string) *Presentity {
	s.mu.Lock()
	defer s.mu.Unlock()
	if p, ok := s.presentities[uri]; ok {
		return p
	}
	p := NewPresentity(uri)
	s.presentities[uri] = p
	return p
}

func (s *Server) GetPresentity(uri string) *Presentity {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.presentities[uri]
}

func (s *Server) HandlePublish(uri string, state PresenceState, note, contactURI string, entityTag string, expires time.Duration) {
	p := s.GetOrCreatePresentity(uri)
	p.Publish(&PresenceRecord{
		URI:         uri,
		State:       state,
		Note:        note,
		Timestamp:   time.Now(),
		Expires:     expires,
		Entity:      entityTag,
		Priority:    0.8,
		ContactAddr: contactURI,
		RawBody: GeneratePIDF(&PIDFDocument{
			URI:       uri,
			Entity:    entityTag,
			Status:    state,
			Note:      note,
			Contact:   contactURI,
			Priority:  0.8,
			Timestamp: time.Now(),
		}),
	})
}

func (s *Server) HandleSubscribe(presentityURI, subscriberURI, subscriptionID string, expires time.Duration, event string) *Subscription {
	p := s.GetOrCreatePresentity(presentityURI)
	sub := &Subscription{
		ID:            subscriptionID,
		PresentityURI: presentityURI,
		SubscriberURI: subscriberURI,
		Event:         event,
		State:         SubscriptionStateActive,
		Expires:       time.Now().Add(expires),
		UpdatedAt:     time.Now(),
	}
	p.AddSubscription(sub)
	return sub
}

func (s *Server) Unsubscribe(presentityURI, subscriptionID string) {
	p := s.GetPresentity(presentityURI)
	if p != nil {
		p.RemoveSubscription(subscriptionID)
	}
}

func (s *Server) Cleanup() {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := time.Now()
	for _, p := range s.presentities {
		p.mu.Lock()
		for entity, rec := range p.Records {
			if now.After(rec.Timestamp.Add(rec.Expires)) {
				delete(p.Records, entity)
			}
		}
		for id, sub := range p.Subscriptions {
			if now.After(sub.Expires) {
				delete(p.Subscriptions, id)
			}
		}
		p.mu.Unlock()
	}
}

func (s *Server) Count() (int, int) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	subTotal := 0
	for _, p := range s.presentities {
		p.mu.RLock()
		subTotal += len(p.Subscriptions)
		p.mu.RUnlock()
	}
	return len(s.presentities), subTotal
}

var _ = fmt.Sprintf

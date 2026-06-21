// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Presence server - matching C presence_sip.c
 *
 * High-level operations for SUBSCRIBE/NOTIFY/PUBLISH handling.
 */

package presence

import (
	"fmt"
	"sync"
	"time"
)

// EventPackage represents a SIP event package.
type EventPackage struct {
	Name           string
	DefaultExpires time.Duration
	MinExpires     time.Duration
	MaxExpires     time.Duration
}

// EventPackageRegistry holds registered event packages.
type EventPackageRegistry struct {
	mu       sync.RWMutex
	packages map[string]*EventPackage
}

var defaultRegistry = &EventPackageRegistry{
	packages: make(map[string]*EventPackage),
}

func init() {
	defaultRegistry.packages["presence"] = &EventPackage{
		Name: "presence", DefaultExpires: 600 * time.Second, MinExpires: 60 * time.Second, MaxExpires: 3600 * time.Second,
	}
	defaultRegistry.packages["dialog"] = &EventPackage{
		Name: "dialog", DefaultExpires: 3600 * time.Second, MinExpires: 60 * time.Second, MaxExpires: 7200 * time.Second,
	}
	defaultRegistry.packages["message-summary"] = &EventPackage{
		Name: "message-summary", DefaultExpires: 3600 * time.Second, MinExpires: 60 * time.Second, MaxExpires: 7200 * time.Second,
	}
}

func GetEventPackage(name string) *EventPackage {
	defaultRegistry.mu.RLock()
	defer defaultRegistry.mu.RUnlock()
	return defaultRegistry.packages[name]
}

func RegisterEventPackage(pkg *EventPackage) {
	defaultRegistry.mu.Lock()
	defer defaultRegistry.mu.Unlock()
	defaultRegistry.packages[pkg.Name] = pkg
}

// ServerHandler provides high-level presence operations.
type ServerHandler struct {
	server *Server
	mu     sync.RWMutex
	// OnNotify is triggered when a presentity's state changes.
	OnNotify func(presentityURI string, newState PresenceState, subscribers []*Subscription)
}

func NewServerHandler() *ServerHandler {
	return &ServerHandler{server: NewServer()}
}

// NotifyPresentity pushes a presence state change event: it updates
// the presentity's records and invokes the configured OnNotify
// callback (if any). Returns the current state after publication and
// the list of subscribers that were notified.
//
// This can be called on its own (for externally triggered notifies)
// or implicitly by Publish when OnNotify is set.
func (h *ServerHandler) NotifyPresentity(presentityURI string, state PresenceState, note, contactURI string, entityTag string, expires time.Duration) (PresenceState, []*Subscription, error) {
	if h == nil || h.server == nil {
		return PresenceStateClosed, nil, fmt.Errorf("nil presence server")
	}
	if presentityURI == "" {
		return PresenceStateClosed, nil, fmt.Errorf("empty presentity URI")
	}

	h.mu.RLock()
	server := h.server
	notifyCallback := h.OnNotify
	h.mu.RUnlock()

	server.HandlePublish(presentityURI, state, note, contactURI, entityTag, expires)

	p := server.GetPresentity(presentityURI)
	if p == nil {
		return PresenceStateClosed, nil, fmt.Errorf("presentity %q missing after publish", presentityURI)
	}
	subscribers := p.Subscribers()

	if notifyCallback != nil && len(subscribers) > 0 {
		notifyCallback(presentityURI, state, subscribers)
	}
	return state, subscribers, nil
}

func (h *ServerHandler) Publish(presentityURI string, state PresenceState, note, contactURI string, entityTag string, expires time.Duration) {
	h.mu.Lock()
	server := h.server
	notifyCallback := h.OnNotify
	h.mu.Unlock()

	server.HandlePublish(presentityURI, state, note, contactURI, entityTag, expires)

	if notifyCallback != nil {
		p := server.GetPresentity(presentityURI)
		if p != nil {
			subscribers := p.Subscribers()
			if len(subscribers) > 0 {
				notifyCallback(presentityURI, state, subscribers)
			}
		}
	}
}

func (h *ServerHandler) Subscribe(presentityURI, subscriberURI, event string, expires time.Duration) (*Subscription, error) {
	pkg := GetEventPackage(event)
	if pkg == nil {
		return nil, fmt.Errorf("event package %q not supported", event)
	}

	if expires > pkg.MaxExpires {
		expires = pkg.MaxExpires
	}
	if expires < pkg.MinExpires {
		expires = pkg.DefaultExpires
	}

	subID := fmt.Sprintf("sub-%s-%d", presentityURI, time.Now().UnixNano())
	return h.server.HandleSubscribe(presentityURI, subscriberURI, subID, expires, event), nil
}

func (h *ServerHandler) Terminate(presentityURI, subscriptionID string) {
	h.server.Unsubscribe(presentityURI, subscriptionID)
}

func (h *ServerHandler) GetState(presentityURI string) PresenceState {
	p := h.server.GetPresentity(presentityURI)
	if p == nil {
		return PresenceStateClosed
	}
	return p.CurrentState()
}

func (h *ServerHandler) GetSubscribers(presentityURI string) []*Subscription {
	p := h.server.GetPresentity(presentityURI)
	if p == nil {
		return nil
	}
	return p.Subscribers()
}

func (h *ServerHandler) GetStateDocument(presentityURI string) string {
	p := h.server.GetPresentity(presentityURI)
	if p == nil {
		return GeneratePIDF(&PIDFDocument{URI: presentityURI, Status: PresenceStateClosed})
	}

	state := p.CurrentState()
	note := ""
	contact := ""
	p.mu.RLock()
	for _, rec := range p.Records {
		if rec.Note != "" {
			note = rec.Note
		}
		if rec.ContactAddr != "" {
			contact = rec.ContactAddr
		}
	}
	p.mu.RUnlock()

	return GeneratePIDF(&PIDFDocument{
		URI:      presentityURI,
		Status:   state,
		Note:     note,
		Contact:  contact,
		Priority: 0.8,
	})
}

func (h *ServerHandler) Stats() (presentities int, subscriptions int) {
	return h.server.Count()
}

func (h *ServerHandler) Cleanup() {
	h.server.Cleanup()
}

func (h *ServerHandler) StartCleanupLoop(interval time.Duration) chan struct{} {
	done := make(chan struct{})
	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				h.Cleanup()
			case <-done:
				return
			}
		}
	}()
	return done
}

// MWIMailbox represents a message waiting mailbox.
type MWIMailbox struct {
	Account    string
	NewMsg     int
	OldMsg     int
	NewUrgent  int
	OldUrgent  int
	LastUpdate time.Time
}

// MWIServer manages MWI subscriptions.
type MWIServer struct {
	mu        sync.RWMutex
	mailboxes map[string]*MWIMailbox
}

func NewMWIServer() *MWIServer {
	return &MWIServer{mailboxes: make(map[string]*MWIMailbox)}
}

func (m *MWIServer) UpdateMessageCount(account string, newMsg, oldMsg, newUrgent, oldUrgent int) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.mailboxes[account] = &MWIMailbox{
		Account: account, NewMsg: newMsg, OldMsg: oldMsg,
		NewUrgent: newUrgent, OldUrgent: oldUrgent, LastUpdate: time.Now(),
	}
}

func (m *MWIServer) NewMessage(account string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	mb, ok := m.mailboxes[account]
	if !ok {
		mb = &MWIMailbox{Account: account}
		m.mailboxes[account] = mb
	}
	mb.NewMsg++
	mb.LastUpdate = time.Now()
}

func (m *MWIServer) MessageRead(account string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	mb, ok := m.mailboxes[account]
	if !ok {
		return
	}
	if mb.NewMsg > 0 {
		mb.NewMsg--
		mb.OldMsg++
	}
	mb.LastUpdate = time.Now()
}

func (m *MWIServer) GetMWIDocument(account string) string {
	m.mu.RLock()
	defer m.mu.RUnlock()
	mb, ok := m.mailboxes[account]
	if !ok {
		return GenerateMWI(account, 0, 0, 0, 0)
	}
	return GenerateMWI(account, mb.NewMsg, mb.OldMsg, mb.NewUrgent, mb.OldUrgent)
}

func (m *MWIServer) GetMWIStatus(account string) (newMsg, oldMsg, newUrgent, oldUrgent int) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	mb, ok := m.mailboxes[account]
	if !ok {
		return 0, 0, 0, 0
	}
	return mb.NewMsg, mb.OldMsg, mb.NewUrgent, mb.OldUrgent
}

func (m *MWIServer) HasWaitingMessages(account string) bool {
	newMsg, _, newUrgent, _ := m.GetMWIStatus(account)
	return newMsg > 0 || newUrgent > 0
}

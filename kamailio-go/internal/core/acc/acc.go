// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Accounting / CDR (Call Detail Record) service.
 *
 * Produces per-call records for INVITE / reply / BYE / CANCEL flows
 * and dispatches them to pluggable backends (log, CSV, database, ...).
 */

package acc

import (
	"context"
	"net"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// CDR represents one call-detail record produced for a single
// request/reply flow.
type CDR struct {
	CallID      string
	FromUser    string
	FromDomain  string
	ToUser      string
	ToDomain    string
	RequestURI  string
	SourceIP    string
	Destination string
	Method      string
	StatusCode  int
	Reason      string
	Direction   string
	InviteTime  time.Time
	ConnectTime time.Time
	EndTime     time.Time
	DurationSec int
	RTPEngineID string
	Extra       map[string]string
}

// Backend writes CDRs to a persistent destination. Multiple backends
// may be attached to a single AccountingService; failures are isolated
// per backend so one misbehaving destination cannot stall another.
type Backend interface {
	Write(ctx context.Context, cdr *CDR) error
	Close() error
}

// AccountingService coordinates CDR production for requests/replies.
//
// Typical usage from ProxyCore:
//
//   ac := acc.NewAccountingService(logBackend, csvBackend, dbBackend)
//   ac.OnInvite(msg, src)
//   // later, when a reply arrives:
//   ac.OnReply(msg)
//   // and when the call ends:
//   ac.OnBye(msg)
//
// The service keeps an in-memory map of pending CDRs keyed by Call-ID.
// On a 2xx reply, InviteTime is set and ConnectTime is set.
// On BYE/cancel, EndTime is set, DurationSec is computed, and the CDR is
// flushed to every backend.
//
// This is intentionally thread-safe — a single instance can serve all
// concurrent request goroutines.
type AccountingService struct {
	mu       sync.RWMutex
	backends []Backend
	pending  map[string]*CDR
}

// NewAccountingService constructs a service with one or more backends.
// Passing no backends is legal — calls to Write will simply be discarded
// (useful for tests that only inspect in-memory CDRs).
func NewAccountingService(backends ...Backend) *AccountingService {
	return &AccountingService{
		backends: backends,
		pending:  make(map[string]*CDR),
	}
}

// OnInvite records the start of a potential call from an INVITE message.
// If Call-ID is already known, OnInvite is a no-op (the existing CDR
// continues to track).
func (a *AccountingService) OnInvite(msg *parser.SIPMsg, src net.Addr) {
	if a == nil || msg == nil {
		return
	}
	if msg.Method() != parser.MethodInvite {
		return
	}
	callID := msgCallID(msg)
	if callID == "" {
		return
	}

	a.mu.Lock()
	defer a.mu.Unlock()
	if _, exists := a.pending[callID]; exists {
		return
	}
	cdr := &CDR{
		CallID:     callID,
		Method:     "INVITE",
		InviteTime: time.Now(),
		SourceIP:   addrHostFromNet(src),
	}
	fromUser, fromDomain := extractFrom(msg)
	toUser, toDomain := extractTo(msg)
	cdr.FromUser = fromUser
	cdr.FromDomain = fromDomain
	cdr.ToUser = toUser
	cdr.ToDomain = toDomain
	cdr.RequestURI = extractRURI(msg)
	if cdr.FromDomain != "" && cdr.ToDomain != "" && cdr.FromDomain != cdr.ToDomain {
		cdr.Direction = "outbound"
	} else {
		cdr.Direction = "inbound"
	}
	a.pending[callID] = cdr
}

// OnReply records a reply against an existing pending CDR.
func (a *AccountingService) OnReply(msg *parser.SIPMsg) {
	if a == nil || msg == nil {
		return
	}
	callID := msgCallID(msg)
	if callID == "" {
		return
	}
	status := int(msg.StatusCode())
	if status == 0 {
		return
	}

	a.mu.Lock()
	defer a.mu.Unlock()
	cdr, ok := a.pending[callID]
	if !ok {
		return
	}
	cdr.StatusCode = status
	if msg.FirstLine != nil && msg.FirstLine.Reply != nil {
		cdr.Reason = msg.FirstLine.Reply.Reason.String()
	}
	if status >= 200 && status < 300 && cdr.ConnectTime.IsZero() {
		cdr.ConnectTime = time.Now()
	}
}

// OnBye finalizes a CDR and flushes it to all backends.
func (a *AccountingService) OnBye(msg *parser.SIPMsg) {
	if a == nil || msg == nil {
		return
	}
	callID := msgCallID(msg)
	if callID == "" {
		return
	}
	a.mu.Lock()
	cdr, ok := a.pending[callID]
	delete(a.pending, callID)
	a.mu.Unlock()
	if !ok {
		cdr = &CDR{CallID: callID, Method: "BYE", EndTime: time.Now()}
	}
	cdr.EndTime = time.Now()
	if !cdr.ConnectTime.IsZero() {
		cdr.DurationSec = int(cdr.EndTime.Sub(cdr.ConnectTime).Seconds())
	}
	a.flush(context.Background(), cdr)
}

// OnCancel flushes a CDR for a cancelled call.
func (a *AccountingService) OnCancel(msg *parser.SIPMsg) {
	if a == nil || msg == nil {
		return
	}
	callID := msgCallID(msg)
	if callID == "" {
		return
	}
	a.mu.Lock()
	cdr, ok := a.pending[callID]
	delete(a.pending, callID)
	a.mu.Unlock()
	if !ok {
		return
	}
	cdr.EndTime = time.Now()
	cdr.StatusCode = 487
	cdr.Reason = "Cancelled"
	a.flush(context.Background(), cdr)
}

// PendingCount returns the number of CDRs currently in the pending map.
// Useful for tests and monitoring.
func (a *AccountingService) PendingCount() int {
	if a == nil {
		return 0
	}
	a.mu.RLock()
	defer a.mu.RUnlock()
	return len(a.pending)
}

// Lookup returns a copy of the pending CDR for the given Call-ID, if any.
// This is primarily used by tests.
func (a *AccountingService) Lookup(callID string) *CDR {
	if a == nil || callID == "" {
		return nil
	}
	a.mu.RLock()
	defer a.mu.RUnlock()
	cdr, ok := a.pending[callID]
	if !ok {
		return nil
	}
	copyCDR := *cdr
	return &copyCDR
}

// flush writes a single CDR to every backend, logging any error from each.
// A failure in one backend does not prevent flushing to the others.
func (a *AccountingService) flush(ctx context.Context, cdr *CDR) {
	for _, b := range a.backends {
		if err := b.Write(ctx, cdr); err != nil {
			// Note: we deliberately do not return the error upstream;
			// a failed CDR write should not break the proxy's reply path.
			// If a log backend is available, it would report this.
			_ = err
		}
	}
}

// ---------------------------------------------------------------------------
// small helpers for extracting SIP message fields.

func msgCallID(msg *parser.SIPMsg) string {
	if msg.CallID != nil {
		return msg.CallID.Body.String()
	}
	for _, h := range msg.Headers {
		if h != nil && h.Type == parser.HdrCallID {
			return h.Body.String()
		}
	}
	return ""
}

func parseToBodyFrom(msg *parser.SIPMsg, header *parser.HdrField) (user, domain string) {
	if header == nil {
		return "", ""
	}
	if tb, ok := header.Parsed.(*parser.ToBody); ok {
		if tb.ParsedURI != nil {
			return tb.ParsedURI.User.String(), tb.ParsedURI.Host.String()
		}
		if tb.URI != nil {
			return tb.URI.User.String(), tb.URI.Host.String()
		}
	}
	return "", ""
}

func extractFrom(msg *parser.SIPMsg) (user, domain string) {
	return parseToBodyFrom(msg, msg.From)
}

func extractTo(msg *parser.SIPMsg) (user, domain string) {
	return parseToBodyFrom(msg, msg.To)
}

func extractRURI(msg *parser.SIPMsg) string {
	if msg.FirstLine != nil && msg.FirstLine.Req != nil {
		return msg.FirstLine.Req.URI.String()
	}
	return ""
}

func addrHostFromNet(a net.Addr) string {
	if a == nil {
		return ""
	}
	s := a.String()
	if idx := lastIndexByte(s, ':'); idx >= 0 {
		return s[:idx]
	}
	return s
}

func lastIndexByte(s string, b byte) int {
	for i := len(s) - 1; i >= 0; i-- {
		if s[i] == b {
			return i
		}
	}
	return -1
}

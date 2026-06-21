// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Dialog management module (Phase 6-1)
 *
 * Tracks SIP dialog state (Early -> Confirmed -> Terminated) and provides
 * helpers for:
 *   - building dialogs from incoming INVITEs (UAS) or outgoing INVITE responses (UAC),
 *   - route-set computation from Route / Record-Route headers,
 *   - CSeq management and validation,
 *   - tag generation and header extraction.
 */

package dialog

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ---------------------------------------------------------------------------
// Dialog state
// ---------------------------------------------------------------------------

// DialogState represents the lifecycle state of a SIP dialog.
type DialogState int

const (
	DialogStateEarly      DialogState = iota // dialog created, not yet confirmed
	DialogStateConfirmed                     // 200 OK received/sent
	DialogStateTerminated                    // BYE received/sent
	DialogStateExpired                       // dialog lifetime expired
)

func (s DialogState) String() string {
	switch s {
	case DialogStateEarly:
		return "Early"
	case DialogStateConfirmed:
		return "Confirmed"
	case DialogStateTerminated:
		return "Terminated"
	case DialogStateExpired:
		return "Expired"
	default:
		return "Unknown"
	}
}

// ---------------------------------------------------------------------------
// Dialog direction
// ---------------------------------------------------------------------------

// DialogDirection identifies whether the local party is the UAS or UAC.
type DialogDirection int

const (
	DialogDirectionUAS DialogDirection = iota
	DialogDirectionUAC
)

func (d DialogDirection) String() string {
	if d == DialogDirectionUAS {
		return "UAS"
	}
	return "UAC"
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

// Dialog represents a single SIP dialog, identified uniquely by
// (Call-ID + local-tag + remote-tag).
type Dialog struct {
	mu sync.RWMutex

	CallID    string
	LocalTag  string
	RemoteTag string

	Direction DialogDirection

	LocalURI  string
	RemoteURI string

	RemoteTarget string
	RouteSet     []string

	LocalCSeq  uint32
	RemoteCSeq uint32

	State DialogState

	CreatedAt time.Time

	ActiveTransactions map[string]bool

	NextExpectedCSeq uint32
	InviteStart       time.Time
	LastResponseCode  int
}

// ---------------------------------------------------------------------------
// Manager
// ---------------------------------------------------------------------------

// Manager maintains a collection of active Dialogs keyed by a deterministic
// combination of Call-ID and tags.
type Manager struct {
	mu       sync.RWMutex
	dialogs  map[string]*Dialog
	profiles *ProfileManager
	callbacks *CallbackManager
	timers   *TimerManager
}

// NewManager creates a new, empty dialog manager.
func NewManager() *Manager {
	mgr := &Manager{
		dialogs:  make(map[string]*Dialog),
		profiles: NewProfileManager(),
		callbacks: NewCallbackManager(),
	}
	return mgr
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// dialogKey produces a stable key from a Call-ID and two tag values.
// The tag positions are normalized (lexicographically) so that the same key
// is produced regardless of which argument is "local" or "remote".
func dialogKey(callID, a, b string) string {
	first, second := a, b
	if a > b {
		first, second = b, a
	}
	return callID + "|" + first + "|" + second
}

// generateTag creates a short, random tag string.
func generateTag() string {
	buf := make([]byte, 8)
	if _, err := rand.Read(buf); err != nil {
		return fmt.Sprintf("tag-%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buf)
}

// extractTag scans a From/To header body and returns the value of the "tag"
// parameter, or an empty string if not found.
//
// Example input:
//   "Alice <sip:alice@ims.example.com>;tag=abc123"
//   returns: "abc123"
func extractTag(body str.Str) string {
	if body.IsEmpty() {
		return ""
	}
	s := body.String()

	idx := strings.Index(s, ";tag=")
	if idx < 0 {
		idx = strings.Index(s, ";TAG=")
	}
	if idx < 0 {
		// also tolerate "tag=" without leading semicolon (first parameter)
		if strings.HasPrefix(strings.ToLower(s), "tag=") {
			rest := s[4:]
			if semi := strings.IndexByte(rest, ';'); semi >= 0 {
				return strings.TrimSpace(rest[:semi])
			}
			return strings.TrimSpace(rest)
		}
		return ""
	}
	rest := s[idx+5:]
	if semi := strings.IndexByte(rest, ';'); semi >= 0 {
		return strings.TrimSpace(rest[:semi])
	}
	return strings.TrimSpace(rest)
}

// extractURIFromAddrSpec scans an address-spec header body and returns the
// URI portion (the part inside angle brackets, or the whole trimmed body if
// no angle brackets are present).
//
// Example input:
//   "Alice <sip:alice@ims.example.com>;tag=abc123"
//   returns: "sip:alice@ims.example.com"
func extractURIFromAddrSpec(body str.Str) string {
	if body.IsEmpty() {
		return ""
	}
	s := body.String()
	lt := strings.IndexByte(s, '<')
	gt := strings.LastIndexByte(s, '>')
	if lt >= 0 && gt > lt {
		return strings.TrimSpace(s[lt+1 : gt])
	}
	// no angle brackets – use the portion before any ';'
	if semi := strings.IndexByte(s, ';'); semi >= 0 {
		return strings.TrimSpace(s[:semi])
	}
	return strings.TrimSpace(s)
}

// parseRouteValues returns the individual URIs from a Route / Record-Route
// header body. The body may be a single value or a comma-separated list.
// Each value is returned without surrounding angle brackets.
func parseRouteValues(body str.Str) []string {
	if body.IsEmpty() {
		return nil
	}
	s := body.String()

	// split on commas that are not inside < > brackets
	var out []string
	depth := 0
	start := 0
	for i := 0; i < len(s); i++ {
		switch s[i] {
		case '<':
			depth++
		case '>':
			if depth > 0 {
				depth--
			}
		case ',':
			if depth == 0 {
				out = append(out, trimRouteToken(s[start:i]))
				start = i + 1
			}
		}
	}
	if start < len(s) {
		out = append(out, trimRouteToken(s[start:]))
	}
	// filter empty tokens
	filtered := out[:0]
	for _, v := range out {
		if v != "" {
			filtered = append(filtered, v)
		}
	}
	return filtered
}

// trimRouteToken strips surrounding whitespace and optional angle brackets.
func trimRouteToken(token string) string {
	t := strings.TrimSpace(token)
	lt := strings.IndexByte(t, '<')
	gt := strings.LastIndexByte(t, '>')
	if lt >= 0 && gt > lt {
		return strings.TrimSpace(t[lt+1 : gt])
	}
	return t
}

// collectRouteURIs gathers all Route / Record-Route URIs from a message.
// Both multiple header instances and comma-separated bodies are supported.
func collectRouteURIs(msg *parser.SIPMsg, hdrType parser.HdrType) []string {
	if msg == nil {
		return nil
	}
	var out []string
	msg.ForEachHeader(hdrType, func(h *parser.HdrField) bool {
		for _, uri := range parseRouteValues(h.Body) {
			if uri != "" {
				out = append(out, uri)
			}
		}
		return true
	})
	return out
}

// ---------------------------------------------------------------------------
// UAS dialog creation
// ---------------------------------------------------------------------------

// CreateUASDialog builds a Dialog for an incoming INVITE received by the
// local party (UAS role). The Route headers from the incoming request form
// the dialog's route set (per RFC 3261 §12).
func CreateUASDialog(msg *parser.SIPMsg, localContact string) (*Dialog, error) {
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}
	if msg.CallID == nil {
		return nil, fmt.Errorf("missing Call-ID header")
	}
	if msg.From == nil {
		return nil, fmt.Errorf("missing From header")
	}
	if msg.To == nil {
		return nil, fmt.Errorf("missing To header")
	}

	callID := strings.TrimSpace(msg.CallID.Body.String())
	if callID == "" {
		return nil, fmt.Errorf("empty Call-ID")
	}

	fromTag := extractTag(msg.From.Body)
	toTag := extractTag(msg.To.Body)
	if toTag == "" {
		toTag = generateTag()
	}

	// UAS view: the remote provided From-tag (remote tag); our local tag is
	// the To-tag (possibly auto-generated above).
	localTag := toTag
	remoteTag := fromTag

	localURI := extractURIFromAddrSpec(msg.To.Body)
	remoteURI := extractURIFromAddrSpec(msg.From.Body)

	remoteTarget := ""
	if contacts, err := parser.ParseContactList(msg.Contact.Body); err == nil && len(contacts) > 0 {
		if contacts[0] != nil && contacts[0].URI != nil {
			remoteTarget = contacts[0].URI.String()
		}
	}
	if remoteTarget == "" {
		remoteTarget = remoteURI
	}

	// UAS: the route set is built from the Route headers of the INVITE.
	routeSet := collectRouteURIs(msg, parser.HdrRoute)

	localCSeq := uint32(0)
	remoteCSeq := uint32(0)
	if msg.CSeq != nil {
		if cs, err := parser.ParseCSeqHeader(msg.CSeq); err == nil {
			remoteCSeq = cs.Number
		}
	}

	d := &Dialog{
		CallID:             callID,
		LocalTag:           localTag,
		RemoteTag:          remoteTag,
		Direction:          DialogDirectionUAS,
		LocalURI:           localURI,
		RemoteURI:          remoteURI,
		RemoteTarget:       remoteTarget,
		RouteSet:           routeSet,
		LocalCSeq:          localCSeq,
		RemoteCSeq:         remoteCSeq,
		State:              DialogStateEarly,
		CreatedAt:          time.Now(),
		ActiveTransactions: make(map[string]bool),
	}

	_ = localContact
	return d, nil
}

// ---------------------------------------------------------------------------
// UAC dialog creation
// ---------------------------------------------------------------------------

// CreateUACDialog builds a Dialog from a response received for an INVITE
// that the local party (UAC role) sent. The Record-Route headers from the
// response form the dialog's route set (per RFC 3261 §12).
func CreateUACDialog(msg *parser.SIPMsg, localTag, localContact string) (*Dialog, error) {
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}
	if msg.CallID == nil {
		return nil, fmt.Errorf("missing Call-ID header")
	}
	if msg.From == nil {
		return nil, fmt.Errorf("missing From header")
	}
	if msg.To == nil {
		return nil, fmt.Errorf("missing To header")
	}

	callID := strings.TrimSpace(msg.CallID.Body.String())
	if callID == "" {
		return nil, fmt.Errorf("empty Call-ID")
	}

	fromTag := extractTag(msg.From.Body)
	if fromTag == "" {
		fromTag = localTag
	}
	toTag := extractTag(msg.To.Body)

	// UAC view: the local tag is the From-tag; the remote tag is the To-tag.
	loTag := fromTag
	remTag := toTag

	localURI := extractURIFromAddrSpec(msg.From.Body)
	remoteURI := extractURIFromAddrSpec(msg.To.Body)

	remoteTarget := ""
	if contacts, err := parser.ParseContactList(msg.Contact.Body); err == nil && len(contacts) > 0 {
		if contacts[0] != nil && contacts[0].URI != nil {
			remoteTarget = contacts[0].URI.String()
		}
	}
	if remoteTarget == "" {
		remoteTarget = remoteURI
	}

	// UAC: the route set is built from the Record-Route headers of the response.
	routeSet := collectRouteURIs(msg, parser.HdrRecordRoute)
	// UAC reverse the route set to get proper outbound order.
	if len(routeSet) > 1 {
		for i, j := 0, len(routeSet)-1; i < j; i, j = i+1, j-1 {
			routeSet[i], routeSet[j] = routeSet[j], routeSet[i]
		}
	}

	localCSeq := uint32(0)
	remoteCSeq := uint32(0)
	if msg.CSeq != nil {
		if cs, err := parser.ParseCSeqHeader(msg.CSeq); err == nil {
			localCSeq = cs.Number
		}
	}

	d := &Dialog{
		CallID:             callID,
		LocalTag:           loTag,
		RemoteTag:          remTag,
		Direction:          DialogDirectionUAC,
		LocalURI:           localURI,
		RemoteURI:          remoteURI,
		RemoteTarget:       remoteTarget,
		RouteSet:           routeSet,
		LocalCSeq:          localCSeq,
		RemoteCSeq:         remoteCSeq,
		State:              DialogStateEarly,
		CreatedAt:          time.Now(),
		ActiveTransactions: make(map[string]bool),
	}

	_ = localContact
	return d, nil
}

// ---------------------------------------------------------------------------
// Manager operations
// ---------------------------------------------------------------------------

// Add stores dialog in the manager. If a dialog with the same key already
// exists, an error is returned.
func (m *Manager) Add(d *Dialog) error {
	if m == nil || d == nil {
		return fmt.Errorf("nil manager or dialog")
	}
	key := dialogKey(d.CallID, d.LocalTag, d.RemoteTag)
	m.mu.Lock()
	defer m.mu.Unlock()
	if _, exists := m.dialogs[key]; exists {
		return fmt.Errorf("dialog already exists for key %s", key)
	}
	m.dialogs[key] = d
	return nil
}

// Lookup returns the Dialog matching the given Call-ID and tags, or nil.
// The ordering of localTag/remoteTag does not affect the result.
func (m *Manager) Lookup(callID, localTag, remoteTag string) *Dialog {
	if m == nil {
		return nil
	}
	key := dialogKey(callID, localTag, remoteTag)
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.dialogs[key]
}

// Remove deletes the Dialog identified by the given Call-ID and tags.
func (m *Manager) Remove(callID, localTag, remoteTag string) {
	if m == nil {
		return
	}
	key := dialogKey(callID, localTag, remoteTag)
	m.mu.Lock()
	defer m.mu.Unlock()
	delete(m.dialogs, key)
}

// Count returns the number of dialogs currently managed.
func (m *Manager) Count() int {
	if m == nil {
		return 0
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	return len(m.dialogs)
}

// List returns a copy of the currently tracked dialogs. If limit is
// positive, only the first `limit` entries (in iteration order) are
// returned. The returned slice is owned by the caller - safe to mutate.
func (m *Manager) List(limit int) []*Dialog {
	if m == nil {
		return nil
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	if len(m.dialogs) == 0 {
		return nil
	}
	out := make([]*Dialog, 0, len(m.dialogs))
	for _, d := range m.dialogs {
		// Build a shallow copy so callers can't race with the manager.
		copy := &Dialog{
			CallID:             d.CallID,
			LocalTag:           d.LocalTag,
			RemoteTag:          d.RemoteTag,
			Direction:          d.Direction,
			LocalURI:           d.LocalURI,
			RemoteURI:          d.RemoteURI,
			RemoteTarget:       d.RemoteTarget,
			RouteSet:           append([]string(nil), d.RouteSet...),
			LocalCSeq:          d.LocalCSeq,
			RemoteCSeq:         d.RemoteCSeq,
			State:              d.State,
			CreatedAt:          d.CreatedAt,
			NextExpectedCSeq:   d.NextExpectedCSeq,
			InviteStart:        d.InviteStart,
			LastResponseCode:   d.LastResponseCode,
			ActiveTransactions: nil,
		}
		out = append(out, copy)
		if limit > 0 && len(out) >= limit {
			break
		}
	}
	return out
}

// ---------------------------------------------------------------------------
// Dialog state transitions
// ---------------------------------------------------------------------------

// Confirm transitions the dialog from Early to Confirmed.
// Idempotent – calling Confirm on an already Confirmed dialog is a no-op.
func (d *Dialog) Confirm() {
	if d == nil {
		return
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.State == DialogStateEarly {
		d.State = DialogStateConfirmed
	}
}

// Terminate transitions the dialog to Terminated.
func (d *Dialog) Terminate() {
	if d == nil {
		return
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	d.State = DialogStateTerminated
}

// IsConfirmed reports whether the dialog has reached the Confirmed state.
func (d *Dialog) IsConfirmed() bool {
	if d == nil {
		return false
	}
	d.mu.RLock()
	defer d.mu.RUnlock()
	return d.State == DialogStateConfirmed
}

// IsTerminated reports whether the dialog has been terminated.
func (d *Dialog) IsTerminated() bool {
	if d == nil {
		return false
	}
	d.mu.RLock()
	defer d.mu.RUnlock()
	return d.State == DialogStateTerminated
}

// ---------------------------------------------------------------------------
// CSeq management
// ---------------------------------------------------------------------------

// NextLocalCSeq returns the CSeq number to use for the next locally-originated
// request within this dialog, and increments the stored counter.
func (d *Dialog) NextLocalCSeq() uint32 {
	if d == nil {
		return 0
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	d.LocalCSeq++
	return d.LocalCSeq
}

// UpdateRemoteCSeq records a CSeq number from a remotely-originated request
// received in this dialog. Returns an error if the supplied CSeq is not
// strictly greater than the last known remote CSeq.
func (d *Dialog) UpdateRemoteCSeq(cseq uint32) error {
	if d == nil {
		return fmt.Errorf("nil dialog")
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.RemoteCSeq > 0 && cseq <= d.RemoteCSeq {
		return fmt.Errorf("invalid remote CSeq: %d is not strictly greater than %d", cseq, d.RemoteCSeq)
	}
	d.RemoteCSeq = cseq
	return nil
}

// ---------------------------------------------------------------------------
// Route header construction
// ---------------------------------------------------------------------------

// BuildRouteHeaders returns the Route header entries (name, value) that an
// in-dialog request sent from the local party should carry. Each entry is
// derived directly from the Route Set stored in the dialog.
func (d *Dialog) BuildRouteHeaders() [][2]string {
	if d == nil {
		return nil
	}
	d.mu.RLock()
	defer d.mu.RUnlock()
	if len(d.RouteSet) == 0 {
		return nil
	}
	out := make([][2]string, 0, len(d.RouteSet))
	for _, uri := range d.RouteSet {
		if uri == "" {
			continue
		}
		out = append(out, [2]string{"Route", fmt.Sprintf("<%s>", uri)})
	}
	return out
}

// ---------------------------------------------------------------------------
// Profile / Callback / Timer accessors
// ---------------------------------------------------------------------------

// Profiles returns the profile manager.
func (m *Manager) Profiles() *ProfileManager { return m.profiles }

// Callbacks returns the callback manager.
func (m *Manager) Callbacks() *CallbackManager { return m.callbacks }

// Timers returns the timer manager.
func (m *Manager) Timers() *TimerManager { return m.timers }

// CleanupExpired removes expired dialogs.
func (m *Manager) CleanupExpired() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	count := 0
	for id, dlg := range m.dialogs {
		if dlg.State == DialogStateTerminated || dlg.State == DialogStateExpired {
			delete(m.dialogs, id)
			count++
		}
	}
	return count
}

// Stats returns the total number of managed dialogs and a breakdown by state.
func (m *Manager) Stats() (count int, early int, confirmed int, terminated int) {
	if m == nil {
		return 0, 0, 0, 0
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	count = len(m.dialogs)
	for _, d := range m.dialogs {
		switch d.State {
		case DialogStateEarly:
			early++
		case DialogStateConfirmed:
			confirmed++
		case DialogStateTerminated, DialogStateExpired:
			terminated++
		}
	}
	return count, early, confirmed, terminated
}

// ---------------------------------------------------------------------------
// High-level message handlers (Phase 40)
// ---------------------------------------------------------------------------

// lookupDialogByMsg extracts the call-id and tag values from a request
// message and returns the matching dialog in the manager, or nil.
func (m *Manager) lookupDialogByMsg(msg *parser.SIPMsg) (*Dialog, string, string) {
	if msg == nil || msg.CallID == nil || msg.From == nil || msg.To == nil {
		return nil, "", ""
	}
	callID := strings.TrimSpace(msg.CallID.Body.String())
	fromTag := extractTag(msg.From.Body)
	toTag := extractTag(msg.To.Body)

	// If the dialog is UAS, our local tag is the To-tag; if UAC, our local
	// tag is the From-tag. We try both orientations.
	d := m.Lookup(callID, fromTag, toTag)
	if d != nil {
		return d, fromTag, toTag
	}
	d = m.Lookup(callID, toTag, fromTag)
	return d, fromTag, toTag
}

// extractBranch returns the branch parameter from the topmost Via header, or
// an empty string if none is present.
func extractBranch(msg *parser.SIPMsg) string {
	if msg == nil || msg.HdrVia1 == nil {
		return ""
	}
	body := msg.HdrVia1.Body.String()
	idx := strings.Index(strings.ToLower(body), "branch=")
	if idx < 0 {
		return ""
	}
	rest := body[idx+len("branch="):]
	if semi := strings.IndexAny(rest, "; \t\r\n"); semi >= 0 {
		return strings.TrimSpace(rest[:semi])
	}
	return strings.TrimSpace(rest)
}

// HandleInvite processes an incoming INVITE request. It creates a new UAS
// dialog if one does not yet exist, or updates the state of an existing
// dialog for re-INVITEs.
func (m *Manager) HandleInvite(msg *parser.SIPMsg) (*Dialog, error) {
	if m == nil {
		return nil, fmt.Errorf("nil manager")
	}
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}
	if msg.Method() != parser.MethodInvite {
		return nil, fmt.Errorf("expected INVITE, got %s", parser.MethodName(msg.Method()))
	}
	if msg.CallID == nil || msg.From == nil {
		return nil, fmt.Errorf("INVITE missing required headers")
	}

	callID := strings.TrimSpace(msg.CallID.Body.String())
	fromTag := extractTag(msg.From.Body)
	toTag := extractTag(msg.To.Body)
	if callID == "" {
		return nil, fmt.Errorf("INVITE with empty Call-ID")
	}

	// Determine whether an existing dialog matches this INVITE. We try
	// both tag orderings to support UAS and UAC dialogs stored in the
	// manager.
	var existing *Dialog
	if toTag != "" {
		existing = m.Lookup(callID, toTag, fromTag)
	}
	if existing == nil {
		existing = m.Lookup(callID, fromTag, toTag)
	}

	if existing != nil {
		existing.mu.Lock()
		defer existing.mu.Unlock()
		if existing.State == DialogStateTerminated || existing.State == DialogStateExpired {
			return nil, fmt.Errorf("re-INVITE on terminated/expired dialog")
		}
		// re-INVITE: move back to Early until final response.
		existing.State = DialogStateEarly
		existing.InviteStart = time.Now()
		if branch := extractBranch(msg); branch != "" {
			if existing.ActiveTransactions == nil {
				existing.ActiveTransactions = make(map[string]bool)
			}
			existing.ActiveTransactions[branch] = true
		}
		if cs, err := parser.ParseCSeqHeader(msg.CSeq); err == nil {
			existing.NextExpectedCSeq = cs.Number + 1
		}
		return existing, nil
	}

	// New dialog: construct a UAS dialog.
	newDlg, err := CreateUASDialog(msg, "")
	if err != nil {
		return nil, err
	}
	newDlg.InviteStart = time.Now()
	if cs, err := parser.ParseCSeqHeader(msg.CSeq); err == nil {
		newDlg.LocalCSeq = cs.Number
		newDlg.RemoteCSeq = cs.Number
		newDlg.NextExpectedCSeq = cs.Number + 1
	}
	if branch := extractBranch(msg); branch != "" {
		if newDlg.ActiveTransactions == nil {
			newDlg.ActiveTransactions = make(map[string]bool)
		}
		newDlg.ActiveTransactions[branch] = true
	}

	if err := m.Add(newDlg); err != nil {
		return nil, err
	}
	return newDlg, nil
}

// HandleReply processes a reply for an INVITE dialog. It locates the
// matching dialog (possibly creating a new UAC dialog on a 2xx response
// when no dialog exists yet) and updates its state.
func (m *Manager) HandleReply(msg *parser.SIPMsg, statusCode int, reason string) (*Dialog, error) {
	if m == nil {
		return nil, fmt.Errorf("nil manager")
	}
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}

	d, _, _ := m.lookupDialogByMsg(msg)

	if d == nil {
		// If no existing dialog but we received a 2xx (e.g. as UAC for an
		// outgoing INVITE), lazily create a UAC dialog from the reply.
		if statusCode >= 200 && statusCode < 300 {
			newDlg, err := CreateUACDialog(msg, "", "")
			if err != nil {
				return nil, err
			}
			newDlg.LastResponseCode = statusCode
			newDlg.State = DialogStateConfirmed
			if err := m.Add(newDlg); err != nil {
				return nil, err
			}
			return newDlg, nil
		}
		return nil, fmt.Errorf("no dialog found for reply %d %s", statusCode, reason)
	}

	d.mu.Lock()
	defer d.mu.Unlock()
	d.LastResponseCode = statusCode

	switch {
	case statusCode >= 100 && statusCode < 200:
		// provisional: keep Early (or move into Early if still in an earlier state)
		if d.State != DialogStateConfirmed {
			d.State = DialogStateEarly
		}
	case statusCode >= 200 && statusCode < 300:
		d.State = DialogStateConfirmed
		if cs, err := parser.ParseCSeqHeader(msg.CSeq); err == nil {
			d.RemoteCSeq = cs.Number
		}
	default:
		// 3xx-6xx: treat as terminated for an INVITE dialog.
		d.State = DialogStateTerminated
	}
	return d, nil
}

// HandleBye terminates the dialog matching the BYE request.
func (m *Manager) HandleBye(msg *parser.SIPMsg) (*Dialog, error) {
	if m == nil {
		return nil, fmt.Errorf("nil manager")
	}
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}
	if msg.Method() != parser.MethodBye {
		return nil, fmt.Errorf("expected BYE, got %s", parser.MethodName(msg.Method()))
	}
	d, _, _ := m.lookupDialogByMsg(msg)
	if d == nil {
		return nil, fmt.Errorf("no dialog found for BYE")
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	d.State = DialogStateTerminated
	d.LastResponseCode = 200
	return d, nil
}

// HandleCancel cancels the INVITE dialog matching the CANCEL request.
func (m *Manager) HandleCancel(msg *parser.SIPMsg) (*Dialog, error) {
	if m == nil {
		return nil, fmt.Errorf("nil manager")
	}
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}
	if msg.Method() != parser.MethodCancel {
		return nil, fmt.Errorf("expected CANCEL, got %s", parser.MethodName(msg.Method()))
	}
	d, _, _ := m.lookupDialogByMsg(msg)
	if d == nil {
		return nil, fmt.Errorf("no dialog found for CANCEL")
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	d.State = DialogStateTerminated
	return d, nil
}

// HandleACK records an ACK on the matching dialog. For UAS dialogs that
// have reached the non-2xx Completed state, this transitions the dialog
// to Terminated. For 2xx ACKs the dialog remains or becomes Confirmed.
func (m *Manager) HandleACK(msg *parser.SIPMsg) (*Dialog, error) {
	if m == nil {
		return nil, fmt.Errorf("nil manager")
	}
	if msg == nil {
		return nil, fmt.Errorf("nil SIP message")
	}
	if msg.Method() != parser.MethodACK {
		return nil, fmt.Errorf("expected ACK, got %s", parser.MethodName(msg.Method()))
	}
	d, _, _ := m.lookupDialogByMsg(msg)
	if d == nil {
		return nil, fmt.Errorf("no dialog found for ACK")
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	switch d.State {
	case DialogStateEarly:
		// 2xx ACK: confirm the dialog.
		d.State = DialogStateConfirmed
	case DialogStateConfirmed:
		// no-op – already confirmed.
	default:
		// Completed or Terminated – leave state alone for non-2xx ACKs.
	}
	return d, nil
}

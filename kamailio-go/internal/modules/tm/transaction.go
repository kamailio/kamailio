// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Transaction management - matching C t_funcs.h / t_lookup.h
 */

package tm

import (
	"errors"
	"fmt"
	"net"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// Manager manages SIP transactions
// C: global tm state
type Manager struct {
	table     *Table
	mutex     sync.RWMutex
	currentT  *Cell
	globalCtx uint32
	timerMgr  *TimerManager
	listeners []*transport.UDPListener
	callbacks RouteCallbacks
}

// NewManager creates a new transaction manager
func NewManager(tableSize uint32) *Manager {
	return &Manager{
		table: NewTable(tableSize),
	}
}

// NewManagerWithTimers creates a new transaction manager with an integrated
// TimerManager. The TimerManager is linked back to this Manager so that
// timer callbacks (retransmit, timeout, cleanup) can invoke send/remove
// operations.
func NewManagerWithTimers(tableSize uint32) *Manager {
	mgr := &Manager{
		table:    NewTable(tableSize),
		timerMgr: NewTimerManager(),
	}
	if mgr.timerMgr != nil {
		mgr.timerMgr.SetManager(mgr)
	}
	return mgr
}

// TimerManager returns the integrated TimerManager, or nil if this
// Manager was created without timers.
func (m *Manager) TimerManager() *TimerManager {
	return m.timerMgr
}

// AddListener registers a UDP listener that can be used for sending
// replies and forwarded requests.
func (m *Manager) AddListener(listener *transport.UDPListener) {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.listeners = append(m.listeners, listener)
}

// GetTable returns the transaction table
func (m *Manager) GetTable() *Table {
	return m.table
}

// GetT returns the current transaction
// C: struct cell *get_t(void)
func (m *Manager) GetT() *Cell {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	return m.currentT
}

// SetT sets the current transaction
func (m *Manager) SetT(t *Cell) {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.currentT = t
}

// UnsetT unsets the current transaction
// C: void t_unset(void)
func (m *Manager) UnsetT() {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.currentT = nil
}

// sendBuffer sends raw bytes to the given UDP destination via registered
// listeners. If no listeners are registered, it is a no-op (useful in
// unit tests that don't exercise the network stack).
func (m *Manager) sendBuffer(data []byte, dst *net.UDPAddr) {
	if dst == nil {
		return
	}
	m.mutex.RLock()
	listeners := make([]*transport.UDPListener, len(m.listeners))
	copy(listeners, m.listeners)
	m.mutex.RUnlock()
	for _, l := range listeners {
		_ = l.Send(dst, data)
	}
}

// removeCell removes a transaction cell from the table. Safe to call
// from timer callbacks.
func (m *Manager) removeCell(cell *Cell) {
	if cell == nil {
		return
	}
	m.table.Remove(cell)
}

// sendReply builds a reply from the given request and status code,
// serializes it, and sends it via registered UDP listeners.
//
// If no listeners are registered, only the serialization step is
// performed (useful for unit tests that verify message construction
// without network operations).
func (m *Manager) sendReply(request *parser.SIPMsg, reply *parser.SIPMsg, dstHost string, dstPort uint16) error {
	if reply == nil {
		return errors.New("nil reply")
	}

	data, err := parser.BuildMessage(reply)
	if err != nil {
		return fmt.Errorf("build message: %w", err)
	}

	m.mutex.RLock()
	hasListeners := len(m.listeners) > 0
	listeners := make([]*transport.UDPListener, len(m.listeners))
	copy(listeners, m.listeners)
	m.mutex.RUnlock()

	if !hasListeners {
		return nil
	}

	dst := &net.UDPAddr{
		IP:   net.ParseIP(dstHost),
		Port: int(dstPort),
	}
	if dst.IP == nil {
		ips, err := net.LookupIP(dstHost)
		if err != nil || len(ips) == 0 {
			return fmt.Errorf("resolve destination host %q", dstHost)
		}
		dst.IP = ips[0]
	}

	for _, l := range listeners {
		_ = l.Send(dst, data)
	}
	return nil
}

// SendReply creates a reply for the given request with the specified
// status code and reason, and sends it through registered UDP listeners
// to the destination host/port.
func (m *Manager) SendReply(request *parser.SIPMsg, statusCode int, reason string, dstHost string, dstPort uint16) error {
	if request == nil {
		return errors.New("nil request")
	}

	opts := parser.ReplyOptions{
		StatusCode:   statusCode,
		ReasonPhrase: reason,
	}
	reply, err := parser.CreateReply(request, opts)
	if err != nil {
		return fmt.Errorf("create reply: %w", err)
	}

	return m.sendReply(request, reply, dstHost, dstPort)
}

// ForwardRequest builds a forwarded request (decrement Max-Forwards,
// prepend a new Via, update R-URI to next hop), serializes it, and
// sends it through registered UDP listeners to the specified next hop.
func (m *Manager) ForwardRequest(msg *parser.SIPMsg, nextHopURI string, proxyHost string, proxyPort int, dstHost string, dstPort uint16) error {
	if msg == nil {
		return errors.New("nil message")
	}

	fwd, err := parser.BuildForwardRequest(msg, "UDP", proxyHost, proxyPort, nextHopURI)
	if err != nil {
		return fmt.Errorf("build forward request: %w", err)
	}

	data, err := parser.BuildMessage(fwd)
	if err != nil {
		return fmt.Errorf("build message: %w", err)
	}

	m.mutex.RLock()
	listeners := make([]*transport.UDPListener, len(m.listeners))
	copy(listeners, m.listeners)
	m.mutex.RUnlock()

	dst := &net.UDPAddr{
		IP:   net.ParseIP(dstHost),
		Port: int(dstPort),
	}
	if dst.IP == nil {
		ips, err := net.LookupIP(dstHost)
		if err != nil || len(ips) == 0 {
			return fmt.Errorf("resolve destination host %q", dstHost)
		}
		dst.IP = ips[0]
	}

	for _, l := range listeners {
		_ = l.Send(dst, data)
	}
	return nil
}

// NewTransaction creates a new transaction from a SIP message
// C: int t_newtran(struct sip_msg *p_msg)
func (m *Manager) NewTransaction(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// Check if transaction already exists (use branch-aware lookup to match storage hash)
	var dupCallID str.Str
	var dupCSeq str.Str
	var dupViaBranch str.Str
	if msg.CallID != nil {
		dupCallID = msg.CallID.Body
	}
	if msg.CSeq != nil {
		if cb, err := parser.ParseCSeqHeader(msg.CSeq); err == nil {
			dupCSeq = str.Mk(fmt.Sprintf("%d", cb.Number))
		}
	}
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		dupViaBranch = vb.Branch.Value
	} else if msg.HdrVia1 != nil {
		rawBody := msg.HdrVia1.Body.String()
		idx := strings.Index(rawBody, "branch=")
		if idx >= 0 {
			val := rawBody[idx+7:]
			if end := strings.IndexAny(val, "; \r\n"); end >= 0 {
				val = val[:end]
			}
			dupViaBranch = str.Mk(val)
		}
	}
	existing := m.table.Lookup(dupCallID, dupCSeq, dupViaBranch)
	if existing != nil {
		existing.Unref()
		return nil, errors.New("transaction already exists")
	}

	// Create new cell
	cell := &Cell{
		CreatedAt: time.Now(),
		State:     TStateTrying,
	}

	// Extract key fields
	if msg.From != nil {
		cell.FromHdr = msg.From.Body
	}
	if msg.CallID != nil {
		cell.CallIDHdr = msg.CallID.Body
		cell.CallIDVal = msg.CallID.Body
	}
	if msg.CSeq != nil {
		cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
		if err == nil {
			cell.CSeqHdrN = cseqBody.Method
			cell.CSeqNum = str.Mk(fmt.Sprintf("%d", cseqBody.Number))
			cell.CSeqMet = cseqBody.Method
		}
	}
	if msg.To != nil {
		cell.ToHdr = msg.To.Body
	}

	// Extract Via branch for transaction matching
	// The parsed Via body may be available via GetParsedVia(); otherwise fall
	// back to manually scanning the raw Via header body for the branch= param.
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		cell.ViaBranch = vb.Branch.Value
	} else if msg.HdrVia1 != nil {
		// Fallback: scan the raw header body for branch=
		rawBody := msg.HdrVia1.Body.String()
		idx := strings.Index(rawBody, "branch=")
		if idx >= 0 {
			val := rawBody[idx+7:]
			if end := strings.IndexAny(val, "; \r\n"); end >= 0 {
				val = val[:end]
			}
			cell.ViaBranch = str.Mk(val)
		}
	}

	// Set method
	if msg.IsRequest() {
		cell.Method = msg.FirstLine.Req.Method
		if msg.Method() == parser.MethodInvite {
			cell.Flags |= TIsInvite
		}
	}

	// Check if this is a local transaction (e.g., locally generated CANCEL or ACK)
	// Local transactions have the TIsLocal flag set externally.

	// Set hash index using the request's Via branch (same branch will appear
	// in the response, so LookupReply can find this cell by the same hash).
	cell.HashIndex = m.table.Hash(cell.CallIDVal, cell.CSeqNum, cell.ViaBranch)

	// Insert into table
	m.table.Insert(cell)

	// Set as current transaction
	m.SetT(cell)

	return cell, nil
}

// NewLocalTransaction creates a local transaction (e.g., for locally
// generated CANCEL or ACK requests). Local transactions are marked with
// TIsLocal and skip the duplicate detection check.
// C: t_newtran() with T_IS_LOCAL
func (m *Manager) NewLocalTransaction(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	cell := &Cell{
		CreatedAt: time.Now(),
		State:     TStateTrying,
	}

	// Extract key fields
	if msg.From != nil {
		cell.FromHdr = msg.From.Body
	}
	if msg.CallID != nil {
		cell.CallIDHdr = msg.CallID.Body
		cell.CallIDVal = msg.CallID.Body
	}
	if msg.CSeq != nil {
		cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
		if err == nil {
			cell.CSeqHdrN = cseqBody.Method
			cell.CSeqNum = str.Mk(fmt.Sprintf("%d", cseqBody.Number))
			cell.CSeqMet = cseqBody.Method
		}
	}
	if msg.To != nil {
		cell.ToHdr = msg.To.Body
	}

	// Extract Via branch
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		cell.ViaBranch = vb.Branch.Value
	} else if msg.HdrVia1 != nil {
		rawBody := msg.HdrVia1.Body.String()
		idx := strings.Index(rawBody, "branch=")
		if idx >= 0 {
			val := rawBody[idx+7:]
			if end := strings.IndexAny(val, "; \r\n"); end >= 0 {
				val = val[:end]
			}
			cell.ViaBranch = str.Mk(val)
		}
	}

	// Set method
	if msg.IsRequest() {
		cell.Method = msg.FirstLine.Req.Method
		cell.MethodValue = msg.Method()
		if msg.Method() == parser.MethodInvite {
			cell.Flags |= TIsInvite
		}
	}

	// Mark as local transaction
	cell.Flags |= TIsLocal

	// Set hash and insert
	cell.HashIndex = m.table.Hash(cell.CallIDVal, cell.CSeqNum, cell.ViaBranch)
	m.table.Insert(cell)
	m.SetT(cell)

	return cell, nil
}

// IsLocalTransaction returns true if the current transaction is local.
// C: int t_is_local(struct sip_msg *p_msg)
func (m *Manager) IsLocalTransaction() bool {
	cell := m.GetT()
	return cell != nil && cell.IsLocal()
}

// LookupRequest looks up a transaction for a request.
// For INVITE/CANCEL/etc. the Via branch is used to find the matching transaction,
// matching the branch-aware hash that NewTransaction uses when inserting.
func (m *Manager) LookupRequest(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// For ACK, need special handling (ACK to 2xx has no branch; other ACKs use branch)
	if msg.Method() == parser.MethodACK {
		return m.LookupACK(msg)
	}

	// For CANCEL, look up the INVITE transaction (same branch)
	if msg.Method() == parser.MethodCancel {
		return m.LookupCancel(msg)
	}

	// Extract Via branch for branch-aware lookup (same hash as NewTransaction)
	var viaBranch str.Str
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		viaBranch = vb.Branch.Value
	} else if msg.Via1 != nil && msg.Via1.Branch != nil {
		viaBranch = msg.Via1.Branch.Value
	}

	// Extract Call-ID and CSeq
	if msg.CallID == nil || msg.CSeq == nil {
		return nil, errors.New("missing Call-ID or CSeq")
	}
	callID := msg.CallID.Body
	cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
	if err != nil {
		return nil, errors.New("missing CSeq")
	}
	cseqStr := str.Mk(fmt.Sprintf("%d", cseqBody.Number))

	// Use branch-aware lookup to match NewTransaction's hash
	cell := m.table.Lookup(callID, cseqStr, viaBranch)
	if cell != nil {
		m.SetT(cell)
		return cell, nil
	}

	return nil, errors.New("transaction not found")
}

// LookupReply looks up a transaction for a reply.
// It first tries branch-aware lookup (matching NewTransaction's storage hash).
// If that fails and a Via branch was provided, it falls back to branch-agnostic
// lookup in the collision list — this handles proxy scenarios where the response's
// Via branch differs from the stored transaction's branch (proxy adds its own Via).
func (m *Manager) LookupReply(msg *parser.SIPMsg) (*Cell, int, error) {
	if msg == nil {
		return nil, -1, errors.New("null message")
	}

	// Extract Call-ID
	if msg.CallID == nil {
		return nil, -1, errors.New("missing Call-ID")
	}
	callID := msg.CallID.Body

	// Extract CSeq number
	cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
	if err != nil {
		return nil, -1, errors.New("missing CSeq")
	}
	cseqStr := str.Mk(fmt.Sprintf("%d", cseqBody.Number))

	// Extract Via branch for proper reply matching (RFC 3261).
	// Prefer the eagerly-parsed Via1; fall back to lazy parsing if needed.
	var viaBranch str.Str
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		viaBranch = vb.Branch.Value
	} else if msg.Via1 != nil && msg.Via1.Branch != nil {
		viaBranch = msg.Via1.Branch.Value
	}

	// Try branch-aware lookup first (same hash as NewTransaction uses)
	cell := m.table.Lookup(callID, cseqStr, viaBranch)
	if cell != nil {
		goto found
	}

	// Fallback: if a branch was provided but didn't match, search all entries
	// by Call-ID + CSeq. This handles proxy responses where the response's Via
	// branch differs from the stored transaction's branch.
	if viaBranch.Len > 0 {
		cell = m.table.LookupByCallIDCSeq(callID, cseqStr)
		if cell != nil {
			goto found
		}
	}

	return nil, -1, errors.New("no matching transaction")

found:
	// Find matching branch (first UAC without reply)
	for i, uac := range cell.UAC {
		if uac != nil && uac.Reply == nil {
			return cell, i, nil
		}
	}
	cell.Unref()
	return nil, -1, errors.New("no matching branch in transaction")
}

// LookupACK looks up a transaction for an ACK.
// ACK to 2xx responses does not create a transaction; it is matched
// to the original INVITE transaction using the Via branch.
func (m *Manager) LookupACK(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// Extract Via branch for branch-aware lookup
	var viaBranch str.Str
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		viaBranch = vb.Branch.Value
	} else if msg.Via1 != nil && msg.Via1.Branch != nil {
		viaBranch = msg.Via1.Branch.Value
	}

	// Extract Call-ID and CSeq
	if msg.CallID == nil || msg.CSeq == nil {
		return nil, errors.New("missing Call-ID or CSeq")
	}
	callID := msg.CallID.Body
	cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
	if err != nil {
		return nil, errors.New("missing CSeq")
	}
	cseqStr := str.Mk(fmt.Sprintf("%d", cseqBody.Number))

	cell := m.table.Lookup(callID, cseqStr, viaBranch)
	if cell == nil && viaBranch.Len > 0 {
		// Fallback: if a branch was provided but didn't match, search all entries
		// by Call-ID + CSeq. This handles proxy scenarios where the ACK's Via
		// branch differs from the stored INVITE's branch (proxy added its own Via).
		cell = m.table.LookupByCallIDCSeq(callID, cseqStr)
	}
	if cell != nil {
		if cell.IsInvite() {
			m.SetT(cell)
			return cell, nil
		}
		cell.Unref()
	}

	return nil, errors.New("no matching INVITE transaction for ACK")
}

// LookupCancel looks up a transaction for a CANCEL.
// CANCEL must use the same Via branch as the INVITE it cancels.
func (m *Manager) LookupCancel(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// Extract Via branch for branch-aware lookup
	var viaBranch str.Str
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		viaBranch = vb.Branch.Value
	} else if msg.Via1 != nil && msg.Via1.Branch != nil {
		viaBranch = msg.Via1.Branch.Value
	}

	// Extract Call-ID and CSeq
	if msg.CallID == nil || msg.CSeq == nil {
		return nil, errors.New("missing Call-ID or CSeq")
	}
	callID := msg.CallID.Body
	cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
	if err != nil {
		return nil, errors.New("missing CSeq")
	}
	cseqStr := str.Mk(fmt.Sprintf("%d", cseqBody.Number))

	cell := m.table.Lookup(callID, cseqStr, viaBranch)
	if cell != nil {
		m.SetT(cell)
		return cell, nil
	}

	return nil, errors.New("no matching transaction for CANCEL")
}

// ReleaseTransaction releases a transaction reference
// C: int t_unref(struct sip_msg *p_msg)
func (m *Manager) ReleaseTransaction(cell *Cell) {
	if cell == nil {
		return
	}

	if cell.Unref() {
		// Remove from table if ref count reaches 0
		m.table.Remove(cell)
	}

	// Clear current transaction if it matches
	if m.GetT() == cell {
		m.UnsetT()
	}
}

// AddBranch adds a new branch to a transaction
// C: int add_branch_label(struct cell *trans, struct sip_msg *p_msg, int branch)
func (m *Manager) AddBranch(cell *Cell, branch int) (*UAClient, error) {
	if cell == nil {
		return nil, errors.New("null cell")
	}

	cell.Lock()
	defer cell.Unlock()

	uac := &UAClient{
		LastReceived: 0,
	}

	// Expand UAC slice if needed
	for len(cell.UAC) <= branch {
		cell.UAC = append(cell.UAC, nil)
	}

	cell.UAC[branch] = uac
	cell.NrOfOutgoings++

	return uac, nil
}

// SetBranchResponse sets the response for a branch
func (m *Manager) SetBranchResponse(cell *Cell, branch int, status int) error {
	if cell == nil {
		return errors.New("null cell")
	}

	cell.Lock()
	defer cell.Unlock()

	if branch >= len(cell.UAC) || cell.UAC[branch] == nil {
		return errors.New("invalid branch")
	}

	cell.UAC[branch].LastReceived = status

	return nil
}

// GetBestResponse returns the best response across all branches
// RFC 3261: best response is the one with highest class, preferring 2xx
func (m *Manager) GetBestResponse(cell *Cell) (int, int) {
	if cell == nil {
		return 0, -1
	}

	cell.RLock()
	defer cell.RUnlock()

	bestStatus := 0
	bestBranch := -1

	for i, uac := range cell.UAC {
		if uac == nil || uac.LastReceived == 0 {
			continue
		}

		status := uac.LastReceived

		// Prefer 2xx responses
		if status >= 200 && status < 300 {
			if bestStatus < 200 || bestStatus >= 300 {
				bestStatus = status
				bestBranch = i
			} else if status > bestStatus {
				bestStatus = status
				bestBranch = i
			}
		} else if status >= 300 && status < 400 {
			if bestStatus < 200 || bestStatus >= 400 {
				bestStatus = status
				bestBranch = i
			}
		} else if status >= 400 && status < 500 {
			if bestStatus < 200 || bestStatus >= 500 {
				bestStatus = status
				bestBranch = i
			}
		} else if status >= 500 && status < 600 {
			if bestStatus < 200 || bestStatus >= 600 {
				bestStatus = status
				bestBranch = i
			}
		} else if status >= 600 {
			if bestStatus < 200 {
				bestStatus = status
				bestBranch = i
			}
		}
	}

	return bestStatus, bestBranch
}

// TransactionCount returns the total number of transactions in the table
func (m *Manager) TransactionCount() int {
	count := 0
	for _, entry := range m.table.Entries {
		entry.RLock()
		for cell := entry.NextC; cell != nil; cell = cell.NextC {
			count++
			if cell.NextC == entry.NextC {
				break
			}
		}
		entry.RUnlock()
	}
	return count
}

// SetCallbacks registers route callbacks for reply, failure, and branch-failure
// events. Pass nil for any callback you don't need.
func (m *Manager) SetCallbacks(cb RouteCallbacks) {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.callbacks = cb
}

// GetCallbacks returns the currently registered route callbacks.
func (m *Manager) GetCallbacks() RouteCallbacks {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	return m.callbacks
}

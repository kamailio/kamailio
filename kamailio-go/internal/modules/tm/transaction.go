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

	// Check if transaction already exists
	existing := m.table.LookupByMsg(msg)
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
	if msg.Via1 != nil && msg.Via1.Branch != nil {
		cell.ViaBranch = msg.Via1.Branch.Value
	}

	// Set method
	if msg.IsRequest() {
		cell.Method = msg.FirstLine.Req.Method
		if msg.Method() == parser.MethodInvite {
			cell.Flags |= TIsInvite
		}
	}

	// Set hash index
	cell.HashIndex = m.table.Hash(cell.CallIDVal, cell.CSeqNum, str.Str{})

	// Insert into table
	m.table.Insert(cell)

	// Set as current transaction
	m.SetT(cell)

	return cell, nil
}

// LookupRequest looks up a transaction for a request
// C: int t_lookup_request(struct sip_msg *p_msg, int leave_new_locked, int *canceled)
func (m *Manager) LookupRequest(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// For ACK, need special handling
	if msg.Method() == parser.MethodACK {
		return m.LookupACK(msg)
	}

	// For CANCEL, look up the INVITE transaction
	if msg.Method() == parser.MethodCancel {
		return m.LookupCancel(msg)
	}

	// Regular lookup
	cell := m.table.LookupByMsg(msg)
	if cell != nil {
		m.SetT(cell)
		return cell, nil
	}

	return nil, errors.New("transaction not found")
}

// LookupReply looks up a transaction for a reply
// C: int t_reply_matching(struct sip_msg *, int *)
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

	// Extract Via branch for proper reply matching (RFC 3261)
	var viaBranch str.Str
	if msg.Via1 != nil && msg.Via1.Branch != nil {
		viaBranch = msg.Via1.Branch.Value
	}

	// Lookup transaction with branch for proper reply matching
	cell := m.table.Lookup(callID, cseqStr, viaBranch)
	if cell != nil {
		// Find matching branch (first UAC without reply)
		for i, uac := range cell.UAC {
			if uac != nil && uac.Reply == nil {
				return cell, i, nil
			}
		}
		cell.Unref()
	}

	return nil, -1, errors.New("no matching transaction")
}

// LookupACK looks up a transaction for an ACK
// C: special handling for ACK
func (m *Manager) LookupACK(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// ACK matching is complex:
	// 1. For 2xx responses, ACK is a new transaction
	// 2. For non-2xx responses, ACK matches the INVITE transaction

	// Try to find matching INVITE transaction
	cell := m.table.LookupByMsg(msg)
	if cell != nil {
		if cell.IsInvite() {
			m.SetT(cell)
			return cell, nil
		}
		cell.Unref()
	}

	return nil, errors.New("no matching INVITE transaction for ACK")
}

// LookupCancel looks up a transaction for a CANCEL
// C: special handling for CANCEL
func (m *Manager) LookupCancel(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, errors.New("null message")
	}

	// CANCEL matches the INVITE transaction
	cell := m.table.LookupByMsg(msg)
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

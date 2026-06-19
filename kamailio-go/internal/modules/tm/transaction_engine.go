// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Transaction state machine - matching C t_reply.c / t_relay.c
 */

package tm

import (
	"fmt"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// RouteCallback is a function called by the TM engine when a specific
// event occurs (reply received, failure, branch failure). It receives
// the transaction cell, the branch index (if applicable), and the SIP
// message that triggered the event.
type RouteCallback func(cell *Cell, branch int, msg *parser.SIPMsg)

// RouteCallbacks holds all registered route callbacks.
type RouteCallbacks struct {
	OnReply         RouteCallback // called when a reply is received
	OnFailure       RouteCallback // called on final failure (timeout/3xx-6xx)
	OnBranchFailure RouteCallback // called when a specific branch fails
}

// isFinalResponse returns true if the status code is a final response
// RFC 3261: status >= 200 && status < 700
func isFinalResponse(status int) bool {
	return status >= 200 && status < 700
}

// isProvisionalResponse returns true if the status code is provisional
// RFC 3261: status >= 100 && status < 200
func isProvisionalResponse(status int) bool {
	return status >= 100 && status < 200
}

// is2xx returns true if the status code is in the 2xx range
func is2xx(status int) bool {
	return status >= 200 && status < 300
}

// IsCompleted returns true if the transaction is in a completed state
func (c *Cell) IsCompleted() bool {
	if c == nil {
		return false
	}
	return c.State == TStateCompleted || c.State == TStateConfirmed
}

// IsProceeding returns true if the transaction is in Trying or Proceeding state
func (c *Cell) IsProceeding() bool {
	if c == nil {
		return false
	}
	return c.State == TStateTrying || c.State == TStateProceeding
}

// UpdateState validates and applies a state transition per RFC 3261
// Allowed transitions:
//   TStateUndefined -> TStateTrying
//   TStateTrying    -> TStateProceeding | TStateCompleted | TStateCalling
//   TStateProceeding -> TStateCompleted | TStateConfirmed
//   TStateCalling   -> TStateProceedingUAC | TStateCompletedUAC
//   TStateProceedingUAC -> TStateCompletedUAC
func (c *Cell) UpdateState(newState TState) error {
	if c == nil {
		return fmt.Errorf("nil cell")
	}

	c.Lock()
	defer c.Unlock()

	// No-op transition (state is already at the desired state) is
	// always valid - this handles the case where HandleResponse
	// advanced the state and a subsequent RelayReply requests the
	// same transition.
	if c.State == newState {
		return nil
	}

	valid := false
	switch c.State {
	case TStateUndefined:
		valid = newState == TStateTrying
	case TStateTrying:
		valid = newState == TStateProceeding ||
			newState == TStateCompleted ||
			newState == TStateCalling
	case TStateProceeding:
		valid = newState == TStateCompleted ||
			newState == TStateConfirmed
	case TStateCompleted:
		valid = newState == TStateConfirmed || newState == TStateCompleted
	case TStateConfirmed:
		valid = newState == TStateConfirmed
	case TStateCalling:
		valid = newState == TStateProceedingUAC ||
			newState == TStateCompletedUAC
	case TStateProceedingUAC:
		valid = newState == TStateCompletedUAC
	case TStateCompletedUAC:
		valid = newState == TStateCompletedUAC
	default:
		valid = false
	}

	if !valid {
		return fmt.Errorf("invalid state transition: %d -> %d", c.State, newState)
	}

	c.State = newState
	return nil
}

// HandleRequest dispatches a request to NewTransaction or LookupRequest based
// on the method. For CANCEL and ACK it looks up the existing transaction.
// For other methods, it creates a new transaction if one does not exist yet
// or returns an error on a duplicate.
//
// When an INVITE transaction is created and the Manager has an integrated
// TimerManager, the FR timer for branch 0 is started to cover the UAS side
// (awaiting an upstream response after relaying).
func (m *Manager) HandleRequest(msg *parser.SIPMsg) (*Cell, error) {
	if msg == nil {
		return nil, fmt.Errorf("nil message")
	}

	if !msg.IsRequest() {
		return nil, fmt.Errorf("not a request")
	}

	method := msg.Method()

	// For ACK and CANCEL, look up an existing transaction
	if method == parser.MethodACK || method == parser.MethodCancel {
		return m.LookupRequest(msg)
	}

	// Try to look up an existing transaction first - if it exists, return error
	if existing := m.table.LookupByMsg(msg); existing != nil {
		existing.Unref()
		return nil, fmt.Errorf("transaction already exists for %s",
			parser.MethodName(method))
	}

	// Create a new transaction
	cell, err := m.NewTransaction(msg)
	if err != nil {
		return nil, err
	}

	// If this Manager has timers and this is an INVITE, start the FR timer
	// for the initial branch (branch 0).
	if m.timerMgr != nil && cell.IsInvite() {
		m.timerMgr.StartFRTimer(cell, 0)
	}

	return cell, nil
}

// HandleResponse dispatches a response to LookupReply and updates the
// UAC state for the matched branch. When a final response is received,
// the FR and retransmit timers are stopped; for non-2xx a wait timer
// is started, and for 2xx a delete timer is started.
func (m *Manager) HandleResponse(msg *parser.SIPMsg) (*Cell, int, error) {
	if msg == nil {
		return nil, -1, fmt.Errorf("nil message")
	}

	if !msg.IsReply() {
		return nil, -1, fmt.Errorf("not a reply")
	}

	cell, branch, err := m.LookupReply(msg)
	if err != nil {
		return nil, -1, err
	}

	status := int(msg.StatusCode())

	// Update the UAC state for the matched branch, and record the
	// status on the UAS so that the transaction reflects the reply.
	cell.Lock()
	if branch >= 0 && branch < len(cell.UAC) && cell.UAC[branch] != nil {
		cell.UAC[branch].LastReceived = status
	}
	cell.UAS.Status = uint16(status)
	cell.Unlock()

	// Apply overall cell state based on response class. We drive the
	// state towards the terminal TStateCompleted / TStateProceeding
	// states regardless of whether the cell previously entered a
	// UAC-style Calling state — this keeps the observable transaction
	// status consistent for callers that only look at cell.State.
	switch {
	case isProvisionalResponse(status):
		if cell.State != TStateProceeding && cell.State != TStateCompleted {
			cell.Lock()
			cell.State = TStateProceeding
			cell.Unlock()
		}
		// Trigger on_reply callback for provisional responses
		if cb := m.GetCallbacks(); cb.OnReply != nil {
			cb.OnReply(cell, branch, msg)
		}
	case isFinalResponse(status):
		cell.Lock()
		cell.State = TStateCompleted
		cell.Unlock()

		// Trigger on_reply callback for final responses
		if cb := m.GetCallbacks(); cb.OnReply != nil {
			cb.OnReply(cell, branch, msg)
		}
		// Trigger on_failure callback for non-2xx final responses
		if !is2xx(status) {
			if cb := m.GetCallbacks(); cb.OnFailure != nil {
				cb.OnFailure(cell, branch, msg)
			}
		}

		// Stop the FR/retransmit timers on final response and start
		// the appropriate post-final timer (wait for non-2xx, delete
		// for 2xx).
		if m.timerMgr != nil {
			m.timerMgr.StopFRTimer(cell)
			m.timerMgr.StopRetransmitTimer(cell)
			if is2xx(status) {
				m.timerMgr.StartDeleteTimer(cell)
			} else {
				m.timerMgr.StartWaitTimer(cell)
			}
		}
	}

	return cell, branch, nil
}

// Reply implements the t_reply() equivalent: it updates the UAS state of
// the current transaction and records the response status + reason.
// When a final response is sent, the FR and retransmit timers are stopped
// and a wait/delete timer is started so the transaction eventually gets
// cleaned up.
func (m *Manager) Reply(msg *parser.SIPMsg, status int, reason string) error {
	if msg == nil {
		return fmt.Errorf("nil message")
	}

	cell := m.GetT()
	if cell == nil {
		// Try to locate the transaction from the message
		var err error
		cell, err = m.LookupRequest(msg)
		if err != nil {
			return fmt.Errorf("no transaction found: %w", err)
		}
	}

	// Update UAS information
	cell.Lock()
	cell.UAS.Status = uint16(status)
	cell.UAS.CancelReason = reason
	cell.Unlock()

	// Apply state transition based on response class
	switch {
	case isProvisionalResponse(status):
		return cell.UpdateState(TStateProceeding)
	case isFinalResponse(status):
		if err := cell.UpdateState(TStateCompleted); err != nil {
			return err
		}
		if m.timerMgr != nil {
			m.timerMgr.StopFRTimer(cell)
			m.timerMgr.StopRetransmitTimer(cell)
			if is2xx(status) {
				m.timerMgr.StartDeleteTimer(cell)
			} else {
				m.timerMgr.StartWaitTimer(cell)
			}
		}
		return nil
	default:
		return fmt.Errorf("invalid status code: %d", status)
	}
}

// RelayReply relays a received reply upstream. It finds the matching
// transaction and stores the reply information on the given branch so
// that subsequent state decisions can use it.
func (m *Manager) RelayReply(msg *parser.SIPMsg, branch int, status int, reason string) error {
	if msg == nil {
		return fmt.Errorf("nil message")
	}

	cell := m.GetT()
	if cell == nil {
		c, err := m.LookupRequest(msg)
		if err != nil {
			return fmt.Errorf("no transaction found: %w", err)
		}
		cell = c
	}

	cell.Lock()
	if branch < 0 || branch >= len(cell.UAC) || cell.UAC[branch] == nil {
		cell.Unlock()
		return fmt.Errorf("invalid branch: %d", branch)
	}

	cell.UAC[branch].LastReceived = status
	cell.RelayedReplyBranch = branch
	cell.UAS.Status = uint16(status)
	cell.UAS.CancelReason = reason
	cell.Unlock()

	// Update UAS state to reflect the reply being relayed
	switch {
	case isProvisionalResponse(status):
		return cell.UpdateState(TStateProceeding)
	case isFinalResponse(status):
		if err := cell.UpdateState(TStateCompleted); err != nil {
			return err
		}
		// Stop timers on final response and schedule cleanup
		if m.timerMgr != nil {
			m.timerMgr.StopFRTimer(cell)
			m.timerMgr.StopRetransmitTimer(cell)
			if is2xx(status) {
				m.timerMgr.StartDeleteTimer(cell)
			} else {
				m.timerMgr.StartWaitTimer(cell)
			}
		}
		return nil
	default:
		return fmt.Errorf("invalid status code: %d", status)
	}
}

// Cancel marks the cell as canceled and records the reason. When active
// branches exist, this triggers CANCEL semantics (flag-only at this layer;
// actual CANCEL request generation is handled by the transport layer).
// All timers associated with the transaction are stopped.
func (m *Manager) Cancel(cell *Cell, reason string) error {
	if cell == nil {
		return fmt.Errorf("nil cell")
	}

	cell.Lock()

	cell.Flags |= TCanceled
	cell.UAS.CancelReason = reason

	// Any active branches should be marked as canceled - the transaction
	// is effectively completed from the UAS perspective.
	cell.State = TStateCompleted

	cell.Unlock()

	if m.timerMgr != nil {
		m.timerMgr.StopAllTimers(cell)
	}

	return nil
}

// RelayRequest implements t_relay() core: add a new branch to the current
// transaction and mark it as relayed. Returns the cell, branch index, and
// any error. If the Manager has a TimerManager the branch FR timer is
// started.
func (m *Manager) RelayRequest(msg *parser.SIPMsg, dstAddr string, dstPort int) (*Cell, int, error) {
	if msg == nil {
		return nil, -1, fmt.Errorf("nil message")
	}

	if dstAddr == "" || dstPort <= 0 || dstPort > 65535 {
		return nil, -1, fmt.Errorf("invalid destination: %s:%d", dstAddr, dstPort)
	}

	// Ensure a transaction exists
	cell := m.GetT()
	if cell == nil {
		var err error
		cell, err = m.HandleRequest(msg)
		if err != nil {
			return nil, -1, err
		}
	}

	// Determine the next branch index
	cell.Lock()
	branch := cell.NrOfOutgoings
	cell.Unlock()

	uac, err := m.AddBranch(cell, branch)
	if err != nil {
		return nil, -1, err
	}

	// Encode the destination on the UAC for later use when generating
	// outgoing requests.
	cell.Lock()
	uac.DstURI = str.Mk(fmt.Sprintf("%s:%d", dstAddr, dstPort))
	uac.Flags |= 1 // mark as active branch
	cell.Unlock()

	// Move the cell to Calling state (request being forwarded)
	if err := cell.UpdateState(TStateCalling); err != nil {
		// It's acceptable if we cannot transition (e.g., already in
		// TStateCalling) - we still have the branch.
	}

	// Start the branch FR timer if available
	if m.timerMgr != nil {
		m.timerMgr.StartFRTimer(cell, branch)
	}

	return cell, branch, nil
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Retransmission handling - matching C timer.h / retransmission logic
 */

package tm

import (
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/log"
)

// Default timer values (RFC 3261)
const (
	DefaultT1 = 500 * time.Millisecond
	DefaultT2 = 4 * time.Second
	DefaultT4 = 5 * time.Second

	// Final response timeout
	DefaultFRTimeout    = 30 * time.Second
	DefaultFRInvTimeout = 120 * time.Second

	// Wait timeout (for completed transactions)
	DefaultWaitTimeout = 5 * time.Second

	// Delete timeout (for confirmed transactions)
	DefaultDeleteTimeout = 30 * time.Second
)

// TimerManager manages transaction timers
// C: timer.c / timer.h
type TimerManager struct {
	t1        time.Duration
	t2        time.Duration
	t4        time.Duration
	frTimeout time.Duration
	frInvTimeout time.Duration

	mu        sync.RWMutex
	timers    map[*Cell]*cellTimers
}

// cellTimers holds timers for a specific cell
type cellTimers struct {
	retrTimer    *time.Timer
	frTimer      *time.Timer
	waitTimer    *time.Timer
	deleteTimer  *time.Timer
}

// NewTimerManager creates a new timer manager
func NewTimerManager() *TimerManager {
	return &TimerManager{
		t1:           DefaultT1,
		t2:           DefaultT2,
		t4:           DefaultT4,
		frTimeout:    DefaultFRTimeout,
		frInvTimeout: DefaultFRInvTimeout,
		timers:       make(map[*Cell]*cellTimers),
	}
}

// StartRetransmitTimer starts the retransmission timer for a branch
// C: start_retr(rb)
func (tm *TimerManager) StartRetransmitTimer(cell *Cell, branch int) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	ct, ok := tm.timers[cell]
	if !ok {
		ct = &cellTimers{}
		tm.timers[cell] = ct
	}

	// Stop existing timer
	if ct.retrTimer != nil {
		ct.retrTimer.Stop()
	}

	// Calculate initial retransmission interval (T1)
	interval := tm.t1

	ct.retrTimer = time.AfterFunc(interval, func() {
		tm.handleRetransmit(cell, branch, interval)
	})
}

// StopRetransmitTimer stops the retransmission timer
func (tm *TimerManager) StopRetransmitTimer(cell *Cell) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if ct, ok := tm.timers[cell]; ok && ct.retrTimer != nil {
		ct.retrTimer.Stop()
		ct.retrTimer = nil
	}
}

// StartFRTimer starts the final response timer
// C: start_fr(rb)
func (tm *TimerManager) StartFRTimer(cell *Cell) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	ct, ok := tm.timers[cell]
	if !ok {
		ct = &cellTimers{}
		tm.timers[cell] = ct
	}

	if ct.frTimer != nil {
		ct.frTimer.Stop()
	}

	timeout := tm.frTimeout
	if cell.IsInvite() {
		timeout = tm.frInvTimeout
	}

	ct.frTimer = time.AfterFunc(timeout, func() {
		tm.handleFRTimeout(cell)
	})
}

// StopFRTimer stops the final response timer
func (tm *TimerManager) StopFRTimer(cell *Cell) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if ct, ok := tm.timers[cell]; ok && ct.frTimer != nil {
		ct.frTimer.Stop()
		ct.frTimer = nil
	}
}

// StartWaitTimer starts the wait timer (for completed transactions)
// C: put_on_wait(t)
func (tm *TimerManager) StartWaitTimer(cell *Cell) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	ct, ok := tm.timers[cell]
	if !ok {
		ct = &cellTimers{}
		tm.timers[cell] = ct
	}

	if ct.waitTimer != nil {
		ct.waitTimer.Stop()
	}

	cell.State = TStateCompleted
	cell.WaitStart = time.Now()

	ct.waitTimer = time.AfterFunc(DefaultWaitTimeout, func() {
		tm.handleWaitTimeout(cell)
	})
}

// StartDeleteTimer starts the delete timer (for confirmed transactions)
func (tm *TimerManager) StartDeleteTimer(cell *Cell) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	ct, ok := tm.timers[cell]
	if !ok {
		ct = &cellTimers{}
		tm.timers[cell] = ct
	}

	if ct.deleteTimer != nil {
		ct.deleteTimer.Stop()
	}

	ct.deleteTimer = time.AfterFunc(DefaultDeleteTimeout, func() {
		tm.handleDeleteTimeout(cell)
	})
}

// handleRetransmit handles retransmission timeout
func (tm *TimerManager) handleRetransmit(cell *Cell, branch int, lastInterval time.Duration) {
	cell.RLock()
	defer cell.RUnlock()

	// Check if transaction is still active
	if cell.State == TStateCompleted || cell.State == TStateConfirmed {
		return
	}

	// Check if branch exists
	if branch >= len(cell.UAC) || cell.UAC[branch] == nil {
		return
	}

	uac := cell.UAC[branch]

	// Don't retransmit if we already have a final response
	if uac.LastReceived >= 200 {
		return
	}

	// Calculate next retransmission interval
	// RFC 3261: double the interval up to T2
	nextInterval := lastInterval * 2
	if nextInterval > tm.t2 {
		nextInterval = tm.t2
	}

	// TODO: M4 - Actually retransmit the request buffer
	log.Debug("Retransmitting request",
		log.String("callid", cell.CallIDVal.String()),
		log.Int("branch", branch),
		log.Int("status", uac.LastReceived),
	)

	// Schedule next retransmission
	tm.mu.Lock()
	if ct, ok := tm.timers[cell]; ok {
		ct.retrTimer = time.AfterFunc(nextInterval, func() {
			tm.handleRetransmit(cell, branch, nextInterval)
		})
	}
	tm.mu.Unlock()
}

// handleFRTimeout handles final response timeout
func (tm *TimerManager) handleFRTimeout(cell *Cell) {
	cell.Lock()
	defer cell.Unlock()

	// Transaction timed out - send 408 or 504
	log.Warn("Transaction timeout",
		log.String("callid", cell.CallIDVal.String()),
		log.String("method", cell.Method.String()),
	)

	// TODO: M4 - Send timeout response
	cell.State = TStateCompleted
}

// handleWaitTimeout handles wait timeout
func (tm *TimerManager) handleWaitTimeout(cell *Cell) {
	cell.Lock()
	defer cell.Unlock()

	// Move to delete state
	cell.State = TStateUndefined

	// TODO: M4 - Clean up transaction
	log.Debug("Transaction wait timeout",
		log.String("callid", cell.CallIDVal.String()),
	)
}

// handleDeleteTimeout handles delete timeout
func (tm *TimerManager) handleDeleteTimeout(cell *Cell) {
	// Transaction can be safely deleted
	log.Debug("Transaction delete timeout",
		log.String("callid", cell.CallIDVal.String()),
	)

	// Clean up timers
	tm.mu.Lock()
	delete(tm.timers, cell)
	tm.mu.Unlock()

	// TODO: M4 - Remove from table if ref count is 0
}

// StopAllTimers stops all timers for a cell
func (tm *TimerManager) StopAllTimers(cell *Cell) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if ct, ok := tm.timers[cell]; ok {
		if ct.retrTimer != nil {
			ct.retrTimer.Stop()
		}
		if ct.frTimer != nil {
			ct.frTimer.Stop()
		}
		if ct.waitTimer != nil {
			ct.waitTimer.Stop()
		}
		if ct.deleteTimer != nil {
			ct.deleteTimer.Stop()
		}
		delete(tm.timers, cell)
	}
}

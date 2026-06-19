// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Retransmission handling - matching C timer.h / retransmission logic
 */

package tm

import (
	"net"
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

// TMConfig holds configurable timer parameters for the transaction module.
// C: tm module parameters (fr_timer, fr_inv_timer, wt_timer, etc.)
type TMConfig struct {
	// T1: initial retransmission interval (RFC 3261 T1, default 500ms)
	T1 time.Duration
	// T2: maximum retransmission interval (RFC 3261 T2, default 4s)
	T2 time.Duration
	// T4: maximum transaction lifetime for INVITE (RFC 3261 T4, default 5s)
	T4 time.Duration
	// FRTimeout: final response timeout for non-INVITE (default 30s)
	FRTimeout time.Duration
	// FRInvTimeout: final response timeout for INVITE (default 64*T1 = 32s)
	FRInvTimeout time.Duration
	// WaitTimeout: time to keep transaction in Completed state (default 5s)
	WaitTimeout time.Duration
	// DeleteTimeout: time before deleting confirmed transaction (default 30s)
	DeleteTimeout time.Duration
	// MaxInvLifetime: maximum INVITE transaction lifetime (default 180s)
	MaxInvLifetime time.Duration
	// MaxNonInvLifetime: maximum non-INVITE transaction lifetime (default 32s)
	MaxNonInvLifetime time.Duration
	// NoisyCTimer: enable noisy C-timer (default false)
	NoisyCTimer bool
	// AutoInv100: automatically send 100 Trying for INVITEs (default true)
	AutoInv100 bool
	// ReplicateTimer: enable timer replication (default false)
	ReplicateTimer bool
}

// DefaultTMConfig returns a TMConfig with RFC 3261 default values.
func DefaultTMConfig() *TMConfig {
	return &TMConfig{
		T1:                DefaultT1,
		T2:                DefaultT2,
		T4:                DefaultT4,
		FRTimeout:         DefaultFRTimeout,
		FRInvTimeout:      DefaultFRInvTimeout,
		WaitTimeout:       DefaultWaitTimeout,
		DeleteTimeout:      DefaultDeleteTimeout,
		MaxInvLifetime:    180 * time.Second,
		MaxNonInvLifetime: 32 * time.Second,
		NoisyCTimer:       false,
		AutoInv100:        true,
		ReplicateTimer:    false,
	}
}

// TimerManager manages transaction timers
// C: timer.c / timer.h
type TimerManager struct {
	t1         time.Duration
	t2         time.Duration
	t4         time.Duration
	frTimeout  time.Duration
	frInvTimeout time.Duration

	// manager is a reference to the owning Manager for callbacks
	// (send buffer, remove transaction from table, etc.)
	manager *Manager

	mu     sync.RWMutex
	timers map[*Cell]*cellTimers
}

// cellTimers holds timers for a specific cell
type cellTimers struct {
	retrTimer   *time.Timer
	frTimer     *time.Timer
	waitTimer   *time.Timer
	deleteTimer *time.Timer
}

// NewTimerManager creates a new timer manager with default settings.
func NewTimerManager() *TimerManager {
	return NewTimerManagerWithConfig(DefaultTMConfig())
}

// NewTimerManagerWithConfig creates a new timer manager with the given config.
func NewTimerManagerWithConfig(cfg *TMConfig) *TimerManager {
	if cfg == nil {
		cfg = DefaultTMConfig()
	}
	return &TimerManager{
		t1:            cfg.T1,
		t2:            cfg.T2,
		t4:            cfg.T4,
		frTimeout:     cfg.FRTimeout,
		frInvTimeout:  cfg.FRInvTimeout,
		timers:        make(map[*Cell]*cellTimers),
	}
}

// SetManager links a Manager to this TimerManager so that timer
// callbacks can invoke send/remove operations.
func (tm *TimerManager) SetManager(mgr *Manager) {
	tm.mu.Lock()
	defer tm.mu.Unlock()
	tm.manager = mgr
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

// StartFRTimer starts the final response timer for a branch
// C: start_fr(rb)
func (tm *TimerManager) StartFRTimer(cell *Cell, branch int) {
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
		tm.handleFRTimeout(cell, branch)
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

// handleRetransmit handles retransmission timeout.
// If the transaction is still active and the branch has a retransmit
// buffer, the buffer is re-sent via the Manager's registered listeners.
//
// IMPORTANT: This callback must NOT blindly reschedule itself. It only
// reschedules when the TimerManager still has a live retrTimer for this
// cell — if the timer was stopped (e.g. by StopAllTimers / StopRetransmitTimer /
// handleFRTimeout), ct.retrTimer will be nil and we must stop.
func (tm *TimerManager) handleRetransmit(cell *Cell, branch int, lastInterval time.Duration) {
	cell.RLock()

	// Check if transaction is still active
	if cell.State == TStateCompleted || cell.State == TStateConfirmed {
		cell.RUnlock()
		return
	}

	// Check if branch exists
	if branch >= len(cell.UAC) || cell.UAC[branch] == nil {
		cell.RUnlock()
		return
	}

	uac := cell.UAC[branch]

	// Don't retransmit if we already have a final response
	if uac.LastReceived >= 200 {
		cell.RUnlock()
		return
	}

	// Capture buffer info under the read lock
	var sendBuf []byte
	var dst *net.UDPAddr
	if uac.Request != nil && len(uac.Request.Buffer) > 0 {
		sendBuf = uac.Request.Buffer
		if uac.Request.Dest != nil {
			if addr, ok := uac.Request.Dest.To.(*net.UDPAddr); ok {
				dst = addr
			}
		}
	}
	cell.RUnlock()

	// Calculate next retransmission interval
	nextInterval := lastInterval * 2
	if nextInterval > tm.t2 {
		nextInterval = tm.t2
	}

	// Actually retransmit if we have a buffer
	if len(sendBuf) > 0 {
		if tm.manager != nil {
			tm.manager.sendBuffer(sendBuf, dst)
		}
		log.Debug("Retransmitting request",
			log.String("callid", cell.CallIDVal.String()),
			log.Int("branch", branch),
		)
	}

	// Only reschedule if the TimerManager still tracks this cell AND the
	// retrTimer is still live. If StopRetransmitTimer / StopAllTimers was
	// called between the fire and this point, ct.retrTimer will be nil and
	// we must NOT create a new timer (that would loop forever).
	tm.mu.Lock()
	ct, ok := tm.timers[cell]
	if ok && ct.retrTimer != nil {
		ct.retrTimer = time.AfterFunc(nextInterval, func() {
			tm.handleRetransmit(cell, branch, nextInterval)
		})
	}
	// If !ok or ct.retrTimer == nil → timer was stopped, do NOT reschedule
	tm.mu.Unlock()
}

// handleFRTimeout handles final response timeout.
// Marks the transaction as timed out, stops all remaining timers (to
// prevent handleRetransmit from firing after the cell is removed),
// and schedules cleanup.
func (tm *TimerManager) handleFRTimeout(cell *Cell, branch int) {
	cell.Lock()

	// Transaction timed out
	log.Warn("Transaction timeout",
		log.String("callid", cell.CallIDVal.String()),
		log.String("method", cell.Method.String()),
		log.Int("branch", branch),
	)

	cell.State = TStateCompleted
	cell.Unlock()

	// Stop ALL timers first to prevent handleRetransmit from
	// scheduling new callbacks after we remove the cell.
	tm.StopAllTimers(cell)

	// Schedule cleanup via the manager
	if tm.manager != nil {
		tm.manager.removeCell(cell)
	}
}

// handleWaitTimeout handles wait timeout - removes the transaction
// from the manager's table so it can be garbage collected.
func (tm *TimerManager) handleWaitTimeout(cell *Cell) {
	cell.Lock()
	cell.State = TStateUndefined
	cell.Unlock()

	log.Debug("Transaction wait timeout, removing from table",
		log.String("callid", cell.CallIDVal.String()),
	)

	// Stop all timers and clean up tracking
	tm.StopAllTimers(cell)

	if tm.manager != nil {
		tm.manager.removeCell(cell)
	}
}

// handleDeleteTimeout handles delete timeout - removes the transaction
// from the manager's table.
func (tm *TimerManager) handleDeleteTimeout(cell *Cell) {
	log.Debug("Transaction delete timeout",
		log.String("callid", cell.CallIDVal.String()),
	)

	// Stop all timers and clean up tracking
	tm.StopAllTimers(cell)

	if tm.manager != nil {
		tm.manager.removeCell(cell)
	}
}

// StopAllTimers stops all timers for a cell and removes its entry.
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

// HasTimers returns true if there are any active timers for the cell.
// Used by tests to verify timer lifecycle.
func (tm *TimerManager) HasTimers(cell *Cell) bool {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	_, ok := tm.timers[cell]
	return ok
}

// StartMaxLifetimeTimer starts a timer that forcibly removes the transaction
// after the maximum allowed lifetime. For INVITE transactions this is
// MaxInvLifetime; for non-INVITE it is MaxNonInvLifetime.
func (tm *TimerManager) StartMaxLifetimeTimer(cell *Cell, maxLifetime time.Duration) {
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

	ct.deleteTimer = time.AfterFunc(maxLifetime, func() {
		tm.handleDeleteTimeout(cell)
	})
}

// SetConfig updates timer parameters at runtime.
func (tm *TimerManager) SetConfig(cfg *TMConfig) {
	if cfg == nil {
		return
	}
	tm.mu.Lock()
	defer tm.mu.Unlock()
	tm.t1 = cfg.T1
	tm.t2 = cfg.T2
	tm.t4 = cfg.T4
	tm.frTimeout = cfg.FRTimeout
	tm.frInvTimeout = cfg.FRInvTimeout
}

// GetConfig returns the current timer configuration.
func (tm *TimerManager) GetConfig() TMConfig {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	return TMConfig{
		T1:           tm.t1,
		T2:           tm.t2,
		T4:           tm.t4,
		FRTimeout:    tm.frTimeout,
		FRInvTimeout: tm.frInvTimeout,
	}
}

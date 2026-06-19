// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Dialog timer management - matching C dlg_timer.c
 *
 * Manages dialog lifetime timers:
 *   - Default lifetime: time before a dialog is considered expired
 *   - Inactive lifetime: time before an early dialog (no 2xx) is cleaned up
 *   - Ping interval: keepalive interval for NAT traversal
 */

package dialog

import (
	"sync"
	"time"
)

// TimerConfig holds dialog timer configuration.
type TimerConfig struct {
	// DefaultLifetime is the default dialog lifetime (default 7200s = 2h).
	DefaultLifetime time.Duration
	// InactiveLifetime is the lifetime for early dialogs (default 180s = 3min).
	InactiveLifetime time.Duration
	// PingInterval is the NAT ping interval (default 30s).
	PingInterval time.Duration
	// CleanupInterval is how often to check for expired dialogs (default 1s).
	CleanupInterval time.Duration
}

// DefaultTimerConfig returns default timer configuration.
func DefaultTimerConfig() *TimerConfig {
	return &TimerConfig{
		DefaultLifetime:  7200 * time.Second,
		InactiveLifetime: 180 * time.Second,
		PingInterval:     30 * time.Second,
		CleanupInterval:  1 * time.Second,
	}
}

// DialogTimer tracks a single dialog's timers.
type DialogTimer struct {
	dialogID  string
	lifetime  time.Duration
	createdAt time.Time
	timer     *time.Timer
	expired   bool
}

// TimerManager manages dialog timers.
type TimerManager struct {
	mu       sync.RWMutex
	config   *TimerConfig
	timers   map[string]*DialogTimer
	dialogs  *Manager // reference to dialog manager for cleanup
	onExpire func(dialogID string)
}

// NewTimerManager creates a new dialog timer manager.
func NewTimerManager(dlgMgr *Manager, onExpire func(string)) *TimerManager {
	return &TimerManager{
		config:   DefaultTimerConfig(),
		timers:   make(map[string]*DialogTimer),
		dialogs:  dlgMgr,
		onExpire: onExpire,
	}
}

// StartLifetimeTimer starts a timer for a dialog.
func (tm *TimerManager) StartLifetimeTimer(dialogID string, lifetime time.Duration) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// Stop existing timer if any
	if existing, ok := tm.timers[dialogID]; ok {
		if existing.timer != nil {
			existing.timer.Stop()
		}
	}

	dt := &DialogTimer{
		dialogID:  dialogID,
		lifetime:  lifetime,
		createdAt: time.Now(),
	}

	dt.timer = time.AfterFunc(lifetime, func() {
		tm.handleExpire(dialogID)
	})

	tm.timers[dialogID] = dt
}

// StopTimer stops the timer for a dialog.
func (tm *TimerManager) StopTimer(dialogID string) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if dt, ok := tm.timers[dialogID]; ok {
		if dt.timer != nil {
			dt.timer.Stop()
		}
		delete(tm.timers, dialogID)
	}
}

// RefreshTimer refreshes (restarts) the timer for a dialog.
func (tm *TimerManager) RefreshTimer(dialogID string) {
	tm.mu.Lock()
	dt, ok := tm.timers[dialogID]
	if !ok {
		tm.mu.Unlock()
		return
	}
	lifetime := dt.lifetime
	tm.mu.Unlock()

	tm.StartLifetimeTimer(dialogID, lifetime)
}

// handleExpire is called when a dialog timer fires.
func (tm *TimerManager) handleExpire(dialogID string) {
	tm.mu.Lock()
	dt, ok := tm.timers[dialogID]
	if !ok {
		tm.mu.Unlock()
		return
	}
	dt.expired = true
	delete(tm.timers, dialogID)
	tm.mu.Unlock()

	// Call the expire callback
	if tm.onExpire != nil {
		tm.onExpire(dialogID)
	}
}

// ActiveCount returns the number of active timers.
func (tm *TimerManager) ActiveCount() int {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	return len(tm.timers)
}

// HasTimer checks if a dialog has an active timer.
func (tm *TimerManager) HasTimer(dialogID string) bool {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	_, ok := tm.timers[dialogID]
	return ok
}

// SetConfig updates timer configuration.
func (tm *TimerManager) SetConfig(cfg *TimerConfig) {
	if cfg == nil {
		return
	}
	tm.mu.Lock()
	defer tm.mu.Unlock()
	tm.config = cfg
}

// GetConfig returns the current timer configuration.
func (tm *TimerManager) GetConfig() *TimerConfig {
	tm.mu.RLock()
	defer tm.mu.RUnlock()
	return tm.config
}

// CleanupExpired removes expired dialogs from the dialog manager.
func (tm *TimerManager) CleanupExpired() int {
	if tm.dialogs == nil {
		return 0
	}
	return tm.dialogs.CleanupExpired()
}

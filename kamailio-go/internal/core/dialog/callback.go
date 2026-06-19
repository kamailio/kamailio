// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Dialog callback system - matching C dlg_cb.c
 *
 * Provides event callbacks for dialog lifecycle events:
 *   - Created: new dialog established
 *   - Confirmed: dialog confirmed (2xx received)
 *   - Terminated: dialog terminated (BYE/timeout)
 *   - Expired: dialog lifetime expired
 */

package dialog

import (
	"sync"
)

// DialogEventType represents the type of dialog event.
type DialogEventType int

const (
	// DialogCreated is fired when a new dialog is created.
	DialogCreated DialogEventType = iota
	// DialogConfirmed is fired when a dialog is confirmed (2xx to INVITE).
	DialogConfirmed
	// DialogTerminated is fired when a dialog is terminated (BYE).
	DialogTerminated
	// DialogExpired is fired when a dialog lifetime expires.
	DialogExpired
	// DialogFailed is fired when a dialog setup fails (non-2xx final response).
	DialogFailed
)

// DialogEvent represents a dialog lifecycle event.
type DialogEvent struct {
	Type   DialogEventType
	Dialog *Dialog
	Msg    interface{} // optional SIP message that triggered the event
}

// DialogCallback is a function called when a dialog event occurs.
type DialogCallback func(event *DialogEvent)

// CallbackManager manages dialog callbacks.
type CallbackManager struct {
	mu        sync.RWMutex
	callbacks map[DialogEventType][]DialogCallback
}

// NewCallbackManager creates a new callback manager.
func NewCallbackManager() *CallbackManager {
	return &CallbackManager{
		callbacks: make(map[DialogEventType][]DialogCallback),
	}
}

// Register adds a callback for a specific event type.
// C: register_dlg_cb()
func (cm *CallbackManager) Register(eventType DialogEventType, cb DialogCallback) {
	cm.mu.Lock()
	defer cm.mu.Unlock()
	cm.callbacks[eventType] = append(cm.callbacks[eventType], cb)
}

// Unregister removes all callbacks for a specific event type.
func (cm *CallbackManager) Unregister(eventType DialogEventType) {
	cm.mu.Lock()
	defer cm.mu.Unlock()
	delete(cm.callbacks, eventType)
}

// Fire triggers all callbacks for a specific event type.
func (cm *CallbackManager) Fire(event *DialogEvent) {
	cm.mu.RLock()
	cbs := cm.callbacks[event.Type]
	cm.mu.RUnlock()

	for _, cb := range cbs {
		cb(event)
	}
}

// FireCreated fires all DialogCreated callbacks.
func (cm *CallbackManager) FireCreated(dlg *Dialog) {
	cm.Fire(&DialogEvent{Type: DialogCreated, Dialog: dlg})
}

// FireConfirmed fires all DialogConfirmed callbacks.
func (cm *CallbackManager) FireConfirmed(dlg *Dialog) {
	cm.Fire(&DialogEvent{Type: DialogConfirmed, Dialog: dlg})
}

// FireTerminated fires all DialogTerminated callbacks.
func (cm *CallbackManager) FireTerminated(dlg *Dialog) {
	cm.Fire(&DialogEvent{Type: DialogTerminated, Dialog: dlg})
}

// FireExpired fires all DialogExpired callbacks.
func (cm *CallbackManager) FireExpired(dlg *Dialog) {
	cm.Fire(&DialogEvent{Type: DialogExpired, Dialog: dlg})
}

// FireFailed fires all DialogFailed callbacks.
func (cm *CallbackManager) FireFailed(dlg *Dialog) {
	cm.Fire(&DialogEvent{Type: DialogFailed, Dialog: dlg})
}

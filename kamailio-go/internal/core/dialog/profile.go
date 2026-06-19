// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Dialog profile management - matching C dlg_profile.c
 *
 * Dialog profiles allow tracking dialogs by custom categories.
 * Each profile has a name and can track:
 *   - Count: number of dialogs in the profile
 *   - Value: sum of a per-dialog value
 *
 * Used for limiting concurrent calls, load balancing, etc.
 */

package dialog

import (
	"fmt"
	"sync"
	"sync/atomic"
)

// ProfileType indicates the profile type.
type ProfileType int

const (
	// ProfileTypeCount tracks the number of dialogs.
	ProfileTypeCount ProfileType = iota
	// ProfileTypeValue tracks the sum of per-dialog values.
	ProfileTypeValue
)

// ProfileEntry represents a single dialog's membership in a profile.
type ProfileEntry struct {
	DialogID string
	Value    int64
}

// Profile defines a dialog profile.
type Profile struct {
	Name  string
	Type  ProfileType
	mu    sync.RWMutex
	// For count profiles: map of dialog ID -> entry
	entries map[string]*ProfileEntry
	// For value profiles: total value
	totalValue int64
}

// NewProfile creates a new dialog profile.
func NewProfile(name string, ptype ProfileType) *Profile {
	return &Profile{
		Name:    name,
		Type:    ptype,
		entries: make(map[string]*ProfileEntry),
	}
}

// Add adds a dialog to the profile.
func (p *Profile) Add(dialogID string, value int64) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if _, exists := p.entries[dialogID]; exists {
		return // already in profile
	}

	p.entries[dialogID] = &ProfileEntry{
		DialogID: dialogID,
		Value:    value,
	}

	if p.Type == ProfileTypeValue {
		atomic.AddInt64(&p.totalValue, value)
	}
}

// Remove removes a dialog from the profile.
func (p *Profile) Remove(dialogID string) {
	p.mu.Lock()
	defer p.mu.Unlock()

	entry, exists := p.entries[dialogID]
	if !exists {
		return
	}

	if p.Type == ProfileTypeValue {
		atomic.AddInt64(&p.totalValue, -entry.Value)
	}

	delete(p.entries, dialogID)
}

// Count returns the number of dialogs in the profile.
func (p *Profile) Count() int {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return len(p.entries)
}

// Value returns the total value for value-type profiles.
func (p *Profile) Value() int64 {
	if p.Type == ProfileTypeCount {
		return int64(p.Count())
	}
	return atomic.LoadInt64(&p.totalValue)
}

// Has checks if a dialog is in the profile.
func (p *Profile) Has(dialogID string) bool {
	p.mu.RLock()
	defer p.mu.RUnlock()
	_, ok := p.entries[dialogID]
	return ok
}

// SetValue updates the value for a dialog in a value-type profile.
func (p *Profile) SetValue(dialogID string, newValue int64) {
	p.mu.Lock()
	defer p.mu.Unlock()

	entry, exists := p.entries[dialogID]
	if !exists {
		return
	}

	if p.Type == ProfileTypeValue {
		diff := newValue - entry.Value
		atomic.AddInt64(&p.totalValue, diff)
	}

	entry.Value = newValue
}

// ProfileManager manages all dialog profiles.
type ProfileManager struct {
	mu       sync.RWMutex
	profiles map[string]*Profile
}

// NewProfileManager creates a new profile manager.
func NewProfileManager() *ProfileManager {
	return &ProfileManager{
		profiles: make(map[string]*Profile),
	}
}

// DefineProfile creates a new profile definition.
func (pm *ProfileManager) DefineProfile(name string, ptype ProfileType) (*Profile, error) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	if _, exists := pm.profiles[name]; exists {
		return nil, fmt.Errorf("profile %q already exists", name)
	}

	p := NewProfile(name, ptype)
	pm.profiles[name] = p
	return p, nil
}

// GetProfile returns a profile by name.
func (pm *ProfileManager) GetProfile(name string) *Profile {
	pm.mu.RLock()
	defer pm.mu.RUnlock()
	return pm.profiles[name]
}

// GetOrCreateProfile returns a profile, creating it if it doesn't exist.
func (pm *ProfileManager) GetOrCreateProfile(name string, ptype ProfileType) *Profile {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	if p, ok := pm.profiles[name]; ok {
		return p
	}

	p := NewProfile(name, ptype)
	pm.profiles[name] = p
	return p
}

// AddToProfile adds a dialog to a named profile.
func (pm *ProfileManager) AddToProfile(profileName, dialogID string, value int64) error {
	p := pm.GetProfile(profileName)
	if p == nil {
		return fmt.Errorf("profile %q not found", profileName)
	}
	p.Add(dialogID, value)
	return nil
}

// RemoveFromProfile removes a dialog from a named profile.
func (pm *ProfileManager) RemoveFromProfile(profileName, dialogID string) error {
	p := pm.GetProfile(profileName)
	if p == nil {
		return fmt.Errorf("profile %q not found", profileName)
	}
	p.Remove(dialogID)
	return nil
}

// ProfileCount returns the count for a named profile.
func (pm *ProfileManager) ProfileCount(name string) (int, error) {
	p := pm.GetProfile(name)
	if p == nil {
		return 0, fmt.Errorf("profile %q not found", name)
	}
	return p.Count(), nil
}

// ProfileValue returns the value for a named profile.
func (pm *ProfileManager) ProfileValue(name string) (int64, error) {
	p := pm.GetProfile(name)
	if p == nil {
		return 0, fmt.Errorf("profile %q not found", name)
	}
	return p.Value(), nil
}

// AllProfiles returns all defined profiles.
func (pm *ProfileManager) AllProfiles() map[string]*Profile {
	pm.mu.RLock()
	defer pm.mu.RUnlock()
	result := make(map[string]*Profile, len(pm.profiles))
	for k, v := range pm.profiles {
		result[k] = v
	}
	return result
}

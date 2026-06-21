// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * User Location Service (usrloc) - matching C usrloc.c / ucontact.c
 *
 * Manages SIP registration contacts (AOR records) in memory.
 * Each Address of Record (AOR) can have multiple contacts with different
 * parameters (q value, expires, +sip.instance, reg-id, path, etc.).
 *
 * Per RFC 3261 Section 10.3:
 *   - AOR = sip:user@domain
 *   - Contacts are bound to an AOR via REGISTER
 *   - Contacts have an expiry time
 *   - Multiple contacts can be registered for the same AOR
 */

package usrloc

import (
	"fmt"
	"sort"
	"sync"
	"time"
)

// ContactFlag represents contact flags
type ContactFlag uint32

const (
	ContactFlagNone      ContactFlag = 0
	ContactFlagPermanent ContactFlag = 1 << 0 // permanent registration (no expiry)
	ContactFlagNatural   ContactFlag = 1 << 1 // natural expiry
	ContactFlagGruu      ContactFlag = 1 << 2 // GRUU contact
	ContactFlagTemp      ContactFlag = 1 << 3 // temporary contact
	ContactFlagReceived   ContactFlag = 1 << 4 // received from NAT
	ContactFlagPath      ContactFlag = 1 << 5 // path header present
	ContactFlagBflag     ContactFlag = 1 << 6 // branch flag
	ContactFlagIflag     ContactFlag = 1 << 7 // instance flag
)

// Contact represents a SIP registration contact (ucontact).
// C: struct ucontact
type Contact struct {
	AOR       string    // Address of Record (sip:user@domain)
	URI       string    // Contact URI
	Expires   time.Time // Expiration time
	Q         float32   // Q value (0.0 - 1.0)
	CSeq      uint32    // Last CSeq number
	Flags     ContactFlag
	Received  string    // Received IP:port (from Via received/rport)
	Path      string    // Path header value
	UserAgent string    // User-Agent header value
	CallID    string    // Call-ID of the REGISTER
	Instance  string    // +sip.instance parameter value
	RegID     uint32    // reg-id parameter value
	Methods   uint32    // Allowed methods bitmask
	LastModified time.Time
}

// IsExpired returns true if the contact has expired.
func (c *Contact) IsExpired() bool {
	return !c.Expires.IsZero() && time.Now().After(c.Expires)
}

// AOR represents an Address of Record with its contacts.
// C: struct udomain / struct hslot
type AOR struct {
	Key      string
	Contacts []*Contact
	mu       sync.RWMutex
}

// ContactCount returns the number of active (non-expired) contacts.
func (a *AOR) ContactCount() int {
	a.mu.RLock()
	defer a.mu.RUnlock()
	count := 0
	for _, c := range a.Contacts {
		if !c.IsExpired() {
			count++
		}
	}
	return count
}

// ActiveContacts returns all non-expired contacts sorted by Q value (descending).
func (a *AOR) ActiveContacts() []*Contact {
	a.mu.RLock()
	defer a.mu.RUnlock()

	var active []*Contact
	for _, c := range a.Contacts {
		if !c.IsExpired() {
			active = append(active, c)
		}
	}

	// Sort by Q value descending
	sort.Slice(active, func(i, j int) bool {
		return active[i].Q > active[j].Q
	})

	return active
}

// Domain represents a registration domain.
// C: struct udomain
//
// When a non-nil Backend is attached, Domain additionally forwards
// AddContact / RemoveContact / PurgeExpired writes to it.  The in-memory
// map always remains authoritative for local lookups.
type Domain struct {
	Name    string
	aors    map[string]*AOR
	mu      sync.RWMutex
	backend Backend
}

// NewDomain creates a new registration domain without a persistence backend.
func NewDomain(name string) *Domain {
	return &Domain{
		Name: name,
		aors: make(map[string]*AOR),
	}
}

// NewDomainWithBackend creates a new registration domain that additionally
// forwards contacts to the given Backend.  Passing nil is equivalent to
// NewDomain.
func NewDomainWithBackend(name string, backend Backend) *Domain {
	return &Domain{
		Name:    name,
		aors:    make(map[string]*AOR),
		backend: backend,
	}
}

// GetAOR returns the AOR record for the given address, or nil.
func (d *Domain) GetAOR(aor string) *AOR {
	d.mu.RLock()
	defer d.mu.RUnlock()
	return d.aors[aor]
}

// GetOrCreateAOR returns the AOR record, creating it if it doesn't exist.
func (d *Domain) GetOrCreateAOR(aor string) *AOR {
	d.mu.Lock()
	defer d.mu.Unlock()
	if a, ok := d.aors[aor]; ok {
		return a
	}
	a := &AOR{Key: aor}
	d.aors[aor] = a
	return a
}

// AddContact adds or updates a contact for the given AOR.
// If a contact with the same URI exists, it is updated.
// Returns the contact and true if new, false if updated.
//
// When a Backend is attached the contact is also persisted to it.
// Backend write errors are swallowed so the in-memory state always
// advances; callers that need to observe them can call
// Backend.UpsertContact() explicitly.
func (d *Domain) AddContact(aor string, contact *Contact) (*Contact, bool) {
	a := d.GetOrCreateAOR(aor)
	a.mu.Lock()
	defer a.mu.Unlock()

	isNew := true
	// Check if contact already exists (by URI)
	for i, existing := range a.Contacts {
		if existing.URI == contact.URI {
			// Update existing contact
			a.Contacts[i] = contact
			isNew = false
			break
		}
	}

	if isNew {
		a.Contacts = append(a.Contacts, contact)
	}

	if d.backend != nil {
		_ = d.backend.UpsertContact(d.Name, aor, contact)
	}

	return contact, isNew
}

// RemoveContact removes a contact from an AOR by URI.
// If a Backend is attached, the contact is also removed from it.
func (d *Domain) RemoveContact(aor, contactURI string) bool {
	a := d.GetAOR(aor)
	if a == nil {
		if d.backend != nil {
			_ = d.backend.RemoveContact(d.Name, aor, contactURI)
		}
		return false
	}
	a.mu.Lock()
	defer a.mu.Unlock()

	for i, c := range a.Contacts {
		if c.URI == contactURI {
			a.Contacts = append(a.Contacts[:i], a.Contacts[i+1:]...)
			if d.backend != nil {
				_ = d.backend.RemoveContact(d.Name, aor, contactURI)
			}
			return true
		}
	}
	return false
}

// PurgeExpired removes all expired contacts from all AORs.
// Returns the number of purged contacts.
//
// If a Backend is attached it is asked to purge stale records too.
// Backend errors are swallowed.
func (d *Domain) PurgeExpired() int {
	d.mu.RLock()
	aorKeys := make([]string, 0, len(d.aors))
	for k := range d.aors {
		aorKeys = append(aorKeys, k)
	}
	d.mu.RUnlock()

	purged := 0
	for _, key := range aorKeys {
		a := d.GetAOR(key)
		if a == nil {
			continue
		}
		a.mu.Lock()
		var remaining []*Contact
		for _, c := range a.Contacts {
			if c.IsExpired() {
				purged++
			} else {
				remaining = append(remaining, c)
			}
		}
		a.Contacts = remaining
		a.mu.Unlock()
	}

	if d.backend != nil {
		_, _ = d.backend.PurgeExpired(d.Name, time.Now())
	}

	return purged
}

// AORCount returns the number of AORs.
func (d *Domain) AORCount() int {
	d.mu.RLock()
	defer d.mu.RUnlock()
	return len(d.aors)
}

// TotalContactCount returns the total number of contacts (including expired).
func (d *Domain) TotalContactCount() int {
	d.mu.RLock()
	defer d.mu.RUnlock()
	count := 0
	for _, a := range d.aors {
		a.mu.RLock()
		count += len(a.Contacts)
		a.mu.RUnlock()
	}
	return count
}

// Lookup returns the contacts for a given AOR, sorted by Q value.
func (d *Domain) Lookup(aor string) []*Contact {
	a := d.GetAOR(aor)
	if a == nil {
		return nil
	}
	return a.ActiveContacts()
}

// Registrar handles SIP REGISTER requests.
// C: save_contacts() / lookup() from registrar.c
type Registrar struct {
	domains map[string]*Domain
	mu      sync.RWMutex
	defaultExpires time.Duration
}

// NewRegistrar creates a new registrar.
func NewRegistrar() *Registrar {
	return &Registrar{
		domains:        make(map[string]*Domain),
		defaultExpires: 3600 * time.Second,
	}
}

// SetDefaultExpires sets the default registration expiry.
func (r *Registrar) SetDefaultExpires(d time.Duration) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.defaultExpires = d
}

// GetDomain returns a domain by name, creating it if it doesn't exist.
func (r *Registrar) GetDomain(name string) *Domain {
	r.mu.Lock()
	defer r.mu.Unlock()
	if d, ok := r.domains[name]; ok {
		return d
	}
	d := NewDomain(name)
	r.domains[name] = d
	return d
}

// Register processes a REGISTER request:
//   - Extracts AOR from the To header URI
//   - Extracts contacts from the Contact header
//   - Applies expires value
//   - Stores/updates contacts in the domain
// Returns the AOR and the list of updated contacts.
func (r *Registrar) Register(domain string, aor string, contacts []*Contact) ([]*Contact, error) {
	d := r.GetDomain(domain)
	if d == nil {
		return nil, fmt.Errorf("domain %q not found", domain)
	}

	var result []*Contact
	for _, c := range contacts {
		if c.Expires.IsZero() {
			c.Expires = time.Now().Add(r.defaultExpires)
		}
		updated, isNew := d.AddContact(aor, c)
		result = append(result, updated)
		_ = isNew
	}

	return result, nil
}

// Query returns contacts for an AOR in a domain.
func (r *Registrar) Query(domain, aor string) []*Contact {
	r.mu.RLock()
	d, ok := r.domains[domain]
	r.mu.RUnlock()
	if !ok {
		return nil
	}
	return d.Lookup(aor)
}

// PurgeExpired purges expired contacts from all domains.
func (r *Registrar) PurgeExpired() int {
	r.mu.RLock()
	doms := make([]*Domain, 0, len(r.domains))
	for _, d := range r.domains {
		doms = append(doms, d)
	}
	r.mu.RUnlock()

	total := 0
	for _, d := range doms {
		total += d.PurgeExpired()
	}
	return total
}

// Stats returns registration statistics.
func (r *Registrar) Stats() map[string]int {
	r.mu.RLock()
	defer r.mu.RUnlock()
	stats := make(map[string]int)
	for name, d := range r.domains {
		stats[name] = d.AORCount()
	}
	return stats
}

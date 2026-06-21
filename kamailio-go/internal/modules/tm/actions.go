// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module - script-friendly action helpers.
 *
 * These free functions expose t_relay / t_reply / t_forward_non_invite /
 * t_lookup / t_is_loca-style entry points so that the script engine or the
 * proxy core can drive the transaction layer without directly operating on
 * a *Manager. The actual SIP reply/forward on the wire is the caller's
 * responsibility - these helpers only maintain the transaction state
 * machine and cell bookkeeping.
 *
 * The helpers are backed by a package-level Manager that must be installed
 * via SetDefaultManager before use. Passing an explicit nil Manager falls
 * back to the default so callers can uniformly write tm.TReply(msg, ...).
 */

package tm

import (
	"errors"
	"fmt"
	"strings"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// defaultManager holds the package-wide Manager used by the action helpers.
// Protected by defaultMu.
var (
	defaultMu      sync.RWMutex
	defaultManager *Manager
)

// SetDefaultManager installs mgr as the package-level Manager used by
// TReply / TRelay / TForwardNonInvite / TLookup / TIsLocal. Passing nil
// uninstalls the current default; subsequent action calls will return an
// error unless a Manager is supplied explicitly.
func SetDefaultManager(mgr *Manager) {
	defaultMu.Lock()
	defer defaultMu.Unlock()
	defaultManager = mgr
}

// getDefault returns the currently installed default Manager, or nil.
func getDefault() *Manager {
	defaultMu.RLock()
	defer defaultMu.RUnlock()
	return defaultManager
}

// resolve picks the provided Manager if non-nil, otherwise falls back to
// the default Manager. Returns an error if neither is available.
func resolve(mgr *Manager) (*Manager, error) {
	if mgr != nil {
		return mgr, nil
	}
	d := getDefault()
	if d == nil {
		return nil, errors.New("tm: no default manager installed; call SetDefaultManager or pass a *Manager")
	}
	return d, nil
}

// ErrNoTransaction is returned when an action requires an existing
// transaction cell but none could be located.
var ErrNoTransaction = errors.New("tm: no transaction cell found")

// ErrCompleted is returned when a caller attempts to send another response
// on a transaction that has already reached a final state (Completed /
// Confirmed / Destroyed) for an INVITE.
var ErrCompleted = errors.New("tm: cannot send additional response on completed/confirmed INVITE transaction")

// TReply implements the UAS side of Kamailio's t_reply(msg, status, reason).
//
// For INVITE transactions the state machine is enforced strictly:
//   - 1xx → TStateProceeding (may be repeated)
//   - 2xx → TStateCompleted (also recorded as Confirmed-ready for ACK matching)
//   - 3xx-6xx → TStateCompleted
//   - If the cell already reached Completed / Confirmed / Destroyed → ErrCompleted
//
// For non-INVITE requests a cell is looked up; if none exists the call is
// treated as a stateless reply and returns nil (the caller is expected to
// synthesise and transmit the response on the wire).
//
// mgr may be nil if SetDefaultManager has been called. msg must be the
// original request, _not_ the reply being generated.
func TReply(mgr *Manager, msg *parser.SIPMsg, statusCode int, reason string) error {
	m, err := resolve(mgr)
	if err != nil {
		return err
	}
	if msg == nil {
		return errors.New("tm.TReply: nil message")
	}
	if statusCode < 100 || statusCode >= 700 {
		return fmt.Errorf("tm.TReply: invalid status code %d", statusCode)
	}

	// Determine whether we're dealing with an INVITE or not.
	isInvite := msg.IsRequest() && msg.Method() == parser.MethodInvite

	// Try to locate an existing transaction cell, preferring the Manager's
	// current transaction then falling back to branch-aware lookup.
	cell := m.GetT()
	if cell == nil {
		if c, err := m.LookupRequest(msg); err == nil && c != nil {
			cell = c
		}
	}

	// No cell: for non-INVITE, treat as a stateless reply and return OK
	// (the caller is responsible for actually writing the SIP response).
	if cell == nil {
		if isInvite {
			// No cell at all for INVITE; create one so that later ACKs can
			// be matched by the proxy core. This mirrors Kamailio's default
			// behaviour where t_reply on an INVITE without an existing
			// transaction still creates one for reply-path bookkeeping.
			c, err := m.NewTransaction(msg)
			if err != nil {
				return fmt.Errorf("tm.TReply: %w", err)
			}
			cell = c
		} else {
			return nil
		}
	}

	// INVITE: reject further responses once the cell has reached a final
	// state. Non-INVITE also honour Completed semantics per RFC 3261.
	switch cell.State {
	case TStateCompleted, TStateConfirmed, TStateDestroyed:
		if isInvite {
			return ErrCompleted
		}
		// Non-INVITE: reject duplicate final replies for consistency.
		if statusCode >= 200 {
			return ErrCompleted
		}
	}

	// Apply state transition.
	switch {
	case statusCode >= 100 && statusCode < 200:
		cell.Lock()
		cell.UAS.Status = uint16(statusCode)
		cell.UAS.CancelReason = reason
		cell.Unlock()
		if err := cell.UpdateState(TStateProceeding); err != nil {
			return fmt.Errorf("tm.TReply: update state: %w", err)
		}
	case statusCode >= 200 && statusCode < 300:
		cell.Lock()
		cell.UAS.Status = uint16(statusCode)
		cell.UAS.CancelReason = reason
		cell.Unlock()
		if isInvite {
			// INVITE 2xx: record "confirmed" so subsequent ACKs can match
			// and duplicate 2xx retransmissions can be rejected. We keep
			// TStateCompleted as the primary terminal state in this phase;
			// callers that also want explicit ACK tracking can inspect the
			// TIsInvite flag.
			if err := cell.UpdateState(TStateCompleted); err != nil {
				return fmt.Errorf("tm.TReply: update state: %w", err)
			}
		} else {
			if err := cell.UpdateState(TStateCompleted); err != nil {
				return fmt.Errorf("tm.TReply: update state: %w", err)
			}
		}
	case statusCode >= 300 && statusCode < 700:
		cell.Lock()
		cell.UAS.Status = uint16(statusCode)
		cell.UAS.CancelReason = reason
		cell.Unlock()
		if err := cell.UpdateState(TStateCompleted); err != nil {
			return fmt.Errorf("tm.TReply: update state: %w", err)
		}
	default:
		return fmt.Errorf("tm.TReply: invalid status code %d", statusCode)
	}
	return nil
}

// TRelay implements Kamailio's t_relay semantics: ensure a transaction cell
// exists for the request and mark it as being relayed. For requests (not
// responses) this means:
//
//   - If a cell already exists → reuse it (retransmission handling by the
//     caller).
//   - If no cell exists → create a new UAS cell via NewTransaction /
//     HandleRequest.
//   - For INVITE requests the cell is left in TStateTrying; downstream
//     callers are responsible for later calling TReply (or RelayReply) with
//     the response status code to drive the state machine forward.
//   - For non-INVITE requests this degenerates to "create cell if needed".
//
// The caller is responsible for actually performing the forward on the
// wire using the returned cell. mgr may be nil if SetDefaultManager was
// called.
func TRelay(mgr *Manager, msg *parser.SIPMsg) (*Cell, error) {
	m, err := resolve(mgr)
	if err != nil {
		return nil, err
	}
	if msg == nil {
		return nil, errors.New("tm.TRelay: nil message")
	}

	// Responses are relayed through LookupReply + the caller's own path;
	// here we only accept requests.
	if !msg.IsRequest() {
		return nil, fmt.Errorf("tm.TRelay: message is not a request")
	}

	// Try to locate an existing cell first - if one is present we just
	// return it (the caller handles retransmission semantics).
	cell := m.GetT()
	if cell == nil {
		if c, err := m.LookupRequest(msg); err == nil && c != nil {
			cell = c
		}
	}

	if cell == nil {
		// No existing cell: create a new UAS cell. We use NewTransaction
		// directly so retransmission detection is preserved.
		cell, err = m.NewTransaction(msg)
		if err != nil {
			// If the table already has the cell (race condition on the
			// hash lookup above), fall back to HandleRequest which tries
			// LookupRequest again.
			if c, err2 := m.HandleRequest(msg); err2 == nil {
				cell = c
			} else {
				return nil, fmt.Errorf("tm.TRelay: %w", err)
			}
		}
	}

	// At this point the cell should be in TStateTrying (for new INVITE
	// cells) or possibly Proceeding/Completed if we just picked up an
	// existing cell. For INVITE we also ensure at least one UAC branch
	// placeholder exists so the caller's RelayReply code-path works.
	if cell != nil && cell.IsInvite() {
		cell.Lock()
		if len(cell.UAC) == 0 {
			cell.Unlock()
			if _, err := m.AddBranch(cell, 0); err != nil {
				return nil, fmt.Errorf("tm.TRelay: add branch: %w", err)
			}
		} else {
			cell.Unlock()
		}
	}

	return cell, nil
}

// TForwardNonInvite handles a non-INVITE request. It looks up any existing
// transaction cell and creates a new one if none is found. It does not
// touch the wire; it only manages the transaction-layer bookkeeping.
//
// Typical use: REGISTER, MESSAGE, OPTIONS, BYE, INFO, SUBSCRIBE, NOTIFY.
//
// The caller is responsible for performing the actual forward (if any).
func TForwardNonInvite(mgr *Manager, msg *parser.SIPMsg) (*Cell, error) {
	m, err := resolve(mgr)
	if err != nil {
		return nil, err
	}
	if msg == nil {
		return nil, errors.New("tm.TForwardNonInvite: nil message")
	}
	if !msg.IsRequest() {
		return nil, fmt.Errorf("tm.TForwardNonInvite: not a request")
	}
	if msg.Method() == parser.MethodInvite {
		return nil, fmt.Errorf("tm.TForwardNonInvite: must not be called for INVITE; use TRelay instead")
	}

	// Look up an existing cell first.
	cell := m.GetT()
	if cell == nil {
		if c, err := m.LookupRequest(msg); err == nil && c != nil {
			cell = c
		}
	}

	if cell == nil {
		cell, err = m.HandleRequest(msg)
		if err != nil {
			return nil, fmt.Errorf("tm.TForwardNonInvite: %w", err)
		}
	}
	return cell, nil
}

// TLookup exposes the unified lookup path used by the script engine:
//   - If msg is a request → LookupRequest(msg)
//   - If msg is a reply → LookupReply(msg) (branch returned is -1 when
//     the branch-agnostic path was used).
//
// Returns (nil, error) if no matching cell exists.
func TLookup(mgr *Manager, msg *parser.SIPMsg) (*Cell, error) {
	m, err := resolve(mgr)
	if err != nil {
		return nil, err
	}
	if msg == nil {
		return nil, errors.New("tm.TLookup: nil message")
	}

	switch {
	case msg.IsRequest():
		cell, err := m.LookupRequest(msg)
		if err != nil || cell == nil {
			return nil, ErrNoTransaction
		}
		return cell, nil
	case msg.IsReply():
		cell, _, err := m.LookupReply(msg)
		if err != nil || cell == nil {
			return nil, ErrNoTransaction
		}
		return cell, nil
	}
	return nil, ErrNoTransaction
}

// extractRURIHost returns the host portion of the request URI embedded in
// msg. URIs are expected to have the form "sip:user@host" or "sip:host";
// anything after the host (port, params, headers) is ignored. Returns ""
// when the URI cannot be parsed.
func extractRURIHost(uri string) string {
	if uri == "" {
		return ""
	}
	// Strip leading scheme if present: "sip:user@host" → "user@host"
	body := uri
	if idx := strings.Index(body, ":"); idx >= 0 {
		body = body[idx+1:]
	}
	// Strip user portion if present: "user@host" → "host"
	if idx := strings.LastIndex(body, "@"); idx >= 0 {
		body = body[idx+1:]
	}
	// Strip trailing params/headers/port separators (first ';', '?', or ':')
	stop := len(body)
	for i := 0; i < len(body); i++ {
		switch body[i] {
		case ';', '?':
			stop = i
			i = len(body)
		}
	}
	host := body[:stop]
	// Strip optional :port suffix from the host (host may itself contain
	// ':' for IPv6 literals in brackets, so only strip terminal :<digits>).
	if idx := strings.LastIndex(host, ":"); idx >= 0 && idx < len(host)-1 {
		// Only strip if everything after ':' is digits (port) AND the
		// portion before ':' is not a bare IPv6 bracket-open expression.
		tail := host[idx+1:]
		allDigits := true
		for j := 0; j < len(tail) && allDigits; j++ {
			if tail[j] < '0' || tail[j] > '9' {
				allDigits = false
			}
		}
		if allDigits && idx > 0 {
			// Only strip if the portion before ':' is not an IPv6 literal
			// opening bracket (very coarse heuristic).
			before := host[:idx]
			if !(strings.Contains(before, "[") && !strings.Contains(before, "]")) {
				host = before
			}
		}
	}
	return strings.ToLower(strings.TrimSpace(host))
}

// TIsLocalTransaction returns true when the request should be considered a
// "local" transaction (i.e. one where this node acts as the terminating
// UAS, for example for REGISTER or 401 challenge responses). It is named
// TIsLocalTransaction (not TIsLocal) to avoid colliding with the TM flag
// constant TIsLocal.
//
// A realm/domain list may be supplied; if empty the function returns true
// by default. Otherwise the request-URI's host portion is compared against
// the provided domains (case-insensitive). A request whose RURI host is
// not in the domains list returns false.
func TIsLocalTransaction(msg *parser.SIPMsg, domains []string) bool {
	if msg == nil || !msg.IsRequest() || msg.FirstLine == nil || msg.FirstLine.Req == nil {
		return true
	}
	if len(domains) == 0 {
		return true
	}

	ruri := msg.FirstLine.Req.URI
	if ruri.IsEmpty() {
		return true
	}
	host := extractRURIHost(ruri.String())
	if host == "" {
		return true
	}
	for _, d := range domains {
		if strings.ToLower(strings.TrimSpace(d)) == host {
			return true
		}
	}
	return false
}

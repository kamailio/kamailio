// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Topoh - topology hiding.
 *
 * Topoh anonymizes SIP messages as they transit the proxy so that
 * internal topology (call-IDs, tags, IP addresses and realms) is not
 * leaked to downstream peers. On egress the mapped values are used;
 * on ingress (for replies coming back) the original values are
 * restored before processing.
 *
 * This is a simple, intentionally minimal implementation:
 *
 *   - Call-ID          <->  "cid-N@realm"
 *   - From/To tag      <->  "tN"
 *   - Request-URI host  ->  realm / public IP (for outbound)
 *
 * It is the kamailio-go equivalent of the `topoh` / `topos`
 * modules.
 */

package topoh

import (
	"strings"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// HideStrategy configures which parts of a message get anonymized.
//
// Typical carrier-grade configuration:
//
//	HideStrategy{
//	    HideIPs:     true,
//	    HideDomains: true,
//	    HideTags:    true,
//	    HideCallID:  true,
//	    Realm:       "carrier.local",
//	    PublicIP:    "203.0.113.1",
//	}
type HideStrategy struct {
	HideIPs     bool
	HideDomains bool
	HideTags    bool
	HideCallID  bool
	Realm       string
	PublicIP    string
}

// Hider anonymizes SIP messages on send and reverses the mapping on
// receive. It is safe for concurrent use.
type Hider struct {
	mu        sync.RWMutex
	strategy  HideStrategy
	tagMap    map[string]string
	invTagMap map[string]string
	callIDMap map[string]string
	invCIDMap map[string]string
	counter   uint64
}

// NewDefault returns a Hider with sensible defaults (Call-IDs and IPs
// hidden; tags left alone; realm "hidden.local" and the RFC 5737
// documentation public IP "203.0.113.1").
func NewDefault() *Hider {
	return New(HideStrategy{
		HideIPs:     true,
		HideDomains: true,
		HideTags:    false,
		HideCallID:  true,
		Realm:       "hidden.local",
		PublicIP:    "203.0.113.1",
	})
}

// New constructs a Hider with the supplied strategy. A zero-value
// strategy hides nothing, which is a useful no-op during tests.
func New(strategy HideStrategy) *Hider {
	return &Hider{
		strategy:  strategy,
		tagMap:    make(map[string]string),
		invTagMap: make(map[string]string),
		callIDMap: make(map[string]string),
		invCIDMap: make(map[string]string),
	}
}

// HideForForward anonymizes msg as if it were about to be forwarded.
// Operates in-place on the parsed msg.
func (h *Hider) HideForForward(msg *parser.SIPMsg) {
	if h == nil || msg == nil {
		return
	}
	// Call-ID
	if h.strategy.HideCallID && msg.CallID != nil {
		internal := msg.CallID.Body.String()
		external := h.mapCallID(internal)
		replaceHeaderBody(msg.CallID, external)
	}
	// From/To tags
	if h.strategy.HideTags {
		if msg.From != nil {
			raw := msg.From.Body.String()
			if newVal, ok := h.replaceTagInValue(raw, "from"); ok {
				replaceHeaderBody(msg.From, newVal)
			}
		}
		if msg.To != nil {
			raw := msg.To.Body.String()
			if newVal, ok := h.replaceTagInValue(raw, "to"); ok {
				replaceHeaderBody(msg.To, newVal)
			}
		}
	}
	// Request-URI host portion -> public IP / realm
	if msg.FirstLine != nil && msg.FirstLine.Req != nil {
		if h.strategy.HideIPs {
			msg.FirstLine.Req.URI = str.Mk("sip:hidden@" + h.strategy.PublicIP)
		} else if h.strategy.HideDomains {
			msg.FirstLine.Req.URI = str.Mk("sip:hidden@" + h.strategy.Realm)
		}
	}
}

// HideForReply mirrors the call-id/tag hiding on an outgoing reply.
func (h *Hider) HideForReply(msg *parser.SIPMsg) {
	if h == nil || msg == nil {
		return
	}
	if h.strategy.HideCallID && msg.CallID != nil {
		internal := msg.CallID.Body.String()
		external := h.mapCallID(internal)
		replaceHeaderBody(msg.CallID, external)
	}
	if h.strategy.HideTags {
		if msg.From != nil {
			raw := msg.From.Body.String()
			if newVal, ok := h.replaceTagInValue(raw, "from"); ok {
				replaceHeaderBody(msg.From, newVal)
			}
		}
		if msg.To != nil {
			raw := msg.To.Body.String()
			if newVal, ok := h.replaceTagInValue(raw, "to"); ok {
				replaceHeaderBody(msg.To, newVal)
			}
		}
	}
}

// UnhideForProcessing reverses the mapping on an incoming message.
// This is the reverse of HideForForward - call it on messages
// received from the external network before processing them
// internally.
func (h *Hider) UnhideForProcessing(msg *parser.SIPMsg) {
	if h == nil || msg == nil {
		return
	}
	if h.strategy.HideCallID && msg.CallID != nil {
		external := msg.CallID.Body.String()
		h.mu.RLock()
		internal := h.callIDMap[external]
		h.mu.RUnlock()
		if internal != "" {
			replaceHeaderBody(msg.CallID, internal)
		}
	}
	if h.strategy.HideTags {
		if msg.From != nil {
			raw := msg.From.Body.String()
			if newVal, ok := h.restoreTagInValue(raw, "from"); ok {
				replaceHeaderBody(msg.From, newVal)
			}
		}
		if msg.To != nil {
			raw := msg.To.Body.String()
			if newVal, ok := h.restoreTagInValue(raw, "to"); ok {
				replaceHeaderBody(msg.To, newVal)
			}
		}
	}
}

// -------------------------------------------------------------------
// internal map helpers

// mapCallID returns the external representation for the given
// internal call-id. It creates a new mapping on first use.
func (h *Hider) mapCallID(internal string) string {
	h.mu.RLock()
	if e, ok := h.invCIDMap[internal]; ok {
		h.mu.RUnlock()
		return e
	}
	h.mu.RUnlock()
	h.mu.Lock()
	defer h.mu.Unlock()
	// double check after upgrade
	if e, ok := h.invCIDMap[internal]; ok {
		return e
	}
	h.counter++
	external := "cid-" + itoa(int(h.counter)) + "@" + h.strategy.Realm
	h.invCIDMap[internal] = external
	h.callIDMap[external] = internal
	return external
}

// mapTag returns the external tag for the given internal tag and
// role. Roles distinguish From-tag values from To-tag values so the
// same tag appearing in both headers still maps to distinct external
// values.
func (h *Hider) mapTag(internal, role string) string {
	h.mu.RLock()
	if e, ok := h.invTagMap[role+"#"+internal]; ok {
		h.mu.RUnlock()
		return e
	}
	h.mu.RUnlock()
	h.mu.Lock()
	defer h.mu.Unlock()
	key := role + "#" + internal
	if e, ok := h.invTagMap[key]; ok {
		return e
	}
	h.counter++
	external := "t" + itoa(int(h.counter))
	h.invTagMap[key] = external
	h.tagMap[external] = internal
	return external
}

// restoreTag returns the original internal value for the given
// external tag, or the empty string if it is unknown.
func (h *Hider) restoreTag(external, role string) string {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return h.tagMap[external]
}

// replaceTagInValue finds the "tag=XYZ" segment in a header body and
// replaces the value with the mapped external tag.
func (h *Hider) replaceTagInValue(raw, role string) (string, bool) {
	idx := strings.Index(raw, "tag=")
	if idx < 0 {
		return raw, false
	}
	start := idx + 4
	end := start
	for end < len(raw) && raw[end] != ';' && raw[end] != '\r' && raw[end] != '\n' {
		end++
	}
	internalTag := raw[start:end]
	externalTag := h.mapTag(internalTag, role)
	return raw[:start] + externalTag + raw[end:], true
}

// restoreTagInValue is the inverse of replaceTagInValue.
func (h *Hider) restoreTagInValue(raw, role string) (string, bool) {
	idx := strings.Index(raw, "tag=")
	if idx < 0 {
		return raw, false
	}
	start := idx + 4
	end := start
	for end < len(raw) && raw[end] != ';' && raw[end] != '\r' && raw[end] != '\n' {
		end++
	}
	externalTag := raw[start:end]
	internalTag := h.restoreTag(externalTag, role)
	if internalTag == "" {
		return raw, false
	}
	return raw[:start] + internalTag + raw[end:], true
}

// replaceHeaderBody overwrites the Body field of a parsed header.
func replaceHeaderBody(hdr *parser.HdrField, value string) {
	if hdr == nil {
		return
	}
	buf := make([]byte, len(value))
	copy(buf, value)
	hdr.Body = str.MkBytes(buf)
}

// -------------------------------------------------------------------
// small utilities

// itoa converts an int to its decimal string representation. Kept
// local to avoid pulling "strconv" for a tiny task.
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	buf := make([]byte, 0, 16)
	for n > 0 {
		buf = append([]byte{byte('0' + n%10)}, buf...)
		n /= 10
	}
	if neg {
		buf = append([]byte{'-'}, buf...)
	}
	return string(buf)
}

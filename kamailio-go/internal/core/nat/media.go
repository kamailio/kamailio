// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * NAT / Media rewrite pipeline
 *
 * Coordinates Contact URI rewriting plus RTP media bridging via an
 * optional RTPEngine instance. Implements the three-method handshake:
 * Offer (on INVITE), Answer (on 200 OK) and Delete (on BYE / CANCEL).
 *
 * When no RTPEngine client is provided, the pipeline still performs
 * Contact header rewriting using a public IP configured at construction
 * time. This is sufficient for simple NAT traversal when media flows
 * peer-to-peer.
 */

package nat

import (
	"fmt"
	"strings"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/rtpengine"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// MediaPipeline coordinates SIP Contact rewriting + RTP media bridging.
// It is safe for concurrent use. The zero value is NOT ready - construct
// via NewPipeline.
type MediaPipeline struct {
	mu       sync.Mutex
	engine   *rtpengine.RTPEngineClient
	sessions map[string]*mediaSession
	publicIP string
}

// mediaSession tracks a single call's lifecycle in the pipeline.
type mediaSession struct {
	FromTag    string
	ToTag      string
	OfferSent  bool
	AnswerSent bool
	Deleted    bool
}

// NewPipeline constructs a MediaPipeline. Pass nil for engine to disable
// RTPEngine bridging; Contact rewriting still works when publicIP is set.
func NewPipeline(engine *rtpengine.RTPEngineClient, publicIP string) *MediaPipeline {
	return &MediaPipeline{
		engine:   engine,
		sessions: make(map[string]*mediaSession),
		publicIP: publicIP,
	}
}

// OnInviteOffer runs on incoming INVITEs. It records a session keyed by
// Call-ID, rewrites the Contact host to the configured public IP (if any),
// and when an RTPEngine client is installed, asks the engine to offer the
// SDP body and rewrites the message body accordingly.
//
// The method returns the Call-ID tag that was derived (for logging) and
// any low-level error. Errors are non-fatal and returned for the caller
// to log - the request is still forwarded.
func (p *MediaPipeline) OnInviteOffer(msg *parser.SIPMsg) (string, error) {
	if p == nil || msg == nil {
		return "", fmt.Errorf("nil pipeline or message")
	}
	callID, fromTag := basicIdent(msg)
	if callID == "" {
		return "", fmt.Errorf("missing call-id")
	}

	p.mu.Lock()
	p.sessions[callID] = &mediaSession{FromTag: fromTag, OfferSent: true}
	p.mu.Unlock()

	if p.publicIP != "" {
		rewriteContactHost(msg, p.publicIP)
	}

	if p.engine != nil {
		sdp, err := extractSDP(msg)
		if err != nil {
			return callID, err
		}
		rewritten, err := p.engine.Offer(callID, fromTag, "media", sdp,
			rtpengine.MediaOptions{
				Direction: rtpengine.DirectionOffer,
				Transport: rtpengine.TransportUDP,
				Flags:     []string{"replace-origin", "replace-session-connection"},
			})
		if err != nil {
			return callID, err
		}
		setSDP(msg, rewritten)
	}
	return callID, nil
}

// OnAnswer runs on 200 OK. It records the To tag and when RTPEngine is
// installed, runs the Answer command and rewrites the body.
func (p *MediaPipeline) OnAnswer(msg *parser.SIPMsg) (string, error) {
	if p == nil || msg == nil {
		return "", fmt.Errorf("nil pipeline or message")
	}
	callID, fromTag := basicIdent(msg)
	toTag := extractToTag(msg)

	p.mu.Lock()
	if s, ok := p.sessions[callID]; ok {
		s.ToTag = toTag
		s.AnswerSent = true
	}
	p.mu.Unlock()

	if p.engine != nil {
		sdp, err := extractSDP(msg)
		if err != nil {
			return callID, err
		}
		rewritten, err := p.engine.Answer(callID, fromTag, toTag, sdp,
			rtpengine.MediaOptions{
				Direction: rtpengine.DirectionAnswer,
				Transport: rtpengine.TransportUDP,
				Flags:     []string{"replace-origin"},
			})
		if err != nil {
			return callID, err
		}
		setSDP(msg, rewritten)
	}
	return callID, nil
}

// OnBye releases the media session on call termination. Safe to call
// multiple times.
func (p *MediaPipeline) OnBye(msg *parser.SIPMsg) error {
	if p == nil || msg == nil {
		return fmt.Errorf("nil pipeline or message")
	}
	callID, fromTag := basicIdent(msg)
	toTag := extractToTag(msg)

	p.mu.Lock()
	if s, ok := p.sessions[callID]; ok {
		s.Deleted = true
		delete(p.sessions, callID)
	}
	p.mu.Unlock()

	if p.engine != nil {
		// We don't propagate RTPEngine errors; a stray delete is not fatal
		// for request processing.
		_ = p.engine.Delete(callID, fromTag, toTag)
	}
	return nil
}

// ActiveSessions returns the number of sessions currently tracked.
// Useful for metrics and tests.
func (p *MediaPipeline) ActiveSessions() int {
	if p == nil {
		return 0
	}
	p.mu.Lock()
	defer p.mu.Unlock()
	return len(p.sessions)
}

// ---------------------------------------------------------------------
// SIP message helpers
// ---------------------------------------------------------------------

// basicIdent pulls the Call-ID and from-tag from a parsed SIP message.
// Both fields are derived from the parser's quick-reference slots.
func basicIdent(msg *parser.SIPMsg) (callID, fromTag string) {
	if msg == nil {
		return "", ""
	}
	if msg.CallID != nil {
		callID = strings.TrimSpace(msg.CallID.Body.String())
	}
	if msg.From != nil {
		fromTag = tagParamValue(msg.From.Body.String())
	}
	return
}

// extractToTag extracts the tag parameter from the To header body.
func extractToTag(msg *parser.SIPMsg) string {
	if msg == nil || msg.To == nil {
		return ""
	}
	return tagParamValue(msg.To.Body.String())
}

// tagParamValue returns the value of the first "tag=" parameter in the
// header body string. Returns empty string if not found.
func tagParamValue(body string) string {
	idx := strings.Index(strings.ToLower(body), "tag=")
	if idx < 0 {
		return ""
	}
	rest := body[idx+len("tag="):]
	for j := 0; j < len(rest); j++ {
		if rest[j] == ';' || rest[j] == '\r' || rest[j] == '\n' || rest[j] == ' ' {
			return rest[:j]
		}
	}
	return rest
}

// rewriteContactHost replaces the Contact header URI host with the given
// IP address. The port, parameters and display-name are preserved. The
// rewrite is conservative: if we can't find a matching URI we leave the
// header alone.
func rewriteContactHost(msg *parser.SIPMsg, hostIP string) {
	if msg == nil || msg.Contact == nil || hostIP == "" {
		return
	}
	body := msg.Contact.Body.String()
	newBody := replaceURIHost(body, hostIP)
	if newBody != "" {
		msg.Contact.Body = str.Mk(newBody)
	}
}

// replaceURIHost replaces the host portion of a SIP URI with newHost.
// It handles a plain "sip:user@host:port;params" style URI or an address
// spec "<sip:host>". Returns "" if the input did not look like a URI.
func replaceURIHost(body, newHost string) string {
	body = strings.TrimSpace(body)
	if body == "" {
		return ""
	}
	// Strip surrounding <...> if present.
	inner := body
	prefix := ""
	suffix := ""
	if strings.HasPrefix(inner, "<") && strings.Contains(inner, ">") {
		idx := strings.Index(inner, "<")
		prefix = inner[:idx]
		inner = inner[idx+1:]
		endIdx := strings.Index(inner, ">")
		suffix = inner[endIdx:]
		inner = inner[:endIdx]
	}

	schemeIdx := strings.Index(inner, ":")
	if schemeIdx < 0 {
		return ""
	}
	scheme := inner[:schemeIdx+1]
	rest := inner[schemeIdx+1:]

	// Find the @ marker - sip:user@host... or sip:host...
	atIdx := strings.Index(rest, "@")
	userPart := ""
	hostStart := 0
	if atIdx >= 0 {
		userPart = rest[:atIdx+1]
		hostStart = atIdx + 1
	}
	hostPart := rest[hostStart:]

	// Now hostPart could be host:port;params...
	paramIdx := strings.IndexAny(hostPart, ";?")
	var host, tail string
	if paramIdx >= 0 {
		host = hostPart[:paramIdx]
		tail = hostPart[paramIdx:]
	} else {
		host = hostPart
		tail = ""
	}

	// Split off port from the host.
	portIdx := -1
	for j := len(host) - 1; j >= 0; j-- {
		if host[j] == ':' {
			portIdx = j
			break
		}
	}
	var newHostPart string
	if portIdx >= 0 {
		newHostPart = newHost + host[portIdx:]
	} else {
		newHostPart = newHost
	}

	rebuilt := scheme + userPart + newHostPart + tail
	if prefix != "" || suffix != "" {
		return prefix + "<" + rebuilt + ">" + suffix
	}
	return rebuilt
}

// extractSDP returns the SDP body (string) of the message. It tries the
// structured Body field first and falls back to the raw buffer (parsing
// after the first blank line).
func extractSDP(msg *parser.SIPMsg) (string, error) {
	if msg == nil {
		return "", fmt.Errorf("nil message")
	}
	if msg.Body != nil {
		switch b := msg.Body.(type) {
		case string:
			return b, nil
		case []byte:
			return string(b), nil
		}
	}
	if len(msg.Buf) > 0 {
		raw := string(msg.Buf)
		for i := 0; i+4 <= len(raw); i++ {
			if raw[i:i+4] == "\r\n\r\n" {
				return raw[i+4:], nil
			}
		}
		for i := 0; i+2 <= len(raw); i++ {
			if raw[i:i+2] == "\n\n" {
				return raw[i+2:], nil
			}
		}
	}
	return "", nil
}

// setSDP writes the provided body into the message and updates Buf so
// higher layers see the modified content.
func setSDP(msg *parser.SIPMsg, body string) {
	if msg == nil {
		return
	}
	msg.Body = body
	if len(msg.Buf) > 0 {
		raw := string(msg.Buf)
		sepIdx := -1
		for i := 0; i+4 <= len(raw); i++ {
			if raw[i:i+4] == "\r\n\r\n" {
				sepIdx = i
				break
			}
		}
		if sepIdx < 0 {
			for i := 0; i+2 <= len(raw); i++ {
				if raw[i:i+2] == "\n\n" {
					sepIdx = i
					break
				}
			}
		}
		if sepIdx >= 0 {
			msg.Buf = []byte(raw[:sepIdx+4] + body)
		} else {
			msg.Buf = []byte(body)
		}
	}
}

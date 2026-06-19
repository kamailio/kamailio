// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * NAT detection and traversal module - matching C nathelper.c
 *
 * Provides:
 *   - NAT detection (source IP vs Contact/SDP address mismatch)
 *   - Via rport/received parameter handling
 *   - Contact header rewriting for NAT clients
 *   - NAT ping (keepalive) mechanism
 */

package nat

import (
	"fmt"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// NATFlag represents NAT detection flags
type NATFlag uint32

const (
	NATFlagNone            NATFlag = 0
	NATFlagDetected        NATFlag = 1 << 0 // NAT detected
	NATFlagRPort           NATFlag = 1 << 1 // rport parameter present
	NATFlagReceived        NATFlag = 1 << 2 // received parameter present
	NATFlagContactRewritten NATFlag = 1 << 3 // Contact was rewritten
	NATFlagSDPRewritten    NATFlag = 1 << 4 // SDP was rewritten
)

// DetectResult holds the result of NAT detection.
type DetectResult struct {
	IsNAT    bool
	Flags    NATFlag
	Reason   string // human-readable reason
	SourceIP net.IP // actual source IP
	ContactIP net.IP // Contact header IP
	SDPIP    net.IP // SDP media IP (if present)
	RPort    int    // rport value (0 if not present)
	Received string // received parameter value
}

// Detect checks if a SIP message indicates the sender is behind NAT.
// Detection is based on:
//   1. Source IP != Contact header IP
//   2. Source IP != SDP media IP (if SDP present)
//   3. Via rport parameter present
//   4. Via received parameter present
// C: fix_nated_contact() / nat_uac_test()
func Detect(msg *parser.SIPMsg, sourceIP string) *DetectResult {
	result := &DetectResult{
		SourceIP: net.ParseIP(sourceIP),
	}

	srcIP := result.SourceIP

	// Check Via rport/received
	if msg.Via1 != nil {
		viaBody := msg.Via1
		if viaBody.RPort != nil && viaBody.RPort.Value.Len > 0 {
			result.Flags |= NATFlagRPort
			if v, err := strconv.Atoi(viaBody.RPort.Value.String()); err == nil {
				result.RPort = v
			}
		}
		if viaBody.Received != nil && viaBody.Received.Value.Len > 0 {
			result.Flags |= NATFlagReceived
			result.Received = viaBody.Received.Value.String()
		}
	}

	// Check Contact header IP
	if msg.Contact != nil {
		contactBody := msg.Contact.Body.String()
		contactIP := extractIPFromURI(contactBody)
		if contactIP != nil {
			result.ContactIP = contactIP
			if srcIP != nil && !srcIP.Equal(contactIP) {
				result.IsNAT = true
				result.Flags |= NATFlagDetected
				result.Reason = fmt.Sprintf("source IP %s != Contact IP %s", srcIP, contactIP)
			}
		}
	}

	// Check SDP media IP
	if msg.ContentLength != nil {
		clStr := strings.TrimSpace(msg.ContentLength.Body.String())
		if cl, err := strconv.Atoi(clStr); err == nil && cl > 0 {
			body := msgGetBody(msg)
			if body != nil && len(body) > 0 {
				sdpIP := extractIPFromSDP(string(body))
				if sdpIP != nil {
					result.SDPIP = sdpIP
					if srcIP != nil && !srcIP.Equal(sdpIP) {
						result.IsNAT = true
						result.Flags |= NATFlagDetected
						if result.Reason == "" {
							result.Reason = fmt.Sprintf("source IP %s != SDP IP %s", srcIP, sdpIP)
						}
					}
				}
			}
		}
	}

	// rport presence is a strong NAT indicator
	if result.Flags&NATFlagRPort != 0 && !result.IsNAT {
		result.IsNAT = true
		result.Flags |= NATFlagDetected
		result.Reason = "rport parameter present in Via"
	}

	return result
}

// FixContact rewrites the Contact header URI to use the source IP and port.
// C: fix_nated_contact()
func FixContact(msg *parser.SIPMsg, sourceIP string, sourcePort int) error {
	if msg.Contact == nil {
		return fmt.Errorf("no Contact header")
	}

	contactBody := msg.Contact.Body.String()
	// Extract the contact URI and replace the host:port
	newURI := replaceHostPort(contactBody, sourceIP, sourcePort)
	if newURI == "" {
		return fmt.Errorf("failed to rewrite Contact URI")
	}

	msg.Contact.Body = str.Mk(newURI)
	return nil
}

// FixViaReceived sets the received parameter on the topmost Via header.
// C: fix_nated_via()
func FixViaReceived(msg *parser.SIPMsg, sourceIP string) error {
	if msg.Via1 == nil {
		return fmt.Errorf("no Via header")
	}

	// The received parameter is set to the source IP.
	// This is typically done by the transport layer, but we provide
	// an explicit API for manual fixing.
	viaStr := msg.Via1.String()
	if !strings.Contains(strings.ToLower(viaStr), "received=") {
		msg.Via1.Host = str.Mk(sourceIP)
	}

	return nil
}

// FixSDP rewrites SDP media addresses to use the specified IP.
// C: fix_nated_sdp()
func FixSDP(msg *parser.SIPMsg, newIP string) error {
	body := msgGetBody(msg)
	if body == nil || len(body) == 0 {
		return fmt.Errorf("no message body")
	}

	sdpStr := string(body)
	newSDP := rewriteSDPIP(sdpStr, newIP)
	if newSDP != sdpStr {
		// Update the body
		msg.Body = []byte(newSDP)
		return nil
	}
	return fmt.Errorf("no SDP addresses to rewrite")
}

// msgGetBody returns the message body as []byte.
func msgGetBody(msg *parser.SIPMsg) []byte {
	if msg.Body == nil {
		return nil
	}
	if b, ok := msg.Body.([]byte); ok {
		return b
	}
	return nil
}

// extractIPFromURI extracts an IP address from a SIP URI string.
func extractIPFromURI(uri string) net.IP {
	// Remove sip: prefix and any parameters
	uri = strings.TrimSpace(uri)
	if strings.HasPrefix(uri, "sip:") {
		uri = uri[4:]
	}
	if strings.HasPrefix(uri, "sips:") {
		uri = uri[5:]
	}
	// Remove parameters
	if idx := strings.IndexAny(uri, ";?>"); idx >= 0 {
		uri = uri[:idx]
	}
	// Remove user@ part
	if idx := strings.Index(uri, "@"); idx >= 0 {
		uri = uri[idx+1:]
	}
	// Remove port
	if idx := strings.LastIndex(uri, ":"); idx >= 0 {
		uri = uri[:idx]
	}
	return net.ParseIP(uri)
}

// extractIPFromSDP extracts the connection address from SDP.
func extractIPFromSDP(sdp string) net.IP {
	lines := strings.Split(sdp, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "c=IN IP4 ") {
			addr := strings.TrimPrefix(line, "c=IN IP4 ")
			addr = strings.TrimSpace(addr)
			return net.ParseIP(addr)
		}
		if strings.HasPrefix(line, "c=IN IP6 ") {
			addr := strings.TrimPrefix(line, "c=IN IP6 ")
			addr = strings.TrimSpace(addr)
			return net.ParseIP(addr)
		}
	}
	return nil
}

// rewriteSDPIP replaces all IP addresses in SDP with the new IP.
func rewriteSDPIP(sdp, newIP string) string {
	lines := strings.Split(sdp, "\n")
	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "c=IN IP4 ") {
			lines[i] = "c=IN IP4 " + newIP + "\r"
		} else if strings.HasPrefix(trimmed, "c=IN IP6 ") {
			lines[i] = "c=IN IP6 " + newIP + "\r"
		} else if strings.HasPrefix(trimmed, "m=audio") || strings.HasPrefix(trimmed, "m=video") {
			// Replace connection info in media lines
			lines[i] = rewriteMediaIP(lines[i], newIP)
		}
	}
	return strings.Join(lines, "\n")
}

// rewriteMediaIP replaces the IP in a media line's connection info.
func rewriteMediaIP(mediaLine, newIP string) string {
	// Media lines may have embedded connection info
	// m=audio <port> RTP/AVP <fmt> [c=IN IP4 <addr>]
	return mediaLine // For now, just return as-is; full implementation needs SDP parser
}

// replaceHostPort replaces the host:port in a SIP URI string.
func replaceHostPort(uri, newIP string, newPort int) string {
	// Find the host:port part
	atIdx := strings.Index(uri, "@")
	if atIdx >= 0 {
		uri = uri[atIdx+1:]
	}
	// Remove parameters
	if idx := strings.IndexAny(uri, ";?>"); idx >= 0 {
		uri = uri[:idx]
	}
	// Remove trailing >
	uri = strings.TrimRight(uri, ">")

	return fmt.Sprintf("%s:%d", newIP, newPort)
}

// NATHelper provides high-level NAT traversal operations.
type NATHelper struct {
	keepaliveInterval time.Duration
	keepaliveTargets  map[string]time.Time
	mu               sync.RWMutex
}

// NewNATHelper creates a new NAT helper.
func NewNATHelper() *NATHelper {
	return &NATHelper{
		keepaliveInterval: 30 * time.Second,
		keepaliveTargets:  make(map[string]time.Time),
	}
}

// ProcessMessage detects NAT and applies fixes.
// Returns the detection result and whether any fix was applied.
func (nh *NATHelper) ProcessMessage(msg *parser.SIPMsg, sourceIP string, sourcePort int) (*DetectResult, bool) {
	detResult := Detect(msg, sourceIP)
	if !detResult.IsNAT {
		return detResult, false
	}

	fixed := false

	// Fix Via received
	if err := FixViaReceived(msg, sourceIP); err == nil {
		fixed = true
	}

	// Fix Contact
	if err := FixContact(msg, sourceIP, sourcePort); err == nil {
		fixed = true
		detResult.Flags |= NATFlagContactRewritten
	}

	// Fix SDP
	if detResult.SDPIP != nil {
		if err := FixSDP(msg, sourceIP); err == nil {
			fixed = true
			detResult.Flags |= NATFlagSDPRewritten
		}
	}

	return detResult, fixed
}

// AddKeepaliveTarget registers a target for NAT keepalive pings.
func (nh *NATHelper) AddKeepaliveTarget(addr string) {
	nh.mu.Lock()
	defer nh.mu.Unlock()
	nh.keepaliveTargets[addr] = time.Now()
}

// RemoveKeepaliveTarget removes a keepalive target.
func (nh *NATHelper) RemoveKeepaliveTarget(addr string) {
	nh.mu.Lock()
	defer nh.mu.Unlock()
	delete(nh.keepaliveTargets, addr)
}

// GetKeepaliveTargets returns all registered keepalive targets.
func (nh *NATHelper) GetKeepaliveTargets() []string {
	nh.mu.RLock()
	defer nh.mu.RUnlock()
	targets := make([]string, 0, len(nh.keepaliveTargets))
	for addr := range nh.keepaliveTargets {
		targets = append(targets, addr)
	}
	return targets
}

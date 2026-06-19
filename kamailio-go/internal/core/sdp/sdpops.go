// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SDP operations module - matching C sdpops.c
 *
 * Provides SDP manipulation functions:
 *   - SDP address/port extraction
 *   - SDP media stream management
 *   - SDP body rewriting
 */

package sdp

import (
	"fmt"
	"net"
	"strconv"
	"strings"
)

// MediaStream represents a single media stream from an SDP body.
type MediaStream struct {
	Type    string            // "audio", "video", "application", etc.
	Port    int               // media port
	Proto   string            // "RTP/AVP", "RTP/SAVP", etc.
	Formats []string          // media formats
	ConnAddr string           // connection address (if different from session level)
	ConnPort int              // connection port (if different from media port)
	Attrs   map[string]string // media attributes (a= lines)
}

// SDPOps provides SDP manipulation operations.
type SDPOps struct {
	Raw string
}

// NewSDPOps creates a new SDPOps from raw SDP content.
func NewSDPOps(raw string) *SDPOps {
	return &SDPOps{Raw: raw}
}

// SessionLevelAddr extracts the session-level connection address.
// Returns the IP address from the "c=" line at session level.
func (so *SDPOps) SessionLevelAddr() string {
	lines := strings.Split(so.Raw, "\n")
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "c=IN IP4 ") {
			return strings.TrimPrefix(trimmed, "c=IN IP4 ")
		}
		if strings.HasPrefix(trimmed, "c=IN IP6 ") {
			return strings.TrimPrefix(trimmed, "c=IN IP6 ")
		}
	}
	return ""
}

// MediaStreams extracts all media streams from the SDP.
func (so *SDPOps) MediaStreams() []*MediaStream {
	var streams []*MediaStream
	lines := strings.Split(so.Raw, "\n")

	var current *MediaStream
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "m=") {
			if current != nil {
				streams = append(streams, current)
			}
			current = parseMediaLine(trimmed)
		} else if current != nil && strings.HasPrefix(trimmed, "a=") {
			attr := strings.TrimPrefix(trimmed, "a=")
			if eqIdx := strings.Index(attr, ":"); eqIdx >= 0 {
				key := attr[:eqIdx]
				val := attr[eqIdx+1:]
				if current.Attrs == nil {
					current.Attrs = make(map[string]string)
				}
				current.Attrs[key] = val
			}
		} else if current != nil && strings.HasPrefix(trimmed, "c=") {
			current.ConnAddr = extractCLineAddr(trimmed)
		}
	}
	if current != nil {
		streams = append(streams, current)
	}

	return streams
}

// RewriteAddr replaces all IP addresses in the SDP with the new address.
func (so *SDPOps) RewriteAddr(oldAddr, newAddr string) string {
	result := so.Raw

	// Replace session-level c= line
	result = replaceCLine(result, oldAddr, newAddr)

	// Replace media-level c= lines
	lines := strings.Split(result, "\n")
	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "c=IN IP") {
			if strings.Contains(trimmed, oldAddr) {
				lines[i] = "c=IN IP4 " + newAddr + "\r"
			}
		}
		// Also replace in o= line origin address
		if strings.HasPrefix(trimmed, "o=") && strings.Contains(trimmed, oldAddr) {
			lines[i] = replaceOriginAddr(line, oldAddr, newAddr)
		}
	}
	return strings.Join(lines, "\n")
}

// RewritePort replaces the media port in the SDP.
func (so *SDPOps) RewritePort(mediaType string, oldPort, newPort int) string {
	lines := strings.Split(so.Raw, "\n")
	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "m="+mediaType+" ") {
			lines[i] = replaceMediaPort(trimmed, oldPort, newPort)
		}
	}
	return strings.Join(lines, "\n")
}

// AddAttribute adds an attribute to a specific media stream.
func (so *SDPOps) AddAttribute(mediaType, key, value string) string {
	lines := strings.Split(so.Raw, "\n")
	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "m="+mediaType+" ") {
			// Find the end of this media block
			insertIdx := i + 1
			for insertIdx < len(lines) {
				nextTrimmed := strings.TrimSpace(lines[insertIdx])
				if strings.HasPrefix(nextTrimmed, "m=") || strings.HasPrefix(nextTrimmed, "v=") {
					break
				}
				insertIdx++
			}
			// Insert attribute before the next media block
			newLine := fmt.Sprintf("a=%s:%s\r", key, value)
			newLines := make([]string, len(lines)+1)
			copy(newLines, lines[:insertIdx])
			newLines[insertIdx] = newLine
			copy(newLines[insertIdx+1:], lines[insertIdx:])
			lines = newLines
			break
		}
	}
	return strings.Join(lines, "\n")
}

// RemoveAttribute removes an attribute from a specific media stream.
func (so *SDPOps) RemoveAttribute(mediaType, key string) string {
	lines := strings.Split(so.Raw, "\n")
	var result []string
	inMedia := false
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "m="+mediaType+" ") {
			inMedia = true
		} else if strings.HasPrefix(trimmed, "m=") {
			inMedia = false
		}
		if inMedia && strings.HasPrefix(trimmed, "a="+key) {
			continue // skip this attribute
		}
		result = append(result, line)
	}
	return strings.Join(result, "\n")
}

// HasMedia returns true if the SDP contains a media stream of the given type.
func (so *SDPOps) HasMedia(mediaType string) bool {
	streams := so.MediaStreams()
	for _, s := range streams {
		if s.Type == mediaType {
			return true
		}
	}
	return false
}

// GetPayloadTypes returns the payload types for a given media type.
func (so *SDPOps) GetPayloadTypes(mediaType string) []string {
	streams := so.MediaStreams()
	for _, s := range streams {
		if s.Type == mediaType {
			return s.Formats
		}
	}
	return nil
}

// String returns the raw SDP content.
func (so *SDPOps) String() string {
	return so.Raw
}

// --- Helper functions ---

func parseMediaLine(line string) *MediaStream {
	// m=<type> <port>/<count> <proto> <fmt> ...
	parts := strings.Fields(line)
	if len(parts) < 4 {
		return nil
	}

	media := &MediaStream{
		Type:  strings.TrimPrefix(parts[0], "m="),
		Proto: parts[2],
	}

	// Parse port (may be port/count for RTP)
	portStr := parts[1]
	if idx := strings.Index(portStr, "/"); idx >= 0 {
		portStr = portStr[:idx]
	}
	port, _ := strconv.Atoi(portStr)
	media.Port = port

	// Formats
	media.Formats = parts[3:]

	return media
}

func extractCLineAddr(line string) string {
	line = strings.TrimSpace(line)
	if strings.HasPrefix(line, "c=IN IP4 ") {
		return strings.TrimPrefix(line, "c=IN IP4 ")
	}
	if strings.HasPrefix(line, "c=IN IP6 ") {
		return strings.TrimPrefix(line, "c=IN IP6 ")
	}
	return ""
}

func replaceCLine(sdp, oldAddr, newAddr string) string {
	lines := strings.Split(sdp, "\n")
	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "c=IN IP") && strings.Contains(trimmed, oldAddr) {
			if strings.Contains(trimmed, "IP4") {
				lines[i] = "c=IN IP4 " + newAddr + "\r"
			} else {
				lines[i] = "c=IN IP6 " + newAddr + "\r"
			}
		}
	}
	return strings.Join(lines, "\n")
}

func replaceOriginAddr(line, oldAddr, newAddr string) string {
	// o=<username> <sess-id> <sess-version> IN IP4 <addr>
	parts := strings.Fields(line)
	for i, p := range parts {
		if p == oldAddr && i > 3 {
			parts[i] = newAddr
		}
	}
	return strings.Join(parts, " ")
}

func replaceMediaPort(line string, oldPort, newPort int) string {
	parts := strings.Fields(line)
	if len(parts) < 2 {
		return line
	}
	portStr := parts[1]
	if idx := strings.Index(portStr, "/"); idx >= 0 {
		parts[1] = fmt.Sprintf("%d/%s", newPort, portStr[idx+1:])
	} else {
		parts[1] = strconv.Itoa(newPort)
	}
	return strings.Join(parts, " ")
}

// Validate checks if the SDP is syntactically valid.
func Validate(sdp string) error {
	hasVersion := false
	hasOrigin := false
	hasSessionName := false
	hasTiming := false

	lines := strings.Split(sdp, "\n")
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		switch {
		case strings.HasPrefix(trimmed, "v="):
			hasVersion = true
		case strings.HasPrefix(trimmed, "o="):
			hasOrigin = true
		case strings.HasPrefix(trimmed, "s="):
			hasSessionName = true
		case strings.HasPrefix(trimmed, "t="):
			hasTiming = true
		}
	}

	if !hasVersion {
		return fmt.Errorf("missing v= line")
	}
	if !hasOrigin {
		return fmt.Errorf("missing o= line")
	}
	if !hasSessionName {
		return fmt.Errorf("missing s= line")
	}
	if !hasTiming {
		return fmt.Errorf("missing t= line")
	}

	return nil
}

// ExtractMediaIP returns the IP address from the first media stream's
// connection info, or the session-level connection if no media-level c= exists.
func ExtractMediaIP(sdp string) net.IP {
	so := NewSDPOps(sdp)
	streams := so.MediaStreams()
	for _, s := range streams {
		if s.ConnAddr != "" {
			return net.ParseIP(s.ConnAddr)
		}
	}
	// Fall back to session level
	addr := so.SessionLevelAddr()
	if addr != "" {
		return net.ParseIP(addr)
	}
	return nil
}

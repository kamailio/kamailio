// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Session-Expires / Min-SE parser (RFC 4028)
 *
 * Session-Expires: <delta-seconds> [;refresher=uac|uas]
 * Min-SE: <delta-seconds> [;refresher=uac|uas]
 */

package parser

import (
	"errors"
	"strconv"
	"strings"
)

// RefresherType indicates who refreshes the session
type RefresherType int

const (
	RefresherUnknown RefresherType = iota
	RefresherUAC
	RefresherUAS
)

// SessionExpiresBody represents a parsed Session-Expires header
// C: struct hdr_session_expires
type SessionExpiresBody struct {
	Seconds   int
	Refresher RefresherType
}

// MinSEBody represents a parsed Min-SE header
type MinSEBody struct {
	Seconds   int
	Refresher RefresherType
}

// ParseSessionExpires parses a Session-Expires header
//
//	C: int parse_session_expires(struct sip_msg *msg, hdr_field_t *hf)
func ParseSessionExpires(header *HdrField) (*SessionExpiresBody, error) {
	if header == nil {
		return nil, errors.New("nil header")
	}
	return parseSessionExpiresValue(header.Body.String())
}

// ParseMinSE parses a Min-SE header
//
//	C: int parse_min_se(struct sip_msg *msg, hdr_field_t *hf)
func ParseMinSE(header *HdrField) (*MinSEBody, error) {
	if header == nil {
		return nil, errors.New("nil header")
	}
	se, err := parseSessionExpiresValue(header.Body.String())
	if err != nil {
		return nil, err
	}
	return &MinSEBody{
		Seconds:   se.Seconds,
		Refresher: se.Refresher,
	}, nil
}

// parseSessionExpiresValue parses the raw header value
// Format: "<delta-seconds>[;refresher=uac|uas]"
func parseSessionExpiresValue(raw string) (*SessionExpiresBody, error) {
	se := &SessionExpiresBody{}
	if raw == "" {
		return nil, errors.New("empty session-expires")
	}

	// Split into value and parameters
	parts := strings.SplitN(raw, ";", 2)
	// Parse delta-seconds
	if n, err := strconv.Atoi(strings.TrimSpace(parts[0])); err != nil {
		return nil, err
	} else if n < 0 {
		return nil, errors.New("negative seconds")
	} else {
		se.Seconds = n
	}

	// Parse refresher parameter
	if len(parts) > 1 {
		for _, param := range strings.Split(parts[1], ";") {
			kv := strings.SplitN(strings.TrimSpace(param), "=", 2)
			if len(kv) == 2 && strings.EqualFold(kv[0], "refresher") {
				switch strings.ToLower(strings.TrimSpace(kv[1])) {
				case "uac":
					se.Refresher = RefresherUAC
				case "uas":
					se.Refresher = RefresherUAS
				default:
					se.Refresher = RefresherUnknown
				}
			}
		}
	}

	return se, nil
}

// IsUASRefresher returns true if refresher is UAS
func (s *SessionExpiresBody) IsUASRefresher() bool {
	return s.Refresher == RefresherUAS
}

// IsUACRefresher returns true if refresher is UAC
func (s *SessionExpiresBody) IsUACRefresher() bool {
	return s.Refresher == RefresherUAC
}

// AboveMinSE checks whether seconds >= minSE
func (s *SessionExpiresBody) AboveMinSE(minSE int) bool {
	return s.Seconds >= minSE
}

// BuildSessionExpires builds a Session-Expires header string
func BuildSessionExpires(seconds int, refresher RefresherType) string {
	switch refresher {
	case RefresherUAC:
		return strconv.Itoa(seconds) + ";refresher=uac"
	case RefresherUAS:
		return strconv.Itoa(seconds) + ";refresher=uas"
	default:
		return strconv.Itoa(seconds)
	}
}

// BuildMinSE builds a Min-SE header string
func BuildMinSE(seconds int) string {
	return strconv.Itoa(seconds)
}

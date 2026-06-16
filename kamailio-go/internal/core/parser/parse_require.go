// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Require / Supported / Proxy-Require header parser - matching C parse_require.c
 *
 * Require = "Require" HCOLON option-tag *(COMMA option-tag)
 * Supported = "Supported" HCOLON option-tag *(COMMA option-tag)
 * Proxy-Require = "Proxy-Require" HCOLON option-tag *(COMMA option-tag)
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// RequireOption represents a SIP capability/option tag
type RequireOption int

const (
	OptionExtension RequireOption = iota
	Option100Rel                  // 100rel - provisional response reliability
	OptionTimer                   // timer - Session-Expires support
	OptionPath                    // path - Path header support (RFC 3327)
	OptionGruu                    // gruu - Globally Routable User Agent URIs
	OptionSipIce                  // sip.ice - Interactive Connectivity Establishment
	OptionReplaces                // replaces - replaces parameter support
	OptionNorefersub              // norefersub - No Refer-Subscribe
	OptionOutbound                // outbound - SIP outbound
	OptionHistinfo                // histinfo - History-Info support
	OptionJoin                    // join - join parameter support
	OptionFromChange              // from-change - From header change support
	OptionAutoAnswer              // answer-mode - auto answer support
)

// OptionInfo stores an option tag string and its recognized type
type OptionInfo struct {
	Type  RequireOption
	Value str.Str
}

// RequireBody represents a parsed Require/Supported/Proxy-Require header body
// C: struct require_body
type RequireBody struct {
	Options []OptionInfo
	Count   int
}

// parseOptionTag converts an option tag string to a RequireOption enum
func parseOptionTag(tag string) RequireOption {
	switch strings.ToLower(tag) {
	case "100rel":
		return Option100Rel
	case "timer":
		return OptionTimer
	case "path":
		return OptionPath
	case "gruu":
		return OptionGruu
	case "sip.ice":
		return OptionSipIce
	case "replaces":
		return OptionReplaces
	case "norefersub":
		return OptionNorefersub
	case "outbound":
		return OptionOutbound
	case "histinfo":
		return OptionHistinfo
	case "join":
		return OptionJoin
	case "from-change":
		return OptionFromChange
	case "answer-mode":
		return OptionAutoAnswer
	default:
		return OptionExtension
	}
}

// optionName returns the string name of an option
func optionName(opt RequireOption) string {
	switch opt {
	case Option100Rel:
		return "100rel"
	case OptionTimer:
		return "timer"
	case OptionPath:
		return "path"
	case OptionGruu:
		return "gruu"
	case OptionSipIce:
		return "sip.ice"
	case OptionReplaces:
		return "replaces"
	case OptionNorefersub:
		return "norefersub"
	case OptionOutbound:
		return "outbound"
	case OptionHistinfo:
		return "histinfo"
	case OptionJoin:
		return "join"
	case OptionFromChange:
		return "from-change"
	case OptionAutoAnswer:
		return "answer-mode"
	default:
		return ""
	}
}

// ParseRequire parses a Require header body
// C: char *parse_require(char *buffer, char *end, struct require_body **rb)
func ParseRequire(body str.Str) (*RequireBody, error) {
	rb := &RequireBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &RequireError{Msg: "empty require body"}
	}

	for _, part := range strings.Split(s, ",") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		opt := OptionInfo{
			Type:  parseOptionTag(part),
			Value: str.Mk(part),
		}
		rb.Options = append(rb.Options, opt)
		rb.Count++
	}

	return rb, nil
}

// ParseSupported parses a Supported header body
// C: char *parse_supported(char *buffer, char *end, struct require_body **sb)
func ParseSupported(body str.Str) (*RequireBody, error) {
	return ParseRequire(body)
}

// ParseProxyRequire parses a Proxy-Require header body
// C: char *parse_proxy_require(char *buffer, char *end, struct require_body **pb)
func ParseProxyRequire(body str.Str) (*RequireBody, error) {
	return ParseRequire(body)
}

// HasOption returns true if the body contains the given option type
func (rb *RequireBody) HasOption(opt RequireOption) bool {
	for _, o := range rb.Options {
		if o.Type == opt {
			return true
		}
	}
	return false
}

// Has100Rel returns true if 100rel is supported/required
func (rb *RequireBody) Has100Rel() bool {
	return rb.HasOption(Option100Rel)
}

// HasTimer returns true if timer is supported/required
func (rb *RequireBody) HasTimer() bool {
	return rb.HasOption(OptionTimer)
}

// HasPath returns true if path is supported/required
func (rb *RequireBody) HasPath() bool {
	return rb.HasOption(OptionPath)
}

// HasGruu returns true if gruu is supported/required
func (rb *RequireBody) HasGruu() bool {
	return rb.HasOption(OptionGruu)
}

// HasReplaces returns true if replaces is supported/required
func (rb *RequireBody) HasReplaces() bool {
	return rb.HasOption(OptionReplaces)
}

// HasOutbound returns true if outbound is supported/required
func (rb *RequireBody) HasOutbound() bool {
	return rb.HasOption(OptionOutbound)
}

// String returns the Require body as a string
func (rb *RequireBody) String() string {
	var parts []string
	for _, o := range rb.Options {
		if o.Type == OptionExtension {
			parts = append(parts, o.Value.String())
		} else {
			parts = append(parts, optionName(o.Type))
		}
	}
	return strings.Join(parts, ", ")
}

// ParseRequireFromHeader parses Require from a header field
func ParseRequireFromHeader(hdr *HdrField) (*RequireBody, error) {
	if hdr == nil {
		return nil, &RequireError{Msg: "nil header"}
	}
	return ParseRequire(hdr.Body)
}

// ParseSupportedFromHeader parses Supported from a header field
func ParseSupportedFromHeader(hdr *HdrField) (*RequireBody, error) {
	if hdr == nil {
		return nil, &RequireError{Msg: "nil header"}
	}
	return ParseSupported(hdr.Body)
}

// ParseProxyRequireFromHeader parses Proxy-Require from a header field
func ParseProxyRequireFromHeader(hdr *HdrField) (*RequireBody, error) {
	if hdr == nil {
		return nil, &RequireError{Msg: "nil header"}
	}
	return ParseProxyRequire(hdr.Body)
}

// RequireError represents a parsing error for Require/Supported headers
type RequireError struct {
	Msg string
}

func (e *RequireError) Error() string {
	return e.Msg
}

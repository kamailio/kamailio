// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Script routing engine
 *
 * Pseudo-variable (PV) references and boolean expression primitives used
 * by the script evaluator. Names follow Kamailio conventions: $rU, $fU,
 * $tU, $ci, $rd, $fd, $rm, $rs, $ru, $si, $realm.
 */

package script

import (
	"net"
	"strconv"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// PVRef identifies a pseudo-variable available in script expressions.
type PVRef int

const (
	PVNone       PVRef = 0
	PVReqUser    PVRef = 1  // $rU — user part of request-URI
	PVFromUser   PVRef = 2  // $fU — user part of From URI
	PVToUser     PVRef = 3  // $tU — user part of To URI
	PVCallID     PVRef = 4  // $ci — Call-ID
	PVReqDomain  PVRef = 5  // $rd — domain (host) part of request-URI
	PVFromDomain PVRef = 6  // $fd — domain of From
	PVMethod     PVRef = 7  // $rm — request method name
	PVStatus     PVRef = 8  // $rs — reply status code
	PVRURI       PVRef = 9  // $ru — full request-URI string
	PVSrcIP      PVRef = 10 // $si — source IP
	PVRealm      PVRef = 11 // $realm — realm for auth
)

// ParsePV parses a PV token like "$rU" into its enum.
// Returns PVNone if the token is not a recognized variable reference.
func ParsePV(token string) PVRef {
	switch token {
	case "$ru", "$RU":
		return PVRURI
	case "$rU", "$Ru":
		return PVReqUser
	case "$fU", "$Fu":
		return PVFromUser
	case "$tU", "$Tu":
		return PVToUser
	case "$ci", "$CI":
		return PVCallID
	case "$rd", "$RD":
		return PVReqDomain
	case "$fd", "$FD":
		return PVFromDomain
	case "$rm", "$RM":
		return PVMethod
	case "$rs", "$RS":
		return PVStatus
	case "$si", "$SI":
		return PVSrcIP
	case "$realm", "$Realm", "$REALM":
		return PVRealm
	}
	return PVNone
}

// resolvePV looks up the value of a PV at runtime for the given message
// and context. The second return indicates whether the PV was resolvable.
func resolvePV(pv PVRef, msg *parser.SIPMsg, ctx *ExecContext) (string, bool) {
	if msg == nil {
		return "", false
	}
	switch pv {
	case PVMethod:
		if msg.FirstLine != nil && msg.FirstLine.Req != nil {
			return msg.FirstLine.Req.Method.String(), true
		}
		return "", false
	case PVStatus:
		if msg.FirstLine != nil && msg.FirstLine.Reply != nil {
			return strconv.Itoa(int(msg.FirstLine.Reply.StatusCode)), true
		}
		return "", false
	case PVCallID:
		if msg.CallID != nil {
			return msg.CallID.Body.String(), true
		}
		return "", false
	case PVReqUser:
		if u := requestURI(msg); u != nil {
			return u.User.String(), true
		}
		return "", false
	case PVFromUser:
		if tb, _ := msg.GetParsedFrom(); tb != nil && tb.URI != nil {
			return tb.URI.User.String(), true
		}
		return "", false
	case PVToUser:
		if tb, _ := msg.GetParsedTo(); tb != nil && tb.URI != nil {
			return tb.URI.User.String(), true
		}
		return "", false
	case PVReqDomain:
		if u := requestURI(msg); u != nil {
			return u.Host.String(), true
		}
		return "", false
	case PVFromDomain:
		if tb, _ := msg.GetParsedFrom(); tb != nil && tb.URI != nil {
			return tb.URI.Host.String(), true
		}
		return "", false
	case PVRURI:
		if ctx != nil && ctx.RURI != "" {
			return ctx.RURI, true
		}
		if msg.FirstLine != nil && msg.FirstLine.Req != nil {
			return msg.FirstLine.Req.URI.String(), true
		}
		if u := requestURI(msg); u != nil {
			host := u.Host.String()
			if host == "" {
				return "", false
			}
			port := u.Port.String()
			if port != "" {
				return "sip:" + u.User.String() + "@" + host + ":" + port, true
			}
			return "sip:" + u.User.String() + "@" + host, true
		}
		return "", false
	case PVSrcIP:
		if ctx != nil && ctx.SrcAddr != nil {
			return addrHost(ctx.SrcAddr), true
		}
		return "", false
	case PVRealm:
		if ctx != nil && ctx.Realm != "" {
			return ctx.Realm, true
		}
		return "", false
	}
	return "", false
}

// requestURI returns the best available parsed request-URI for msg.
func requestURI(msg *parser.SIPMsg) *parser.SIPURI {
	if msg == nil {
		return nil
	}
	if msg.ParsedURI != nil {
		return msg.ParsedURI
	}
	if msg.ParsedOrigRURI != nil {
		return msg.ParsedOrigRURI
	}
	return nil
}

// addrHost extracts the host portion from a net.Addr (host:port).
func addrHost(a net.Addr) string {
	s := a.String()
	if idx := lastColon(s); idx >= 0 {
		return s[:idx]
	}
	return s
}

// lastColon returns the index of the last ':' in s or -1.
func lastColon(s string) int {
	idx := -1
	for i := 0; i < len(s); i++ {
		if s[i] == ':' {
			idx = i
		}
	}
	return idx
}

// Expr is a boolean expression used in `if (expr) { ... }` blocks.
// Supported forms:
//    PVREF == "literal"
//    PVREF != "literal"
//    method == "INVITE"   (same as $rm == "INVITE")
//    uri == "sip:foo"     (same as $ru == "sip:foo")
//    flag(N)              — true if flag N is set
//    !flag(N)
//    $var(name) == "value"
type Expr struct {
	LeftPV  PVRef  // PV to look up on the left
	LeftStr string // or literal method/uri/flag/$var(...) for special-case forms
	Op      string // "==", "!="
	Right   string
	IsFlag  bool
	FlagN   int
	Negate  bool // for !flag
}

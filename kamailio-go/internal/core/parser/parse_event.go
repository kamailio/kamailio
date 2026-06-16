// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Event and Subscription-State header parser - matching C parse_event.c
 *
 * Event = "Event" HCOLON event-type *(SEMI event-param)
 * event-type = token
 * event-param = generic-param / ( "id" EQUAL token )
 *
 * Subscription-State = "Subscription-State" HCOLON substate-value *(SEMI subexp-param)
 * substate-value = "active" / "pending" / "terminated" / extension-substate
 * extension-substate = token
 * subexp-param = generic-param / "reason" EQUAL ( "deactivated" / "probation" / "rejected" / "timeout" / "giveup" / "noresource" / "invariant" / extension-reason )
 * extension-reason = token
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// EventParam represents an event parameter
type EventParam struct {
	Name  str.Str
	Value str.Str
	Next  *EventParam
}

// EventBody represents a parsed Event header body
// C: struct event_body
type EventBody struct {
	EventType str.Str
	Params    *EventParam
	LastParam *EventParam
}

// SubscriptionStateValue represents subscription state values
type SubscriptionStateValue int

const (
	SubscriptionActive SubscriptionStateValue = iota
	SubscriptionPending
	SubscriptionTerminated
	SubscriptionExtension
)

// SubscriptionReason represents subscription termination reasons
type SubscriptionReason int

const (
	ReasonDeactivated SubscriptionReason = iota
	ReasonProbation
	ReasonRejected
	ReasonTimeout
	ReasonGiveup
	ReasonNoResource
	ReasonInvariant
	ReasonExtension
)

// SubStateParam represents a subscription state parameter
type SubStateParam struct {
	Name  str.Str
	Value str.Str
	Next  *SubStateParam
}

// SubscriptionStateBody represents a parsed Subscription-State header body
// C: struct subs_state_body
type SubscriptionStateBody struct {
	State       SubscriptionStateValue
	StateStr    str.Str
	Reason      SubscriptionReason
	ReasonStr   str.Str
	Expires     int
	RetryAfter  int
	Params      *SubStateParam
	LastParam   *SubStateParam
}

// ParseEvent parses an Event header body
// C: char *parse_event(char *buffer, char *end, struct event_body **eb)
func ParseEvent(body str.Str) (*EventBody, error) {
	eb := &EventBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &EventError{Msg: "empty event body"}
	}

	// Find first semicolon to separate event-type from params
	semicolonIdx := strings.Index(s, ";")
	if semicolonIdx != -1 {
		eb.EventType = str.Mk(strings.TrimSpace(s[:semicolonIdx]))
		paramsStr := s[semicolonIdx+1:]
		parseEventParams(eb, paramsStr)
	} else {
		eb.EventType = str.Mk(s)
	}

	return eb, nil
}

// parseEventParams parses event parameters
func parseEventParams(eb *EventBody, paramsStr string) {
	for _, paramStr := range strings.Split(paramsStr, ";") {
		paramStr = strings.TrimSpace(paramStr)
		if paramStr == "" {
			continue
		}

		var name, value string
		if eqIdx := strings.Index(paramStr, "="); eqIdx != -1 {
			name = strings.TrimSpace(paramStr[:eqIdx])
			value = strings.TrimSpace(paramStr[eqIdx+1:])
		} else {
			name = paramStr
		}

		param := &EventParam{
			Name:  str.Mk(name),
			Value: str.Mk(value),
		}

		if eb.Params == nil {
			eb.Params = param
			eb.LastParam = param
		} else {
			eb.LastParam.Next = param
			eb.LastParam = param
		}
	}
}

// GetParam returns the parameter with the given name
func (eb *EventBody) GetParam(name string) *EventParam {
	for p := eb.Params; p != nil; p = p.Next {
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// GetID returns the id parameter value
func (eb *EventBody) GetID() string {
	param := eb.GetParam("id")
	if param != nil {
		return param.Value.String()
	}
	return ""
}

// String returns the Event as a string
func (eb *EventBody) String() string {
	var sb strings.Builder
	sb.WriteString(eb.EventType.String())

	for p := eb.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}

	return sb.String()
}

// ParseEventFromHeader parses Event from a header field
func ParseEventFromHeader(hdr *HdrField) (*EventBody, error) {
	if hdr == nil {
		return nil, &EventError{Msg: "nil header"}
	}
	return ParseEvent(hdr.Body)
}

// ParseSubscriptionState parses a Subscription-State header body
// C: char *parse_subscription_state(char *buffer, char *end, struct subs_state_body **ssb)
func ParseSubscriptionState(body str.Str) (*SubscriptionStateBody, error) {
	ssb := &SubscriptionStateBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &SubscriptionStateError{Msg: "empty subscription state body"}
	}

	semicolonIdx := strings.Index(s, ";")
	if semicolonIdx != -1 {
		stateStr := strings.TrimSpace(s[:semicolonIdx])
		ssb.StateStr = str.Mk(stateStr)
		ssb.State = parseSubscriptionStateValue(stateStr)
		paramsStr := s[semicolonIdx+1:]
		parseSubscriptionStateParams(ssb, paramsStr)
	} else {
		ssb.StateStr = str.Mk(s)
		ssb.State = parseSubscriptionStateValue(s)
	}

	return ssb, nil
}

// parseSubscriptionStateValue parses the subscription state value
func parseSubscriptionStateValue(stateStr string) SubscriptionStateValue {
	switch strings.ToLower(stateStr) {
	case "active":
		return SubscriptionActive
	case "pending":
		return SubscriptionPending
	case "terminated":
		return SubscriptionTerminated
	default:
		return SubscriptionExtension
	}
}

// parseSubscriptionStateParams parses subscription state parameters
func parseSubscriptionStateParams(ssb *SubscriptionStateBody, paramsStr string) {
	for _, paramStr := range strings.Split(paramsStr, ";") {
		paramStr = strings.TrimSpace(paramStr)
		if paramStr == "" {
			continue
		}

		var name, value string
		if eqIdx := strings.Index(paramStr, "="); eqIdx != -1 {
			name = strings.TrimSpace(paramStr[:eqIdx])
			value = strings.TrimSpace(paramStr[eqIdx+1:])
		} else {
			name = paramStr
		}

		switch strings.ToLower(name) {
		case "expires":
			if v, err := strconvParseInt(value, 10, 64); err == nil {
				ssb.Expires = int(v)
			}
		case "retry-after":
			if v, err := strconvParseInt(value, 10, 64); err == nil {
				ssb.RetryAfter = int(v)
			}
		case "reason":
			ssb.ReasonStr = str.Mk(value)
			ssb.Reason = parseSubscriptionReason(value)
		default:
			param := &SubStateParam{
				Name:  str.Mk(name),
				Value: str.Mk(value),
			}
			if ssb.Params == nil {
				ssb.Params = param
				ssb.LastParam = param
			} else {
				ssb.LastParam.Next = param
				ssb.LastParam = param
			}
		}
	}
}

// parseSubscriptionReason parses the subscription reason value
func parseSubscriptionReason(reasonStr string) SubscriptionReason {
	switch strings.ToLower(reasonStr) {
	case "deactivated":
		return ReasonDeactivated
	case "probation":
		return ReasonProbation
	case "rejected":
		return ReasonRejected
	case "timeout":
		return ReasonTimeout
	case "giveup":
		return ReasonGiveup
	case "noresource":
		return ReasonNoResource
	case "invariant":
		return ReasonInvariant
	default:
		return ReasonExtension
	}
}

// strconvParseInt is a helper to avoid importing strconv in the main code
func strconvParseInt(s string, base int, bitSize int) (int64, error) {
	var result int64
	var sign int64 = 1
	var i int

	if len(s) == 0 {
		return 0, &strconvError{}
	}

	if s[0] == '-' {
		sign = -1
		i = 1
	} else if s[0] == '+' {
		i = 1
	}

	for ; i < len(s); i++ {
		c := s[i]
		if c < '0' || c > '9' {
			return 0, &strconvError{}
		}
		result = result*int64(base) + int64(c-'0')
	}

	return result * sign, nil
}

type strconvError struct{}

func (e *strconvError) Error() string {
	return "invalid integer"
}

// IsActive returns true if the subscription is active
func (ssb *SubscriptionStateBody) IsActive() bool {
	return ssb.State == SubscriptionActive
}

// IsPending returns true if the subscription is pending
func (ssb *SubscriptionStateBody) IsPending() bool {
	return ssb.State == SubscriptionPending
}

// IsTerminated returns true if the subscription is terminated
func (ssb *SubscriptionStateBody) IsTerminated() bool {
	return ssb.State == SubscriptionTerminated
}

// String returns the Subscription-State as a string
func (ssb *SubscriptionStateBody) String() string {
	var sb strings.Builder
	sb.WriteString(ssb.StateStr.String())

	if ssb.Expires > 0 {
		sb.WriteString(";expires=")
		sb.WriteString(strconvItoa(ssb.Expires))
	}

	if ssb.RetryAfter > 0 {
		sb.WriteString(";retry-after=")
		sb.WriteString(strconvItoa(ssb.RetryAfter))
	}

	if ssb.ReasonStr.Len > 0 {
		sb.WriteString(";reason=")
		sb.WriteString(ssb.ReasonStr.String())
	}

	for p := ssb.Params; p != nil; p = p.Next {
		sb.WriteString(";")
		sb.WriteString(p.Name.String())
		if p.Value.Len > 0 {
			sb.WriteString("=")
			sb.WriteString(p.Value.String())
		}
	}

	return sb.String()
}

// strconvItoa is a helper to convert int to string
func strconvItoa(i int) string {
	if i == 0 {
		return "0"
	}

	var sb strings.Builder
	negative := i < 0
	if negative {
		i = -i
	}

	for i > 0 {
		sb.WriteByte(byte('0' + i%10))
		i /= 10
	}

	if negative {
		sb.WriteByte('-')
	}

	// Reverse
	runes := []rune(sb.String())
	for j, k := 0, len(runes)-1; j < k; j, k = j+1, k-1 {
		runes[j], runes[k] = runes[k], runes[j]
	}

	return string(runes)
}

// ParseSubscriptionStateFromHeader parses Subscription-State from a header field
func ParseSubscriptionStateFromHeader(hdr *HdrField) (*SubscriptionStateBody, error) {
	if hdr == nil {
		return nil, &SubscriptionStateError{Msg: "nil header"}
	}
	return ParseSubscriptionState(hdr.Body)
}

// EventError represents an event parsing error
type EventError struct {
	Msg string
}

func (e *EventError) Error() string {
	return e.Msg
}

// SubscriptionStateError represents a subscription state parsing error
type SubscriptionStateError struct {
	Msg string
}

func (e *SubscriptionStateError) Error() string {
	return e.Msg
}
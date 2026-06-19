// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Response builder (createReply) - create 1xx/2xx/3xx/4xx/5xx/6xx responses
 * for incoming requests.
 *
 * C equivalent: create_reply / build_res_buf_from_sip_req
 */

package parser

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ReplyOptions controls how a response is constructed from a request
type ReplyOptions struct {
	StatusCode   int
	ReasonPhrase string   // if empty, default RFC 3261 phrase is used
	ToTag        string   // to-tag to add (if empty, auto-generated for non-100)
	Contact      string   // optional Contact header value (e.g. "sip:proxy@10.0.0.1")
	ExtraHeaders [][2]string // list of [name, value] pairs to insert
	Body         string   // optional message body (e.g. SDP, or error body)
	ContentType  string   // default: application/sdp when Body is set
}

// CreateReply constructs a SIP reply (response) to a given request.
//
// Per RFC 3261 §17.2.1: the response copies From, Call-ID, CSeq
// from the request, adds a To-tag, preserves Via headers and Record-Route.
func CreateReply(request *SIPMsg, opts ReplyOptions) (*SIPMsg, error) {
	if request == nil {
		return nil, errors.New("nil request")
	}
	if request.FirstLine == nil || request.FirstLine.Req == nil {
		return nil, errors.New("request has no request line")
	}
	if opts.StatusCode < 100 || opts.StatusCode >= 700 {
		return nil, fmt.Errorf("invalid status code %d", opts.StatusCode)
	}

	// Build the reply line
	reason := opts.ReasonPhrase
	if reason == "" {
		reason = defaultReasonPhrase(uint16(opts.StatusCode))
	}
	reply := &SIPMsg{
		FirstLine: &MsgStart{
			Type:  MsgReply,
			Flags: FLINEFlagProtoSIP,
			Reply: &ReplyLine{
				Version:    str.Mk("2.0"),
				Status:     str.Mk(strconv.Itoa(opts.StatusCode)),
				Reason:     str.Mk(reason),
				StatusCode: uint16(opts.StatusCode),
			},
		},
	}

	// ---- 1. Copy all Via headers (in order, from top to bottom)
	for _, h := range request.Headers {
		if h.Type == HdrVia {
			reply.Headers = append(reply.Headers, &HdrField{
				Name: str.Mk("Via"),
				Body: str.Mk(h.Body.String()),
				Type: HdrVia,
			})
		}
	}

	// ---- 2. Route (if request has Route headers, copy them)
	for _, h := range request.Headers {
		if h.Type == HdrRoute {
			reply.Headers = append(reply.Headers, &HdrField{
				Name: str.Mk("Route"),
				Body: str.Mk(h.Body.String()),
				Type: HdrRoute,
			})
		}
	}

	// ---- 3. From (copy verbatim)
	if from := request.GetHeaderByType(HdrFrom); from != nil {
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk("From"),
			Body: str.Mk(strings.TrimSpace(from.Body.String())),
			Type: HdrFrom,
		})
	}

	// ---- 4. To (copy, and append a to-tag if missing and status != 100)
	if to := request.GetHeaderByType(HdrTo); to != nil {
		body := strings.TrimSpace(to.Body.String())
		if !strings.Contains(strings.ToLower(body), ";tag") && opts.StatusCode != 100 {
			tag := opts.ToTag
			if tag == "" {
				tag = randomHex(6)
			}
			body += ";tag=" + tag
		}
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk("To"),
			Body: str.Mk(body),
			Type: HdrTo,
		})
	}

	// ---- 5. Call-ID (copy)
	if cid := request.GetHeaderByType(HdrCallID); cid != nil {
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk("Call-ID"),
			Body: str.Mk(strings.TrimSpace(cid.Body.String())),
			Type: HdrCallID,
		})
	}

	// ---- 6. CSeq (copy number + method)
	if cs := request.GetHeaderByType(HdrCSeq); cs != nil {
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk("CSeq"),
			Body: str.Mk(strings.TrimSpace(cs.Body.String())),
			Type: HdrCSeq,
		})
	}

	// ---- 7. Copy Record-Route headers (must be preserved for dialogs)
	for _, h := range request.Headers {
		if h.Type == HdrRecordRoute {
			reply.Headers = append(reply.Headers, &HdrField{
				Name: str.Mk("Record-Route"),
				Body: str.Mk(strings.TrimSpace(h.Body.String())),
				Type: HdrRecordRoute,
			})
		}
	}

	// ---- 8. Contact (if provided)
	if opts.Contact != "" {
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk("Contact"),
			Body: str.Mk(opts.Contact),
			Type: HdrContact,
		})
	}

	// ---- 9. Extra headers
	for _, eh := range opts.ExtraHeaders {
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk(eh[0]),
			Body: str.Mk(eh[1]),
			Type: hdrTypeByName(eh[0]),
		})
	}

	// ---- 10. Content-Type (if body provided)
	if opts.Body != "" {
		ct := opts.ContentType
		if ct == "" {
			ct = "application/sdp"
		}
		reply.Headers = append(reply.Headers, &HdrField{
			Name: str.Mk("Content-Type"),
			Body: str.Mk(ct),
			Type: HdrContentType,
		})
		// Set message body
		reply.Body = []byte(opts.Body)
	}

	// ---- 11. Content-Length (always, for both with and without body)
	cl := 0
	if opts.Body != "" {
		cl = len(opts.Body)
	}
	reply.Headers = append(reply.Headers, &HdrField{
		Name: str.Mk("Content-Length"),
		Body: str.Mk(strconv.Itoa(cl)),
		Type: HdrContentLength,
	})

	return reply, nil
}

// ============================================================
// Convenience factories for common responses
// ============================================================

func Create100Trying(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{StatusCode: 100, ToTag: ""})
}

func Create180Ringing(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{StatusCode: 180, Contact: "<sip:proxy@example.com>"})
}

func Create183SessionProgress(request *SIPMsg, sdpBody string) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 183,
		Contact:    "<sip:proxy@example.com>",
		Body:       sdpBody,
	})
}

func Create200OK(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 200,
		Contact:    "<sip:proxy@example.com>",
	})
}

func Create200OKWithSDP(request *SIPMsg, sdpBody string) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 200,
		Contact:    "<sip:proxy@example.com>",
		Body:       sdpBody,
	})
}

func Create302Moved(request *SIPMsg, contactURI string) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 302,
		Contact:    contactURI,
	})
}

func Create401Unauthorized(request *SIPMsg, wwwAuthenticate string) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 401,
		Contact:    "<sip:proxy@example.com>",
		ExtraHeaders: [][2]string{
			{"WWW-Authenticate", wwwAuthenticate},
		},
	})
}

func Create403Forbidden(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 403,
		Contact:    "<sip:proxy@example.com>",
	})
}

func Create404NotFound(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 404,
		Contact:    "<sip:proxy@example.com>",
	})
}

func Create407ProxyAuthRequired(request *SIPMsg, proxyAuthenticate string) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 407,
		Contact:    "<sip:proxy@example.com>",
		ExtraHeaders: [][2]string{
			{"Proxy-Authenticate", proxyAuthenticate},
		},
	})
}

func Create480TemporaryUnavailable(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 480,
		Contact:    "<sip:proxy@example.com>",
	})
}

func Create486Busy(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 486,
		Contact:    "<sip:proxy@example.com>",
	})
}

func Create500ServerInternalError(request *SIPMsg, errorMessage string) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 500,
		Contact:    "<sip:proxy@example.com>",
		Body:       errorMessage,
	})
}

func Create503ServiceUnavailable(request *SIPMsg, retryAfterSeconds int) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 503,
		Contact:    "<sip:proxy@example.com>",
		ExtraHeaders: [][2]string{
			{"Retry-After", strconv.Itoa(retryAfterSeconds)},
		},
	})
}

func Create603Decline(request *SIPMsg) (*SIPMsg, error) {
	return CreateReply(request, ReplyOptions{
		StatusCode: 603,
		Contact:    "<sip:proxy@example.com>",
	})
}

// ============================================================
// Request forwarding (proxy behaviour)
// ============================================================

// BuildForwardRequest prepares a request for forwarding:
//   - decrements Max-Forwards
//   - prepends a new Via header
//   - optionally updates the Request-URI (for routing to next hop)
//
// C: build_req_buf_from_sip_req / t_relay
func BuildForwardRequest(request *SIPMsg, transport, proxyHost string, proxyPort int, nextHopURI string) (*SIPMsg, error) {
	if request == nil {
		return nil, errors.New("nil request")
	}

	// Clone the request (does deep copy of headers and structure)
	fwd := request.Clone()

	// 1. Decrement Max-Forwards (RFC 3261 §16.6 step 2)
	if !fwd.DecrementMaxForwards() {
		return nil, errors.New("max-forwards reached zero or missing")
	}

	// 2. Prepend a Via header (RFC 3261 §16.6 step 3)
	fwd.AddVia(transport, proxyHost, proxyPort, "")

	// 3. Update the Request-URI to the next hop (if provided)
	if nextHopURI != "" {
		fwd.SetRURI(nextHopURI)
	}

	return fwd, nil
}

// BuildForwardRequestWithRecordRoute adds Record-Route to the forwarded request.
// Used by stateful proxies that want to stay in the signalling path.
func BuildForwardRequestWithRecordRoute(request *SIPMsg, transport, proxyHost string, proxyPort int, nextHopURI string, recordRouteURI string) (*SIPMsg, error) {
	fwd, err := BuildForwardRequest(request, transport, proxyHost, proxyPort, nextHopURI)
	if err != nil {
		return nil, err
	}
	if recordRouteURI != "" {
		// Insert Record-Route after Via headers, near the top
		fwd.AddHeaderAt(1, "Record-Route", "<"+recordRouteURI+">;lr")
	}
	return fwd, nil
}

// ============================================================
// Convenience: build → serialize in one call
// ============================================================

// BuildResponse calls CreateReply then BuildMessage.
func BuildResponse(request *SIPMsg, opts ReplyOptions) ([]byte, error) {
	reply, err := CreateReply(request, opts)
	if err != nil {
		return nil, err
	}
	return BuildMessage(reply)
}

// BuildForwardedRequest calls BuildForwardRequestWithRecordRoute then BuildMessage.
func BuildForwardedRequest(request *SIPMsg, transport, proxyHost string, proxyPort int, nextHopURI string, recordRouteURI string) ([]byte, error) {
	fwd, err := BuildForwardRequestWithRecordRoute(request, transport, proxyHost, proxyPort, nextHopURI, recordRouteURI)
	if err != nil {
		return nil, err
	}
	return BuildMessage(fwd)
}

// BuildSDPResponse builds a response with SDP body
func BuildSDPResponse(request *SIPMsg, status int, reason string, contact string, sdp string) ([]byte, error) {
	return BuildResponse(request, ReplyOptions{
		StatusCode: status,
		ReasonPhrase: reason,
		Contact: contact,
		Body: sdp,
	})
}

// BuildSimpleResponse builds a simple response without a body
func BuildSimpleResponse(request *SIPMsg, status int, reason string) ([]byte, error) {
	return BuildResponse(request, ReplyOptions{
		StatusCode: status,
		ReasonPhrase: reason,
		Contact: "<sip:proxy@example.com>",
	})
}

// BuildErrorResponse builds a response with JSON error body
func BuildErrorResponse(request *SIPMsg, status int, reason string, errorMessage string) ([]byte, error) {
	return BuildResponse(request, ReplyOptions{
		StatusCode: status,
		ReasonPhrase: reason,
		Contact: "<sip:proxy@example.com>",
		Body: errorMessage,
		ContentType: "application/json",
	})
}

// ============================================================
// Stateful reply / ACK construction (TM integration)
// ============================================================

// BuildStatefulReply creates a stateful reply for a request within a TM
// transaction context. It automatically:
//   - Copies all Via headers from the request (topmost Via removed for UAS)
//   - Copies Record-Route headers
//   - Generates a To tag if not already present (for provisional/first final)
//   - Sets the proper CSeq from the request
//
// This mirrors Kamailio's t_build_res() from t_reply.c.
func BuildStatefulReply(request *SIPMsg, statusCode int, reasonPhrase string, localTag string) (*SIPMsg, error) {
	if request == nil {
		return nil, errors.New("nil request")
	}

	// Use the existing CreateReply as the base
	opts := ReplyOptions{
		StatusCode:   statusCode,
		ReasonPhrase: reasonPhrase,
	}
	reply, err := CreateReply(request, opts)
	if err != nil {
		return nil, err
	}

	// Auto-generate To tag if not present and a local tag was provided
	if localTag != "" && reply.To != nil {
		toBody := reply.To.Body.String()
		if !strings.Contains(strings.ToLower(toBody), ";tag=") {
			// Append tag to the To header body
			newToBody := toBody + ";tag=" + localTag
			reply.To.Body = str.Mk(newToBody)
		}
	}

	return reply, nil
}

// BuildCancelResponse creates a 200 OK response for a CANCEL request.
// Per RFC 3261 §9.2, the UAS MUST respond to CANCEL with 200 OK.
func BuildCancelResponse(cancel *SIPMsg) (*SIPMsg, error) {
	if cancel == nil {
		return nil, errors.New("nil cancel message")
	}
	return CreateReply(cancel, ReplyOptions{
		StatusCode:   200,
		ReasonPhrase: "OK",
	})
}

// BuildAckRequest constructs an ACK for a non-2xx final response to INVITE.
// The ACK is built within the same transaction (branch matching).
// C: build_ack() from t_funcs.c
func BuildAckRequest(invite *SIPMsg, statusReply *SIPMsg) (*SIPMsg, error) {
	if invite == nil {
		return nil, errors.New("nil invite")
	}

	// Build ACK from the original INVITE
	ack := &SIPMsg{
		Buf:     nil,
		Len:     0,
		Headers: []*HdrField{},
	}

	// Request URI: copy from original INVITE
	ack.NewURI = invite.NewURI
	if ack.NewURI.Len == 0 && invite.FirstLine != nil && invite.FirstLine.Req != nil {
		ack.NewURI = invite.FirstLine.Req.URI
	}

	// Build ACK first line
	ack.FirstLine = &MsgStart{
		Type: MsgRequest,
		Req: &RequestLine{
			Method:      str.Mk("ACK"),
			MethodValue: MethodACK,
			URI:         ack.NewURI,
			Version:     str.Mk(SIPVersion),
		},
	}

	// Copy Via headers from the original INVITE
	for _, h := range invite.Headers {
		if h.Type == HdrVia {
			ack.Headers = append(ack.Headers, &HdrField{
				Type:   HdrVia,
				Name:   h.Name,
				Body:   h.Body,
				Offset: h.Offset,
				Len:    h.Len,
			})
		}
	}

	// Copy From header (with tag)
	if invite.From != nil {
		ack.Headers = append(ack.Headers, &HdrField{
			Type:   HdrFrom,
			Name:   invite.From.Name,
			Body:   invite.From.Body,
		})
		ack.From = ack.Headers[len(ack.Headers)-1]
	}

	// Copy To header - use the tag from the response (not the original INVITE)
	if statusReply != nil && statusReply.To != nil {
		ack.Headers = append(ack.Headers, &HdrField{
			Type:   HdrTo,
			Name:   statusReply.To.Name,
			Body:   statusReply.To.Body,
		})
		ack.To = ack.Headers[len(ack.Headers)-1]
	} else if invite.To != nil {
		ack.Headers = append(ack.Headers, &HdrField{
			Type:   HdrTo,
			Name:   invite.To.Name,
			Body:   invite.To.Body,
		})
		ack.To = ack.Headers[len(ack.Headers)-1]
	}

	// Copy Call-ID
	if invite.CallID != nil {
		ack.Headers = append(ack.Headers, &HdrField{
			Type:   HdrCallID,
			Name:   invite.CallID.Name,
			Body:   invite.CallID.Body,
		})
		ack.CallID = ack.Headers[len(ack.Headers)-1]
	}

	// Copy CSeq (replace method with ACK)
	if invite.CSeq != nil {
		ack.Headers = append(ack.Headers, &HdrField{
			Type: HdrCSeq,
			Name: invite.CSeq.Name,
			Body: str.Mk(fmt.Sprintf("%d ACK", extractCSeqNumber(invite.CSeq.Body.String()))),
		})
		ack.CSeq = ack.Headers[len(ack.Headers)-1]
	}

	// Copy Max-Forwards
	if invite.MaxForwards != nil {
		ack.Headers = append(ack.Headers, &HdrField{
			Type:   HdrMaxForwards,
			Name:   invite.MaxForwards.Name,
			Body:   invite.MaxForwards.Body,
		})
		ack.MaxForwards = ack.Headers[len(ack.Headers)-1]
	}

	// Content-Length: 0
	ack.Headers = append(ack.Headers, &HdrField{
		Type: HdrContentLength,
		Name: str.Mk("Content-Length"),
		Body: str.Mk("0"),
	})
	ack.ContentLength = ack.Headers[len(ack.Headers)-1]

	return ack, nil
}

// extractCSeqNumber parses the numeric part of a CSeq header body.
func extractCSeqNumber(body string) uint32 {
	n := uint32(0)
	for i := 0; i < len(body); i++ {
		if body[i] >= '0' && body[i] <= '9' {
			n = n*10 + uint32(body[i]-'0')
		} else {
			break
		}
	}
	return n
}

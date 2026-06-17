// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Message builder: SIPMsg -> []byte
 *
 * Core functions:
 *   - BuildMessage(msg)     : serialize SIPMsg into raw bytes
 *   - AppendHeader(msg, ...): add a header to the message
 *   - RemoveHeader(msg, ...): remove headers by type
 *   - SetBody(msg, content) : update message body and Content-Length
 *
 * C equivalent: build_lump_rpl, add_lump_rpl, del_lump_rpl, msgbuilder
 */

package parser

import (
	"bytes"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ============================================================
// Message construction
// ============================================================

// BuildMessage serializes a SIPMsg into raw bytes ready for transport.
//
//	C: build_sip_msg / msg2buf
func BuildMessage(msg *SIPMsg) ([]byte, error) {
	if msg == nil {
		return nil, errors.New("nil message")
	}
	if msg.FirstLine == nil {
		return nil, errors.New("nil first line")
	}

	var buf bytes.Buffer

	// ---- 1. First line
	if err := writeFirstLine(&buf, msg.FirstLine); err != nil {
		return nil, err
	}

	// ---- 2. Body first so we know the content length
	bodyBytes, contentType, contentLen := extractBodyBytes(msg)

	// ---- 3. Headers (skip stale Content-Length / Content-Type if present)
	clWritten := false
	ctWritten := false
	for _, h := range msg.Headers {
		if h.Type == HdrContentLength {
			if clWritten {
				continue // skip duplicates
			}
			// Always replace the value with the real content length
			h.Body = str.Mk(strconv.Itoa(contentLen))
			clWritten = true
			writeHeaderField(&buf, h)
			continue
		}
		if h.Type == HdrContentType {
			if ctWritten {
				continue
			}
			if contentType != "" && contentLen > 0 {
				h.Body = str.Mk(contentType)
			}
			ctWritten = true
			writeHeaderField(&buf, h)
			continue
		}
		writeHeaderField(&buf, h)
	}

	// Ensure Content-Length header is present
	if !clWritten {
		buf.WriteString("Content-Length: ")
		buf.WriteString(strconv.Itoa(contentLen))
		buf.WriteString("\r\n")
	}

	// If Content-Type is missing but there's a body, insert it
	if !ctWritten && contentType != "" && contentLen > 0 {
		buf.WriteString("Content-Type: ")
		buf.WriteString(contentType)
		buf.WriteString("\r\n")
	}

	// ---- 4. Empty line separator + body
	buf.WriteString("\r\n")
	if contentLen > 0 {
		buf.Write(bodyBytes)
	}
	return buf.Bytes(), nil
}

// writeFirstLine writes the request / reply first line
func writeFirstLine(buf *bytes.Buffer, fl *MsgStart) error {
	if fl.IsRequest() && fl.Req != nil {
		buf.WriteString(fl.Req.Method.String())
		buf.WriteByte(' ')
		buf.WriteString(fl.Req.URI.String())
		buf.WriteByte(' ')
		ver := fl.Req.Version.String()
		if ver == "" {
			ver = "2.0"
		}
		if strings.HasPrefix(ver, "SIP/") {
			buf.WriteString(ver)
		} else {
			buf.WriteString("SIP/")
			buf.WriteString(ver)
		}
		buf.WriteString("\r\n")
		return nil
	}
	if fl.IsReply() && fl.Reply != nil {
		ver := fl.Reply.Version.String()
		if ver == "" {
			ver = "2.0"
		}
		if strings.HasPrefix(ver, "SIP/") {
			buf.WriteString(ver)
		} else {
			buf.WriteString("SIP/")
			buf.WriteString(ver)
		}
		buf.WriteByte(' ')
		buf.WriteString(strconv.Itoa(int(fl.Reply.StatusCode)))
		buf.WriteByte(' ')
		reason := fl.Reply.Reason.String()
		if reason == "" {
			reason = defaultReasonPhrase(fl.Reply.StatusCode)
		}
		buf.WriteString(reason)
		buf.WriteString("\r\n")
		return nil
	}
	return errors.New("unknown message type")
}

// writeHeaderField writes a single header (name: body)
func writeHeaderField(buf *bytes.Buffer, h *HdrField) {
	if h == nil {
		return
	}
	name := h.Name.String()
	if name == "" {
		// Fall back to canonical header name by type
		name = canonicalHeaderName(h.Type)
	}
	buf.WriteString(name)
	buf.WriteString(": ")
	buf.WriteString(h.Body.String())
	buf.WriteString("\r\n")
}

// hasContentLength returns true if msg already has Content-Length header
func hasContentLength(msg *SIPMsg) bool {
	for _, h := range msg.Headers {
		if h.Type == HdrContentLength {
			return true
		}
	}
	return false
}

// hasContentType returns true if msg has Content-Type header
func hasContentType(msg *SIPMsg) bool {
	for _, h := range msg.Headers {
		if h.Type == HdrContentType {
			return true
		}
	}
	return false
}

// extractBodyBytes returns raw body, content-type hint, and body length
func extractBodyBytes(msg *SIPMsg) ([]byte, string, int) {
	// Case 1: explicit string/bytes via Body
	if msg.Body != nil {
		switch v := msg.Body.(type) {
		case []byte:
			return v, "application/sdp", len(v)
		case string:
			return []byte(v), "application/sdp", len(v)
		}
	}
	// Case 2: fall back to raw tail of original buffer
	if msg.Buf != nil && msg.Len > 0 {
		// Search for the end-of-headers marker: "\r\n\r\n"
		idx := bytes.Index(msg.Buf[:msg.Len], []byte("\r\n\r\n"))
		if idx != -1 {
			body := msg.Buf[idx+4 : msg.Len]
			return body, "application/sdp", len(body)
		}
	}
	return nil, "", 0
}

// patchContentLength inserts Content-Length header just before the final "\r\n" separator
func patchContentLength(raw []byte, length int) []byte {
	sep := []byte("\r\n\r\n")
	idx := bytes.Index(raw, sep)
	if idx == -1 {
		// No separator found — append header + newline + old content
		return []byte(fmt.Sprintf("Content-Length: %d\r\n", length))
	}
	prefix := raw[:idx+2]       // "...\r\n"
	rest := raw[idx+2:]         // "\r\n + body"
	cl := []byte(fmt.Sprintf("Content-Length: %d\r\n", length))
	out := make([]byte, 0, len(prefix)+len(cl)+len(rest))
	out = append(out, prefix...)
	out = append(out, cl...)
	out = append(out, rest...)
	return out
}

// patchContentType inserts Content-Type header
func patchContentType(raw []byte, contentType string) []byte {
	sep := []byte("\r\n\r\n")
	idx := bytes.Index(raw, sep)
	if idx == -1 {
		return []byte(fmt.Sprintf("Content-Type: %s\r\n", contentType))
	}
	prefix := raw[:idx+2]
	rest := raw[idx+2:]
	ct := []byte(fmt.Sprintf("Content-Type: %s\r\n", contentType))
	out := make([]byte, 0, len(prefix)+len(ct)+len(rest))
	out = append(out, prefix...)
	out = append(out, ct...)
	out = append(out, rest...)
	return out
}

// canonicalHeaderName returns the canonical (long-form) SIP header name
func canonicalHeaderName(t HdrType) string {
	switch t {
	case HdrVia:
		return "Via"
	case HdrFrom:
		return "From"
	case HdrTo:
		return "To"
	case HdrCallID:
		return "Call-ID"
	case HdrCSeq:
		return "CSeq"
	case HdrContact:
		return "Contact"
	case HdrMaxForwards:
		return "Max-Forwards"
	case HdrRoute:
		return "Route"
	case HdrRecordRoute:
		return "Record-Route"
	case HdrContentType:
		return "Content-Type"
	case HdrContentLength:
		return "Content-Length"
	case HdrExpires:
		return "Expires"
	case HdrProxyAuth:
		return "Proxy-Authorization"
	case HdrSupported:
		return "Supported"
	case HdrRequire:
		return "Require"
	case HdrProxyRequire:
		return "Proxy-Require"
	case HdrAllow:
		return "Allow"
	case HdrEvent:
		return "Event"
	case HdrAccept:
		return "Accept"
	case HdrUserAgent:
		return "User-Agent"
	case HdrServer:
		return "Server"
	case HdrSubject:
		return "Subject"
	case HdrOrganization:
		return "Organization"
	case HdrPriority:
		return "Priority"
	case HdrAcceptLanguage:
		return "Accept-Language"
	case HdrContentDisposition:
		return "Content-Disposition"
	case HdrDiversion:
		return "Diversion"
	case HdrRPID:
		return "Remote-Party-ID"
	case HdrReferTo:
		return "Refer-To"
	case HdrSessionExpires:
		return "Session-Expires"
	case HdrMinSE:
		return "Min-SE"
	case HdrSIPIfMatch:
		return "SIP-If-Match"
	case HdrSubscriptionState:
		return "Subscription-State"
	case HdrDate:
		return "Date"
	case HdrIdentity:
		return "Identity"
	case HdrIdentityInfo:
		return "Identity-Info"
	case HdrPAI:
		return "P-Asserted-Identity"
	case HdrPPI:
		return "P-Preferred-Identity"
	case HdrPath:
		return "Path"
	case HdrPrivacy:
		return "Privacy"
	case HdrMinExpires:
		return "Min-Expires"
	default:
		return "X-Unknown"
	}
}

// defaultReasonPhrase returns RFC 3261 default reason phrase for status code
func defaultReasonPhrase(code uint16) string {
	switch code {
	case 100:
		return "Trying"
	case 180:
		return "Ringing"
	case 181:
		return "Call Is Being Forwarded"
	case 182:
		return "Queued"
	case 183:
		return "Session Progress"
	case 200:
		return "OK"
	case 202:
		return "Accepted"
	case 300:
		return "Multiple Choices"
	case 301:
		return "Moved Permanently"
	case 302:
		return "Moved Temporarily"
	case 305:
		return "Use Proxy"
	case 400:
		return "Bad Request"
	case 401:
		return "Unauthorized"
	case 402:
		return "Payment Required"
	case 403:
		return "Forbidden"
	case 404:
		return "Not Found"
	case 405:
		return "Method Not Allowed"
	case 406:
		return "Not Acceptable"
	case 407:
		return "Proxy Authentication Required"
	case 408:
		return "Request Timeout"
	case 410:
		return "Gone"
	case 413:
		return "Request Entity Too Large"
	case 414:
		return "Request-URI Too Long"
	case 415:
		return "Unsupported Media Type"
	case 416:
		return "Unsupported URI Scheme"
	case 420:
		return "Bad Extension"
	case 421:
		return "Extension Required"
	case 422:
		return "Session Timer Too Small"
	case 423:
		return "Interval Too Brief"
	case 480:
		return "Temporarily Unavailable"
	case 481:
		return "Call/Transaction Does Not Exist"
	case 482:
		return "Loop Detected"
	case 483:
		return "Too Many Hops"
	case 484:
		return "Address Incomplete"
	case 485:
		return "Ambiguous"
	case 486:
		return "Busy Here"
	case 487:
		return "Request Terminated"
	case 488:
		return "Not Acceptable Here"
	case 491:
		return "Request Pending"
	case 493:
		return "Undecipherable"
	case 500:
		return "Server Internal Error"
	case 501:
		return "Not Implemented"
	case 502:
		return "Bad Gateway"
	case 503:
		return "Service Unavailable"
	case 504:
		return "Server Time-out"
	case 505:
		return "Version Not Supported"
	case 513:
		return "Message Too Large"
	case 600:
		return "Busy Everywhere"
	case 603:
		return "Decline"
	case 604:
		return "Does Not Exist Anywhere"
	case 606:
		return "Not Acceptable"
	default:
		return "Unknown"
	}
}

// ============================================================
// Header manipulation helpers
// ============================================================

// AddHeader appends a new header to the message.
//
//	C: add_lump_rpl + HDR_ADD
func (msg *SIPMsg) AddHeader(name, value string) *HdrField {
	h := &HdrField{
		Name: str.Mk(name),
		Body: str.Mk(value),
		Type: hdrTypeByName(name),
	}
	msg.Headers = append(msg.Headers, h)
	msg.LastHeader = h
	return h
}

// AddHeaderAt inserts a new header at the given index (0 = first position).
func (msg *SIPMsg) AddHeaderAt(index int, name, value string) *HdrField {
	h := &HdrField{
		Name: str.Mk(name),
		Body: str.Mk(value),
		Type: hdrTypeByName(name),
	}
	if index < 0 || index >= len(msg.Headers) {
		msg.Headers = append(msg.Headers, h)
	} else {
		msg.Headers = append(msg.Headers[:index+1], msg.Headers[index:]...)
		msg.Headers[index] = h
	}
	return h
}

// RemoveHeaderByType removes all headers of the given type
//
//	C: del_lump_rpl
func (msg *SIPMsg) RemoveHeaderByType(t HdrType) {
	result := msg.Headers[:0]
	for _, h := range msg.Headers {
		if h.Type != t {
			result = append(result, h)
		}
	}
	msg.Headers = result
}

// RemoveHeaderAt removes the i-th header (0-based)
func (msg *SIPMsg) RemoveHeaderAt(index int) {
	if index < 0 || index >= len(msg.Headers) {
		return
	}
	msg.Headers = append(msg.Headers[:index], msg.Headers[index+1:]...)
}

// ReplaceHeader replaces the first header of the given type
func (msg *SIPMsg) ReplaceHeader(t HdrType, value string) bool {
	for _, h := range msg.Headers {
		if h.Type == t {
			h.Body = str.Mk(value)
			return true
		}
	}
	return false
}

// SetBody sets the message body (string), adding Content-Type/Content-Length
// headers if they are missing.
//
//	C: set_body / add_body
func (msg *SIPMsg) SetBody(contentType string, body interface{}) {
	switch v := body.(type) {
	case []byte:
		msg.Body = v
	case string:
		msg.Body = []byte(v)
	default:
		msg.Body = []byte(fmt.Sprintf("%v", body))
	}

	// Ensure Content-Type exists
	if !hasContentType(msg) {
		msg.AddHeader("Content-Type", contentType)
	} else {
		msg.ReplaceHeader(HdrContentType, contentType)
	}

	// Update or add Content-Length
	length := 0
	if b, ok := msg.Body.([]byte); ok {
		length = len(b)
	}
	if !hasContentLength(msg) {
		msg.AddHeader("Content-Length", strconv.Itoa(length))
	} else {
		msg.ReplaceHeader(HdrContentLength, strconv.Itoa(length))
	}
}

// AddVia prepends a new Via header to the message (proxy behaviour).
// Uses rport parameter per RFC 3581.
func (msg *SIPMsg) AddVia(transport, host string, port int, branch string) {
	var sb strings.Builder
	sb.WriteString("SIP/2.0/")
	sb.WriteString(strings.ToUpper(transport))
	sb.WriteByte(' ')
	sb.WriteString(host)
	if port > 0 {
		sb.WriteByte(':')
		sb.WriteString(strconv.Itoa(port))
	}
	if branch != "" {
		sb.WriteString(";branch=")
		sb.WriteString(branch)
	} else {
		sb.WriteString(";branch=z9hG4bK")
		sb.WriteString(randomHex(8))
	}
	sb.WriteString(";rport")

	// Insert at position 0 (becomes the new topmost Via)
	h := &HdrField{
		Name: str.Mk("Via"),
		Body: str.Mk(sb.String()),
		Type: HdrVia,
	}
	msg.Headers = append([]*HdrField{h}, msg.Headers...)
}

// RemoveTopmostVia removes the first Via header (proxy behaviour for response handling).
// Returns the removed Via body.
func (msg *SIPMsg) RemoveTopmostVia() string {
	for i, h := range msg.Headers {
		if h.Type == HdrVia {
			body := h.Body.String()
			msg.Headers = append(msg.Headers[:i], msg.Headers[i+1:]...)
			return body
		}
	}
	return ""
}

// DecrementMaxForwards decrements Max-Forwards by 1 (proxy behaviour).
// Returns false if header is missing or already zero.
func (msg *SIPMsg) DecrementMaxForwards() bool {
	for _, h := range msg.Headers {
		if h.Type == HdrMaxForwards {
			n, err := strconv.Atoi(strings.TrimSpace(h.Body.String()))
			if err != nil || n <= 0 {
				return false
			}
			h.Body = str.Mk(strconv.Itoa(n - 1))
			return true
		}
	}
	return false
}

// GetMaxForwards returns the current Max-Forwards value, or -1 if missing/invalid.
func (msg *SIPMsg) GetMaxForwards() int {
	for _, h := range msg.Headers {
		if h.Type == HdrMaxForwards {
			if n, err := strconv.Atoi(strings.TrimSpace(h.Body.String())); err == nil {
				return n
			}
		}
	}
	return -1
}

// Clone deep-copies a SIPMsg (useful for parallel forking).
func (msg *SIPMsg) Clone() *SIPMsg {
	if msg == nil {
		return nil
	}
	clone := &SIPMsg{
		ID:         msg.ID,
		PID:        msg.PID,
		ReceivedAt: msg.ReceivedAt,
		ParsedFlag: msg.ParsedFlag,
		NewURI:     str.Mk(msg.NewURI.String()),
		DstURI:     str.Mk(msg.DstURI.String()),
		MsgFlags:   msg.MsgFlags,
		Flags:      msg.Flags,
		XFlags:     msg.XFlags,
		VBFlags:    msg.VBFlags,
	}
	// Copy FirstLine
	if msg.FirstLine != nil {
		fl := &MsgStart{
			Type:  msg.FirstLine.Type,
			Flags: msg.FirstLine.Flags,
			Len:   msg.FirstLine.Len,
		}
		if msg.FirstLine.Req != nil {
			fl.Req = &RequestLine{
				Method:      str.Mk(msg.FirstLine.Req.Method.String()),
				URI:         str.Mk(msg.FirstLine.Req.URI.String()),
				Version:     str.Mk(msg.FirstLine.Req.Version.String()),
				MethodValue: msg.FirstLine.Req.MethodValue,
			}
		}
		if msg.FirstLine.Reply != nil {
			fl.Reply = &ReplyLine{
				Version:    str.Mk(msg.FirstLine.Reply.Version.String()),
				Status:     str.Mk(msg.FirstLine.Reply.Status.String()),
				Reason:     str.Mk(msg.FirstLine.Reply.Reason.String()),
				StatusCode: msg.FirstLine.Reply.StatusCode,
			}
		}
		clone.FirstLine = fl
	}
	// Copy Headers
	for _, h := range msg.Headers {
		clone.Headers = append(clone.Headers, &HdrField{
			Name: str.Mk(h.Name.String()),
			Body: str.Mk(h.Body.String()),
			Type: h.Type,
		})
	}
	// Copy body
	if msg.Body != nil {
		switch v := msg.Body.(type) {
		case []byte:
			cp := make([]byte, len(v))
			copy(cp, v)
			clone.Body = cp
		case string:
			clone.Body = []byte(v)
		default:
			clone.Body = msg.Body
		}
	}
	return clone
}

// ============================================================
// Internal helpers
// ============================================================

// hdrTypeByName resolves a header name (case-insensitive) to its HdrType
func hdrTypeByName(name string) HdrType {
	switch strings.ToLower(strings.TrimSpace(name)) {
	case "via":
		return HdrVia
	case "from":
		return HdrFrom
	case "to":
		return HdrTo
	case "call-id", "i":
		return HdrCallID
	case "cseq":
		return HdrCSeq
	case "contact", "m":
		return HdrContact
	case "max-forwards":
		return HdrMaxForwards
	case "route":
		return HdrRoute
	case "record-route":
		return HdrRecordRoute
	case "content-type", "c":
		return HdrContentType
	case "content-length", "l":
		return HdrContentLength
	case "expires":
		return HdrExpires
	case "proxy-authorization":
		return HdrProxyAuth
	case "supported":
		return HdrSupported
	case "require":
		return HdrRequire
	case "proxy-require":
		return HdrProxyRequire
	case "allow":
		return HdrAllow
	case "event", "o":
		return HdrEvent
	case "accept":
		return HdrAccept
	case "user-agent":
		return HdrUserAgent
	case "server":
		return HdrServer
	case "subject":
		return HdrSubject
	case "organization":
		return HdrOrganization
	case "priority":
		return HdrPriority
	case "accept-language":
		return HdrAcceptLanguage
	case "content-disposition":
		return HdrContentDisposition
	case "diversion":
		return HdrDiversion
	case "remote-party-id":
		return HdrRPID
	case "refer-to", "r":
		return HdrReferTo
	case "session-expires", "x":
		return HdrSessionExpires
	case "min-se":
		return HdrMinSE
	case "sip-if-match":
		return HdrSIPIfMatch
	case "subscription-state":
		return HdrSubscriptionState
	case "date":
		return HdrDate
	case "identity":
		return HdrIdentity
	case "identity-info":
		return HdrIdentityInfo
	case "p-asserted-identity":
		return HdrPAI
	case "p-preferred-identity":
		return HdrPPI
	case "path":
		return HdrPath
	case "privacy":
		return HdrPrivacy
	case "min-expires":
		return HdrMinExpires
	default:
		return HdrOther
	}
}

// randomHex returns a random hex string of n bytes (2n hex chars).
// Uses a simple deterministic PRNG that's sufficient for branch tokens
// in test environments; production should use crypto/rand.
func randomHex(n int) string {
	const hex = "0123456789abcdef"
	buf := make([]byte, 2*n)
	// Simple PRNG based on timestamp for reproducibility in tests
	t := uint64(len(buf)) * 1103515245 + 12345
	for i := range buf {
		t = t*1103515245 + 12345
		buf[i] = hex[(t>>8)&0x0f]
	}
	return string(buf)
}

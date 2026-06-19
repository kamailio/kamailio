// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Message-related Pseudo Variables - matching C pv_core.c
 *
 * Implements core PVs for SIP message access:
 *   $ru - Request URI
 *   $rd - Request URI domain
 *   $rp - Request URI port
 *   $rU - Request URI username (lowercase)
 *   $rP - Request URI username (original case)
 *   $du - Destination URI
 *   $ci - Call-ID
 *   $rm - Request method
 *   $rs - Response status
 *   $rr - Reply reason
 *   $cs - CSeq number
 *   $fU - From username
 *   $fD - From domain
 *   $fP - From display name
 *   $tU - To username
 *   $tD - To domain
 *   $tP - To display name
 *   $ft - From tag
 *   $tt - To tag
 *   $mb - Message body
 *   $br - Branch
 *   $rb - Message buffer (raw)
 */

package pv

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// PVContext wraps a SIP message for PV resolution.
type PVContext struct {
	Msg         *parser.SIPMsg
	Vars        map[string]string // script variables
	AVPs        map[string]string // AVP values
	DestURI     string            // $du
	ReplyCode   int
	ReplyReason string
	Branch      string
}

// NewPVContext creates a new PV context.
func NewPVContext(msg *parser.SIPMsg) *PVContext {
	return &PVContext{
		Msg:  msg,
		Vars: make(map[string]string),
		AVPs: make(map[string]string),
	}
}

// InitCorePVs registers all core message PVs.
func InitCorePVs() {
	// Request URI PVs
	Register("ru", PVClassRURI, PVTypeReadWrite, pvGetRU, pvSetRU)
	Register("rd", PVClassRURI, PVTypeReadOnly, pvGetRD, nil)
	Register("rp", PVClassRURI, PVTypeReadOnly, pvGetRP, nil)
	Register("rU", PVClassRURI, PVTypeReadOnly, pvGetRULower, nil)
	Register("rP", PVClassRURI, PVTypeReadOnly, pvGetRUPreserve, nil)

	// Destination URI PVs
	Register("du", PVClassDU, PVTypeReadWrite, pvGetDU, pvSetDU)

	// Call-ID
	Register("ci", PVClassCallID, PVTypeReadOnly, pvGetCI, nil)

	// Method
	Register("rm", PVClassMsg, PVTypeReadOnly, pvGetRM, nil)
	Register("rs", PVClassMsg, PVTypeReadOnly, pvGetRS, nil)
	Register("rr", PVClassMsg, PVTypeReadOnly, pvGetRR, nil)

	// CSeq
	Register("cs", PVClassCSeq, PVTypeReadOnly, pvGetCS, nil)

	// From PVs
	Register("fU", PVClassFrom, PVTypeReadOnly, pvGetFU, nil)
	Register("fD", PVClassFrom, PVTypeReadOnly, pvGetFD, nil)
	Register("fP", PVClassFrom, PVTypeReadOnly, pvGetFP, nil)
	Register("fn", PVClassFrom, PVTypeReadOnly, pvGetFN, nil)
	Register("ft", PVClassFrom, PVTypeReadOnly, pvGetFT, nil)

	// To PVs
	Register("tU", PVClassTo, PVTypeReadOnly, pvGetTU, nil)
	Register("tD", PVClassTo, PVTypeReadOnly, pvGetTD, nil)
	Register("tP", PVClassTo, PVTypeReadOnly, pvGetTP, nil)
	Register("tn", PVClassTo, PVTypeReadOnly, pvGetTN, nil)
	Register("tt", PVClassTo, PVTypeReadOnly, pvGetTT, nil)

	// Message body
	Register("mb", PVClassMsgBody, PVTypeReadOnly, pvGetMB, nil)

	// Branch
	Register("br", PVClassBranch, PVTypeReadOnly, pvGetBR, nil)
}

func getCtx(ctx interface{}) *PVContext {
	if c, ok := ctx.(*PVContext); ok {
		return c
	}
	return nil
}

// --- Request URI PVs ---

func pvGetRU(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil {
		return PVValue{OK: false}
	}
	if c.Msg.NewURI.Len > 0 {
		return PVValue{Str: c.Msg.NewURI.String(), OK: true}
	}
	if c.Msg.FirstLine != nil && c.Msg.FirstLine.Req != nil {
		return PVValue{Str: c.Msg.FirstLine.Req.URI.String(), OK: true}
	}
	return PVValue{OK: false}
}

func pvSetRU(ctx interface{}, val PVValue) error {
	c := getCtx(ctx)
	if c == nil {
		return fmt.Errorf("nil context")
	}
	// Store in context for later use
	c.Msg.NewURI = str.Mk(val.Str)
	return nil
}

func pvGetRD(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil {
		return PVValue{OK: false}
	}
	uri := pvGetRU(ctx)
	if !uri.OK {
		return PVValue{OK: false}
	}
	// Extract host from URI
	host := extractHost(uri.Str)
	return PVValue{Str: host, OK: true}
}

func pvGetRP(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil {
		return PVValue{OK: false}
	}
	uri := pvGetRU(ctx)
	if !uri.OK {
		return PVValue{OK: false}
	}
	port := extractPort(uri.Str)
	return PVValue{Str: port, OK: true}
}

func pvGetRULower(ctx interface{}) PVValue {
	uri := pvGetRU(ctx)
	if !uri.OK {
		return PVValue{OK: false}
	}
	return PVValue{Str: strings.ToLower(extractUser(uri.Str)), OK: true}
}

func pvGetRUPreserve(ctx interface{}) PVValue {
	uri := pvGetRU(ctx)
	if !uri.OK {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractUser(uri.Str), OK: true}
}

// --- Destination URI PVs ---

func pvGetDU(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil {
		return PVValue{OK: false}
	}
	if c.DestURI != "" {
		return PVValue{Str: c.DestURI, OK: true}
	}
	return PVValue{OK: false}
}

func pvSetDU(ctx interface{}, val PVValue) error {
	c := getCtx(ctx)
	if c == nil {
		return fmt.Errorf("nil context")
	}
	c.DestURI = val.Str
	return nil
}

// --- Call-ID ---

func pvGetCI(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.CallID == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: c.Msg.CallID.Body.String(), OK: true}
}

// --- Method ---

func pvGetRM(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil {
		return PVValue{OK: false}
	}
	if c.Msg.IsRequest() {
		return PVValue{Str: c.Msg.FirstLine.Req.Method.String(), OK: true}
	}
	return PVValue{Str: "SIP/2.0", OK: true}
}

func pvGetRS(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil {
		return PVValue{OK: false}
	}
	if c.ReplyCode > 0 {
		return PVValue{Str: fmt.Sprintf("%d", c.ReplyCode), Int: c.ReplyCode, OK: true}
	}
	if c.Msg != nil && c.Msg.IsReply() && c.Msg.FirstLine != nil && c.Msg.FirstLine.Reply != nil {
		code := c.Msg.FirstLine.Reply.StatusCode
		return PVValue{Str: fmt.Sprintf("%d", code), Int: int(code), OK: true}
	}
	return PVValue{OK: false}
}

func pvGetRR(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil {
		return PVValue{OK: false}
	}
	if c.ReplyReason != "" {
		return PVValue{Str: c.ReplyReason, OK: true}
	}
	if c.Msg != nil && c.Msg.IsReply() && c.Msg.FirstLine != nil && c.Msg.FirstLine.Reply != nil {
		return PVValue{Str: c.Msg.FirstLine.Reply.Reason.String(), OK: true}
	}
	return PVValue{OK: false}
}

// --- CSeq ---

func pvGetCS(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.CSeq == nil {
		return PVValue{OK: false}
	}
	body := c.Msg.CSeq.Body.String()
	// Extract number (before the method name)
	n := ""
	for _, ch := range body {
		if ch >= '0' && ch <= '9' {
			n += string(ch)
		} else {
			break
		}
	}
	if n != "" {
		num, _ := strconv.Atoi(n)
		return PVValue{Str: n, Int: num, OK: true}
	}
	return PVValue{OK: false}
}

// --- From PVs ---

func pvGetFU(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.From == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractUser(c.Msg.From.Body.String()), OK: true}
}

func pvGetFD(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.From == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractHost(c.Msg.From.Body.String()), OK: true}
}

func pvGetFP(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.From == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractDisplayName(c.Msg.From.Body.String()), OK: true}
}

func pvGetFN(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.From == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: c.Msg.From.Body.String(), OK: true}
}

func pvGetFT(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.From == nil {
		return PVValue{OK: false}
	}
	tag := extractTag(c.Msg.From.Body.String())
	return PVValue{Str: tag, OK: tag != ""}
}

// --- To PVs ---

func pvGetTU(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.To == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractUser(c.Msg.To.Body.String()), OK: true}
}

func pvGetTD(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.To == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractHost(c.Msg.To.Body.String()), OK: true}
}

func pvGetTP(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.To == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: extractDisplayName(c.Msg.To.Body.String()), OK: true}
}

func pvGetTN(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.To == nil {
		return PVValue{OK: false}
	}
	return PVValue{Str: c.Msg.To.Body.String(), OK: true}
}

func pvGetTT(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil || c.Msg.To == nil {
		return PVValue{OK: false}
	}
	tag := extractTag(c.Msg.To.Body.String())
	return PVValue{Str: tag, OK: tag != ""}
}

// --- Message body ---

func pvGetMB(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil || c.Msg == nil {
		return PVValue{OK: false}
	}
	if body, ok := c.Msg.Body.([]byte); ok {
		return PVValue{Str: string(body), OK: true}
	}
	return PVValue{OK: false}
}

// --- Branch ---

func pvGetBR(ctx interface{}) PVValue {
	c := getCtx(ctx)
	if c == nil {
		return PVValue{OK: false}
	}
	if c.Branch != "" {
		return PVValue{Str: c.Branch, OK: true}
	}
	if c.Msg != nil && c.Msg.Via1 != nil && c.Msg.Via1.Branch != nil {
		return PVValue{Str: c.Msg.Via1.Branch.Value.String(), OK: true}
	}
	return PVValue{OK: false}
}

// --- URI helper functions ---

// extractUser extracts the username part from a SIP URI string.
func extractUser(uri string) string {
	uri = strings.TrimSpace(uri)
	// Remove display name and angle brackets
	if idx := strings.Index(uri, "<"); idx >= 0 {
		end := strings.Index(uri, ">")
		if end >= 0 {
			uri = uri[idx+1 : end]
		} else {
			uri = uri[idx+1:]
		}
	}
	// Remove sip: prefix
	if strings.HasPrefix(uri, "sip:") {
		uri = uri[4:]
	}
	if strings.HasPrefix(uri, "sips:") {
		uri = uri[5:]
	}
	// Extract user part (before @)
	if idx := strings.Index(uri, "@"); idx >= 0 {
		return uri[:idx]
	}
	return ""
}

// extractHost extracts the host part from a SIP URI string.
func extractHost(uri string) string {
	uri = strings.TrimSpace(uri)
	if idx := strings.Index(uri, "<"); idx >= 0 {
		end := strings.Index(uri, ">")
		if end >= 0 {
			uri = uri[idx+1 : end]
		} else {
			uri = uri[idx+1:]
		}
	}
	if strings.HasPrefix(uri, "sip:") {
		uri = uri[4:]
	}
	if strings.HasPrefix(uri, "sips:") {
		uri = uri[5:]
	}
	host := uri
	if idx := strings.Index(host, "@"); idx >= 0 {
		host = host[idx+1:]
	}
	// Remove port and params
	if idx := strings.IndexAny(host, ":;?"); idx >= 0 {
		host = host[:idx]
	}
	return host
}

// extractPort extracts the port from a SIP URI string.
func extractPort(uri string) string {
	uri = strings.TrimSpace(uri)
	if idx := strings.Index(uri, "<"); idx >= 0 {
		end := strings.Index(uri, ">")
		if end >= 0 {
			uri = uri[idx+1 : end]
		}
	}
	if strings.HasPrefix(uri, "sip:") {
		uri = uri[4:]
	}
	if idx := strings.Index(uri, "@"); idx >= 0 {
		uri = uri[idx+1:]
	}
	// Find port after host
	if idx := strings.Index(uri, ":"); idx >= 0 {
		port := uri[idx+1:]
		if endIdx := strings.IndexAny(port, ";?>"); endIdx >= 0 {
			port = port[:endIdx]
		}
		return port
	}
	return ""
}

// extractDisplayName extracts the display name from a From/To header value.
func extractDisplayName(from string) string {
	// Display name is before the < in the From header
	if idx := strings.Index(from, "<"); idx >= 0 {
		name := strings.TrimSpace(from[:idx])
		// Remove surrounding quotes
		if len(name) >= 2 && name[0] == '"' && name[len(name)-1] == '"' {
			name = name[1 : len(name)-1]
		}
		return name
	}
	return ""
}

// extractTag extracts the tag parameter from a From/To header value.
func extractTag(header string) string {
	lower := strings.ToLower(header)
	tagIdx := strings.Index(lower, ";tag=")
	if tagIdx < 0 {
		return ""
	}
	tag := header[tagIdx+5:]
	if endIdx := strings.IndexAny(tag, ";>\r\n"); endIdx >= 0 {
		tag = tag[:endIdx]
	}
	return strings.TrimSpace(tag)
}

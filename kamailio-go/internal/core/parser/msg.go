// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP message structure - matching C sip_msg_t (msg_parser.h)
 */

package parser

import (
	"time"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// URIType represents SIP URI scheme types
// C: enum _uri_type
type URIType int

const (
	ErrorURIT URIType = 0
	SIPURIT   URIType = 1
	SIPSURIT  URIType = 2
	TELURIT   URIType = 3
	TELSURIT  URIType = 4
	URNURIT   URIType = 5
)

// URIFlags represents URI flags
// C: enum _uri_flags
type URIFlags uint32

const (
	URIUserNormalize URIFlags = 1 << 0
	URISIPUserPhone  URIFlags = 1 << 1
)

// SIPURI represents a parsed SIP URI
// C: struct sip_uri
type SIPURI struct {
	User       str.Str
	Passwd     str.Str
	Host       str.Str
	Port       str.Str
	Params     str.Str
	SIPParams  str.Str
	Headers    str.Str
	PortNo     uint16
	Proto      uint16
	Type       URIType
	Flags      URIFlags
	Transport  str.Str
	TTL        str.Str
	UserParam  str.Str
	MAddr      str.Str
	Method     str.Str
	LR         str.Str
	R2         str.Str
	GR         str.Str
	TransportVal str.Str
	TTLVal       str.Str
	UserParamVal str.Str
	MAddrVal     str.Str
	MethodVal    str.Str
	LRVal        str.Str
	R2Val        str.Str
	GRVal        str.Str
}

// ViaParam represents a Via parameter
// C: struct via_param
type ViaParam struct {
	Type  int
	Flags uint
	Name  str.Str
	Value str.Str
	Next  *ViaParam
}

// ViaBody represents a parsed Via header body
// C: struct via_body
type ViaBody struct {
	Error      int
	Hdr        str.Str
	Name       str.Str
	Version    str.Str
	Transport  str.Str
	Host       str.Str
	Proto      int16
	Port       uint16
	PortStr    str.Str
	Params     str.Str
	Comment    str.Str
	ParamList  *ViaParam
	LastParam  *ViaParam
	Branch     *ViaParam
	TID        str.Str
	Received   *ViaParam
	RPort      *ViaParam
	I          *ViaParam
	Alias      *ViaParam
	Next       *ViaBody
}

// ToBody represents a parsed To/From header body
// C: struct to_body
type ToBody struct {
	DisplayName str.Str
	URI         *SIPURI
	Tag         str.Str
	Params      str.Str
	ParsedURI   *SIPURI
}

// ContactBody represents a parsed Contact header body
// C: struct contact_body
type ContactBody struct {
	DisplayName str.Str
	URI         *SIPURI
	Expires     int
	QValue      float32
	Instance    str.Str
	RegID       uint32
	Params      str.Str
	Next        *ContactBody
}

// CSeqBody represents a parsed CSeq header body
// C: struct cseq_body
type CSeqBody struct {
	Method      str.Str
	Number      uint32
	MethodValue RequestMethod
}

// MsgFlags represents message flags
// C: msg_flags_t
type MsgFlags uint64

// SIPMsg represents a parsed SIP message
// C: struct sip_msg / sip_msg_t
type SIPMsg struct {
	ID          uint32
	PID         int
	ReceivedAt  time.Time

	FirstLine   *MsgStart
	Via1        *ViaBody
	Via2        *ViaBody
	Headers     []*HdrField
	LastHeader  *HdrField
	ParsedFlag  HdrFlag

	// Quick references to important headers
	HdrVia1        *HdrField
	HdrVia2        *HdrField
	CallID         *HdrField
	To             *HdrField
	CSeq           *HdrField
	From           *HdrField
	Contact        *HdrField
	MaxForwards    *HdrField
	Route          *HdrField
	RecordRoute    *HdrField
	ContentType    *HdrField
	ContentLength  *HdrField
	Authorization  *HdrField
	Expires        *HdrField
	ProxyAuth      *HdrField
	Supported      *HdrField
	Require        *HdrField
	ProxyRequire   *HdrField
	Allow          *HdrField
	Event          *HdrField
	Accept         *HdrField
	AcceptLanguage *HdrField
	Organization   *HdrField
	Priority       *HdrField
	Subject        *HdrField
	UserAgent      *HdrField
	Server         *HdrField
	ContentDisposition *HdrField
	Diversion      *HdrField
	RPID           *HdrField
	ReferTo        *HdrField
	SessionExpires *HdrField
	MinSE          *HdrField
	SIPIfMatch     *HdrField
	SubscriptionState *HdrField
	Date           *HdrField
	Identity       *HdrField
	IdentityInfo   *HdrField
	PAI            *HdrField
	PPI            *HdrField
	Path           *HdrField
	Privacy        *HdrField
	MinExpires     *HdrField
	PAccessNetworkInfo *HdrField
	PVisitedNetworkID *HdrField

	Body        interface{} // SDP or other body

	Buf         []byte // original/modified message buffer
	Len         int    // original message length
	BufSize     int    // buffer capacity

	// Routing
	NewURI      str.Str
	DstURI      str.Str
	ParsedURI   *SIPURI
	ParsedOrigRURI *SIPURI

	MsgFlags    MsgFlags
	Flags       uint32
	XFlags      [4]uint32
	VBFlags     uint32

	// Socket info
	ForceSendSocket interface{}
}

// IsRequest returns true if the message is a SIP request
func (m *SIPMsg) IsRequest() bool {
	return m.FirstLine != nil && m.FirstLine.IsRequest()
}

// IsReply returns true if the message is a SIP reply
func (m *SIPMsg) IsReply() bool {
	return m.FirstLine != nil && m.FirstLine.IsReply()
}

// Method returns the request method (for requests)
func (m *SIPMsg) Method() RequestMethod {
	if m.FirstLine != nil && m.FirstLine.Req != nil {
		return m.FirstLine.Req.MethodValue
	}
	return MethodUndefined
}

// StatusCode returns the reply status code (for replies)
func (m *SIPMsg) StatusCode() uint16 {
	if m.FirstLine != nil && m.FirstLine.Reply != nil {
		return m.FirstLine.Reply.StatusCode
	}
	return 0
}

// GetHeaderByType returns the first header of the given type
func (m *SIPMsg) GetHeaderByType(ht HdrType) *HdrField {
	for _, h := range m.Headers {
		if h.Type == ht {
			return h
		}
	}
	return nil
}

// GetAllHeadersByType returns all headers of the given type
func (m *SIPMsg) GetAllHeadersByType(ht HdrType) []*HdrField {
	var result []*HdrField
	for _, h := range m.Headers {
		if h.Type == ht {
			result = append(result, h)
		}
	}
	return result
}

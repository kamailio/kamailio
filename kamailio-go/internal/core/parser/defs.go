// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Parser definitions - matching C parse_def.h
 */

package parser

// ParseResult represents parsing result
type ParseResult int

const (
	ParseError ParseResult = -1
	ParseOK    ParseResult = 1
)

// MsgType represents SIP message type (first line)
type MsgType int

const (
	MsgInvalid MsgType = 0
	MsgRequest MsgType = 1
	MsgReply   MsgType = 2
)

// FirstLineFlags for first line flags
type FirstLineFlags uint16

const (
	FLINEFlagProtoSIP  FirstLineFlags = 1 << 0
	FLINEFlagProtoHTTP FirstLineFlags = 1 << 1
	FLINEFlagProtoHTTP2 FirstLineFlags = 1 << 2
)

// SIP version constants
const (
	SIPVersion    = "SIP/2.0"
	SIPVersionLen = 7
)

// RequestMethod represents SIP request methods as power-of-two bitmap
// C: enum request_method
type RequestMethod uint32

const (
	MethodUndefined RequestMethod = 0
	MethodInvite    RequestMethod = 1
	MethodCancel    RequestMethod = 2
	MethodACK       RequestMethod = 4
	MethodBye       RequestMethod = 8
	MethodInfo      RequestMethod = 16
	MethodRegister  RequestMethod = 32
	MethodSubscribe RequestMethod = 64
	MethodNotify    RequestMethod = 128
	MethodMessage   RequestMethod = 256
	MethodOptions   RequestMethod = 512
	MethodPRACK     RequestMethod = 1024
	MethodUpdate    RequestMethod = 2048
	MethodRefer     RequestMethod = 4096
	MethodPublish   RequestMethod = 8192
	MethodKDMQ      RequestMethod = 16384
	MethodGet       RequestMethod = 32768
	MethodPost      RequestMethod = 65536
	MethodPut       RequestMethod = 131072
	MethodDelete    RequestMethod = 262144
	MethodOther     RequestMethod = 524288
)

// MethodName returns the string name of a method
func MethodName(m RequestMethod) string {
	switch m {
	case MethodInvite:
		return "INVITE"
	case MethodCancel:
		return "CANCEL"
	case MethodACK:
		return "ACK"
	case MethodBye:
		return "BYE"
	case MethodInfo:
		return "INFO"
	case MethodRegister:
		return "REGISTER"
	case MethodSubscribe:
		return "SUBSCRIBE"
	case MethodNotify:
		return "NOTIFY"
	case MethodMessage:
		return "MESSAGE"
	case MethodOptions:
		return "OPTIONS"
	case MethodPRACK:
		return "PRACK"
	case MethodUpdate:
		return "UPDATE"
	case MethodRefer:
		return "REFER"
	case MethodPublish:
		return "PUBLISH"
	case MethodKDMQ:
		return "KDMQ"
	case MethodGet:
		return "GET"
	case MethodPost:
		return "POST"
	case MethodPut:
		return "PUT"
	case MethodDelete:
		return "DELETE"
	default:
		return "UNKNOWN"
	}
}

// ParseMethod parses a method string and returns the method enum
func ParseMethod(s []byte) RequestMethod {
	if len(s) == 0 {
		return MethodUndefined
	}
	switch {
	case len(s) == 6 &&
		(s[0] == 'I' || s[0] == 'i') &&
		(s[1] == 'N' || s[1] == 'n') &&
		(s[2] == 'V' || s[2] == 'v') &&
		(s[3] == 'I' || s[3] == 'i') &&
		(s[4] == 'T' || s[4] == 't') &&
		(s[5] == 'E' || s[5] == 'e'):
		return MethodInvite
	case len(s) == 6 &&
		(s[0] == 'C' || s[0] == 'c') &&
		(s[1] == 'A' || s[1] == 'a') &&
		(s[2] == 'N' || s[2] == 'n') &&
		(s[3] == 'C' || s[3] == 'c') &&
		(s[4] == 'E' || s[4] == 'e') &&
		(s[5] == 'L' || s[5] == 'l'):
		return MethodCancel
	case len(s) == 3 &&
		(s[0] == 'A' || s[0] == 'a') &&
		(s[1] == 'C' || s[1] == 'c') &&
		(s[2] == 'K' || s[2] == 'k'):
		return MethodACK
	case len(s) == 3 &&
		(s[0] == 'B' || s[0] == 'b') &&
		(s[1] == 'Y' || s[1] == 'y') &&
		(s[2] == 'E' || s[2] == 'e'):
		return MethodBye
	case len(s) == 4 &&
		(s[0] == 'I' || s[0] == 'i') &&
		(s[1] == 'N' || s[1] == 'n') &&
		(s[2] == 'F' || s[2] == 'f') &&
		(s[3] == 'O' || s[3] == 'o'):
		return MethodInfo
	case len(s) == 8 &&
		(s[0] == 'R' || s[0] == 'r') &&
		(s[1] == 'E' || s[1] == 'e') &&
		(s[2] == 'G' || s[2] == 'g') &&
		(s[3] == 'I' || s[3] == 'i') &&
		(s[4] == 'S' || s[4] == 's') &&
		(s[5] == 'T' || s[5] == 't') &&
		(s[6] == 'E' || s[6] == 'e') &&
		(s[7] == 'R' || s[7] == 'r'):
		return MethodRegister
	case len(s) == 9 &&
		(s[0] == 'S' || s[0] == 's') &&
		(s[1] == 'U' || s[1] == 'u') &&
		(s[2] == 'B' || s[2] == 'b') &&
		(s[3] == 'S' || s[3] == 's') &&
		(s[4] == 'C' || s[4] == 'c') &&
		(s[5] == 'R' || s[5] == 'r') &&
		(s[6] == 'I' || s[6] == 'i') &&
		(s[7] == 'B' || s[7] == 'b') &&
		(s[8] == 'E' || s[8] == 'e'):
		return MethodSubscribe
	case len(s) == 6 &&
		(s[0] == 'N' || s[0] == 'n') &&
		(s[1] == 'O' || s[1] == 'o') &&
		(s[2] == 'T' || s[2] == 't') &&
		(s[3] == 'I' || s[3] == 'i') &&
		(s[4] == 'F' || s[4] == 'f') &&
		(s[5] == 'Y' || s[5] == 'y'):
		return MethodNotify
	case len(s) == 7 &&
		(s[0] == 'M' || s[0] == 'm') &&
		(s[1] == 'E' || s[1] == 'e') &&
		(s[2] == 'S' || s[2] == 's') &&
		(s[3] == 'S' || s[3] == 's') &&
		(s[4] == 'A' || s[4] == 'a') &&
		(s[5] == 'G' || s[5] == 'g') &&
		(s[6] == 'E' || s[6] == 'e'):
		return MethodMessage
	case len(s) == 7 &&
		(s[0] == 'O' || s[0] == 'o') &&
		(s[1] == 'P' || s[1] == 'p') &&
		(s[2] == 'T' || s[2] == 't') &&
		(s[3] == 'I' || s[3] == 'i') &&
		(s[4] == 'O' || s[4] == 'o') &&
		(s[5] == 'N' || s[5] == 'n') &&
		(s[6] == 'S' || s[6] == 's'):
		return MethodOptions
	case len(s) == 5 &&
		(s[0] == 'P' || s[0] == 'p') &&
		(s[1] == 'R' || s[1] == 'r') &&
		(s[2] == 'A' || s[2] == 'a') &&
		(s[3] == 'C' || s[3] == 'c') &&
		(s[4] == 'K' || s[4] == 'k'):
		return MethodPRACK
	case len(s) == 6 &&
		(s[0] == 'U' || s[0] == 'u') &&
		(s[1] == 'P' || s[1] == 'p') &&
		(s[2] == 'D' || s[2] == 'd') &&
		(s[3] == 'A' || s[3] == 'a') &&
		(s[4] == 'T' || s[4] == 't') &&
		(s[5] == 'E' || s[5] == 'e'):
		return MethodUpdate
	case len(s) == 5 &&
		(s[0] == 'R' || s[0] == 'r') &&
		(s[1] == 'E' || s[1] == 'e') &&
		(s[2] == 'F' || s[2] == 'f') &&
		(s[3] == 'E' || s[3] == 'e') &&
		(s[4] == 'R' || s[4] == 'r'):
		return MethodRefer
	case len(s) == 7 &&
		(s[0] == 'P' || s[0] == 'p') &&
		(s[1] == 'U' || s[1] == 'u') &&
		(s[2] == 'B' || s[2] == 'b') &&
		(s[3] == 'L' || s[3] == 'l') &&
		(s[4] == 'I' || s[4] == 'i') &&
		(s[5] == 'S' || s[5] == 's') &&
		(s[6] == 'H' || s[6] == 'h'):
		return MethodPublish
	case len(s) == 3 &&
		(s[0] == 'G' || s[0] == 'g') &&
		(s[1] == 'E' || s[1] == 'e') &&
		(s[2] == 'T' || s[2] == 't'):
		return MethodGet
	case len(s) == 4 &&
		(s[0] == 'P' || s[0] == 'p') &&
		(s[1] == 'O' || s[1] == 'o') &&
		(s[2] == 'S' || s[2] == 's') &&
		(s[3] == 'T' || s[3] == 't'):
		return MethodPost
	case len(s) == 3 &&
		(s[0] == 'P' || s[0] == 'p') &&
		(s[1] == 'U' || s[1] == 'u') &&
		(s[2] == 'T' || s[2] == 't'):
		return MethodPut
	case len(s) == 6 &&
		(s[0] == 'D' || s[0] == 'd') &&
		(s[1] == 'E' || s[1] == 'e') &&
		(s[2] == 'L' || s[2] == 'l') &&
		(s[3] == 'E' || s[3] == 'e') &&
		(s[4] == 'T' || s[4] == 't') &&
		(s[5] == 'E' || s[5] == 'e'):
		return MethodDelete
	default:
		return MethodOther
	}
}

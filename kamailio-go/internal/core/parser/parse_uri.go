// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SIP URI parser - matching C parse_uri.c
 *
 * Grammar: scheme:[user[:password]@]host[:port][;params][?headers]
 */

package parser

import (
	"errors"
	"net"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ParseURI parses a SIP URI string into a SIPURI structure
// C: int parse_uri(char *buf, int len, struct sip_uri *uri)
func ParseURI(uriStr string) (*SIPURI, error) {
	uri := &SIPURI{}

	// Determine scheme
	schemeEnd := strings.Index(uriStr, ":")
	if schemeEnd == -1 {
		return nil, errors.New("no scheme found")
	}

	scheme := strings.ToLower(uriStr[:schemeEnd])
	switch scheme {
	case "sip":
		uri.Type = SIPURIT
	case "sips":
		uri.Type = SIPSURIT
	case "tel":
		uri.Type = TELURIT
	case "tels":
		uri.Type = TELSURIT
	case "urn":
		uri.Type = URNURIT
	default:
		return nil, errors.New("unknown scheme: " + scheme)
	}

	// Parse the rest after scheme:
	rest := uriStr[schemeEnd+1:]

	// Check for userinfo
	if atIdx := strings.Index(rest, "@"); atIdx >= 0 {
		userinfo := rest[:atIdx]
		rest = rest[atIdx+1:]

		// Check for password
		if colonIdx := strings.Index(userinfo, ":"); colonIdx >= 0 {
			uri.User = str.Mk(userinfo[:colonIdx])
			uri.Passwd = str.Mk(userinfo[colonIdx+1:])
		} else {
			uri.User = str.Mk(userinfo)
		}
	}

	// Parse host and port
	// Check for IPv6 address
	hostPort := rest
	if paramsIdx := strings.Index(rest, ";"); paramsIdx >= 0 {
		hostPort = rest[:paramsIdx]
		uri.Params = str.Mk(rest[paramsIdx+1:])
	}
	if headersIdx := strings.Index(hostPort, "?"); headersIdx >= 0 {
		uri.Headers = str.Mk(hostPort[headersIdx+1:])
		hostPort = hostPort[:headersIdx]
	}

	// Parse host:port
	if hostPort[0] == '[' {
		// IPv6 address
		endBracket := strings.Index(hostPort, "]")
		if endBracket == -1 {
			return nil, errors.New("unclosed IPv6 address")
		}
		uri.Host = str.Mk(hostPort[1:endBracket])
		if endBracket+1 < len(hostPort) && hostPort[endBracket+1] == ':' {
			portStr := hostPort[endBracket+2:]
			port, err := strconv.Atoi(portStr)
			if err != nil {
				return nil, errors.New("invalid port")
			}
			uri.PortNo = uint16(port)
			uri.Port = str.Mk(portStr)
		}
	} else {
		// IPv4 or hostname
		if colonIdx := strings.LastIndex(hostPort, ":"); colonIdx >= 0 {
			uri.Host = str.Mk(hostPort[:colonIdx])
			portStr := hostPort[colonIdx+1:]
			port, err := strconv.Atoi(portStr)
			if err != nil {
				return nil, errors.New("invalid port")
			}
			uri.PortNo = uint16(port)
			uri.Port = str.Mk(portStr)
		} else {
			uri.Host = str.Mk(hostPort)
			uri.PortNo = 5060 // Default SIP port
		}
	}

	// Parse URI parameters
	if uri.Params.Len > 0 {
		parseURIParams(uri, uri.Params.String())
	}

	return uri, nil
}

// parseURIParams parses URI parameters
func parseURIParams(uri *SIPURI, params string) {
	parts := strings.Split(params, ";")
	for _, part := range parts {
		part = strings.TrimSpace(part)
		kv := strings.SplitN(part, "=", 2)
		if len(kv) == 0 {
			continue
		}

		key := strings.ToLower(kv[0])
		var value string
		if len(kv) == 2 {
			value = kv[1]
		}

		switch key {
		case "transport":
			uri.Transport = str.Mk(key)
			uri.TransportVal = str.Mk(value)
			// Set proto
			switch strings.ToLower(value) {
			case "udp":
				uri.Proto = 1
			case "tcp":
				uri.Proto = 2
			case "tls":
				uri.Proto = 3
			case "sctp":
				uri.Proto = 4
			case "ws":
				uri.Proto = 5
			case "wss":
				uri.Proto = 6
			}
		case "ttl":
			uri.TTL = str.Mk(key)
			uri.TTLVal = str.Mk(value)
		case "user":
			uri.UserParam = str.Mk(key)
			uri.UserParamVal = str.Mk(value)
		case "method":
			uri.Method = str.Mk(key)
			uri.MethodVal = str.Mk(value)
		case "maddr":
			uri.MAddr = str.Mk(key)
			uri.MAddrVal = str.Mk(value)
		case "lr":
			uri.LR = str.Mk(key)
			if value != "" {
				uri.LRVal = str.Mk(value)
			}
		case "r2":
			uri.R2 = str.Mk(key)
			if value != "" {
				uri.R2Val = str.Mk(value)
			}
		case "gr":
			uri.GR = str.Mk(key)
			if value != "" {
				uri.GRVal = str.Mk(value)
			}
		default:
			// Unknown parameter - store in SIPParams
			if uri.SIPParams.Len > 0 {
				uri.SIPParams = str.Mk(uri.SIPParams.String() + ";" + key)
			} else {
				uri.SIPParams = str.Mk(key)
			}
			if value != "" {
				uri.SIPParams = str.Mk(uri.SIPParams.String() + "=" + value)
			}
		}
	}
}

// ParseMsgURI parses the URI from a SIP message
// C: int parse_sip_msg_uri(struct sip_msg *msg)
func ParseMsgURI(msg *SIPMsg) error {
	if msg == nil {
		return errors.New("null message")
	}

	var uriStr string
	if msg.IsRequest() && msg.FirstLine.Req != nil {
		uriStr = msg.FirstLine.Req.URI.String()
	} else {
		return errors.New("not a request")
	}

	uri, err := ParseURI(uriStr)
	if err != nil {
		return err
	}

	msg.ParsedURI = uri
	return nil
}

// ParseOrigRURI parses the original request URI
// C: int parse_orig_ruri(struct sip_msg *msg)
func ParseOrigRURI(msg *SIPMsg) error {
	return ParseMsgURI(msg)
}

// HostIP returns the host as a net.IP
func (u *SIPURI) HostIP() net.IP {
	return net.ParseIP(u.Host.String())
}

// IsSecure returns true if the URI uses a secure scheme
func (u *SIPURI) IsSecure() bool {
	return u.Type == SIPSURIT || u.Type == TELSURIT
}

// String returns the string representation of the URI
func (u *SIPURI) String() string {
	var sb strings.Builder

	switch u.Type {
	case SIPURIT:
		sb.WriteString("sip:")
	case SIPSURIT:
		sb.WriteString("sips:")
	case TELURIT:
		sb.WriteString("tel:")
	case TELSURIT:
		sb.WriteString("tels:")
	case URNURIT:
		sb.WriteString("urn:")
	}

	if u.User.Len > 0 {
		sb.WriteString(u.User.String())
		if u.Passwd.Len > 0 {
			sb.WriteString(":")
			sb.WriteString(u.Passwd.String())
		}
		sb.WriteString("@")
	}

	if u.Host.Len > 0 {
		host := u.Host.String()
		if net.ParseIP(host) != nil && strings.Contains(host, ":") {
			// IPv6
			sb.WriteString("[")
			sb.WriteString(host)
			sb.WriteString("]")
		} else {
			sb.WriteString(host)
		}
	}

	if u.Port.Len > 0 {
		sb.WriteString(":")
		sb.WriteString(u.Port.String())
	}

	if u.Params.Len > 0 {
		sb.WriteString(";")
		sb.WriteString(u.Params.String())
	}

	if u.Headers.Len > 0 {
		sb.WriteString("?")
		sb.WriteString(u.Headers.String())
	}

	return sb.String()
}

// ExtractHostPortFromURI returns the host, port and user from a parsed SIP URI.
// Defaults: port falls back to 5060 for sip and 5061 for sips; user may be empty.
func ExtractHostPortFromURI(uri *SIPURI) (host string, port uint16, user string) {
	if uri == nil {
		return "", 0, ""
	}
	host = strings.TrimSpace(uri.Host.String())
	user = strings.TrimSpace(uri.User.String())
	port = uri.PortNo
	if port == 0 {
		if uri.Type == SIPSURIT {
			port = 5061
		} else {
			port = 5060
		}
	}
	return host, port, user
}

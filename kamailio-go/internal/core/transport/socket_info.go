// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Socket information structures - matching C ip_addr.h / socket_info.h
 */

package transport

import (
	"fmt"
	"net"
	"strconv"
)

// Protocol represents SIP transport protocols
// C: enum sip_protos
type Protocol int

const (
	ProtoNone Protocol = iota
	ProtoUDP
	ProtoTCP
	ProtoTLS
	ProtoSCTP
	ProtoWS
	ProtoWSS
	ProtoOther
)

// String returns the protocol name
func (p Protocol) String() string {
	switch p {
	case ProtoUDP:
		return "udp"
	case ProtoTCP:
		return "tcp"
	case ProtoTLS:
		return "tls"
	case ProtoSCTP:
		return "sctp"
	case ProtoWS:
		return "ws"
	case ProtoWSS:
		return "wss"
	default:
		return "none"
	}
}

// ParseProtocol parses a protocol string
func ParseProtocol(s string) Protocol {
	switch s {
	case "udp":
		return ProtoUDP
	case "tcp":
		return ProtoTCP
	case "tls":
		return ProtoTLS
	case "sctp":
		return ProtoSCTP
	case "ws":
		return ProtoWS
	case "wss":
		return ProtoWSS
	default:
		return ProtoNone
	}
}

// SIPPort returns the default SIP port for the protocol
func (p Protocol) SIPPort() uint16 {
	switch p {
	case ProtoTLS, ProtoWSS:
		return 5061
	default:
		return 5060
	}
}

// SocketFlags represents socket flags
// C: enum si_flags
type SocketFlags uint32

const (
	SINone      SocketFlags = 0
	SIIsIP      SocketFlags = 1 << 0
	SIIsLO      SocketFlags = 1 << 1
	SIIsMcast   SocketFlags = 1 << 2
	SIIsAny     SocketFlags = 1 << 3
	SIIsMhomed  SocketFlags = 1 << 4
	SIIsVirtual SocketFlags = 1 << 5
)

// SocketInfo represents a listening socket
// C: struct socket_info
type SocketInfo struct {
	Name      string
	Address   net.IP
	Port      uint16
	Protocol  Protocol
	Flags     SocketFlags
	Listeners int // number of worker listeners
}

// String returns the socket string representation
func (si *SocketInfo) String() string {
	return fmt.Sprintf("%s:%s:%d", si.Protocol.String(), si.Address.String(), si.Port)
}

// Network returns the network type for net.Listen (UDP).
func (si *SocketInfo) Network() string {
	if si.Address.To4() != nil {
		return "udp4"
	}
	return "udp6"
}

// TCPNetwork returns the network type for net.Listen (TCP).
func (si *SocketInfo) TCPNetwork() string {
	if si.Address.To4() != nil {
		return "tcp4"
	}
	return "tcp6"
}

// UDPAddr returns the UDP address for this socket
func (si *SocketInfo) UDPAddr() *net.UDPAddr {
	return &net.UDPAddr{
		IP:   si.Address,
		Port: int(si.Port),
	}
}

// TCPAddr returns the TCP address for this socket
func (si *SocketInfo) TCPAddr() *net.TCPAddr {
	return &net.TCPAddr{
		IP:   si.Address,
		Port: int(si.Port),
	}
}

// ReceiveInfo represents receive information for a message
// C: struct receive_info
type ReceiveInfo struct {
	SrcIP        net.IP
	DstIP        net.IP
	SrcPort      uint16
	DstPort      uint16
	Proto        Protocol
	BindAddress  *SocketInfo
}

// DestInfo represents destination information for sending
// C: struct dest_info
type DestInfo struct {
	SendSock   *SocketInfo
	To         net.Addr
	Proto      Protocol
	SendFlags  SendFlags
}

// SendFlags represents send flags
// C: enum send_flags
type SendFlags uint32

const (
	SNDForceConReuse SendFlags = 1 << 0
	SNDConClose      SendFlags = 1 << 1
	SNDForceSocket   SendFlags = 1 << 2
	SNDForceProto    SendFlags = 1 << 3
	SNDWSXOutbound   SendFlags = 1 << 4
)

// ParseSocketInfo parses a socket string like "udp:192.168.1.1:5060"
func ParseSocketInfo(s string) (*SocketInfo, error) {
	// Try format: proto:ip:port
	var protoStr, ipStr, portStr string
	
	// Check for protocol prefix
	if idx := len(s); idx > 0 {
		// Simple parsing: proto:host:port or host:port
		parts := splitSocketString(s)
		switch len(parts) {
		case 3:
			protoStr = parts[0]
			ipStr = parts[1]
			portStr = parts[2]
		case 2:
			protoStr = "udp"
			ipStr = parts[0]
			portStr = parts[1]
		default:
			return nil, fmt.Errorf("invalid socket format: %s", s)
		}
	}

	port, err := strconv.Atoi(portStr)
	if err != nil {
		return nil, fmt.Errorf("invalid port: %s", portStr)
	}

	ip := net.ParseIP(ipStr)
	if ip == nil {
		// Try to resolve hostname
		addrs, err := net.LookupIP(ipStr)
		if err != nil || len(addrs) == 0 {
			return nil, fmt.Errorf("invalid IP or hostname: %s", ipStr)
		}
		ip = addrs[0]
	}

	return &SocketInfo{
		Name:     s,
		Address:  ip,
		Port:     uint16(port),
		Protocol: ParseProtocol(protoStr),
	}, nil
}

// splitSocketString splits a socket string by colon, handling IPv6 addresses
func splitSocketString(s string) []string {
	var parts []string
	
	// Check for IPv6 address in brackets
	if len(s) > 0 && s[0] == '[' {
		// IPv6 format: [addr]:port or proto:[addr]:port
		idx := 0
		for i := 1; i < len(s); i++ {
			if s[i] == ']' {
				idx = i
				break
			}
		}
		if idx > 0 {
			addr := s[1:idx]
			rest := s[idx+1:]
			if len(rest) > 0 && rest[0] == ':' {
				parts = append(parts, addr, rest[1:])
			} else {
				parts = append(parts, addr)
			}
		}
		return parts
	}

	// Simple split by colon
	start := 0
	for i := 0; i < len(s); i++ {
		if s[i] == ':' {
			parts = append(parts, s[start:i])
			start = i + 1
		}
	}
	if start < len(s) {
		parts = append(parts, s[start:])
	}
	
	return parts
}

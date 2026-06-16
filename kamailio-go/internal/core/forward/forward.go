// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Stateless message forwarding - matching C forward.c
 */

package forward

import (
	"errors"
	"fmt"
	"net"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// Forwarder handles message forwarding
type Forwarder struct {
	udpListeners map[string]*transport.UDPListener
	tcpListeners map[string]*transport.TCPListener
}

// NewForwarder creates a new forwarder
func NewForwarder() *Forwarder {
	return &Forwarder{
		udpListeners: make(map[string]*transport.UDPListener),
		tcpListeners: make(map[string]*transport.TCPListener),
	}
}

// RegisterUDPListener registers a UDP listener for forwarding
func (f *Forwarder) RegisterUDPListener(si *transport.SocketInfo, listener *transport.UDPListener) {
	f.udpListeners[si.String()] = listener
}

// RegisterTCPListener registers a TCP listener for forwarding
func (f *Forwarder) RegisterTCPListener(si *transport.SocketInfo, listener *transport.TCPListener) {
	f.tcpListeners[si.String()] = listener
}

// ForwardRequest forwards a SIP request statelessly
// C: int forward_request(struct sip_msg *msg, str *dst, unsigned short port, struct dest_info *send_info)
func (f *Forwarder) ForwardRequest(msg *parser.SIPMsg, dst str.Str, port uint16, proto transport.Protocol) error {
	if msg == nil {
		return errors.New("null message")
	}

	// Get destination address
	dstAddr := dst.String()
	if port == 0 {
		port = proto.SIPPort()
	}

	// Build destination UDP address
	udpAddr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("%s:%d", dstAddr, port))
	if err != nil {
		return fmt.Errorf("failed to resolve destination: %w", err)
	}

	// Select send socket
	sendSock := f.getSendSocket(proto, udpAddr)
	if sendSock == nil {
		return errors.New("no sending socket found")
	}

	// Build forwarded message
	fwdMsg, err := f.buildForwardedRequest(msg)
	if err != nil {
		return fmt.Errorf("failed to build forwarded request: %w", err)
	}

	// Send via appropriate transport
	switch proto {
	case transport.ProtoUDP:
		listener := f.udpListeners[sendSock.String()]
		if listener == nil {
			return errors.New("no UDP listener registered")
		}
		return listener.Send(udpAddr, fwdMsg)
	case transport.ProtoTCP:
		// TODO: M3 - TCP forwarding
		return errors.New("TCP forwarding not yet implemented")
	default:
		return fmt.Errorf("unsupported protocol: %s", proto.String())
	}
}

// ForwardReply forwards a SIP reply statelessly
// C: int forward_reply(struct sip_msg *msg)
func (f *Forwarder) ForwardReply(msg *parser.SIPMsg) error {
	if msg == nil {
		return errors.New("null message")
	}

	// Get destination from Via header
	if msg.Via1 == nil {
		return errors.New("no Via header found")
	}

	// TODO: M3 - Parse Via and extract destination
	// For now, this is a placeholder
	return errors.New("reply forwarding not yet implemented")
}

// getSendSocket selects the appropriate sending socket
// C: struct socket_info *get_send_socket(struct sip_msg *msg, union sockaddr_union *su, int proto)
func (f *Forwarder) getSendSocket(proto transport.Protocol, dst net.Addr) *transport.SocketInfo {
	// Simple implementation: return first matching listener
	switch proto {
	case transport.ProtoUDP:
		for _, listener := range f.udpListeners {
			if listener.LocalAddr() != nil {
				return &transport.SocketInfo{
					Address:  listener.LocalAddr().(*net.UDPAddr).IP,
					Port:     uint16(listener.LocalAddr().(*net.UDPAddr).Port),
					Protocol: proto,
				}
			}
		}
	case transport.ProtoTCP:
		for _, listener := range f.tcpListeners {
			if listener.LocalAddr() != nil {
				return &transport.SocketInfo{
					Address:  listener.LocalAddr().(*net.TCPAddr).IP,
					Port:     uint16(listener.LocalAddr().(*net.TCPAddr).Port),
					Protocol: proto,
				}
			}
		}
	}
	return nil
}

// buildForwardedRequest builds the forwarded request message
// C: msg_translator.c - various functions
func (f *Forwarder) buildForwardedRequest(msg *parser.SIPMsg) ([]byte, error) {
	// TODO: M3 - Full message translation
	// For now, just return the original buffer
	if msg.Buf != nil {
		return msg.Buf, nil
	}
	return nil, errors.New("message buffer is nil")
}

// UpdateMaxForwards decrements the Max-Forwards header
// C: part of msg_translator.c
func UpdateMaxForwards(msg *parser.SIPMsg) error {
	if msg.MaxForwards == nil {
		return errors.New("no Max-Forwards header")
	}

	// Parse current value
	val := msg.MaxForwards.Body.String()
	var mf int
	_, err := fmt.Sscanf(val, "%d", &mf)
	if err != nil {
		return fmt.Errorf("invalid Max-Forwards value: %w", err)
	}

	if mf <= 0 {
		return errors.New("Max-Forwards reached zero")
	}

	// TODO: M3 - Update header in buffer
	// For now, just return success
	return nil
}

// AddViaHeader adds a Via header for the outgoing request
// C: part of msg_translator.c
func AddViaHeader(msg *parser.SIPMsg, si *transport.SocketInfo, branch str.Str) error {
	// TODO: M3 - Add Via header to message buffer
	return nil
}

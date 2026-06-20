// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Proxy listener adapters - bridge the transport package's concrete
 * UDP/TCP listeners to the proxy.Listener interface.
 *
 * These adapters let ProxyCore stay transport-agnostic while still
 * sending responses back through the sockets that accepted the
 * incoming requests.
 */

package proxy

import (
	"fmt"
	"net"

	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// UDPListenerAdapter wraps transport.UDPListener to implement Listener.
type UDPListenerAdapter struct {
	L  *transport.UDPListener
	SI *transport.SocketInfo
}

// SendTo forwards a payload to the destination address. Accepts any
// net.Addr that represents a UDP destination; otherwise returns an
// error describing the mismatch.
func (a *UDPListenerAdapter) SendTo(dst net.Addr, data []byte) error {
	if a.L == nil {
		return fmt.Errorf("nil UDP listener")
	}
	udpAddr, ok := dst.(*net.UDPAddr)
	if !ok {
		return fmt.Errorf("destination is not a UDP address")
	}
	return a.L.Send(udpAddr, data)
}

// LocalAddr returns the listener's bound address.
func (a *UDPListenerAdapter) LocalAddr() net.Addr {
	if a.L == nil {
		return nil
	}
	return a.L.LocalAddr()
}

// SendSocketInfo returns the socket descriptor used when constructing
// outgoing Via / Record-Route headers.
func (a *UDPListenerAdapter) SendSocketInfo() *transport.SocketInfo {
	return a.SI
}

// AddrString returns the listener's local address as a string.
func (a *UDPListenerAdapter) AddrString() string {
	if a.L == nil {
		return ""
	}
	loc := a.L.LocalAddr()
	if loc == nil {
		return ""
	}
	return "udp:" + loc.String()
}

// TCPListenerAdapter wraps transport.TCPListener to implement Listener.
//
// TCP responses are dispatched by opening a fresh outbound TCP
// connection to the destination address. This mirrors common SIP proxy
// behaviour where each response does not necessarily traverse the same
// TCP connection that carried the request.
type TCPListenerAdapter struct {
	L  *transport.TCPListener
	SI *transport.SocketInfo
}

// SendTo forwards a payload over a fresh TCP connection to dst.
// Accepts only *net.TCPAddr destinations.
func (a *TCPListenerAdapter) SendTo(dst net.Addr, data []byte) error {
	if a.L == nil {
		return fmt.Errorf("nil TCP listener")
	}
	tcpAddr, ok := dst.(*net.TCPAddr)
	if !ok {
		return fmt.Errorf("destination is not a TCP address")
	}
	sender, err := transport.NewTCPSender(fmt.Sprintf("%s:%d", tcpAddr.IP.String(), tcpAddr.Port))
	if err != nil {
		return err
	}
	defer sender.Close()
	return sender.Send(data)
}

// LocalAddr returns the listener's bound address.
func (a *TCPListenerAdapter) LocalAddr() net.Addr {
	if a.L == nil {
		return nil
	}
	return a.L.LocalAddr()
}

// SendSocketInfo returns the socket descriptor used when constructing
// outgoing Via / Record-Route headers.
func (a *TCPListenerAdapter) SendSocketInfo() *transport.SocketInfo {
	return a.SI
}

// AddrString returns the listener's local address as a string.
func (a *TCPListenerAdapter) AddrString() string {
	if a.L == nil {
		return ""
	}
	loc := a.L.LocalAddr()
	if loc == nil {
		return ""
	}
	return "tcp:" + loc.String()
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * UDP transport layer - matching C udp_server.c
 */

package transport

import (
	"context"
	"errors"
	"fmt"
	"net"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/log"
)

const (
	// MaxRecvBufferSize matches C MAX_RECV_BUFFER_SIZE
	MaxRecvBufferSize = 256 * 1024
	// MaxSendBufferSize matches C MAX_SEND_BUFFER_SIZE
	MaxSendBufferSize = 256 * 1024
)

// MessageHandler is the callback for received messages
// C: receive_msg function pointer
type MessageHandler func(data []byte, srcAddr *net.UDPAddr, rcvInfo *ReceiveInfo)

// UDPListener represents a UDP listening socket
// C: udp_rcv_loop context
type UDPListener struct {
	si       *SocketInfo
	conn     *net.UDPConn
	handler  MessageHandler
	bufSize  int
	running  bool
	stopCh   chan struct{}
	wg       sync.WaitGroup
}

// NewUDPListener creates a new UDP listener
func NewUDPListener(si *SocketInfo, handler MessageHandler) *UDPListener {
	return &UDPListener{
		si:      si,
		handler: handler,
		bufSize: MaxRecvBufferSize,
		stopCh:  make(chan struct{}),
	}
}

// ListenAndServe starts the UDP listener and serves requests
// C: int udp_rcv_loop(void)
func (u *UDPListener) ListenAndServe() error {
	addr := u.si.UDPAddr()
	
	conn, err := net.ListenUDP(u.si.Network(), addr)
	if err != nil {
		return fmt.Errorf("failed to listen UDP on %s: %w", addr.String(), err)
	}
	
	u.conn = conn
	u.running = true
	
	log.Info("UDP listener started",
		log.String("address", addr.String()),
		log.Int("buffer_size", u.bufSize),
	)
	
	// Start receiving
	u.wg.Add(1)
	go u.receiveLoop()
	
	return nil
}

// receiveLoop is the main receive loop
// C: udp_rcv_loop() main loop
func (u *UDPListener) receiveLoop() {
	defer u.wg.Done()
	
	buf := make([]byte, u.bufSize)
	
	for u.running {
		// Set read deadline to allow checking stopCh periodically
		u.conn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
		
		n, srcAddr, err := u.conn.ReadFromUDP(buf)
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				return
			}
			if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
				// Timeout - check if we should stop
				select {
				case <-u.stopCh:
					return
				default:
					continue
				}
			}
			log.Warn("UDP read error", log.ErrField(err))
			continue
		}
		
		// Check stop signal
		select {
		case <-u.stopCh:
			return
		default:
		}
		
		// Get local address for receive info
		localAddr := u.conn.LocalAddr().(*net.UDPAddr)
		
		rcvInfo := &ReceiveInfo{
			SrcIP:       srcAddr.IP,
			DstIP:       localAddr.IP,
			SrcPort:     uint16(srcAddr.Port),
			DstPort:     uint16(localAddr.Port),
			Proto:       ProtoUDP,
			BindAddress: u.si,
		}
		
		// Call handler with a copy of the data
		data := make([]byte, n)
		copy(data, buf[:n])
		
		if u.handler != nil {
			u.handler(data, srcAddr, rcvInfo)
		}
	}
}

// Send sends data to the specified destination
// C: int udp_send(struct dest_info *dst, char *buf, unsigned len)
func (u *UDPListener) Send(dst *net.UDPAddr, data []byte) error {
	if u.conn == nil {
		return errors.New("UDP listener not initialized")
	}
	
	if len(data) > MaxSendBufferSize {
		return fmt.Errorf("data too large: %d > %d", len(data), MaxSendBufferSize)
	}
	
	_, err := u.conn.WriteToUDP(data, dst)
	if err != nil {
		return fmt.Errorf("UDP send failed: %w", err)
	}
	
	return nil
}

// SendTo sends data using a specific destination info
func (u *UDPListener) SendTo(di *DestInfo, data []byte) error {
	udpAddr, ok := di.To.(*net.UDPAddr)
	if !ok {
		return errors.New("destination is not a UDP address")
	}
	
	return u.Send(udpAddr, data)
}

// Shutdown stops the UDP listener
func (u *UDPListener) Shutdown(ctx context.Context) error {
	if !u.running {
		return nil
	}
	
	u.running = false
	close(u.stopCh)
	
	if u.conn != nil {
		u.conn.Close()
	}
	
	// Wait for receive loop to finish with timeout
	done := make(chan struct{})
	go func() {
		u.wg.Wait()
		close(done)
	}()
	
	select {
	case <-done:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

// LocalAddr returns the local address
func (u *UDPListener) LocalAddr() net.Addr {
	if u.conn == nil {
		return nil
	}
	return u.conn.LocalAddr()
}

// UDPSender provides UDP sending capabilities
// Can be used without a listener for outbound-only UDP
type UDPSender struct {
	conn *net.UDPConn
}

// NewUDPSender creates a new UDP sender with an ephemeral port
func NewUDPSender() (*UDPSender, error) {
	addr := &net.UDPAddr{IP: net.IPv4zero, Port: 0}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		return nil, fmt.Errorf("failed to create UDP sender: %w", err)
	}
	
	return &UDPSender{conn: conn}, nil
}

// Send sends data to the specified destination
func (s *UDPSender) Send(dst *net.UDPAddr, data []byte) error {
	if s.conn == nil {
		return errors.New("UDP sender not initialized")
	}
	
	_, err := s.conn.WriteToUDP(data, dst)
	if err != nil {
		return fmt.Errorf("UDP send failed: %w", err)
	}
	
	return nil
}

// Close closes the UDP sender
func (s *UDPSender) Close() error {
	if s.conn != nil {
		return s.conn.Close()
	}
	return nil
}

// LocalAddr returns the local address
func (s *UDPSender) LocalAddr() net.Addr {
	if s.conn == nil {
		return nil
	}
	return s.conn.LocalAddr()
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TCP transport layer - matching C tcp_main.c / tcp_server.c
 */

package transport

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"net"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/log"
)

const (
	// TCPReadTimeout is the read timeout for TCP connections
	TCPReadTimeout = 30 * time.Second
	// TCPWriteTimeout is the write timeout for TCP connections
	TCPWriteTimeout = 30 * time.Second
	// TCPConnTimeout is the connection timeout
	TCPConnTimeout = 10 * time.Second
)

// TCPMessageHandler is the callback for received TCP messages
type TCPMessageHandler func(data []byte, conn *TCPConnection, rcvInfo *ReceiveInfo)

// TCPListener represents a TCP listening socket
type TCPListener struct {
	si       *SocketInfo
	listener net.Listener
	handler  TCPMessageHandler
	running  bool
	stopCh   chan struct{}
	wg       sync.WaitGroup
	conns    sync.Map // map[*TCPConnection]bool
}

// NewTCPListener creates a new TCP listener
func NewTCPListener(si *SocketInfo, handler TCPMessageHandler) *TCPListener {
	return &TCPListener{
		si:      si,
		handler: handler,
		stopCh:  make(chan struct{}),
	}
}

// ListenAndServe starts the TCP listener and serves connections
func (t *TCPListener) ListenAndServe() error {
	addr := t.si.TCPAddr()

	listener, err := net.Listen(t.si.Network(), addr.String())
	if err != nil {
		return fmt.Errorf("failed to listen TCP on %s: %w", addr.String(), err)
	}

	t.listener = listener
	t.running = true

	log.Info("TCP listener started",
		log.String("address", addr.String()),
	)

	// Accept loop
	t.wg.Add(1)
	go t.acceptLoop()

	return nil
}

// LocalAddr returns the listener's local address
func (t *TCPListener) LocalAddr() net.Addr {
	if t.listener == nil {
		return nil
	}
	return t.listener.Addr()
}

// acceptLoop accepts incoming connections
func (t *TCPListener) acceptLoop() {
	defer t.wg.Done()

	for t.running {
		// Set accept deadline to allow checking stopCh
		t.listener.(*net.TCPListener).SetDeadline(time.Now().Add(100 * time.Millisecond))

		conn, err := t.listener.Accept()
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				return
			}
			if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
				select {
				case <-t.stopCh:
					return
				default:
					continue
				}
			}
			log.Warn("TCP accept error", log.ErrField(err))
			continue
		}

		// Handle connection
		t.wg.Add(1)
		go t.handleConnection(conn)
	}
}

// handleConnection handles a single TCP connection
func (t *TCPListener) handleConnection(conn net.Conn) {
	defer t.wg.Done()

	tcpConn := &TCPConnection{
		Conn:       conn,
		LocalAddr:  conn.LocalAddr(),
		RemoteAddr: conn.RemoteAddr(),
		CreatedAt:  time.Now(),
	}

	t.conns.Store(tcpConn, true)
	defer t.conns.Delete(tcpConn)

	// Get local address for receive info
	localAddr := conn.LocalAddr().(*net.TCPAddr)
	remoteAddr := conn.RemoteAddr().(*net.TCPAddr)

	rcvInfo := &ReceiveInfo{
		SrcIP:       remoteAddr.IP,
		DstIP:       localAddr.IP,
		SrcPort:     uint16(remoteAddr.Port),
		DstPort:     uint16(localAddr.Port),
		Proto:       ProtoTCP,
		BindAddress: t.si,
	}

	// Read loop
	reader := bufio.NewReader(conn)
	for t.running {
		conn.SetReadDeadline(time.Now().Add(TCPReadTimeout))

		// For SIP over TCP, messages are delimited by Content-Length
		// This is a simplified version - full implementation needs proper framing
		data, err := t.readSIPMessage(reader)
		if err != nil {
			if errors.Is(err, net.ErrClosed) || errors.Is(err, errors.New("EOF")) {
				return
			}
			if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
				continue
			}
			log.Warn("TCP read error", log.ErrField(err))
			return
		}

		if t.handler != nil {
			t.handler(data, tcpConn, rcvInfo)
		}
	}
}

// readSIPMessage reads a complete SIP message from the TCP connection
// TODO: M3 - Proper SIP message framing using Content-Length
func (t *TCPListener) readSIPMessage(reader *bufio.Reader) ([]byte, error) {
	// Read headers until empty line
	var headers []byte
	for {
		line, err := reader.ReadBytes('\n')
		if err != nil {
			return nil, err
		}

		headers = append(headers, line...)

		// Check for empty line (end of headers)
		if len(line) == 2 && line[0] == '\r' && line[1] == '\n' {
			break
		}
		if len(line) == 1 && line[0] == '\n' {
			break
		}
	}

	// TODO: Parse Content-Length and read body
	// For now, just return headers
	return headers, nil
}

// Shutdown stops the TCP listener
func (t *TCPListener) Shutdown(ctx context.Context) error {
	if !t.running {
		return nil
	}

	t.running = false
	close(t.stopCh)

	// Close all connections
	t.conns.Range(func(key, value interface{}) bool {
		if conn, ok := key.(*TCPConnection); ok {
			conn.Close()
		}
		return true
	})

	if t.listener != nil {
		t.listener.Close()
	}

	// Wait for accept loop to finish
	done := make(chan struct{})
	go func() {
		t.wg.Wait()
		close(done)
	}()

	select {
	case <-done:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

// TCPConnection represents a TCP connection
type TCPConnection struct {
	net.Conn
	LocalAddr  net.Addr
	RemoteAddr net.Addr
	CreatedAt  time.Time
	ID         int64
}

// TCPSender provides TCP sending capabilities
type TCPSender struct {
	conn net.Conn
	mu   sync.Mutex
}

// NewTCPSender creates a new TCP connection to the specified address
func NewTCPSender(addr string) (*TCPSender, error) {
	conn, err := net.DialTimeout("tcp", addr, TCPConnTimeout)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to %s: %w", addr, err)
	}

	return &TCPSender{conn: conn}, nil
}

// Send sends data over the TCP connection
func (s *TCPSender) Send(data []byte) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.conn == nil {
		return errors.New("TCP sender not connected")
	}

	s.conn.SetWriteDeadline(time.Now().Add(TCPWriteTimeout))

	_, err := s.conn.Write(data)
	if err != nil {
		return fmt.Errorf("TCP send failed: %w", err)
	}

	return nil
}

// Close closes the TCP connection
func (s *TCPSender) Close() error {
	if s.conn != nil {
		return s.conn.Close()
	}
	return nil
}

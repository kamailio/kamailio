// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TLS transport layer - matching C tls_server.c / tls_wolfssl.c
 *
 * Provides TLS/TLS-PSK transport for SIP over TCP.
 * Uses Go's crypto/tls package for the TLS implementation.
 */

package transport

import (
	"crypto/tls"
	"crypto/x509"
	"errors"
	"fmt"
	"io/ioutil"
	"net"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// TLSConfig holds TLS configuration parameters.
// C: tls_domain_t / tls.cfg
type TLSConfig struct {
	// Certificate file path (PEM format)
	CertFile string
	// Private key file path (PEM format)
	KeyFile string
	// CA certificate file path for client verification
	CAFile string
	// Server name for SNI
	ServerName string
	// Minimum TLS version (default: TLS 1.2)
	MinVersion uint16
	// Maximum TLS version (default: TLS 1.3)
	MaxVersion uint16
	// Verify client certificate
	VerifyClient bool
	// Verify server certificate
	VerifyServer bool
	// Cipher suites (empty = Go defaults)
	CipherSuites []uint16
	// PSK identity and key (for TLS-PSK)
	PSKIdentity string
	PSKKey       []byte
}

// DefaultTLSConfig returns a default TLS configuration.
func DefaultTLSConfig() *TLSConfig {
	return &TLSConfig{
		MinVersion:   tls.VersionTLS12,
		MaxVersion:   tls.VersionTLS13,
		VerifyClient: false,
		VerifyServer: true,
	}
}

// ToGoTLSConfig converts TLSConfig to Go's tls.Config.
func (c *TLSConfig) ToGoTLSConfig() (*tls.Config, error) {
	cfg := &tls.Config{
		ServerName:         c.ServerName,
		MinVersion:         c.MinVersion,
		MaxVersion:         c.MaxVersion,
		InsecureSkipVerify: !c.VerifyServer,
	}

	// Load certificates
	if c.CertFile != "" && c.KeyFile != "" {
		cert, err := tls.LoadX509KeyPair(c.CertFile, c.KeyFile)
		if err != nil {
			return nil, fmt.Errorf("load key pair: %w", err)
		}
		cfg.Certificates = []tls.Certificate{cert}
	}

	// Load CA for client verification
	if c.CAFile != "" && c.VerifyClient {
		caCert, err := ioutil.ReadFile(c.CAFile)
		if err != nil {
			return nil, fmt.Errorf("read CA file: %w", err)
		}
		caPool := x509.NewCertPool()
		caPool.AppendCertsFromPEM(caCert)
		cfg.ClientCAs = caPool
		cfg.ClientAuth = tls.RequireAndVerifyClientCert
	}

	if len(c.CipherSuites) > 0 {
		cfg.CipherSuites = c.CipherSuites
	}

	return cfg, nil
}

// TLSListener wraps a TLS listener for SIP connections.
type TLSListener struct {
	listener net.Listener
	config   *TLSConfig
	addr     string
}

// NewTLSListener creates a new TLS listener.
func NewTLSListener(addr string, config *TLSConfig) (*TLSListener, error) {
	if config == nil {
		config = DefaultTLSConfig()
	}

	tlsCfg, err := config.ToGoTLSConfig()
	if err != nil {
		return nil, err
	}

	ln, err := tls.Listen("tcp", addr, tlsCfg)
	if err != nil {
		return nil, fmt.Errorf("tls listen: %w", err)
	}

	return &TLSListener{
		listener: ln,
		config:   config,
		addr:     addr,
	}, nil
}

// Accept waits for and returns the next TLS connection.
func (l *TLSListener) Accept() (net.Conn, error) {
	return l.listener.Accept()
}

// Close closes the listener.
func (l *TLSListener) Close() error {
	return l.listener.Close()
}

// Addr returns the listener address.
func (l *TLSListener) Addr() net.Addr {
	return l.listener.Addr()
}

// TLSConnection wraps a TLS connection with SIP message framing.
type TLSConnection struct {
	conn       net.Conn
	tlsConn    *tls.Conn
	localAddr  net.Addr
	remoteAddr net.Addr
}

// NewTLSConnection wraps an existing TLS connection.
func NewTLSConnection(conn *tls.Conn) *TLSConnection {
	return &TLSConnection{
		conn:       conn,
		tlsConn:    conn,
		localAddr:  conn.LocalAddr(),
		remoteAddr: conn.RemoteAddr(),
	}
}

// ReadMessage reads a SIP message from the TLS connection.
func (c *TLSConnection) ReadMessage() (*parser.SIPMsg, error) {
	buf := make([]byte, 65535)
	n, err := c.conn.Read(buf)
	if err != nil {
		return nil, err
	}
	return parser.ParseMsg(buf[:n])
}

// WriteMessage sends raw SIP message data over the TLS connection.
func (c *TLSConnection) WriteMessage(data []byte) error {
	_, err := c.conn.Write(data)
	return err
}

// Close closes the connection.
func (c *TLSConnection) Close() error {
	return c.conn.Close()
}

// RemoteAddr returns the remote address.
func (c *TLSConnection) RemoteAddr() net.Addr {
	return c.remoteAddr
}

// LocalAddr returns the local address.
func (c *TLSConnection) LocalAddr() net.Addr {
	return c.localAddr
}

// ConnectionState returns TLS connection state.
func (c *TLSConnection) ConnectionState() tls.ConnectionState {
	return c.tlsConn.ConnectionState()
}

// TLSDialer creates outbound TLS connections.
type TLSDialer struct {
	config *TLSConfig
}

// NewTLSDialer creates a new TLS dialer.
func NewTLSDialer(config *TLSConfig) *TLSDialer {
	if config == nil {
		config = DefaultTLSConfig()
	}
	return &TLSDialer{config: config}
}

// Dial connects to a TLS server.
func (d *TLSDialer) Dial(addr string) (*TLSConnection, error) {
	tlsCfg, err := d.config.ToGoTLSConfig()
	if err != nil {
		return nil, err
	}

	conn, err := tls.DialWithDialer(
		&net.Dialer{Timeout: 5 * time.Second},
		"tcp",
		addr,
		tlsCfg,
	)
	if err != nil {
		return nil, fmt.Errorf("tls dial: %w", err)
	}

	return NewTLSConnection(conn), nil
}

// TLSManager manages TLS listeners and connections.
type TLSManager struct {
	mu        sync.RWMutex
	listeners map[string]*TLSListener
	dialer    *TLSDialer
}

// NewTLSManager creates a new TLS manager.
func NewTLSManager() *TLSManager {
	return &TLSManager{
		listeners: make(map[string]*TLSListener),
		dialer:    NewTLSDialer(nil),
	}
}

// AddListener creates and registers a TLS listener.
func (m *TLSManager) AddListener(addr string, config *TLSConfig) (*TLSListener, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	ln, err := NewTLSListener(addr, config)
	if err != nil {
		return nil, err
	}
	m.listeners[addr] = ln
	return ln, nil
}

// GetListener returns a listener by address.
func (m *TLSManager) GetListener(addr string) *TLSListener {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.listeners[addr]
}

// Dial creates an outbound TLS connection.
func (m *TLSManager) Dial(addr string) (*TLSConnection, error) {
	return m.dialer.Dial(addr)
}

// CloseAll closes all listeners.
func (m *TLSManager) CloseAll() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	var errs []error
	for _, ln := range m.listeners {
		if err := ln.Close(); err != nil {
			errs = append(errs, err)
		}
	}
	if len(errs) > 0 {
		return errors.Join(errs...)
	}
	return nil
}

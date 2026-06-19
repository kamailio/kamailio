// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * RTPEngine client module - matching C rtpengine.c
 *
 * Provides an interface to the NGCP RTPEngine media proxy.
 * RTPEngine handles RTP/RTCP media streams for NAT traversal,
 * media recording, transcoding, etc.
 *
 * Communication is via UDP control socket (binary protocol)
 * or HTTP/JSON API (preferred).
 */

package rtpengine

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"strings"
	"sync"
	"time"
)

// Direction indicates media direction
type Direction string

const (
	DirectionOffer  Direction = "offer"
	DirectionAnswer Direction = "answer"
	DirectionDelete Direction = "delete"
)

// Transport protocol
type Transport string

const (
	TransportUDP  Transport = "UDP"
	TransportTCP  Transport = "TCP"
	TransportTLS  Transport = "TLS"
	TransportDTLS Transport = "DTLS"
)

// MediaOptions represents media stream options for RTPEngine.
type MediaOptions struct {
	Direction    Direction
	Transport    Transport
	Flags        []string // e.g., "replace-origin", "replace-session-connection", "SDES-no"
	ICE          bool
	ICEForce     bool
	DTLS         bool
	SDES         bool
	MediaAddress string // explicit media address
}

// RTPEngineClient communicates with an RTPEngine instance.
// C: bencode_client / rtpengine.c
type RTPEngineClient struct {
	// Control URL (HTTP API)
	controlURL string
	// Control address for UDP (host:port)
	controlAddr string
	// Timeout for HTTP requests
	httpTimeout time.Duration
	// Timeout for UDP requests
	udpTimeout time.Duration
	// Preferred mode: "http" or "udp"
	mode string
}

// RTPEngineConfig holds RTPEngine client configuration.
type RTPEngineConfig struct {
	ControlURL  string        // e.g., "http://localhost:2223"
	ControlAddr string        // e.g., "localhost:2223"
	HTTPTimeout time.Duration
	UDPTimeout  time.Duration
	Mode        string        // "http" or "udp"
}

// NewRTPEngineClient creates a new RTPEngine client.
func NewRTPEngineClient(cfg RTPEngineConfig) *RTPEngineClient {
	if cfg.HTTPTimeout == 0 {
		cfg.HTTPTimeout = 1 * time.Second
	}
	if cfg.UDPTimeout == 0 {
		cfg.UDPTimeout = 500 * time.Millisecond
	}
	if cfg.Mode == "" {
		cfg.Mode = "http"
	}
	return &RTPEngineClient{
		controlURL:  cfg.ControlURL,
		controlAddr: cfg.ControlAddr,
		httpTimeout: cfg.HTTPTimeout,
		udpTimeout:  cfg.UDPTimeout,
		mode:        cfg.Mode,
	}
}

// Offer sends an SDP offer to RTPEngine and returns the modified SDP.
// C: rtpengine_offer()
func (c *RTPEngineClient) Offer(callID, fromTag, toTag, sdpBody string, opts MediaOptions) (string, error) {
	params := map[string]interface{}{
		"command":             "offer",
		"sdp":                 sdpBody,
		"call-id":             callID,
		"from-tag":            fromTag,
		"to-tag":              toTag,
		"direction":           string(opts.Direction),
		"transport-protocol":  string(opts.Transport),
	}

	if opts.MediaAddress != "" {
		params["media-address"] = opts.MediaAddress
	}
	if len(opts.Flags) > 0 {
		params["flags"] = strings.Join(opts.Flags, " ")
	}

	return c.sendCommand(params)
}

// Answer sends an SDP answer to RTPEngine.
// C: rtpengine_answer()
func (c *RTPEngineClient) Answer(callID, fromTag, toTag, sdpBody string, opts MediaOptions) (string, error) {
	params := map[string]interface{}{
		"command":             "answer",
		"sdp":                 sdpBody,
		"call-id":             callID,
		"from-tag":            fromTag,
		"to-tag":              toTag,
		"direction":           string(opts.Direction),
		"transport-protocol":  string(opts.Transport),
	}

	return c.sendCommand(params)
}

// Delete removes a media session from RTPEngine.
// C: rtpengine_delete()
func (c *RTPEngineClient) Delete(callID, fromTag, toTag string) error {
	params := map[string]interface{}{
		"command":  "delete",
		"call-id":  callID,
		"from-tag": fromTag,
		"to-tag":   toTag,
	}

	_, err := c.sendCommand(params)
	return err
}

// Query queries the state of a media session.
func (c *RTPEngineClient) Query(callID, fromTag, toTag string) (map[string]interface{}, error) {
	params := map[string]interface{}{
		"command":  "query",
		"call-id":  callID,
		"from-tag": fromTag,
		"to-tag":   toTag,
	}

	result, err := c.sendCommandRaw(params)
	if err != nil {
		return nil, err
	}

	var resp map[string]interface{}
	if err := json.Unmarshal([]byte(result), &resp); err != nil {
		return nil, fmt.Errorf("unmarshal query response: %w", err)
	}
	return resp, nil
}

// Update updates an existing media session.
func (c *RTPEngineClient) Update(callID, fromTag, toTag, sdpBody string, opts MediaOptions) (string, error) {
	params := map[string]interface{}{
		"command":             "update",
		"call-id":             callID,
		"from-tag":            fromTag,
		"to-tag":              toTag,
		"sdp":                 sdpBody,
		"direction":           string(opts.Direction),
		"transport-protocol":  string(opts.Transport),
	}

	return c.sendCommand(params)
}

// Ping checks if RTPEngine is reachable.
func (c *RTPEngineClient) Ping() error {
	params := map[string]interface{}{
		"command": "ping",
	}

	_, err := c.sendCommand(params)
	return err
}

// ListSessions lists all active media sessions.
func (c *RTPEngineClient) ListSessions() ([]map[string]interface{}, error) {
	params := map[string]interface{}{
		"command": "list",
	}

	result, err := c.sendCommandRaw(params)
	if err != nil {
		return nil, err
	}

	var sessions []map[string]interface{}
	if err := json.Unmarshal([]byte(result), &sessions); err != nil {
		return nil, fmt.Errorf("unmarshal list response: %w", err)
	}
	return sessions, nil
}

// sendCommand sends a command to RTPEngine via the configured mode.
// For SDP-oriented commands, it extracts and returns the SDP body.
func (c *RTPEngineClient) sendCommand(params map[string]interface{}) (string, error) {
	switch c.mode {
	case "http":
		return c.sendHTTP(params)
	case "udp":
		return c.sendUDP(params)
	default:
		return c.sendHTTP(params)
	}
}

// sendCommandRaw sends a command and returns the raw JSON response body.
// Used by Query and ListSessions which need the full JSON, not just the SDP field.
func (c *RTPEngineClient) sendCommandRaw(params map[string]interface{}) (string, error) {
	switch c.mode {
	case "http":
		return c.sendHTTPRaw(params)
	case "udp":
		return c.sendUDP(params)
	default:
		return c.sendHTTPRaw(params)
	}
}

// sendHTTPRaw sends a command via HTTP/JSON API and returns the raw response body.
func (c *RTPEngineClient) sendHTTPRaw(params map[string]interface{}) (string, error) {
	jsonData, err := json.Marshal(params)
	if err != nil {
		return "", fmt.Errorf("marshal: %w", err)
	}

	client := &http.Client{Timeout: c.httpTimeout}
	resp, err := client.Post(c.controlURL, "application/json", bytes.NewReader(jsonData))
	if err != nil {
		return "", fmt.Errorf("http post: %w", err)
	}
	defer resp.Body.Close()

	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("read response: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("rtpengine error: %s (status %d)", string(body), resp.StatusCode)
	}

	return string(body), nil
}

// sendHTTP sends a command via HTTP/JSON API.
func (c *RTPEngineClient) sendHTTP(params map[string]interface{}) (string, error) {
	jsonData, err := json.Marshal(params)
	if err != nil {
		return "", fmt.Errorf("marshal: %w", err)
	}

	client := &http.Client{Timeout: c.httpTimeout}
	resp, err := client.Post(c.controlURL, "application/json", bytes.NewReader(jsonData))
	if err != nil {
		return "", fmt.Errorf("http post: %w", err)
	}
	defer resp.Body.Close()

	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("read response: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("rtpengine error: %s (status %d)", string(body), resp.StatusCode)
	}

	var result struct {
		SDP   string `json:"sdp,omitempty"`
		Error string `json:"error,omitempty"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return string(body), nil
	}
	if result.Error != "" {
		return "", fmt.Errorf("rtpengine: %s", result.Error)
	}

	return result.SDP, nil
}

// sendUDP sends a command via UDP (bencode protocol).
func (c *RTPEngineClient) sendUDP(params map[string]interface{}) (string, error) {
	if c.controlAddr == "" {
		return "", fmt.Errorf("no UDP control address configured")
	}

	conn, err := net.DialTimeout("udp", c.controlAddr, c.udpTimeout)
	if err != nil {
		return "", fmt.Errorf("udp dial: %w", err)
	}
	defer conn.Close()

	// For UDP, we use a simple JSON-over-UDP protocol
	// (RTPEngine also supports bencode, but JSON is simpler)
	jsonData, err := json.Marshal(params)
	if err != nil {
		return "", fmt.Errorf("marshal: %w", err)
	}

	conn.SetDeadline(time.Now().Add(c.udpTimeout))
	_, err = conn.Write(jsonData)
	if err != nil {
		return "", fmt.Errorf("udp write: %w", err)
	}

	buf := make([]byte, 65535)
	n, err := conn.Read(buf)
	if err != nil {
		return "", fmt.Errorf("udp read: %w", err)
	}

	return string(buf[:n]), nil
}

// RTPEngineManager manages multiple RTPEngine instances.
type RTPEngineManager struct {
	mu      sync.RWMutex
	engines map[string]*RTPEngineClient
}

// NewRTPEngineManager creates a new RTPEngine manager.
func NewRTPEngineManager() *RTPEngineManager {
	return &RTPEngineManager{
		engines: make(map[string]*RTPEngineClient),
	}
}

// AddEngine adds an RTPEngine instance.
func (m *RTPEngineManager) AddEngine(name string, client *RTPEngineClient) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.engines[name] = client
}

// GetEngine returns an RTPEngine client by name.
func (m *RTPEngineManager) GetEngine(name string) *RTPEngineClient {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.engines[name]
}

// Offer sends an offer to a named RTPEngine.
func (m *RTPEngineManager) Offer(engineName, callID, fromTag, toTag, sdpBody string, opts MediaOptions) (string, error) {
	engine := m.GetEngine(engineName)
	if engine == nil {
		return "", fmt.Errorf("rtpengine %q not found", engineName)
	}
	return engine.Offer(callID, fromTag, toTag, sdpBody, opts)
}

// Answer sends an answer to a named RTPEngine.
func (m *RTPEngineManager) Answer(engineName, callID, fromTag, toTag, sdpBody string, opts MediaOptions) (string, error) {
	engine := m.GetEngine(engineName)
	if engine == nil {
		return "", fmt.Errorf("rtpengine %q not found", engineName)
	}
	return engine.Answer(callID, fromTag, toTag, sdpBody, opts)
}

// Delete removes a session from a named RTPEngine.
func (m *RTPEngineManager) Delete(engineName, callID, fromTag, toTag string) error {
	engine := m.GetEngine(engineName)
	if engine == nil {
		return fmt.Errorf("rtpengine %q not found", engineName)
	}
	return engine.Delete(callID, fromTag, toTag)
}

// PingAll checks all RTPEngine instances.
func (m *RTPEngineManager) PingAll() map[string]error {
	m.mu.RLock()
	engines := make(map[string]*RTPEngineClient, len(m.engines))
	for name, engine := range m.engines {
		engines[name] = engine
	}
	m.mu.RUnlock()

	results := make(map[string]error)
	for name, engine := range engines {
		results[name] = engine.Ping()
	}
	return results
}

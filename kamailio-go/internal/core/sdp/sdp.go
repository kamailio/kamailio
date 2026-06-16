// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SDP body parser - matching C sdp.h / sdp.c
 *
 * SDP format (RFC 4566):
 * v= (protocol version)
 * o= (originator and session identifier)
 * s= (session name)
 * i=* (session information)
 * u=* (URI of description)
 * e=* (email address)
 * p=* (phone number)
 * c=* (connection information)
 * b=* (bandwidth information)
 * t= (time session is active)
 * r=* (repeat times)
 * z=* (time zone adjustments)
 * k=* (encryption key)
 * a=* (session attribute lines)
 * m= (media name and transport address)
 * ... media-level attributes ...
 */

package sdp

import (
	"bufio"
	"fmt"
	"net"
	"strconv"
	"strings"
)

// Session represents an SDP session description
// C: struct sdp_session_cell
type Session struct {
	// Session-level fields
	Version     int
	Origin      *Origin
	SessionName string
	Info        string
	URI         string
	Email       string
	Phone       string
	Connection  *Connection
	Bandwidth   []*Bandwidth
	Timing      []*Timing
	Attributes  []string

	// Media streams
	Media []*Media

	// Raw content
	RawBody string
}

// Origin represents the o= line
// C: struct sdp_origin
type Origin struct {
	Username   string
	SessionID  uint64
	Version    uint64
	NetType    string
	AddrType   string
	UnicastAddr string
}

// Connection represents the c= line
// C: struct sdp_connection
type Connection struct {
	NetType    string
	AddrType   string
	ConnectionAddr string
	TTL        int
	NumAddr    int
	IP         net.IP
}

// Bandwidth represents the b= line
// C: struct sdp_bandwidth
type Bandwidth struct {
	Type  string
	Value int
}

// Timing represents the t= line
// C: struct sdp_time
type Timing struct {
	Start   uint64
	Stop    uint64
	Repeat  []*Repeat
}

// Repeat represents the r= line
// C: struct sdp_repeat
type Repeat struct {
	Interval int
	Duration int
	Offsets  []int
}

// Media represents an m= line and associated attributes
// C: struct sdp_media_cell
type Media struct {
	MediaType   string
	Port        int
	PortCount   int
	Proto       string
	Formats     []string
	Connection  *Connection
	Bandwidth   []*Bandwidth
	Attributes  []string

	// Parsed attributes
	RTPMap      map[int]*RTPMap
	FMTP        map[int]string
	PTime       int
	MaxPTime    int
	Direction   string // sendrecv, sendonly, recvonly, inactive
	Crypto      []*Crypto
}

// RTPMap represents the rtpmap attribute
// C: struct sdp_rtpmap
type RTPMap struct {
	PayloadType int
	Encoding    string
	ClockRate   int
	Params      string
}

// Crypto represents the crypto attribute (SRTP)
type Crypto struct {
	Tag       int
	CryptoSuite string
	KeyParams   string
	SessionParams string
}

// Parser parses SDP body
type Parser struct {
	session *Session
	currentMedia *Media
}

// Parse parses an SDP body
func Parse(body string) (*Session, error) {
	p := &Parser{
		session: &Session{
			Version: -1,
		},
	}

	scanner := bufio.NewScanner(strings.NewReader(body))
	for scanner.Scan() {
		line := scanner.Text()
		if len(line) < 2 {
			continue
		}

		if line[1] != '=' {
			continue
		}

		field := line[0]
		value := line[2:]

		if err := p.parseLine(field, value); err != nil {
			return nil, err
		}
	}

	if p.session.Version < 0 {
		return nil, fmt.Errorf("missing v= line")
	}

	p.session.RawBody = body
	return p.session, nil
}

// parseLine parses a single SDP line
func (p *Parser) parseLine(field byte, value string) error {
	switch field {
	case 'v':
		return p.parseVersion(value)
	case 'o':
		return p.parseOrigin(value)
	case 's':
		p.session.SessionName = value
	case 'i':
		if p.currentMedia != nil {
			// Media-level info
		} else {
			p.session.Info = value
		}
	case 'u':
		p.session.URI = value
	case 'e':
		p.session.Email = value
	case 'p':
		p.session.Phone = value
	case 'c':
		return p.parseConnection(value)
	case 'b':
		return p.parseBandwidth(value)
	case 't':
		return p.parseTiming(value)
	case 'r':
		return p.parseRepeat(value)
	case 'a':
		return p.parseAttribute(value)
	case 'm':
		return p.parseMedia(value)
	case 'k':
		// Encryption key - deprecated, ignore
	}
	return nil
}

// parseVersion parses v= line
func (p *Parser) parseVersion(value string) error {
	v, err := strconv.Atoi(value)
	if err != nil {
		return fmt.Errorf("invalid version: %s", value)
	}
	p.session.Version = v
	return nil
}

// parseOrigin parses o= line
// o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
func (p *Parser) parseOrigin(value string) error {
	parts := strings.Fields(value)
	if len(parts) < 6 {
		return fmt.Errorf("invalid origin line: %s", value)
	}

	o := &Origin{
		Username:   parts[0],
		NetType:    parts[3],
		AddrType:   parts[4],
		UnicastAddr: parts[5],
	}

	// Parse session ID
	if sessID, err := strconv.ParseUint(parts[1], 10, 64); err == nil {
		o.SessionID = sessID
	}

	// Parse version
	if ver, err := strconv.ParseUint(parts[2], 10, 64); err == nil {
		o.Version = ver
	}

	p.session.Origin = o
	return nil
}

// parseConnection parses c= line
// c=<nettype> <addrtype> <connection-address>
func (p *Parser) parseConnection(value string) error {
	parts := strings.Fields(value)
	if len(parts) < 3 {
		return fmt.Errorf("invalid connection line: %s", value)
	}

	c := &Connection{
		NetType:      parts[0],
		AddrType:     parts[1],
		ConnectionAddr: parts[2],
	}

	// Parse TTL and number of addresses for multicast
	if strings.Contains(parts[2], "/") {
		addrParts := strings.Split(parts[2], "/")
		c.ConnectionAddr = addrParts[0]
		if len(addrParts) > 1 {
			if ttl, err := strconv.Atoi(addrParts[1]); err == nil {
				c.TTL = ttl
			}
		}
		if len(addrParts) > 2 {
			if num, err := strconv.Atoi(addrParts[2]); err == nil {
				c.NumAddr = num
			}
		}
	}

	// Parse IP
	c.IP = net.ParseIP(c.ConnectionAddr)

	if p.currentMedia != nil {
		p.currentMedia.Connection = c
	} else {
		p.session.Connection = c
	}

	return nil
}

// parseBandwidth parses b= line
// b=<bwtype>:<bandwidth>
func (p *Parser) parseBandwidth(value string) error {
	parts := strings.SplitN(value, ":", 2)
	if len(parts) != 2 {
		return fmt.Errorf("invalid bandwidth line: %s", value)
	}

	bw, err := strconv.Atoi(parts[1])
	if err != nil {
		return fmt.Errorf("invalid bandwidth value: %s", parts[1])
	}

	b := &Bandwidth{
		Type:  parts[0],
		Value: bw,
	}

	if p.currentMedia != nil {
		p.currentMedia.Bandwidth = append(p.currentMedia.Bandwidth, b)
	} else {
		p.session.Bandwidth = append(p.session.Bandwidth, b)
	}

	return nil
}

// parseTiming parses t= line
// t=<start-time> <stop-time>
func (p *Parser) parseTiming(value string) error {
	parts := strings.Fields(value)
	if len(parts) < 2 {
		return fmt.Errorf("invalid timing line: %s", value)
	}

	start, _ := strconv.ParseUint(parts[0], 10, 64)
	stop, _ := strconv.ParseUint(parts[1], 10, 64)

	t := &Timing{
		Start: start,
		Stop:  stop,
	}

	p.session.Timing = append(p.session.Timing, t)
	return nil
}

// parseRepeat parses r= line
// r=<repeat interval> <active duration> <offsets from start-time>
func (p *Parser) parseRepeat(value string) error {
	if len(p.session.Timing) == 0 {
		return fmt.Errorf("r= line without t= line")
	}

	parts := strings.Fields(value)
	if len(parts) < 2 {
		return fmt.Errorf("invalid repeat line: %s", value)
	}

	r := &Repeat{}

	// Parse interval and duration (may be in NTP format)
	r.Interval = parseTimeValue(parts[0])
	r.Duration = parseTimeValue(parts[1])

	// Parse offsets
	for i := 2; i < len(parts); i++ {
		r.Offsets = append(r.Offsets, parseTimeValue(parts[i]))
	}

	// Add to last timing
	lastTiming := p.session.Timing[len(p.session.Timing)-1]
	lastTiming.Repeat = append(lastTiming.Repeat, r)

	return nil
}

// parseTimeValue parses SDP time values (NTP format)
func parseTimeValue(s string) int {
	// Check for NTP format: <number><unit>
	// d = days, h = hours, m = minutes, s = seconds
	if len(s) > 1 {
		unit := s[len(s)-1]
		value, err := strconv.Atoi(s[:len(s)-1])
		if err == nil {
			switch unit {
			case 'd':
				return value * 86400
			case 'h':
				return value * 3600
			case 'm':
				return value * 60
			case 's':
				return value
			}
		}
	}

	// Plain seconds
	v, _ := strconv.Atoi(s)
	return v
}

// parseMedia parses m= line
// m=<media> <port> <proto> <fmt> ...
func (p *Parser) parseMedia(value string) error {
	parts := strings.Fields(value)
	if len(parts) < 4 {
		return fmt.Errorf("invalid media line: %s", value)
	}

	m := &Media{
		MediaType: parts[0],
		Proto:     parts[2],
		Formats:   parts[3:],
		RTPMap:    make(map[int]*RTPMap),
		FMTP:      make(map[int]string),
		Direction: "sendrecv", // default
	}

	// Parse port
	portParts := strings.Split(parts[1], "/")
	if port, err := strconv.Atoi(portParts[0]); err == nil {
		m.Port = port
	}
	if len(portParts) > 1 {
		if count, err := strconv.Atoi(portParts[1]); err == nil {
			m.PortCount = count
		}
	} else {
		m.PortCount = 1
	}

	p.session.Media = append(p.session.Media, m)
	p.currentMedia = m

	return nil
}

// parseAttribute parses a= line
// a=<attribute>
// a=<attribute>:<value>
func (p *Parser) parseAttribute(value string) error {
	var name, attrValue string
	if colon := strings.Index(value, ":"); colon != -1 {
		name = value[:colon]
		attrValue = value[colon+1:]
	} else {
		name = value
	}

	// Add to appropriate attribute list
	attr := value
	if p.currentMedia != nil {
		p.currentMedia.Attributes = append(p.currentMedia.Attributes, attr)
		p.parseMediaAttribute(name, attrValue)
	} else {
		p.session.Attributes = append(p.session.Attributes, attr)
	}

	return nil
}

// parseMediaAttribute parses media-level attributes
func (p *Parser) parseMediaAttribute(name, value string) {
	if p.currentMedia == nil {
		return
	}

	switch strings.ToLower(name) {
	case "rtpmap":
		// a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding params>]
		p.parseRTPMap(value)

	case "fmtp":
		// a=fmtp:<format> <format specific parameters>
		p.parseFMTP(value)

	case "ptime":
		// a=ptime:<packet time>
		if pt, err := strconv.Atoi(value); err == nil {
			p.currentMedia.PTime = pt
		}

	case "maxptime":
		// a=maxptime:<maximum packet time>
		if pt, err := strconv.Atoi(value); err == nil {
			p.currentMedia.MaxPTime = pt
		}

	case "sendrecv":
		p.currentMedia.Direction = "sendrecv"

	case "sendonly":
		p.currentMedia.Direction = "sendonly"

	case "recvonly":
		p.currentMedia.Direction = "recvonly"

	case "inactive":
		p.currentMedia.Direction = "inactive"

	case "crypto":
		p.parseCrypto(value)
	}
}

// parseRTPMap parses rtpmap attribute
func (p *Parser) parseRTPMap(value string) {
	// <payload type> <encoding name>/<clock rate>[/<encoding params>]
	parts := strings.SplitN(value, " ", 2)
	if len(parts) != 2 {
		return
	}

	pt, err := strconv.Atoi(parts[0])
	if err != nil {
		return
	}

	// Parse encoding/clockrate
	encParts := strings.SplitN(parts[1], "/", 3)
	if len(encParts) < 2 {
		return
	}

	rtp := &RTPMap{
		PayloadType: pt,
		Encoding:    encParts[0],
	}

	if cr, err := strconv.Atoi(encParts[1]); err == nil {
		rtp.ClockRate = cr
	}

	if len(encParts) > 2 {
		rtp.Params = encParts[2]
	}

	p.currentMedia.RTPMap[pt] = rtp
}

// parseFMTP parses fmtp attribute
func (p *Parser) parseFMTP(value string) {
	parts := strings.SplitN(value, " ", 2)
	if len(parts) != 2 {
		return
	}

	pt, err := strconv.Atoi(parts[0])
	if err != nil {
		return
	}

	p.currentMedia.FMTP[pt] = parts[1]
}

// parseCrypto parses crypto attribute (SRTP)
func (p *Parser) parseCrypto(value string) {
	// <tag> <crypto-suite> <key-params> [<session-params>]
	parts := strings.Fields(value)
	if len(parts) < 3 {
		return
	}

	tag, err := strconv.Atoi(parts[0])
	if err != nil {
		return
	}

	c := &Crypto{
		Tag:         tag,
		CryptoSuite: parts[1],
		KeyParams:   parts[2],
	}

	if len(parts) > 3 {
		c.SessionParams = strings.Join(parts[3:], " ")
	}

	p.currentMedia.Crypto = append(p.currentMedia.Crypto, c)
}

// GetMediaByType returns media streams of a specific type
func (s *Session) GetMediaByType(mediaType string) []*Media {
	var result []*Media
	for _, m := range s.Media {
		if strings.EqualFold(m.MediaType, mediaType) {
			result = append(result, m)
		}
	}
	return result
}

// GetAudio returns all audio media streams
func (s *Session) GetAudio() []*Media {
	return s.GetMediaByType("audio")
}

// GetVideo returns all video media streams
func (s *Session) GetVideo() []*Media {
	return s.GetMediaByType("video")
}

// GetConnection returns the connection address (media-level or session-level)
func (m *Media) GetConnection() *Connection {
	if m.Connection != nil {
		return m.Connection
	}
	return nil // Caller should check session-level connection
}

// GetCodec returns the codec for a payload type
func (m *Media) GetCodec(pt int) *RTPMap {
	return m.RTPMap[pt]
}

// HasAttribute returns true if the media has the specified attribute
func (m *Media) HasAttribute(attr string) bool {
	for _, a := range m.Attributes {
		if strings.HasPrefix(strings.ToLower(a), strings.ToLower(attr)) {
			return true
		}
	}
	return false
}

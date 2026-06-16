// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * SDP parser tests
 */

package sdp

import (
	"testing"
)

const testSDP = `v=0
o=- 1234567890 1234567891 IN IP4 10.0.0.1
s=Test Session
c=IN IP4 10.0.0.1
t=0 0
m=audio 49170 RTP/AVP 0 8 101
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:101 telephone-event/8000
a=sendrecv
m=video 49172 RTP/AVP 96
a=rtpmap:96 H264/90000
a=fmtp:96 profile-level-id=42e01f
a=sendonly
`

func TestParseSDP(t *testing.T) {
	session, err := Parse(testSDP)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	// Check version
	if session.Version != 0 {
		t.Errorf("Expected version 0, got %d", session.Version)
	}

	// Check origin
	if session.Origin == nil {
		t.Fatal("Expected origin")
	}
	if session.Origin.Username != "-" {
		t.Errorf("Expected username '-', got %q", session.Origin.Username)
	}
	if session.Origin.SessionID != 1234567890 {
		t.Errorf("Expected session ID 1234567890, got %d", session.Origin.SessionID)
	}

	// Check session name
	if session.SessionName != "Test Session" {
		t.Errorf("Expected session name 'Test Session', got %q", session.SessionName)
	}

	// Check connection
	if session.Connection == nil {
		t.Fatal("Expected connection")
	}
	if session.Connection.ConnectionAddr != "10.0.0.1" {
		t.Errorf("Expected connection address '10.0.0.1', got %q", session.Connection.ConnectionAddr)
	}

	// Check media count
	if len(session.Media) != 2 {
		t.Fatalf("Expected 2 media streams, got %d", len(session.Media))
	}
}

func TestParseSDPMedia(t *testing.T) {
	session, err := Parse(testSDP)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	// Check audio media
	audio := session.GetAudio()
	if len(audio) != 1 {
		t.Fatalf("Expected 1 audio stream, got %d", len(audio))
	}

	if audio[0].Port != 49170 {
		t.Errorf("Expected audio port 49170, got %d", audio[0].Port)
	}

	if audio[0].Proto != "RTP/AVP" {
		t.Errorf("Expected proto 'RTP/AVP', got %q", audio[0].Proto)
	}

	if len(audio[0].Formats) != 3 {
		t.Errorf("Expected 3 formats, got %d", len(audio[0].Formats))
	}

	// Check RTP map
	codec0 := audio[0].GetCodec(0)
	if codec0 == nil {
		t.Fatal("Expected codec for PT 0")
	}
	if codec0.Encoding != "PCMU" {
		t.Errorf("Expected encoding 'PCMU', got %q", codec0.Encoding)
	}
	if codec0.ClockRate != 8000 {
		t.Errorf("Expected clock rate 8000, got %d", codec0.ClockRate)
	}

	// Check direction
	if audio[0].Direction != "sendrecv" {
		t.Errorf("Expected direction 'sendrecv', got %q", audio[0].Direction)
	}

	// Check video media
	video := session.GetVideo()
	if len(video) != 1 {
		t.Fatalf("Expected 1 video stream, got %d", len(video))
	}

	if video[0].Port != 49172 {
		t.Errorf("Expected video port 49172, got %d", video[0].Port)
	}

	// Check FMTP
	fmtp := video[0].FMTP[96]
	if fmtp == "" {
		t.Error("Expected FMTP for PT 96")
	}

	// Check direction
	if video[0].Direction != "sendonly" {
		t.Errorf("Expected direction 'sendonly', got %q", video[0].Direction)
	}
}

func TestParseSDPConnection(t *testing.T) {
	session, err := Parse(testSDP)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	conn := session.Connection
	if conn == nil {
		t.Fatal("Expected connection")
	}

	if conn.NetType != "IN" {
		t.Errorf("Expected net type 'IN', got %q", conn.NetType)
	}

	if conn.AddrType != "IP4" {
		t.Errorf("Expected addr type 'IP4', got %q", conn.AddrType)
	}

	if conn.IP == nil {
		t.Error("Expected parsed IP")
	}
}

func TestParseSDPMulticast(t *testing.T) {
	sdp := `v=0
o=- 1 1 IN IP4 10.0.0.1
s=Multicast Test
c=IN IP4 224.1.1.1/127/3
t=0 0
m=audio 49170 RTP/AVP 0
`

	session, err := Parse(sdp)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	conn := session.Connection
	if conn == nil {
		t.Fatal("Expected connection")
	}

	if conn.TTL != 127 {
		t.Errorf("Expected TTL 127, got %d", conn.TTL)
	}

	if conn.NumAddr != 3 {
		t.Errorf("Expected NumAddr 3, got %d", conn.NumAddr)
	}
}

func TestParseSDPBandwidth(t *testing.T) {
	sdp := `v=0
o=- 1 1 IN IP4 10.0.0.1
s=Bandwidth Test
c=IN IP4 10.0.0.1
t=0 0
b=AS:128
b=TIAS:100000
m=audio 49170 RTP/AVP 0
b=AS:64
`

	session, err := Parse(sdp)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	// Check session-level bandwidth
	if len(session.Bandwidth) != 2 {
		t.Fatalf("Expected 2 session bandwidth lines, got %d", len(session.Bandwidth))
	}

	if session.Bandwidth[0].Type != "AS" {
		t.Errorf("Expected bandwidth type 'AS', got %q", session.Bandwidth[0].Type)
	}

	if session.Bandwidth[0].Value != 128 {
		t.Errorf("Expected bandwidth value 128, got %d", session.Bandwidth[0].Value)
	}

	// Check media-level bandwidth
	audio := session.GetAudio()
	if len(audio) != 1 {
		t.Fatal("Expected 1 audio stream")
	}

	if len(audio[0].Bandwidth) != 1 {
		t.Fatalf("Expected 1 media bandwidth line, got %d", len(audio[0].Bandwidth))
	}

	if audio[0].Bandwidth[0].Value != 64 {
		t.Errorf("Expected media bandwidth value 64, got %d", audio[0].Bandwidth[0].Value)
	}
}

func TestParseSDPTiming(t *testing.T) {
	sdp := `v=0
o=- 1 1 IN IP4 10.0.0.1
s=Timing Test
c=IN IP4 10.0.0.1
t=1234567890 1234571490
r=604800 3600 0 90000
`

	session, err := Parse(sdp)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	if len(session.Timing) != 1 {
		t.Fatalf("Expected 1 timing line, got %d", len(session.Timing))
	}

	timing := session.Timing[0]
	if timing.Start != 1234567890 {
		t.Errorf("Expected start 1234567890, got %d", timing.Start)
	}

	if len(timing.Repeat) != 1 {
		t.Fatalf("Expected 1 repeat line, got %d", len(timing.Repeat))
	}

	repeat := timing.Repeat[0]
	if repeat.Interval != 604800 {
		t.Errorf("Expected interval 604800, got %d", repeat.Interval)
	}
}

func TestParseSDPCrypto(t *testing.T) {
	sdp := `v=0
o=- 1 1 IN IP4 10.0.0.1
s=Crypto Test
c=IN IP4 10.0.0.1
t=0 0
m=audio 49170 RTP/SAVP 0
a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:W2hXgqxsXKvFJ3NPTzQxTQ==
`

	session, err := Parse(sdp)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	audio := session.GetAudio()
	if len(audio) != 1 {
		t.Fatal("Expected 1 audio stream")
	}

	if len(audio[0].Crypto) != 1 {
		t.Fatalf("Expected 1 crypto line, got %d", len(audio[0].Crypto))
	}

	crypto := audio[0].Crypto[0]
	if crypto.Tag != 1 {
		t.Errorf("Expected crypto tag 1, got %d", crypto.Tag)
	}

	if crypto.CryptoSuite != "AES_CM_128_HMAC_SHA1_80" {
		t.Errorf("Expected crypto suite 'AES_CM_128_HMAC_SHA1_80', got %q", crypto.CryptoSuite)
	}
}

func TestParseSDPTimeValues(t *testing.T) {
	tests := []struct {
		input  string
		expect int
	}{
		{"1d", 86400},
		{"1h", 3600},
		{"1m", 60},
		{"1s", 1},
		{"3600", 3600},
	}

	for _, tt := range tests {
		result := parseTimeValue(tt.input)
		if result != tt.expect {
			t.Errorf("parseTimeValue(%q) = %d, want %d", tt.input, result, tt.expect)
		}
	}
}

func TestMediaHasAttribute(t *testing.T) {
	session, err := Parse(testSDP)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}

	audio := session.GetAudio()
	if len(audio) != 1 {
		t.Fatal("Expected 1 audio stream")
	}

	if !audio[0].HasAttribute("sendrecv") {
		t.Error("Expected sendrecv attribute")
	}

	if audio[0].HasAttribute("inactive") {
		t.Error("Should not have inactive attribute")
	}
}

func TestParseSDPInvalid(t *testing.T) {
	// Missing v= line
	_, err := Parse("s=Test\n")
	if err == nil {
		t.Error("Expected error for missing v= line")
	}

	// Invalid version
	_, err = Parse("v=abc\n")
	if err == nil {
		t.Error("Expected error for invalid version")
	}
}

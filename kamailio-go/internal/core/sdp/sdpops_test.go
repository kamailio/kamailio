// SPDX-License-Identifier: GPL-2.0-or-later
package sdp

import (
	"strings"
	"testing"
)

const testSDPOps = "v=0\r\n" +
	"o=- 0 0 IN IP4 192.168.1.10\r\n" +
	"s=-\r\n" +
	"c=IN IP4 192.168.1.10\r\n" +
	"t=0 0\r\n" +
	"m=audio 8000 RTP/AVP 0 8 101\r\n" +
	"a=rtpmap:0 PCMU/8000\r\n" +
	"a=rtpmap:8 PCMA/8000\r\n" +
	"m=video 9000 RTP/AVP 96\r\n" +
	"a=rtpmap:96 H264/90000\r\n"

func TestSDPOps_SessionLevelAddr(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	addr := so.SessionLevelAddr()
	if addr != "192.168.1.10" {
		t.Errorf("addr = %q, want 192.168.1.10", addr)
	}
}

func TestSDPOps_MediaStreams(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	streams := so.MediaStreams()
	if len(streams) != 2 {
		t.Fatalf("expected 2 streams, got %d", len(streams))
	}
	if streams[0].Type != "audio" {
		t.Errorf("stream 0 type = %q, want audio", streams[0].Type)
	}
	if streams[0].Port != 8000 {
		t.Errorf("stream 0 port = %d, want 8000", streams[0].Port)
	}
	if streams[1].Type != "video" {
		t.Errorf("stream 1 type = %q, want video", streams[1].Type)
	}
}

func TestSDPOps_RewriteAddr(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	newSDP := so.RewriteAddr("192.168.1.10", "203.0.113.50")
	if !strings.Contains(newSDP, "203.0.113.50") {
		t.Error("expected new IP in rewritten SDP")
	}
	if strings.Contains(newSDP, "192.168.1.10") {
		t.Error("expected old IP to be removed from rewritten SDP")
	}
}

func TestSDPOps_RewritePort(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	newSDP := so.RewritePort("audio", 8000, 10000)
	if !strings.Contains(newSDP, "m=audio 10000") {
		t.Error("expected rewritten audio port")
	}
}

func TestSDPOps_HasMedia(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	if !so.HasMedia("audio") {
		t.Error("expected audio media")
	}
	if !so.HasMedia("video") {
		t.Error("expected video media")
	}
	if so.HasMedia("application") {
		t.Error("expected no application media")
	}
}

func TestSDPOps_GetPayloadTypes(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	pts := so.GetPayloadTypes("audio")
	if len(pts) != 3 {
		t.Errorf("expected 3 payload types, got %d", len(pts))
	}
	if pts[0] != "0" {
		t.Errorf("first PT = %q, want 0", pts[0])
	}
}

func TestSDPOps_AddAttribute(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	newSDP := so.AddAttribute("audio", "sendrecv", "")
	if !strings.Contains(newSDP, "a=sendrecv") {
		t.Error("expected added attribute")
	}
}

func TestSDPOps_RemoveAttribute(t *testing.T) {
	so := NewSDPOps(testSDPOps)
	newSDP := so.RemoveAttribute("audio", "rtpmap:0 PCMU/8000")
	if strings.Contains(newSDP, "a=rtpmap:0 PCMU/8000") {
		t.Error("expected attribute to be removed")
	}
}

func TestValidate(t *testing.T) {
	if err := Validate(testSDPOps); err != nil {
		t.Errorf("valid SDP should pass: %v", err)
	}
	if err := Validate("invalid"); err == nil {
		t.Error("invalid SDP should fail")
	}
}

func TestExtractMediaIP(t *testing.T) {
	ip := ExtractMediaIP(testSDPOps)
	if ip == nil {
		t.Fatal("expected non-nil IP")
	}
	if ip.String() != "192.168.1.10" {
		t.Errorf("IP = %q, want 192.168.1.10", ip.String())
	}
}

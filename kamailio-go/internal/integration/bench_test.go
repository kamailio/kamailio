// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * End-to-end performance benchmarks. These tests exercise
 * higher-level components that depend on the parser – they
 * live here (instead of in internal/core/parser/bench_test.go)
 * to avoid Go import cycles.
 */

package integration

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
)

func benchLargeINVITE() []byte {
	sdp := "v=0\r\n" +
		"o=- 12345 67890 IN IP4 198.51.100.1\r\n" +
		"s=Session\r\n" +
		"i=Benchmark SIP Session\r\n" +
		"c=IN IP4 198.51.100.1\r\n" +
		"t=0 0\r\n" +
		"a=sendrecv\r\n" +
		"m=audio 49152 RTP/AVP 0 8 101 111\r\n" +
		"a=rtpmap:0 PCMU/8000\r\n" +
		"a=rtpmap:8 PCMA/8000\r\n" +
		"a=rtpmap:101 telephone-event/8000\r\n" +
		"a=rtpmap:111 opus/48000/2\r\n" +
		"a=fmtp:111 minptime=10;useinbandfec=1\r\n" +
		"a=sendrecv\r\n" +
		"m=video 49154 RTP/AVP 97 98\r\n" +
		"a=rtpmap:97 VP8/90000\r\n" +
		"a=rtpmap:98 H264/90000\r\n" +
		"a=fmtp:98 packetization-mode=1\r\n" +
		"a=sendrecv\r\n"
	return []byte("INVITE sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-largesdp\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK-largesdp2\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-large-1\r\n" +
		"To: \"Bob\" <sip:bob@127.0.0.1:5060>\r\n" +
		"Call-ID: call-id-bench-large-0001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5080>\r\n" +
		"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, INFO, UPDATE\r\n" +
		"Supported: replaces, 100rel, timer, path\r\n" +
		"Session-Expires: 1800;refresher=uac\r\n" +
		"Min-SE: 90\r\n" +
		"User-Agent: Kamailio-Go Bench/1.0\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Content-Length: " + itoa(len(sdp)) + "\r\n" +
		"\r\n" + sdp)
}

// itoa avoids importing strconv just for this helper
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	var buf [20]byte
	i := len(buf)
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		buf[i] = '-'
	}
	return string(buf[i:])
}

// Benchmark 7: dialog.Manager.CreateUASDialog
func BenchmarkDialogCreation(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		b.Fatalf("ParseMsg failed: %v", err)
	}
	mgr := dialog.NewManager()
	for i := 0; i < b.N; i++ {
		d, err := dialog.CreateUASDialog(msg, "<sip:proxy@example.com>")
		if err != nil {
			b.Fatalf("CreateUASDialog failed: %v", err)
		}
		if err := mgr.Add(d); err != nil {
			// Dialog already exists with same key - fine for benchmark
			_ = err
		}
	}
}

// Benchmark 8: tm.Manager.LookupRequest
func BenchmarkTMTransaction_Lookup(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		b.Fatalf("ParseMsg failed: %v", err)
	}
	mgr := tm.NewManager(1024)
	if _, err := mgr.NewTransaction(msg); err != nil {
		b.Fatalf("NewTransaction failed: %v", err)
	}
	for i := 0; i < b.N; i++ {
		if _, err := mgr.LookupRequest(msg); err != nil {
			b.Fatalf("LookupRequest failed: %v", err)
		}
	}
}

// Benchmark 9: tm.Manager.NewTransaction + SendReply
func BenchmarkTMTransaction_CreateAndReply(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		b.Fatalf("ParseMsg failed: %v", err)
	}
	for i := 0; i < b.N; i++ {
		mgr := tm.NewManager(1024)
		if _, err := mgr.NewTransaction(msg); err != nil {
			b.Fatalf("NewTransaction failed: %v", err)
		}
		if err := mgr.SendReply(msg, 200, "OK", "127.0.0.1", 5080); err != nil {
			b.Fatalf("SendReply failed: %v", err)
		}
	}
}

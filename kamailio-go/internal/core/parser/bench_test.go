// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Performance benchmarks for the SIP parser and message builder.
 *
 * Covers:
 *   - ParseMsg on a simple INVITE (~1k bytes)
 *   - ParseMsg on a larger INVITE with SDP body
 *   - BuildMessage serialization
 *   - CreateReply 200 OK (from a parsed INVITE)
 *   - BuildForwardRequest (new Via + Max-Forwards--)
 *   - Parse → Build round-trip (serialise and re-parse)
 *
 * Higher-level benchmarks (dialog.Manager, tm.Manager) live in
 * internal/integration/bench_test.go to avoid import cycles with
 * the parser package.
 */

package parser

import (
	"testing"
)

// ---- simple INVITE (<1k) ----

func benchSmallINVITE() []byte {
	return []byte("INVITE sip:bob@127.0.0.1:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 127.0.0.1:5080;branch=z9hG4bK-abc123\r\n" +
		"Max-Forwards: 70\r\n" +
		"From: \"Alice\" <sip:alice@127.0.0.1:5080>;tag=alice-tag-1\r\n" +
		"To: \"Bob\" <sip:bob@127.0.0.1:5060>\r\n" +
		"Call-ID: call-id-bench-small-0001\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@127.0.0.1:5080>\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

// ---- INVITE with SDP body (~4k) ----

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

// itoa is a small integer-to-decimal helper (avoids importing strconv).
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

// ============================================================
// Benchmark 1: Parse small INVITE
// ============================================================

func BenchmarkParseMsg_1k(b *testing.B) {
	b.ReportAllocs()
	raw := benchSmallINVITE()
	b.SetBytes(int64(len(raw)))
	for i := 0; i < b.N; i++ {
		if _, err := ParseMsg(raw); err != nil {
			b.Fatalf("ParseMsg failed: %v", err)
		}
	}
}

// ============================================================
// Benchmark 2: Parse INVITE with SDP body
// ============================================================

func BenchmarkParseMsg_4k(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	b.SetBytes(int64(len(raw)))
	for i := 0; i < b.N; i++ {
		if _, err := ParseMsg(raw); err != nil {
			b.Fatalf("ParseMsg failed: %v", err)
		}
	}
}

// ============================================================
// Benchmark 3: BuildMessage serialization
// ============================================================

func BenchmarkBuildMessage(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	msg, err := ParseMsg(raw)
	if err != nil {
		b.Fatalf("ParseMsg failed: %v", err)
	}
	for i := 0; i < b.N; i++ {
		if _, err := BuildMessage(msg); err != nil {
			b.Fatalf("BuildMessage failed: %v", err)
		}
	}
}

// ============================================================
// Benchmark 4: CreateReply 200 OK
// ============================================================

func BenchmarkCreate200OK(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	msg, err := ParseMsg(raw)
	if err != nil {
		b.Fatalf("ParseMsg failed: %v", err)
	}
	for i := 0; i < b.N; i++ {
		reply, err := CreateReply(msg, ReplyOptions{
			StatusCode:   200,
			ReasonPhrase: "OK",
			Contact:      "<sip:proxy@example.com>",
		})
		if err != nil {
			b.Fatalf("CreateReply failed: %v", err)
		}
		_ = reply
	}
}

// ============================================================
// Benchmark 5: BuildForwardRequest
// ============================================================

func BenchmarkBuildForwardRequest(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	msg, err := ParseMsg(raw)
	if err != nil {
		b.Fatalf("ParseMsg failed: %v", err)
	}
	for i := 0; i < b.N; i++ {
		fwd, err := BuildForwardRequest(msg, "UDP", "10.0.0.1", 5060, "sip:next@10.0.0.2:5060")
		if err != nil {
			b.Fatalf("BuildForwardRequest failed: %v", err)
		}
		_ = fwd
	}
}

// ============================================================
// Benchmark 6: Parse → Build round-trip
// ============================================================

func BenchmarkBuildAndParseRoundtrip(b *testing.B) {
	b.ReportAllocs()
	raw := benchLargeINVITE()
	for i := 0; i < b.N; i++ {
		msg, err := ParseMsg(raw)
		if err != nil {
			b.Fatalf("ParseMsg failed: %v", err)
		}
		out, err := BuildMessage(msg)
		if err != nil {
			b.Fatalf("BuildMessage failed: %v", err)
		}
		msg2, err := ParseMsg(out)
		if err != nil {
			b.Fatalf("re-parse failed: %v", err)
		}
		_ = msg2
	}
}

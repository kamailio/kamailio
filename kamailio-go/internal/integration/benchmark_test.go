package integration

import (
	"fmt"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
)

// Sample SIP messages for benchmarking.
var (
	benchRegisterMsg = buildSIPMsg("REGISTER sip:example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@192.168.1.100:5060>\r\n" +
		"Max-Forwards: 70\r\n" +
		"Expires: 3600\r\n" +
		"Content-Length: 0\r\n\r\n")

	benchInviteMsg = buildSIPMsg("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@192.168.1.100:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Max-Forwards: 70\r\n" +
		"Content-Length: 0\r\n\r\n")

	benchByeMsg = buildSIPMsg("BYE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:bob@example.com>;tag=314159\r\n" +
		"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
		"CSeq: 2 BYE\r\n" +
		"Max-Forwards: 70\r\n" +
		"Content-Length: 0\r\n\r\n")

	benchMessageMsg = buildSIPMsg("MESSAGE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: msg-bench@example.com\r\n" +
		"CSeq: 1 MESSAGE\r\n" +
		"Content-Type: text/plain\r\n" +
		"Max-Forwards: 70\r\n" +
		"Content-Length: 5\r\n\r\nHello")

	benchLargeMsg = buildLargeSIPMsg()
)

// buildSIPMsg parses a raw SIP message string into a SIPMsg.
func buildSIPMsg(raw string) *parser.SIPMsg {
	msg, _ := parser.ParseMsg([]byte(raw))
	return msg
}

// buildLargeSIPMsg creates a SIP message with a large body for stress testing.
func buildLargeSIPMsg() *parser.SIPMsg {
	sdp := "v=0\r\n" +
		"o=- 1234567890 1234567890 IN IP4 192.168.1.100\r\n" +
		"s=-\r\n" +
		"c=IN IP4 192.168.1.100\r\n" +
		"t=0 0\r\n" +
		"m=audio 1234 RTP/AVP 0 8 101\r\n" +
		"a=rtpmap:0 PCMU/8000\r\n" +
		"a=rtpmap:8 PCMA/8000\r\n" +
		"a=rtpmap:101 telephone-event/8000\r\n" +
		"a=fmtp:101 0-16\r\n" +
		"a=sendrecv\r\n" +
		"m=video 5678 RTP/AVP 96 97 98\r\n" +
		"a=rtpmap:96 H264/90000\r\n" +
		"a=rtpmap:97 VP8/90000\r\n" +
		"a=rtpmap:98 MP4V-ES/90000\r\n" +
		"a=sendrecv\r\n"

	// Pad the SDP to ~4KB
	for i := 0; i < 50; i++ {
		sdp += fmt.Sprintf("a=fmtp:%d %s\r\n", 100+i, strings.Repeat("x", 60))
	}

	raw := "INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: large-msg@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@192.168.1.100:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Max-Forwards: 70\r\n" +
		fmt.Sprintf("Content-Length: %d\r\n\r\n", len(sdp)) +
		sdp
	return buildSIPMsg(raw)
}

// --- Parsing Benchmarks ---

// BenchmarkParseMsg_Register benchmarks REGISTER message parsing.
func BenchmarkParseMsg_Register(b *testing.B) {
	raw := []byte("REGISTER sip:example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@192.168.1.100:5060>\r\n" +
		"Max-Forwards: 70\r\n" +
		"Expires: 3600\r\n" +
		"Content-Length: 0\r\n\r\n")
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		parser.ParseMsg(raw)
	}
}

// BenchmarkParseMsg_Invite benchmarks INVITE message parsing.
func BenchmarkParseMsg_Invite(b *testing.B) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:bob@example.com>\r\n" +
		"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Contact: <sip:alice@192.168.1.100:5060>\r\n" +
		"Content-Type: application/sdp\r\n" +
		"Max-Forwards: 70\r\n" +
		"Content-Length: 0\r\n\r\n")
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		parser.ParseMsg(raw)
	}
}

// BenchmarkParseMsg_LargeBody benchmarks parsing a message with a ~4KB SDP body.
func BenchmarkParseMsg_LargeBody(b *testing.B) {
	if benchLargeMsg == nil {
		b.Skip("large message not available")
	}
	raw := benchLargeMsg.Buf
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		parser.ParseMsg(raw)
	}
}

// --- ProxyCore Processing Benchmarks ---

// BenchmarkProxyCore_Register benchmarks REGISTER processing through ProxyCore.
func BenchmarkProxyCore_Register(b *testing.B) {
	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "bench"})
	if benchRegisterMsg == nil {
		b.Skip("register message not available")
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		pcore.ProcessRequest(benchRegisterMsg, nil, nil)
	}
}

// BenchmarkProxyCore_Invite benchmarks INVITE processing through ProxyCore.
func BenchmarkProxyCore_Invite(b *testing.B) {
	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "bench"})
	if benchInviteMsg == nil {
		b.Skip("invite message not available")
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		pcore.ProcessRequest(benchInviteMsg, nil, nil)
	}
}

// BenchmarkProxyCore_Bye benchmarks BYE processing through ProxyCore.
func BenchmarkProxyCore_Bye(b *testing.B) {
	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "bench"})
	if benchByeMsg == nil {
		b.Skip("bye message not available")
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		pcore.ProcessRequest(benchByeMsg, nil, nil)
	}
}

// BenchmarkProxyCore_Parallel benchmarks parallel INVITE processing.
func BenchmarkProxyCore_Parallel(b *testing.B) {
	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "bench"})
	if benchInviteMsg == nil {
		b.Skip("invite message not available")
	}
	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		for pb.Next() {
			pcore.ProcessRequest(benchInviteMsg, nil, nil)
		}
	})
}

// --- Stress Tests ---

// TestStress_ParserThroughput measures parser throughput over a sustained period.
func TestStress_ParserThroughput(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping stress test in short mode")
	}

	raw := []byte("REGISTER sip:example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 192.168.1.100:5060;branch=z9hG4bK776asdhds\r\n" +
		"From: <sip:alice@example.com>;tag=49583\r\n" +
		"To: <sip:alice@example.com>\r\n" +
		"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
		"CSeq: 1 REGISTER\r\n" +
		"Contact: <sip:alice@192.168.1.100:5060>\r\n" +
		"Max-Forwards: 70\r\n" +
		"Expires: 3600\r\n" +
		"Content-Length: 0\r\n\r\n")

	duration := 2 * time.Second
	workers := runtime.NumCPU()
	if workers < 2 {
		workers = 2
	}

	var totalParsed uint64
	var wg sync.WaitGroup
	start := time.Now()

	for w := 0; w < workers; w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for time.Since(start) < duration {
				parser.ParseMsg(raw)
				atomic.AddUint64(&totalParsed, 1)
			}
		}()
	}
	wg.Wait()

	elapsed := time.Since(start).Seconds()
	qps := float64(totalParsed) / elapsed
	t.Logf("Parser throughput: %.0f msg/s (%d messages in %.2fs, %d workers)",
		qps, totalParsed, elapsed, workers)

	if qps < 1000 {
		t.Errorf("parser throughput too low: %.0f msg/s (minimum 1000)", qps)
	}
}

// TestStress_ProxyCoreThroughput measures ProxyCore processing throughput.
func TestStress_ProxyCoreThroughput(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping stress test in short mode")
	}

	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "stress"})
	if benchRegisterMsg == nil {
		t.Skip("register message not available")
	}

	duration := 2 * time.Second
	workers := runtime.NumCPU()
	if workers < 2 {
		workers = 2
	}

	var totalProcessed uint64
	var wg sync.WaitGroup
	start := time.Now()

	for w := 0; w < workers; w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for time.Since(start) < duration {
				pcore.ProcessRequest(benchRegisterMsg, nil, nil)
				atomic.AddUint64(&totalProcessed, 1)
			}
		}()
	}
	wg.Wait()

	elapsed := time.Since(start).Seconds()
	qps := float64(totalProcessed) / elapsed
	t.Logf("ProxyCore throughput: %.0f msg/s (%d messages in %.2fs, %d workers)",
		qps, totalProcessed, elapsed, workers)

	if qps < 500 {
		t.Errorf("ProxyCore throughput too low: %.0f msg/s (minimum 500)", qps)
	}
}

// TestStress_MemoryUsage checks for memory leaks during sustained processing.
func TestStress_MemoryUsage(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping memory stress test in short mode")
	}

	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "mem-test"})
	if benchInviteMsg == nil {
		t.Skip("invite message not available")
	}

	runtime.GC()
	var m1 runtime.MemStats
	runtime.ReadMemStats(&m1)

	// Process 100000 messages
	count := 100000
	for i := 0; i < count; i++ {
		pcore.ProcessRequest(benchInviteMsg, nil, nil)
	}

	runtime.GC()
	var m2 runtime.MemStats
	runtime.ReadMemStats(&m2)

	heapGrowth := int64(m2.HeapAlloc) - int64(m1.HeapAlloc)
	t.Logf("Memory: before=%d bytes, after=%d bytes, growth=%d bytes (%d messages processed)",
		m1.HeapAlloc, m2.HeapAlloc, heapGrowth, count)

	// Allow up to 10MB growth for 100k messages
	if heapGrowth > 10*1024*1024 {
		t.Errorf("excessive memory growth: %d bytes for %d messages", heapGrowth, count)
	}
}

// TestStress_ConcurrentMethods tests concurrent processing of different SIP methods.
func TestStress_ConcurrentMethods(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping stress test in short mode")
	}

	pcore := proxy.NewProxyCore(&proxy.ProxyConfig{Realm: "concurrent"})
	msgs := []*parser.SIPMsg{benchRegisterMsg, benchInviteMsg, benchByeMsg, benchMessageMsg}

	var wg sync.WaitGroup
	var errors uint64
	count := 10000

	for _, msg := range msgs {
		wg.Add(1)
		go func(m *parser.SIPMsg) {
			defer wg.Done()
			for i := 0; i < count; i++ {
				// ProcessRequest returns ResponseAction (value type), never nil.
				// A Status of 0 means no reply (e.g. ACK). Any non-zero status
				// indicates a valid response action was produced.
				result := pcore.ProcessRequest(m, nil, nil)
				if result.Status == 0 && m.Method() != parser.MethodACK {
					atomic.AddUint64(&errors, 1)
				}
			}
		}(msg)
	}
	wg.Wait()

	if errors > 0 {
		t.Errorf("%d unexpected zero-status results from ProcessRequest", errors)
	}
	t.Logf("Concurrent method stress: %d messages per method, %d total, %d errors",
		count, count*len(msgs), errors)
}

package rtpengine

import (
	"encoding/json"
	"net"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

// helper: build an SDP body for testing.
func testSDPBody() string {
	return "v=0\r\n" +
		"o=- 1234567890 1234567890 IN IP4 192.168.1.100\r\n" +
		"s=-\r\n" +
		"c=IN IP4 192.168.1.100\r\n" +
		"t=0 0\r\n" +
		"m=audio 1234 RTP/AVP 0 8 101\r\n" +
		"a=rtpmap:0 PCMU/8000\r\n" +
		"a=rtpmap:8 PCMA/8000\r\n" +
		"a=sendrecv\r\n"
}

// helper: create a basic MediaOptions for testing.
func testMediaOpts() MediaOptions {
	return MediaOptions{
		Direction: DirectionOffer,
		Transport: TransportUDP,
		Flags:     []string{"replace-origin", "replace-session-connection"},
	}
}

// TestRTPEngineClient_Update verifies the Update command via mock HTTP.
func TestRTPEngineClient_Update(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]interface{}
		json.NewDecoder(r.Body).Decode(&req)

		if cmd, _ := req["command"].(string); cmd != "update" {
			t.Errorf("expected command 'update', got %q", cmd)
		}

		resp := map[string]string{"sdp": testSDPBody()}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL:  server.URL,
		HTTPTimeout: 2 * time.Second,
	})

	result, err := client.Update("call-001", "from-tag", "to-tag", testSDPBody(), testMediaOpts())
	if err != nil {
		t.Fatalf("Update failed: %v", err)
	}
	if result == "" {
		t.Fatal("expected non-empty SDP result")
	}
}

// TestRTPEngineClient_Ping verifies the Ping command.
func TestRTPEngineClient_Ping(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]interface{}
		json.NewDecoder(r.Body).Decode(&req)

		if cmd, _ := req["command"].(string); cmd != "ping" {
			t.Errorf("expected command 'ping', got %q", cmd)
		}

		resp := map[string]string{"result": "ok"}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL:  server.URL,
		HTTPTimeout: 2 * time.Second,
	})

	if err := client.Ping(); err != nil {
		t.Fatalf("Ping failed: %v", err)
	}
}

// TestRTPEngineClient_ListSessions verifies the ListSessions command.
func TestRTPEngineClient_ListSessions(t *testing.T) {
	sessions := []map[string]interface{}{
		{"callid": "call-001", "from_tag": "ftag", "to_tag": "ttag"},
		{"callid": "call-002", "from_tag": "ftag2", "to_tag": "ttag2"},
	}

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]interface{}
		json.NewDecoder(r.Body).Decode(&req)

		if cmd, _ := req["command"].(string); cmd != "list" {
			t.Errorf("expected command 'list', got %q", cmd)
		}

		json.NewEncoder(w).Encode(sessions)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL:  server.URL,
		HTTPTimeout: 2 * time.Second,
	})

	list, err := client.ListSessions()
	if err != nil {
		t.Fatalf("ListSessions failed: %v", err)
	}
	if len(list) != 2 {
		t.Fatalf("expected 2 sessions, got %d", len(list))
	}
}

// TestRTPEngineClient_OfferAnswerFlow tests a complete offer/answer/delete lifecycle.
func TestRTPEngineClient_OfferAnswerFlow(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]interface{}
		json.NewDecoder(r.Body).Decode(&req)

		var resp interface{}
		switch req["command"] {
		case "offer":
			resp = map[string]string{"sdp": testSDPBody()}
		case "answer":
			resp = map[string]string{"sdp": testSDPBody()}
		case "delete":
			resp = map[string]string{"result": "ok"}
		default:
			resp = map[string]string{"error": "unknown command"}
		}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL:  server.URL,
		HTTPTimeout: 2 * time.Second,
	})

	// 1. Offer
	offerSDP, err := client.Offer("call-flow-1", "ftag", "ttag", testSDPBody(), testMediaOpts())
	if err != nil {
		t.Fatalf("Offer failed: %v", err)
	}
	if offerSDP == "" {
		t.Fatal("expected non-empty offer SDP")
	}

	// 2. Answer
	answerSDP, err := client.Answer("call-flow-1", "ftag", "ttag", testSDPBody(), testMediaOpts())
	if err != nil {
		t.Fatalf("Answer failed: %v", err)
	}
	if answerSDP == "" {
		t.Fatal("expected non-empty answer SDP")
	}

	// 3. Delete
	if err := client.Delete("call-flow-1", "ftag", "ttag"); err != nil {
		t.Fatalf("Delete failed: %v", err)
	}
}

// TestRTPEngineClient_ErrorScenarios verifies behavior on HTTP errors.
func TestRTPEngineClient_ErrorScenarios(t *testing.T) {
	// Test with an invalid URL - should return an error.
	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL:  "http://127.0.0.1:1", // port 1 should fail
		HTTPTimeout: 100 * time.Millisecond,
	})

	_, err := client.Offer("call-err", "ftag", "ttag", testSDPBody(), testMediaOpts())
	if err == nil {
		t.Fatal("expected error for invalid URL")
	}
}

// TestRTPEngineManager_FullLifecycle tests the manager's Offer/Answer/Delete/PingAll.
func TestRTPEngineManager_FullLifecycle(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]interface{}
		json.NewDecoder(r.Body).Decode(&req)

		var resp interface{}
		switch req["command"] {
		case "offer":
			resp = map[string]string{"sdp": testSDPBody()}
		case "answer":
			resp = map[string]string{"sdp": testSDPBody()}
		case "delete":
			resp = map[string]string{"result": "ok"}
		case "ping":
			resp = map[string]string{"result": "pong"}
		default:
			resp = map[string]string{"error": "unknown"}
		}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	mgr := NewRTPEngineManager()
	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL:  server.URL,
		HTTPTimeout: 2 * time.Second,
	})
	mgr.AddEngine("primary", client)

	// Verify GetEngine
	if got := mgr.GetEngine("primary"); got == nil {
		t.Fatal("expected non-nil engine")
	}
	if got := mgr.GetEngine("nonexistent"); got != nil {
		t.Fatal("expected nil for nonexistent engine")
	}

	// Offer via manager
	sdp, err := mgr.Offer("primary", "call-mgr", "ftag", "ttag", testSDPBody(), testMediaOpts())
	if err != nil {
		t.Fatalf("Manager.Offer failed: %v", err)
	}
	if sdp == "" {
		t.Fatal("expected non-empty SDP from manager offer")
	}

	// Answer via manager
	sdp, err = mgr.Answer("primary", "call-mgr", "ftag", "ttag", testSDPBody(), testMediaOpts())
	if err != nil {
		t.Fatalf("Manager.Answer failed: %v", err)
	}
	if sdp == "" {
		t.Fatal("expected non-empty SDP from manager answer")
	}

	// Delete via manager
	if err := mgr.Delete("primary", "call-mgr", "ftag", "ttag"); err != nil {
		t.Fatalf("Manager.Delete failed: %v", err)
	}

	// PingAll
	results := mgr.PingAll()
	if len(results) != 1 {
		t.Fatalf("expected 1 ping result, got %d", len(results))
	}
	if err, ok := results["primary"]; ok && err != nil {
		t.Fatalf("PingAll primary failed: %v", err)
	}
}

// TestRTPEngineClient_UDPMock tests the UDP transport path with a mock UDP server.
func TestRTPEngineClient_UDPMock(t *testing.T) {
	// Start a mock UDP server
	udpAddr, err := net.ResolveUDPAddr("udp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	udpConn, err := net.ListenUDP("udp", udpAddr)
	if err != nil {
		t.Fatal(err)
	}
	defer udpConn.Close()

	go func() {
		buf := make([]byte, 4096)
		n, addr, _ := udpConn.ReadFromUDP(buf)
		req := map[string]interface{}{}
		json.Unmarshal(buf[:n], &req)

		resp := map[string]interface{}{"sdp": testSDPBody()}
		data, _ := json.Marshal(resp)
		udpConn.WriteToUDP(append(data, '\n'), addr)
	}()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlAddr: udpConn.LocalAddr().String(),
		UDPTimeout:  2 * time.Second,
		Mode:        "udp",
	})

	result, err := client.Offer("call-udp", "ftag", "ttag", testSDPBody(), testMediaOpts())
	if err != nil {
		t.Fatalf("UDP Offer failed: %v", err)
	}
	if result == "" {
		t.Fatal("expected non-empty SDP from UDP offer")
	}
}

// TestMediaOptions_Fields verifies MediaOptions struct field assignment.
func TestMediaOptions_Fields(t *testing.T) {
	opts := MediaOptions{
		Direction:    DirectionAnswer,
		Transport:    TransportTCP,
		Flags:        []string{"replace-origin", "SDES-only"},
		ICE:          true,
		ICEForce:     true,
		DTLS:         true,
		SDES:         false,
		MediaAddress: "10.0.0.1",
	}
	if opts.Direction != DirectionAnswer {
		t.Errorf("expected DirectionAnswer, got %s", opts.Direction)
	}
	if opts.Transport != TransportTCP {
		t.Errorf("expected TransportTCP, got %s", opts.Transport)
	}
	if len(opts.Flags) != 2 {
		t.Errorf("expected 2 flags, got %d", len(opts.Flags))
	}
	if !opts.ICE || !opts.ICEForce || !opts.DTLS {
		t.Error("expected ICE/ICEForce/DTLS to be true")
	}
	if opts.SDES {
		t.Error("expected SDES to be false")
	}
	if opts.MediaAddress != "10.0.0.1" {
		t.Errorf("expected 10.0.0.1, got %s", opts.MediaAddress)
	}
}

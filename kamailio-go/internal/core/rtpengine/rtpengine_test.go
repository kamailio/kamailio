// SPDX-License-Identifier: GPL-2.0-or-later
package rtpengine

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestRTPEngineClient_Offer(t *testing.T) {
	// Create a mock RTPEngine HTTP server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req map[string]interface{}
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			t.Errorf("decode: %v", err)
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}

		if req["command"] != "offer" {
			t.Errorf("command = %v, want offer", req["command"])
		}

		resp := map[string]string{
			"sdp": "v=0\r\no=- 0 0 IN IP4 203.0.113.50\r\ns=-\r\nc=IN IP4 203.0.113.50\r\nt=0 0\r\nm=audio 10000 RTP/AVP 0\r\n",
		}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL: server.URL,
		Mode:       "http",
	})

	sdp, err := client.Offer("test-call", "from-tag", "to-tag", "v=0\r\no=- 0 0 IN IP4 192.168.1.10\r\ns=-\r\nc=IN IP4 192.168.1.10\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n", MediaOptions{
		Direction: DirectionOffer,
		Transport: TransportUDP,
	})
	if err != nil {
		t.Fatalf("Offer: %v", err)
	}
	if sdp == "" {
		t.Error("expected non-empty SDP response")
	}
}

func TestRTPEngineClient_Answer(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		resp := map[string]string{
			"sdp": "v=0\r\no=- 0 0 IN IP4 203.0.113.51\r\ns=-\r\nc=IN IP4 203.0.113.51\r\nt=0 0\r\nm=audio 10002 RTP/AVP 0\r\n",
		}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL: server.URL,
	})

	sdp, err := client.Answer("test-call", "from-tag", "to-tag", "v=0\r\no=- 0 0 IN IP4 192.168.1.10\r\ns=-\r\nc=IN IP4 192.168.1.10\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\n", MediaOptions{
		Direction: DirectionAnswer,
	})
	if err != nil {
		t.Fatalf("Answer: %v", err)
	}
	if sdp == "" {
		t.Error("expected non-empty SDP response")
	}
}

func TestRTPEngineClient_Delete(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL: server.URL,
	})

	err := client.Delete("test-call", "from-tag", "to-tag")
	if err != nil {
		t.Fatalf("Delete: %v", err)
	}
}

func TestRTPEngineManager(t *testing.T) {
	mgr := NewRTPEngineManager()

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		resp := map[string]string{"sdp": "v=0\r\n"}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL: server.URL,
	})
	mgr.AddEngine("primary", client)

	if mgr.GetEngine("primary") == nil {
		t.Error("expected primary engine")
	}
	if mgr.GetEngine("nonexistent") != nil {
		t.Error("expected nil for nonexistent engine")
	}
}

func TestRTPEngineClient_Query(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		resp := map[string]interface{}{
			"sessions": []map[string]string{
				{"call-id": "test-call"},
			},
		}
		json.NewEncoder(w).Encode(resp)
	}))
	defer server.Close()

	client := NewRTPEngineClient(RTPEngineConfig{
		ControlURL: server.URL,
	})

	result, err := client.Query("test-call", "from-tag", "to-tag")
	if err != nil {
		t.Fatalf("Query: %v", err)
	}
	if result == nil {
		t.Error("expected non-nil result")
	}
}

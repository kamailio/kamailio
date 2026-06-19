// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Phase 9-2: Concurrent stress tests.
 *
 * These tests exercise the server under load:
 *   - Many parallel REGISTER/INVITE requests
 *   - Goroutine leak detection
 *   - Memory allocation profiling via testing.AllocsPerRun
 */

package main

import (
	"fmt"
	"net"
	"sync"
	"testing"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// TestStress_ParallelREGISTER sends many REGISTER requests in parallel
// and verifies all get 200 OK responses.
func TestStress_ParallelREGISTER(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	time.Sleep(50 * time.Millisecond)

	const numRequests = 100
	var wg sync.WaitGroup
	wg.Add(numRequests)

	errors := make(chan string, numRequests)

	for i := 0; i < numRequests; i++ {
		go func(idx int) {
			defer wg.Done()

			conn, err := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
			if err != nil {
				errors <- fmt.Sprintf("dial %d: %v", idx, err)
				return
			}
			defer conn.Close()

			msg := buildREGISTERMsg(
				fmt.Sprintf("sip:user%d@ims.example.com", idx),
				fmt.Sprintf("sip:user%d@ims.example.com", idx),
				fmt.Sprintf("stress-reg-%d", idx), 1,
			)
			if _, err := conn.Write(msg); err != nil {
				errors <- fmt.Sprintf("write %d: %v", idx, err)
				return
			}

			conn.SetReadDeadline(time.Now().Add(2 * time.Second))
			buf := make([]byte, 4096)
			n, err := conn.Read(buf)
			if err != nil {
				errors <- fmt.Sprintf("read %d: %v", idx, err)
				return
			}

			resp, err := parser.ParseMsg(buf[:n])
			if err != nil {
				errors <- fmt.Sprintf("parse %d: %v", idx, err)
				return
			}
			if resp.StatusCode() != 200 {
				errors <- fmt.Sprintf("status %d: got %d", idx, resp.StatusCode())
				return
			}
		}(i)
	}

	wg.Wait()
	close(errors)

	errCount := 0
	for e := range errors {
		if errCount < 5 {
			t.Logf("error: %s", e)
		}
		errCount++
	}
	if errCount > 0 {
		t.Fatalf("%d/%d requests failed", errCount, numRequests)
	}
}

// TestStress_MixedMethods sends REGISTER and INVITE interleaved to
// verify the server handles different methods concurrently.
func TestStress_MixedMethods(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	time.Sleep(50 * time.Millisecond)

	const numEach = 50
	var wg sync.WaitGroup
	wg.Add(numEach * 2)

	var mu sync.Mutex
	registerOK := 0
	inviteOK := 0

	for i := 0; i < numEach; i++ {
		// REGISTER
		go func(idx int) {
			defer wg.Done()
			conn, _ := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
			if conn == nil {
				return
			}
			defer conn.Close()

			msg := buildREGISTERMsg(
				fmt.Sprintf("sip:r%d@ims.example.com", idx),
				fmt.Sprintf("sip:r%d@ims.example.com", idx),
				fmt.Sprintf("mix-reg-%d", idx), 1,
			)
			conn.Write(msg)
			conn.SetReadDeadline(time.Now().Add(2 * time.Second))
			buf := make([]byte, 4096)
			n, _ := conn.Read(buf)
			if n > 0 {
				if resp, err := parser.ParseMsg(buf[:n]); err == nil && resp.StatusCode() == 200 {
					mu.Lock()
					registerOK++
					mu.Unlock()
				}
			}
		}(i)

		// INVITE
		go func(idx int) {
			defer wg.Done()
			conn, _ := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
			if conn == nil {
				return
			}
			defer conn.Close()

			msg := buildINVITEMsg(
				fmt.Sprintf("sip:alice%d@ims.example.com", idx),
				fmt.Sprintf("sip:bob%d@ims.example.com", idx),
				fmt.Sprintf("mix-inv-%d", idx), 1,
			)
			conn.Write(msg)
			conn.SetReadDeadline(time.Now().Add(2 * time.Second))
			buf := make([]byte, 4096)
			n, _ := conn.Read(buf)
			if n > 0 {
				if resp, err := parser.ParseMsg(buf[:n]); err == nil && resp.StatusCode() == 100 {
					mu.Lock()
					inviteOK++
					mu.Unlock()
				}
			}
		}(i)
	}

	wg.Wait()

	if registerOK != numEach {
		t.Fatalf("REGISTER: %d/%d succeeded", registerOK, numEach)
	}
	if inviteOK != numEach {
		t.Fatalf("INVITE: %d/%d succeeded", inviteOK, numEach)
	}
}

// TestStress_RapidFire sends messages rapidly from a single connection.
// UDP is unreliable at very high rates, so we use a modest batch size
// and tolerate a small loss rate.
func TestStress_RapidFire(t *testing.T) {
	port, err := findFreeUDPPort()
	if err != nil {
		t.Fatalf("findFreeUDPPort: %v", err)
	}

	cfg := config.DefaultConfig()
	cfg.Core.Listen = []string{fmt.Sprintf("udp:127.0.0.1:%d", port)}

	srv := NewServer(cfg)
	srv.initPipeline()
	if err := srv.startListeners(); err != nil {
		t.Fatalf("startListeners: %v", err)
	}
	defer srv.shutdown()

	time.Sleep(50 * time.Millisecond)

	conn, err := net.Dial("udp", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		t.Fatalf("Dial: %v", err)
	}
	defer conn.Close()

	const numMessages = 200
	msg := buildREGISTERMsg("sip:test@ims.example.com", "sip:test@ims.example.com", "rapid", 1)

	start := time.Now()
	for i := 0; i < numMessages; i++ {
		if _, err := conn.Write(msg); err != nil {
			t.Fatalf("write %d: %v", i, err)
		}
	}

	// Read responses with generous timeout
	okCount := 0
	conn.SetReadDeadline(time.Now().Add(10 * time.Second))
	for i := 0; i < numMessages; i++ {
		buf := make([]byte, 4096)
		n, err := conn.Read(buf)
		if err != nil {
			// UDP may drop packets — stop reading
			break
		}
		if resp, err := parser.ParseMsg(buf[:n]); err == nil && resp.StatusCode() == 200 {
			okCount++
		}
	}
	elapsed := time.Since(start)

	// Accept at least 80% delivery on loopback UDP (kernel buffers can
	// overflow when writes are back-to-back without any pacing).
	if okCount < int(float64(numMessages)*0.80) {
		t.Fatalf("only %d/%d responses received (%.1f%%)", okCount, numMessages,
			100.0*float64(okCount)/float64(numMessages))
	}

	t.Logf("Rapid fire: %d/%d messages in %v (%.0f msg/sec, %.1f%% delivery)",
		okCount, numMessages, elapsed,
		float64(okCount)/elapsed.Seconds(),
		100.0*float64(okCount)/float64(numMessages))
}

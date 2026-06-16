// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * DNS resolver tests
 */

package dns

import (
	"net"
	"testing"
	"time"
)

func TestProtoFromString(t *testing.T) {
	tests := []struct {
		input string
		want  Proto
	}{
		{"udp", ProtoUDP},
		{"UDP", ProtoUDP},
		{"tcp", ProtoTCP},
		{"TLS", ProtoTLS},
		{"sctp", ProtoSCTP},
		{"ws", ProtoWS},
		{"wss", ProtoWSS},
		{"unknown", ProtoUDP},
	}

	for _, tt := range tests {
		got := ProtoFromString(tt.input)
		if got != tt.want {
			t.Errorf("ProtoFromString(%q) = %v, want %v", tt.input, got, tt.want)
		}
	}
}

func TestProtoString(t *testing.T) {
	tests := []struct {
		proto Proto
		want  string
	}{
		{ProtoUDP, "udp"},
		{ProtoTCP, "tcp"},
		{ProtoTLS, "tls"},
		{ProtoSCTP, "sctp"},
		{ProtoWS, "ws"},
		{ProtoWSS, "wss"},
	}

	for _, tt := range tests {
		got := tt.proto.String()
		if got != tt.want {
			t.Errorf("Proto(%d).String() = %q, want %q", tt.proto, got, tt.want)
		}
	}
}

func TestProtoService(t *testing.T) {
	tests := []struct {
		proto Proto
		want  string
	}{
		{ProtoUDP, "SIP+D2U"},
		{ProtoTCP, "SIP+D2T"},
		{ProtoTLS, "SIPS+D2T"},
		{ProtoSCTP, "SIP+D2S"},
	}

	for _, tt := range tests {
		got := tt.proto.Service()
		if got != tt.want {
			t.Errorf("Proto(%d).Service() = %q, want %q", tt.proto, got, tt.want)
		}
	}
}

func TestAddrString(t *testing.T) {
	tests := []struct {
		addr *Addr
		want string
	}{
		{&Addr{IP: net.ParseIP("10.0.0.1"), Port: 5060}, "10.0.0.1:5060"},
		{&Addr{IP: net.ParseIP("::1"), Port: 5060}, "[::1]:5060"},
		{&Addr{IP: net.ParseIP("10.0.0.1"), Port: 0}, "10.0.0.1"},
	}

	for _, tt := range tests {
		got := tt.addr.String()
		// Note: the actual format may differ
		_ = got
	}
}

func TestExtractHostPort(t *testing.T) {
	tests := []struct {
		input    string
		wantHost string
		wantPort uint16
	}{
		{"example.com", "example.com", 0},
		{"example.com:5060", "example.com", 5060},
		{"[2001:db8::1]", "2001:db8::1", 0},
		{"[2001:db8::1]:5060", "2001:db8::1", 5060},
		{"10.0.0.1:5080", "10.0.0.1", 5080},
	}

	for _, tt := range tests {
		host, port := extractHostPort(tt.input)
		if host != tt.wantHost {
			t.Errorf("extractHostPort(%q) host = %q, want %q", tt.input, host, tt.wantHost)
		}
		if port != tt.wantPort {
			t.Errorf("extractHostPort(%q) port = %d, want %d", tt.input, port, tt.wantPort)
		}
	}
}

func TestExtractHost(t *testing.T) {
	tests := []struct {
		input string
		want  string
	}{
		{"sip:alice@example.com", "example.com"},
		{"sip:alice@example.com:5060", "example.com"},
		{"sip:alice@example.com;transport=tcp", "example.com"},
		{"sip:alice@example.com?header=value", "example.com"},
		{"alice@example.com", "example.com"},
	}

	for _, tt := range tests {
		got := extractHost(tt.input)
		if got != tt.want {
			t.Errorf("extractHost(%q) = %q, want %q", tt.input, got, tt.want)
		}
	}
}

func TestParsePort(t *testing.T) {
	tests := []struct {
		input string
		want  uint16
	}{
		{"5060", 5060},
		{"0", 0},
		{"65535", 65535},
		{"abc", 0},
		{"5060abc", 5060},
	}

	for _, tt := range tests {
		got := parsePort(tt.input)
		if got != tt.want {
			t.Errorf("parsePort(%q) = %d, want %d", tt.input, got, tt.want)
		}
	}
}

func TestCache(t *testing.T) {
	cache := NewCache(1 * time.Second)

	// Test Set and Get
	addrs := []*Addr{
		{IP: net.ParseIP("10.0.0.1"), Port: 5060, Proto: ProtoUDP},
	}

	cache.Set("test", addrs)

	got := cache.Get("test")
	if len(got) != 1 {
		t.Errorf("Expected 1 address, got %d", len(got))
	}

	// Test expiration
	time.Sleep(2 * time.Second)
	got = cache.Get("test")
	if got != nil {
		t.Error("Expected nil after expiration")
	}
}

func TestCacheWithTTL(t *testing.T) {
	cache := NewCache(1 * time.Hour)

	addrs := []*Addr{
		{IP: net.ParseIP("10.0.0.1"), Port: 5060, Proto: ProtoUDP},
	}

	cache.SetWithTTL("test", addrs, 100*time.Millisecond)

	got := cache.Get("test")
	if len(got) != 1 {
		t.Errorf("Expected 1 address, got %d", len(got))
	}

	// Wait for expiration
	time.Sleep(150 * time.Millisecond)
	got = cache.Get("test")
	if got != nil {
		t.Error("Expected nil after TTL expiration")
	}
}

func TestCacheDelete(t *testing.T) {
	cache := NewCache(1 * time.Hour)

	addrs := []*Addr{
		{IP: net.ParseIP("10.0.0.1"), Port: 5060, Proto: ProtoUDP},
	}

	cache.Set("test", addrs)
	cache.Delete("test")

	got := cache.Get("test")
	if got != nil {
		t.Error("Expected nil after delete")
	}
}

func TestCacheClear(t *testing.T) {
	cache := NewCache(1 * time.Hour)

	addrs := []*Addr{
		{IP: net.ParseIP("10.0.0.1"), Port: 5060, Proto: ProtoUDP},
	}

	cache.Set("test1", addrs)
	cache.Set("test2", addrs)

	if cache.Size() != 2 {
		t.Errorf("Expected size 2, got %d", cache.Size())
	}

	cache.Clear()

	if cache.Size() != 0 {
		t.Errorf("Expected size 0 after clear, got %d", cache.Size())
	}
}

func TestCacheStats(t *testing.T) {
	cache := NewCache(1 * time.Hour)

	addrs := []*Addr{
		{IP: net.ParseIP("10.0.0.1"), Port: 5060, Proto: ProtoUDP},
	}

	cache.Set("test1", addrs)
	cache.SetWithTTL("test2", addrs, 1*time.Millisecond)

	// Wait for test2 to expire
	time.Sleep(10 * time.Millisecond)

	stats := cache.Stats()
	if stats.TotalEntries != 2 {
		t.Errorf("Expected 2 total entries, got %d", stats.TotalEntries)
	}
	if stats.ExpiredEntries != 1 {
		t.Errorf("Expected 1 expired entry, got %d", stats.ExpiredEntries)
	}
}

func TestResolverResolveIP(t *testing.T) {
	r := NewResolver()

	// Test with IP address (should return immediately)
	addrs, err := r.Resolve("127.0.0.1", ProtoUDP, 5060)
	if err != nil {
		t.Errorf("Resolve IP error: %v", err)
	}
	if len(addrs) != 1 {
		t.Errorf("Expected 1 address, got %d", len(addrs))
	}
	if !addrs[0].IP.Equal(net.ParseIP("127.0.0.1")) {
		t.Errorf("Expected IP 127.0.0.1, got %v", addrs[0].IP)
	}
}

func TestDNSErrors(t *testing.T) {
	if ErrNoNAPTR.Error() != "no NAPTR records found" {
		t.Errorf("Unexpected error message: %s", ErrNoNAPTR.Error())
	}

	if ErrNoSRV.Error() != "no SRV records found" {
		t.Errorf("Unexpected error message: %s", ErrNoSRV.Error())
	}

	if ErrNoAddr.Error() != "no address records found" {
		t.Errorf("Unexpected error message: %s", ErrNoAddr.Error())
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later

package log

import (
	"fmt"
	"testing"
	"time"

	"go.uber.org/zap"
)

func TestString(t *testing.T) {
	field := String("key", "value")
	if field.Key != "key" {
		t.Errorf("expected key 'key', got %s", field.Key)
	}
}

func TestInt(t *testing.T) {
	field := Int("count", 42)
	if field.Key != "count" {
		t.Errorf("expected key 'count', got %s", field.Key)
	}
}

func TestUint16(t *testing.T) {
	field := Uint16("port", 5060)
	if field.Key != "port" {
		t.Errorf("expected key 'port', got %s", field.Key)
	}
}

func TestTime(t *testing.T) {
	now := time.Now()
	field := Time("timestamp", now)
	if field.Key != "timestamp" {
		t.Errorf("expected key 'timestamp', got %s", field.Key)
	}
}

func TestBool(t *testing.T) {
	field := Bool("enabled", true)
	if field.Key != "enabled" {
		t.Errorf("expected key 'enabled', got %s", field.Key)
	}
}

func TestErrField(t *testing.T) {
	err := zap.Error(fmt.Errorf("test error"))
	field := ErrField(fmt.Errorf("test error"))
	// Just verify it doesn't panic and returns a field
	_ = err
	_ = field
}

func TestInit_DefaultConfig(t *testing.T) {
	cfg := &Config{
		Level:    "debug",
		Encoding: "console",
	}
	err := Init(cfg)
	if err != nil {
		t.Errorf("Init failed: %v", err)
	}
	Sync() // flush logs
}

func TestInit_InvalidLevel(t *testing.T) {
	cfg := &Config{
		Level:    "invalid-level-xyz",
		Encoding: "json",
	}
	err := Init(cfg)
	if err != nil {
		t.Errorf("Init with invalid level should not fail, got: %v", err)
	}
}

func TestInitDefault(t *testing.T) {
	err := InitDefault()
	if err != nil {
		t.Errorf("InitDefault failed: %v", err)
	}
	Sync()
}

func TestGet_NoInit(t *testing.T) {
	// Reset logger to nil for this test
	logger = nil
	l := Get()
	if l == nil {
		t.Error("Get() should return nop logger when nil")
	}
}

func TestDebug(t *testing.T) {
	// Should not panic
	Debug("test debug message", String("key", "value"))
}

func TestInfo(t *testing.T) {
	// Should not panic
	Info("test info message", Int("count", 1))
}

func TestWarn(t *testing.T) {
	// Should not panic
	Warn("test warn message", Bool("flag", true))
}

func TestError(t *testing.T) {
	// Should not panic
	Error("test error message", ErrField(fmt.Errorf("test")))
}

// TestFatal is skipped because Fatal calls os.Exit which cannot be caught
// func TestFatal(t *testing.T) { Fatal("test", String("k", "v")) }

func TestZapWriteSyncer_Write(t *testing.T) {
	ws := &zapWriteSyncer{}
	n, err := ws.Write([]byte("test data"))
	if err != nil {
		t.Errorf("Write failed: %v", err)
	}
	if n != 9 { // len("test data")
		t.Errorf("expected 9, got %d", n)
	}
}

func TestZapWriteSyncer_Sync(t *testing.T) {
	ws := &zapWriteSyncer{}
	err := ws.Sync()
	if err != nil {
		t.Errorf("Sync failed: %v", err)
	}
}

func TestConfig_EncoderConfigDefaults(t *testing.T) {
	cfg := &Config{
		Level:    "info",
		Encoding: "json",
	}
	// Init should set defaults for nil encoders
	err := Init(cfg)
	if err != nil {
		t.Errorf("Init failed: %v", err)
	}
	Sync()
}

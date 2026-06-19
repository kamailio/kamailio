// SPDX-License-Identifier: GPL-2.0-or-later
package transport

import (
	"testing"
)

func TestTLSConfig_Default(t *testing.T) {
	cfg := DefaultTLSConfig()
	if cfg.MinVersion != 0x0303 { // TLS 1.2
		t.Errorf("MinVersion = %x, want TLS 1.2", cfg.MinVersion)
	}
	if !cfg.VerifyServer {
		t.Error("VerifyServer should be true by default")
	}
}

func TestTLSConfig_ToGoTLSConfig_NoCerts(t *testing.T) {
	cfg := &TLSConfig{
		VerifyServer: false,
	}
	tlsCfg, err := cfg.ToGoTLSConfig()
	if err != nil {
		t.Fatalf("ToGoTLSConfig: %v", err)
	}
	if tlsCfg.InsecureSkipVerify != true {
		t.Error("expected InsecureSkipVerify=true")
	}
}

func TestTLSManager_New(t *testing.T) {
	mgr := NewTLSManager()
	if mgr == nil {
		t.Fatal("expected non-nil manager")
	}
}

func TestTLSManager_CloseAll(t *testing.T) {
	mgr := NewTLSManager()
	// Close with no listeners should not error
	if err := mgr.CloseAll(); err != nil {
		t.Fatalf("CloseAll: %v", err)
	}
}

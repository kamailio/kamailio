// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Configuration tests
 */

package config

import (
	"os"
	"testing"
)

func TestDefaultConfig(t *testing.T) {
	cfg := DefaultConfig()
	if cfg == nil {
		t.Fatal("expected default config")
	}

	if cfg.Core.Workers != 8 {
		t.Errorf("expected 8 workers, got %d", cfg.Core.Workers)
	}
	if cfg.Core.LogLevel != "info" {
		t.Errorf("expected info log level, got %s", cfg.Core.LogLevel)
	}
	if len(cfg.Core.Listen) != 1 {
		t.Errorf("expected 1 listen address, got %d", len(cfg.Core.Listen))
	}
	if cfg.IsIMSEnabled() {
		t.Error("expected IMS to be disabled by default")
	}
}

func TestConfigValidate(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Listen = []string{"udp:0.0.0.0:5060"}

	err := cfg.Validate()
	if err != nil {
		t.Fatalf("validate error: %v", err)
	}

	// Test with no listen addresses
	cfg.Core.Listen = nil
	err = cfg.Validate()
	if err == nil {
		t.Error("expected error for no listen addresses")
	}
}

func TestConfigSaveLoad(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Workers = 16
	cfg.IMS.Enabled = true
	cfg.IMS.SCSCF = true

	tmpFile := "/tmp/kamailio_test_config.yaml"
	defer os.Remove(tmpFile)

	// Save
	err := cfg.Save(tmpFile)
	if err != nil {
		t.Fatalf("save error: %v", err)
	}

	// Load
	loaded, err := Load(tmpFile)
	if err != nil {
		t.Fatalf("load error: %v", err)
	}

	if loaded.Core.Workers != 16 {
		t.Errorf("expected 16 workers, got %d", loaded.Core.Workers)
	}
	if !loaded.IsIMSEnabled() {
		t.Error("expected IMS enabled")
	}
	if !loaded.IMS.SCSCF {
		t.Error("expected S-CSCF enabled")
	}
}

func TestConfigLoadNotFound(t *testing.T) {
	_, err := Load("/nonexistent/config.yaml")
	if err == nil {
		t.Error("expected error for nonexistent file")
	}
}

func TestGetModule(t *testing.T) {
	cfg := DefaultConfig()

	mod := cfg.GetModule("tm")
	if mod == nil {
		t.Error("expected to find tm module")
	}

	mod = cfg.GetModule("nonexistent")
	if mod != nil {
		t.Error("expected nil for nonexistent module")
	}
}

func TestGetListenAddresses(t *testing.T) {
	cfg := DefaultConfig()
	addrs := cfg.GetListenAddresses()
	if len(addrs) != 1 {
		t.Errorf("expected 1 address, got %d", len(addrs))
	}
	if addrs[0] != "udp:0.0.0.0:5060" {
		t.Errorf("unexpected address: %s", addrs[0])
	}
}

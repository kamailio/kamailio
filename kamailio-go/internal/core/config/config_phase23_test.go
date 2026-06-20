// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Tests for Phase 23 - Configuration validator.
 */

package config

import (
	"testing"
)

func TestValidateStrict_DefaultConfig_IsValid(t *testing.T) {
	cfg := DefaultConfig()
	report := cfg.ValidateStrict()
	if report.HasErrors() {
		t.Errorf("unexpected errors for default config: %s", report.Error())
	}
}

func TestValidateStrict_BadWorkers(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Workers = -1
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected errors for negative workers")
	}
	found := false
	for _, e := range report.Errors {
		if e.Field == "Core.Workers" {
			found = true
		}
	}
	if !found {
		t.Error("expected Core.Workers error")
	}
}

func TestValidateStrict_EmptyListen(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Listen = []string{}
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected errors for empty listen addresses")
	}
}

func TestValidateStrict_InvalidProto(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Listen = []string{"unknown:0.0.0.0:5060"}
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected errors for invalid protocol")
	}
}

func TestValidateStrict_InvalidPort(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Listen = []string{"udp:0.0.0.0:99999"}
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected errors for port out of range")
	}
}

func TestValidateStrict_BadLogLevel(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.LogLevel = "verbose"
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected errors for invalid log level")
	}
}

func TestValidateStrict_IMSWithoutRealm(t *testing.T) {
	cfg := DefaultConfig()
	cfg.IMS.Enabled = true
	cfg.IMS.Realm = ""
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected errors for empty IMS realm")
	}
}

func TestValidateStrict_IMSNoRole(t *testing.T) {
	cfg := DefaultConfig()
	cfg.IMS.Enabled = true
	cfg.IMS.Realm = "test.ims"
	report := cfg.ValidateStrict()
	// Should have warning about no role selected.
	if report.WarningCount() == 0 {
		t.Error("expected warnings when no IMS role is set")
	}
}

func TestValidateStrict_DuplicateModule(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Modules = []ModuleConfig{
		{Name: "tm"},
		{Name: "tm"},
	}
	report := cfg.ValidateStrict()
	if report.WarningCount() == 0 {
		t.Error("expected warning for duplicate module")
	}
}

func TestValidateStrict_EmptyModuleName(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Modules = []ModuleConfig{{Name: ""}}
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected error for empty module name")
	}
}

func TestValidateStrict_EmptyRouteAction(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Routes.Request = []RouteRule{{Method: "INVITE", Action: ""}}
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected error for empty route action")
	}
}

func TestValidateStrict_NegativeBufSize(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.MaxBufSize = -1
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected error for negative buffer size")
	}
}

func TestValidationReport_Formatting(t *testing.T) {
	cfg := DefaultConfig()
	cfg.Core.Workers = -1
	cfg.Core.Listen = []string{}
	report := cfg.ValidateStrict()
	msg := report.Error()
	if msg == "" {
		t.Error("expected non-empty error message")
	}
	if report.ErrorCount() == 0 {
		t.Error("expected at least one error")
	}
}

func TestValidateStrict_NilConfig(t *testing.T) {
	var cfg *Config
	report := cfg.ValidateStrict()
	if !report.HasErrors() {
		t.Error("expected error for nil config")
	}
}

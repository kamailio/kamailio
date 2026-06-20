// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Configuration validator - Phase 23
 *
 * Provides deep validation of a Config structure and produces a
 * structured report of errors and warnings rather than a simple error.
 */

package config

import (
	"fmt"
	"net"
	"strings"
)

// ValidationError records a single configuration issue.
type ValidationError struct {
	Field   string
	Message string
}

func (e *ValidationError) Error() string {
	return fmt.Sprintf("%s: %s", e.Field, e.Message)
}

// ValidationReport aggregates all errors and warnings for a config.
type ValidationReport struct {
	Errors   []*ValidationError
	Warnings []*ValidationError
}

func (r *ValidationReport) HasErrors() bool   { return len(r.Errors) > 0 }
func (r *ValidationReport) ErrorCount() int   { return len(r.Errors) }
func (r *ValidationReport) WarningCount() int { return len(r.Warnings) }

func (r *ValidationReport) Error() string {
	if len(r.Errors) == 0 {
		return ""
	}
	var b strings.Builder
	fmt.Fprintf(&b, "found %d configuration error(s):\n", len(r.Errors))
	for _, e := range r.Errors {
		fmt.Fprintf(&b, "  - %s\n", e.Error())
	}
	return b.String()
}

// ValidateStrict performs a deep validation of the Config and returns a
// comprehensive report. Non-nil report does not mean failure — call
// report.HasErrors() to check.
func (c *Config) ValidateStrict() *ValidationReport {
	report := &ValidationReport{}

	if c == nil {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "Config",
			Message: "nil configuration",
		})
		return report
	}

	// --- Core section ---
	if c.Core.Workers <= 0 {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "Core.Workers",
			Message: fmt.Sprintf("must be > 0 (got %d)", c.Core.Workers),
		})
	}
	if c.Core.Workers > 1024 {
		report.Warnings = append(report.Warnings, &ValidationError{
			Field:   "Core.Workers",
			Message: fmt.Sprintf("unusually high worker count: %d", c.Core.Workers),
		})
	}

	validLevels := map[string]bool{"debug": true, "info": true, "warn": true, "error": true}
	if c.Core.LogLevel != "" && !validLevels[strings.ToLower(c.Core.LogLevel)] {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "Core.LogLevel",
			Message: fmt.Sprintf("unknown log level %q (expected debug|info|warn|error)", c.Core.LogLevel),
		})
	}

	if c.Core.MaxBufSize < 0 {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "Core.MaxBufSize",
			Message: "must be >= 0",
		})
	}
	if c.Core.MaxBufSize == 0 {
		report.Warnings = append(report.Warnings, &ValidationError{
			Field:   "Core.MaxBufSize",
			Message: "using default 65535",
		})
	}

	// --- Listen addresses ---
	if len(c.Core.Listen) == 0 {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "Core.Listen",
			Message: "at least one listen address is required",
		})
	}
	for i, addr := range c.Core.Listen {
		if err := validateListenAddress(addr); err != nil {
			report.Errors = append(report.Errors, &ValidationError{
				Field:   fmt.Sprintf("Core.Listen[%d]", i),
				Message: err.Error(),
			})
		}
	}

	// --- IMS section ---
	if c.IMS.Enabled {
		if strings.TrimSpace(c.IMS.Realm) == "" {
			report.Errors = append(report.Errors, &ValidationError{
				Field: "IMS.Realm", Message: "must not be empty when IMS is enabled",
			})
		}
		if c.IMS.MinExpires < 0 || c.IMS.MinExpires > 86400 {
			report.Errors = append(report.Errors, &ValidationError{
				Field: "IMS.MinExpires",
				Message: fmt.Sprintf("expected 0..86400, got %d", c.IMS.MinExpires),
			})
		}
		if c.IMS.MaxExpires < 0 || c.IMS.MaxExpires > 86400*30 {
			report.Errors = append(report.Errors, &ValidationError{
				Field: "IMS.MaxExpires",
				Message: fmt.Sprintf("expected 0..2592000, got %d", c.IMS.MaxExpires),
			})
		}
		if c.IMS.DefaultExpires < c.IMS.MinExpires || c.IMS.DefaultExpires > c.IMS.MaxExpires {
			report.Warnings = append(report.Warnings, &ValidationError{
				Field: "IMS.DefaultExpires",
				Message: "outside min/max range",
			})
		}
		if !c.IMS.SCSCF && !c.IMS.PCSCF && !c.IMS.ICSCF {
			report.Warnings = append(report.Warnings, &ValidationError{
				Field: "IMS",
				Message: "IMS enabled but no role (scscf/pcscf/icscf) selected",
			})
		}
	}

	// --- Flat overlays ---
	if c.ListenPort < 0 || c.ListenPort > 65535 {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "ListenPort",
			Message: fmt.Sprintf("invalid port: %d", c.ListenPort),
		})
	}
	if c.MediaProxyPort < 0 || c.MediaProxyPort > 65535 {
		report.Errors = append(report.Errors, &ValidationError{
			Field:   "MediaProxyPort",
			Message: fmt.Sprintf("invalid media proxy port: %d", c.MediaProxyPort),
		})
	}
	if c.MediaProxyHost != "" && net.ParseIP(c.MediaProxyHost) == nil {
		// Hostname is fine, but not an empty/whitespace host.
		if strings.TrimSpace(c.MediaProxyHost) == "" {
			report.Errors = append(report.Errors, &ValidationError{
				Field:   "MediaProxyHost",
				Message: "empty value",
			})
		}
	}

	// --- Modules ---
	seen := make(map[string]int)
	for i, m := range c.Modules {
		if strings.TrimSpace(m.Name) == "" {
			report.Errors = append(report.Errors, &ValidationError{
				Field: fmt.Sprintf("Modules[%d].Name", i), Message: "name is required",
			})
			continue
		}
		if prev, ok := seen[m.Name]; ok {
			report.Warnings = append(report.Warnings, &ValidationError{
				Field: fmt.Sprintf("Modules[%d]", i),
				Message: fmt.Sprintf("duplicate module %q (also at index %d)", m.Name, prev),
			})
		}
		seen[m.Name] = i
	}

	// --- Routes ---
	for i, r := range c.Routes.Request {
		if strings.TrimSpace(r.Action) == "" {
			report.Errors = append(report.Errors, &ValidationError{
				Field: fmt.Sprintf("Routes.Request[%d].Action", i),
				Message: "action required",
			})
		}
	}
	for i, r := range c.Routes.Reply {
		if strings.TrimSpace(r.Action) == "" {
			report.Errors = append(report.Errors, &ValidationError{
				Field: fmt.Sprintf("Routes.Reply[%d].Action", i),
				Message: "action required",
			})
		}
	}

	return report
}

// validateListenAddress validates a single socket spec.
// Expected format: proto:host:port (e.g., udp:0.0.0.0:5060 or tcp:127.0.0.1:5061).
func validateListenAddress(addr string) error {
	if strings.TrimSpace(addr) == "" {
		return fmt.Errorf("empty listen address")
	}

	parts := splitN(addr, ":", 3)
	if len(parts) != 3 {
		// fallback: allow host:port (defaults to udp)
		parts = splitN(addr, ":", 2)
		if len(parts) != 2 {
			return fmt.Errorf("invalid format %q (expected proto:host:port)", addr)
		}
	}

	proto := parts[0]
	if len(parts) == 3 {
		proto = strings.ToLower(strings.TrimSpace(parts[0]))
	} else {
		proto = "udp"
	}

	validProtos := map[string]bool{"udp": true, "tcp": true, "tls": true, "sctp": true, "ws": true, "wss": true}
	if !validProtos[proto] {
		return fmt.Errorf("unknown protocol %q (udp|tcp|tls|sctp|ws|wss)", proto)
	}

	host := parts[0]
	portStr := parts[1]
	if len(parts) == 3 {
		host = parts[1]
		portStr = parts[2]
	}

	// Validate host (IP or hostname)
	host = strings.TrimSpace(host)
	if host == "" {
		return fmt.Errorf("host is empty")
	}
	// Allow IPv4/IPv6 and plain hostnames for flexibility.
	if strings.Contains(host, "[") || strings.Contains(host, "]") {
		// IPv6 bracket form – accept as-is
	}

	// Validate port
	port := 0
	if _, err := fmt.Sscanf(strings.TrimSpace(portStr), "%d", &port); err != nil {
		return fmt.Errorf("invalid port %q: %w", portStr, err)
	}
	if port <= 0 || port > 65535 {
		return fmt.Errorf("port out of range: %d", port)
	}

	return nil
}

// splitN splits s into at most n segments by separator.
func splitN(s, sep string, n int) []string {
	if n <= 0 {
		return nil
	}
	if n == 1 {
		return []string{s}
	}
	var parts []string
	remaining := s
	count := 0
	for count < n-1 {
		idx := strings.Index(remaining, sep)
		if idx < 0 {
			break
		}
		parts = append(parts, remaining[:idx])
		remaining = remaining[idx+len(sep):]
		count++
	}
	if remaining != "" || len(parts) > 0 {
		parts = append(parts, remaining)
	}
	return parts
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Boot / Startup Manager
 *
 * Handles configuration loading and module initialization.
 */

package app

import (
	"fmt"
	"io"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/config"
)

// ModuleState represents a module's lifecycle state.
type ModuleState int

const (
	ModuleStateUninitialized ModuleState = iota
	ModuleStateInitialized
	ModuleStateRunning
	ModuleStateStopped
	ModuleStateError
)

// ModuleInfo describes a registered module.
type ModuleInfo struct {
	Name        string
	State       ModuleState
	Initialized time.Time
	Error       string
}

// BootManager coordinates configuration loading and module startup.
type BootManager struct {
	mu       sync.RWMutex
	config   *config.Config
	modules  map[string]*ModuleInfo
	engine   *Engine
	logLevel string
}

// NewBootManager creates a boot manager with default configuration.
func NewBootManager() *BootManager {
	return &BootManager{
		modules:  make(map[string]*ModuleInfo),
		logLevel: "info",
	}
}

// LoadConfig reads configuration from a file path.
// If the path is empty or the file is missing, a default configuration is used.
func (b *BootManager) LoadConfig(path string) (*config.Config, error) {
	cfg := config.DefaultConfig()
	if path != "" {
		if err := b.loadConfigFile(path, cfg); err != nil {
			// Fall back to defaults on parse error but report it
			return cfg, fmt.Errorf("config parse: %w (using defaults)", err)
		}
	}
	b.mu.Lock()
	b.config = cfg
	b.mu.Unlock()
	return cfg, nil
}

// loadConfigFile reads key=value pairs from a file and applies them to cfg.
func (b *BootManager) loadConfigFile(path string, cfg *config.Config) error {
	f, err := os.Open(path)
	if err != nil {
		return nil // Missing file is acceptable; use defaults
	}
	defer f.Close()

	data, err := io.ReadAll(f)
	if err != nil {
		return err
	}

	return parseConfigBytes(data, cfg)
}

// parseConfigBytes parses simple `key=value` style configuration content.
func parseConfigBytes(data []byte, cfg *config.Config) error {
	lines := strings.Split(string(data), "\n")
	for _, raw := range lines {
		line := strings.TrimSpace(raw)
		if line == "" || strings.HasPrefix(line, "#") || strings.HasPrefix(line, "//") {
			continue
		}
		eqIdx := strings.Index(line, "=")
		if eqIdx < 0 {
			continue
		}
		key := strings.TrimSpace(line[:eqIdx])
		value := strings.TrimSpace(line[eqIdx+1:])
		value = strings.Trim(value, "\"'")
		applyConfigKeyValue(cfg, key, value)
	}
	return nil
}

// applyConfigKeyValue applies a single key/value pair to the config.
func applyConfigKeyValue(cfg *config.Config, key, value string) {
	switch strings.ToLower(key) {
	case "listen_ip", "listenip", "listen-ip":
		cfg.ListenIP = value
	case "listen_port", "listenport", "listen-port":
		port := 0
		fmt.Sscanf(value, "%d", &port)
		if port > 0 {
			cfg.ListenPort = port
		}
	case "realm", "sip_realm":
		cfg.Realm = value
	case "log_level", "loglevel", "log-level":
		cfg.LogLevel = value
	case "enable_media_proxy", "enable-mediaproxy":
		cfg.EnableMediaProxy = parseBool(value)
	case "media_proxy_host":
		cfg.MediaProxyHost = value
	case "media_proxy_port":
		port := 0
		fmt.Sscanf(value, "%d", &port)
		if port > 0 {
			cfg.MediaProxyPort = port
		}
	case "auth_enabled", "auth":
		cfg.AuthEnabled = parseBool(value)
	case "nat_enabled", "nat":
		cfg.NATEnabled = parseBool(value)
	case "presence_enabled", "presence":
		cfg.PresenceEnabled = parseBool(value)
	}
}

// parseBool parses a boolean string value.
func parseBool(s string) bool {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "true", "1", "yes", "on", "enable", "enabled":
		return true
	default:
		return false
	}
}

// RegisterModule registers a module by name for tracking.
func (b *BootManager) RegisterModule(name string) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.modules[name] = &ModuleInfo{
		Name:  name,
		State: ModuleStateUninitialized,
	}
}

// SetModuleState updates a module's runtime state.
func (b *BootManager) SetModuleState(name string, state ModuleState, errMsg string) {
	b.mu.Lock()
	defer b.mu.Unlock()
	m, ok := b.modules[name]
	if !ok {
		m = &ModuleInfo{Name: name}
		b.modules[name] = m
	}
	m.State = state
	m.Error = errMsg
	if state == ModuleStateInitialized || state == ModuleStateRunning {
		m.Initialized = time.Now()
	}
}

// Boot initializes all core modules and returns a running Engine.
// This registers well-known modules (usrloc, auth, nat, dialog, presence)
// and transitions them through initialization to the running state.
func (b *BootManager) Boot() (*Engine, error) {
	modules := []string{"usrloc", "auth", "nat", "dialog", "presence", "parser"}
	for _, m := range modules {
		b.RegisterModule(m)
	}

	if b.config == nil {
		b.config = config.DefaultConfig()
	}

	for _, m := range modules {
		b.SetModuleState(m, ModuleStateInitialized, "")
	}

	engine := NewEngine(b.config)
	if err := engine.Start(); err != nil {
		for _, m := range modules {
			b.SetModuleState(m, ModuleStateError, err.Error())
		}
		return nil, err
	}

	for _, m := range modules {
		b.SetModuleState(m, ModuleStateRunning, "")
	}

	b.mu.Lock()
	b.engine = engine
	b.mu.Unlock()

	return engine, nil
}

// Shutdown stops the engine and marks modules as stopped.
func (b *BootManager) Shutdown() {
	b.mu.Lock()
	if b.engine != nil {
		b.engine.Stop()
	}
	for _, m := range b.modules {
		m.State = ModuleStateStopped
	}
	b.mu.Unlock()
}

// ModuleList returns a snapshot of all module states.
func (b *BootManager) ModuleList() []*ModuleInfo {
	b.mu.RLock()
	defer b.mu.RUnlock()
	list := make([]*ModuleInfo, 0, len(b.modules))
	for _, m := range b.modules {
		info := *m
		list = append(list, &info)
	}
	return list
}

// Config returns the current configuration snapshot.
func (b *BootManager) Config() *config.Config {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return b.config
}

// Engine returns the running engine (if any).
func (b *BootManager) Engine() *Engine {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return b.engine
}

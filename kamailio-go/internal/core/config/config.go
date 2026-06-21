// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Configuration system - matching C cfg.y / coreparam.c
 *
 * Supports YAML configuration files for defining:
 * - Listening sockets
 * - Modules and their parameters
 * - Routing logic (simplified)
 * - IMS-specific settings
 */

package config

import (
	"fmt"
	"os"

	"gopkg.in/yaml.v3"
)

// Config represents the server configuration
type Config struct {
	Core     CoreConfig     `yaml:"core"`
	IMS      IMSConfig      `yaml:"ims,omitempty"`
	Modules  []ModuleConfig `yaml:"modules,omitempty"`
	Routes   RouteConfig    `yaml:"routes,omitempty"`

	// Flat fields for simple key/value configuration overlays used by
	// the boot manager.
	ListenIP         string `yaml:"listen_ip,omitempty"`
	ListenPort       int    `yaml:"listen_port,omitempty"`
	Realm            string `yaml:"realm,omitempty"`
	LogLevel         string `yaml:"log_level,omitempty"`
	EnableMediaProxy bool   `yaml:"enable_media_proxy,omitempty"`
	MediaProxyHost   string `yaml:"media_proxy_host,omitempty"`
	MediaProxyPort   int    `yaml:"media_proxy_port,omitempty"`
	AuthEnabled      bool   `yaml:"auth_enabled,omitempty"`
	NATEnabled       bool   `yaml:"nat_enabled,omitempty"`
	PresenceEnabled  bool   `yaml:"presence_enabled,omitempty"`
	HealthListenAddr string `yaml:"health_listen_addr,omitempty"`
}

// CoreConfig represents core server settings
type CoreConfig struct {
	Debug      int      `yaml:"debug"`
	LogLevel   string   `yaml:"log_level"`
	LogStderr  bool     `yaml:"log_stderr"`
	User       string   `yaml:"user,omitempty"`
	Group      string   `yaml:"group,omitempty"`
	Workers    int      `yaml:"workers"`
	Listen     []string `yaml:"listen"`
	Aliases    []string `yaml:"aliases,omitempty"`
	MaxBufSize int      `yaml:"max_buffer_size,omitempty"`
}

// IMSConfig represents IMS-specific settings
type IMSConfig struct {
	Enabled           bool   `yaml:"enabled"`
	Realm             string `yaml:"realm"`
	SCSCF             bool   `yaml:"scscf"`
	PCSCF             bool   `yaml:"pcscf"`
	ICSCF             bool   `yaml:"icscf"`
	AKAAlgorithm      string `yaml:"aka_algorithm,omitempty"`
	DefaultExpires    int    `yaml:"default_expires,omitempty"`
	MinExpires        int    `yaml:"min_expires,omitempty"`
	MaxExpires        int    `yaml:"max_expires,omitempty"`
	VisitedNetworkID  string `yaml:"visited_network_id,omitempty"`
}

// ModuleConfig represents a module configuration
type ModuleConfig struct {
	Name   string                 `yaml:"name"`
	Params map[string]interface{} `yaml:"params,omitempty"`
}

// RouteConfig represents routing configuration
type RouteConfig struct {
	Request []RouteRule `yaml:"request,omitempty"`
	Reply   []RouteRule `yaml:"reply,omitempty"`
}

// RouteRule represents a single routing rule
type RouteRule struct {
	Method  string `yaml:"method,omitempty"`
	Header  string `yaml:"header,omitempty"`
	Action  string `yaml:"action"`
	Target  string `yaml:"target,omitempty"`
}

// DefaultConfig returns a default configuration
func DefaultConfig() *Config {
	return &Config{
		Core: CoreConfig{
			Debug:      3,
			LogLevel:   "info",
			LogStderr:  true,
			Workers:    8,
			Listen:     []string{"udp:0.0.0.0:5060"},
			MaxBufSize: 65535,
		},
		IMS: IMSConfig{
			Enabled:        false,
			Realm:          "ims.mnc001.mcc460.gprs",
			SCSCF:          false,
			PCSCF:          false,
			ICSCF:          false,
			AKAAlgorithm:   "AKAv1-MD5",
			DefaultExpires: 3600,
			MinExpires:     60,
			MaxExpires:     86400,
		},
		Modules: []ModuleConfig{
			{Name: "tm"},
			{Name: "sl"},
		},
		Routes: RouteConfig{
			Request: []RouteRule{
				{Method: "REGISTER", Action: "handle_register"},
				{Method: "INVITE", Action: "handle_invite"},
				{Method: "BYE", Action: "handle_bye"},
				{Method: "ACK", Action: "relay"},
				{Method: "CANCEL", Action: "handle_cancel"},
			},
			Reply: []RouteRule{
				{Action: "relay"},
			},
		},
	}
}

// Load loads configuration from a YAML file
func Load(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read config file: %w", err)
	}

	cfg := DefaultConfig()
	if err := yaml.Unmarshal(data, cfg); err != nil {
		return nil, fmt.Errorf("failed to parse config file: %w", err)
	}

	// Validate configuration
	if err := cfg.Validate(); err != nil {
		return nil, fmt.Errorf("invalid configuration: %w", err)
	}

	return cfg, nil
}

// Validate validates the configuration
func (c *Config) Validate() error {
	if c.Core.Workers <= 0 {
		c.Core.Workers = 8
	}
	if len(c.Core.Listen) == 0 {
		return fmt.Errorf("no listen sockets configured")
	}
	if c.Core.LogLevel == "" {
		c.Core.LogLevel = "info"
	}
	return nil
}

// Save saves the configuration to a YAML file
func (c *Config) Save(path string) error {
	data, err := yaml.Marshal(c)
	if err != nil {
		return fmt.Errorf("failed to marshal config: %w", err)
	}

	if err := os.WriteFile(path, data, 0644); err != nil {
		return fmt.Errorf("failed to write config file: %w", err)
	}

	return nil
}

// GetModule returns a module configuration by name
func (c *Config) GetModule(name string) *ModuleConfig {
	for i := range c.Modules {
		if c.Modules[i].Name == name {
			return &c.Modules[i]
		}
	}
	return nil
}

// GetListenAddresses returns all listen addresses
func (c *Config) GetListenAddresses() []string {
	return c.Core.Listen
}

// IsIMSEnabled returns true if IMS is enabled
func (c *Config) IsIMSEnabled() bool {
	return c.IMS.Enabled
}

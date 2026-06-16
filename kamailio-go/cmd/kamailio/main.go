// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Main entry point for the SIP server
 */

package main

import (
	"context"
	"flag"
	"fmt"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/config"
	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/transport"
	"github.com/kamailio/kamailio-go/internal/ims/scscf"
)

var (
	Version   = "1.0.0"
	GitCommit = "unknown"
)

// Server represents the SIP server
type Server struct {
	cfg       *config.Config
	listeners []*transport.UDPListener
	registrar *scscf.Registrar
	sessionH  *scscf.SessionHandler
	ctx       context.Context
	cancel    context.CancelFunc
}

func main() {
	// Parse command line flags
	showVersion := flag.Bool("v", false, "Show version")
	showHelp := flag.Bool("h", false, "Show help")
	configFile := flag.String("f", "", "Configuration file")
	logLevel := flag.String("L", "info", "Log level (debug, info, warn, error)")
	flag.Parse()

	if *showVersion {
		fmt.Printf("Kamailio-Go version %s (git: %s)\n", Version, GitCommit)
		os.Exit(0)
	}

	if *showHelp {
		flag.Usage()
		os.Exit(0)
	}

	// Load configuration
	var cfg *config.Config
	var err error
	if *configFile != "" {
		cfg, err = config.Load(*configFile)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to load config: %v\n", err)
			os.Exit(1)
		}
	} else {
		cfg = config.DefaultConfig()
	}

	// Override log level if specified
	if *logLevel != "" {
		cfg.Core.LogLevel = *logLevel
	}

	// Initialize logging
	logCfg := &log.Config{
		Level:    cfg.Core.LogLevel,
		Encoding: "console",
	}
	if err := log.Init(logCfg); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to initialize logging: %v\n", err)
		os.Exit(1)
	}
	defer log.Sync()

	log.Info("Kamailio-Go starting",
		log.String("version", Version),
		log.String("git", GitCommit),
		log.Int("workers", cfg.Core.Workers),
	)

	// Create server
	server := &Server{
		cfg: cfg,
	}
	server.ctx, server.cancel = context.WithCancel(context.Background())

	// Initialize IMS if enabled
	if cfg.IsIMSEnabled() {
		server.registrar = scscf.NewRegistrar(cfg.IMS.Realm)
		server.sessionH = scscf.NewSessionHandler(server.registrar)
		log.Info("IMS enabled",
			log.String("realm", cfg.IMS.Realm),
			log.Bool("scscf", cfg.IMS.SCSCF),
			log.Bool("pcscf", cfg.IMS.PCSCF),
		)
	}

	// Start listeners
	if err := server.startListeners(); err != nil {
		log.Error("Failed to start listeners", log.ErrField(err))
		os.Exit(1)
	}

	// Wait for interrupt signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	sig := <-sigChan
	log.Info("Received signal, shutting down",
		log.String("signal", sig.String()),
	)

	// Graceful shutdown
	server.shutdown()

	log.Info("Kamailio-Go stopped")
}

// startListeners starts all configured listeners
func (s *Server) startListeners() error {
	for _, addr := range s.cfg.GetListenAddresses() {
		si, err := transport.ParseSocketInfo(addr)
		if err != nil {
			return fmt.Errorf("invalid listen address %s: %w", addr, err)
		}

		switch si.Protocol {
		case transport.ProtoUDP:
			listener := transport.NewUDPListener(si, s.handleMessage)
			if err := listener.ListenAndServe(); err != nil {
				return fmt.Errorf("failed to start UDP listener on %s: %w", addr, err)
			}
			s.listeners = append(s.listeners, listener)
			log.Info("UDP listener started", log.String("address", addr))
		default:
			log.Warn("Unsupported protocol", log.String("protocol", si.Protocol.String()))
		}
	}
	return nil
}

// handleMessage handles incoming SIP messages
func (s *Server) handleMessage(data []byte, srcAddr *net.UDPAddr, rcvInfo *transport.ReceiveInfo) {
	// Parse the message
	msg, err := parser.ParseMsg(data)
	if err != nil {
		log.Warn("Failed to parse message", log.ErrField(err))
		return
	}

	// Route the message
	if msg.IsRequest() {
		s.handleRequest(msg)
	} else {
		s.handleReply(msg)
	}
}

// handleRequest handles SIP requests
func (s *Server) handleRequest(msg *parser.SIPMsg) {
	method := msg.Method()

	switch method {
	case parser.MethodRegister:
		if s.registrar != nil {
			result, err := s.registrar.HandleRegister(msg)
			if err != nil {
				log.Warn("Register handling failed", log.ErrField(err))
				return
			}
			log.Info("REGISTER handled",
				log.Uint16("status", result.StatusCode),
			)
		}
	case parser.MethodInvite:
		if s.sessionH != nil {
			result, err := s.sessionH.HandleInvite(msg)
			if err != nil {
				log.Warn("INVITE handling failed", log.ErrField(err))
				return
			}
			log.Info("INVITE handled",
				log.Uint16("status", result.StatusCode),
			)
		}
	case parser.MethodBye:
		if s.sessionH != nil {
			result, err := s.sessionH.HandleBye(msg)
			if err != nil {
				log.Warn("BYE handling failed", log.ErrField(err))
				return
			}
			log.Info("BYE handled",
				log.Uint16("status", result.StatusCode),
			)
		}
	default:
		log.Debug("Request received",
			log.String("method", parser.MethodName(method)),
		)
	}
}

// handleReply handles SIP replies
func (s *Server) handleReply(msg *parser.SIPMsg) {
	if s.sessionH != nil {
		_, err := s.sessionH.HandleReply(msg)
		if err != nil {
			log.Warn("Reply handling failed", log.ErrField(err))
		}
	}
}

// shutdown gracefully shuts down the server
func (s *Server) shutdown() {
	s.cancel()

	// Stop all listeners
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	for _, listener := range s.listeners {
		if err := listener.Shutdown(ctx); err != nil {
			log.Warn("Listener shutdown error", log.ErrField(err))
		}
	}
}

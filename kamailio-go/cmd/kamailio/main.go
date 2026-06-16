// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Main entry point for the SIP server
 */

package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"go.uber.org/zap"

	"github.com/kamailio/kamailio-go/internal/core/log"
)

var (
	Version   = "1.0.0"
	GitCommit = "unknown"
)

func main() {
	// Parse command line flags
	showVersion := flag.Bool("v", false, "Show version")
	showHelp := flag.Bool("h", false, "Show help")
	_ = flag.String("f", "", "Configuration file") // TODO: use config
	logLevel := flag.String("L", "info", "Log level (debug, info, warn, error)")
	workers := flag.Int("n", 8, "Number of worker processes")
	port := flag.Int("l", 5060, "Listen port")
	flag.Parse()

	if *showVersion {
		fmt.Printf("Kamailio-Go version %s (git: %s)\n", Version, GitCommit)
		fmt.Printf("Go version: %s\n", os.Getenv("GOVERSION"))
		os.Exit(0)
	}

	if *showHelp {
		flag.Usage()
		os.Exit(0)
	}

	// Initialize logging
	logCfg := &log.Config{
		Level:    *logLevel,
		Encoding: "console",
	}
	if err := log.Init(logCfg); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to initialize logging: %v\n", err)
		os.Exit(1)
	}
	defer log.Sync()

	log.Info("Kamailio-Go starting",
		zap.String("version", Version),
		zap.String("git", GitCommit),
		zap.Int("workers", *workers),
		zap.Int("port", *port),
	)

	// TODO: M1 - placeholder startup
	// Full implementation will include:
	// - Configuration loading
	// - Socket setup (UDP/TCP/TLS)
	// - Worker pool initialization
	// - Signal handling

	// Wait for interrupt signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	sig := <-sigChan
	log.Info("Received signal, shutting down",
		zap.String("signal", sig.String()),
	)

	log.Info("Kamailio-Go stopped")
}

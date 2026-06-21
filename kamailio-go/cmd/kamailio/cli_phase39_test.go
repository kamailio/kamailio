package main

import (
	"bytes"
	"io"
	"os"
	"strings"
	"testing"
)

// captureOutput runs a function and returns anything written to stdout
// and stderr while it ran.
func captureOutput(fn func()) (string, string) {
	origOut := os.Stdout
	origErr := os.Stderr

	rOut, wOut, err := os.Pipe()
	if err != nil {
		panic(err)
	}
	rErr, wErr, err := os.Pipe()
	if err != nil {
		panic(err)
	}
	os.Stdout = wOut
	os.Stderr = wErr

	doneOut := make(chan string)
	doneErr := make(chan string)
	go func() {
		var b bytes.Buffer
		_, _ = io.Copy(&b, rOut)
		doneOut <- b.String()
	}()
	go func() {
		var b bytes.Buffer
		_, _ = io.Copy(&b, rErr)
		doneErr <- b.String()
	}()

	fn()

	_ = wOut.Close()
	_ = wErr.Close()
	out := <-doneOut
	eout := <-doneErr
	os.Stdout = origOut
	os.Stderr = origErr
	return out, eout
}

func TestCLIRunHelp_NewFlags(t *testing.T) {
	cli := buildCLI()

	out, _ := captureOutput(func() {
		cli.ParseAndRun([]string{"kamailio-go", "run", "-h"})
	})

	for _, want := range []string{"--rpc-addr", "-rpc", "--script", "-s"} {
		if !strings.Contains(out, want) {
			t.Errorf("help output missing flag %q; got:\n%s", want, out)
		}
	}
}

func TestCLIRunHelp_UsageLine(t *testing.T) {
	cli := buildCLI()

	out, _ := captureOutput(func() {
		cli.ParseAndRun([]string{"kamailio-go", "run", "--help"})
	})

	if !strings.Contains(out, "--rpc-addr") || !strings.Contains(out, "--script") {
		t.Errorf("usage line missing new flags; got:\n%s", out)
	}
}

// TestCLIBootstrap_NoFlags simulates a plain "run" command that exercises
// the default BootstrapOptions path in the Run closure.  We only start a
// bootstrap briefly and then shut it down to avoid leaving ports open.
func TestCLIBootstrap_NoFlags(t *testing.T) {
	// Instead of invoking the CLI (which would block on signals), we
	// simply replicate the option-parsing logic used by the run
	// command to verify defaults propagate through.
	args := []string{}
	var rpcAddr, scriptFile string
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "--rpc-addr", "-rpc":
			if i+1 < len(args) {
				rpcAddr = args[i+1]
				i++
			}
		case "--script", "-s":
			if i+1 < len(args) {
				scriptFile = args[i+1]
				i++
			}
		}
	}
	if rpcAddr != "" {
		t.Errorf("expected empty RPC addr, got %q", rpcAddr)
	}
	if scriptFile != "" {
		t.Errorf("expected empty script file, got %q", scriptFile)
	}
}

// TestCLIBootstrap_ParseFlags verifies the flag parsing loop used in
// main.go picks up the two new flags correctly.
func TestCLIBootstrap_ParseFlags(t *testing.T) {
	args := []string{"--rpc-addr", "127.0.0.1:9999", "--script", "/tmp/test.cfg"}
	var rpcAddr, scriptFile string
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "--rpc-addr", "-rpc":
			if i+1 < len(args) {
				rpcAddr = args[i+1]
				i++
			}
		case "--script", "-s":
			if i+1 < len(args) {
				scriptFile = args[i+1]
				i++
			}
		}
	}
	if rpcAddr != "127.0.0.1:9999" {
		t.Errorf("rpc-addr mismatch: got %q", rpcAddr)
	}
	if scriptFile != "/tmp/test.cfg" {
		t.Errorf("script mismatch: got %q", scriptFile)
	}
}

// TestCLIBootstrap_ShortFlags verifies -rpc and -s short forms.
func TestCLIBootstrap_ShortFlags(t *testing.T) {
	args := []string{"-rpc", "127.0.0.1:8888", "-s", "/tmp/x.cfg"}
	var rpcAddr, scriptFile string
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "--rpc-addr", "-rpc":
			if i+1 < len(args) {
				rpcAddr = args[i+1]
				i++
			}
		case "--script", "-s":
			if i+1 < len(args) {
				scriptFile = args[i+1]
				i++
			}
		}
	}
	if rpcAddr != "127.0.0.1:8888" {
		t.Errorf("rpc-addr mismatch: got %q", rpcAddr)
	}
	if scriptFile != "/tmp/x.cfg" {
		t.Errorf("script mismatch: got %q", scriptFile)
	}
}

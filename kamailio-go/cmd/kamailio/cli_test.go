package main

import (
	"testing"
)

func TestBuildCLI_HasExpectedCommands(t *testing.T) {
	cli := buildCLI()
	if cli == nil {
		t.Fatal("expected non-nil CLI")
	}
	for _, name := range []string{"run", "check-config", "version", "test", "help", "start"} {
		if _, ok := cli.Commands[name]; !ok {
			t.Errorf("expected command %q to be registered", name)
		}
	}
}

func TestCLI_ParseAndRun_Version(t *testing.T) {
	cli := buildCLI()
	code := cli.ParseAndRun([]string{"kamailio-go", "version"})
	if code != 0 {
		t.Errorf("version exit = %d, want 0", code)
	}
}

func TestCLI_ParseAndRun_Help(t *testing.T) {
	cli := buildCLI()
	code := cli.ParseAndRun([]string{"kamailio-go", "--help"})
	if code != 0 {
		t.Errorf("help exit = %d, want 0", code)
	}
}

func TestCLI_ParseAndRun_UnknownCommand(t *testing.T) {
	cli := buildCLI()
	code := cli.ParseAndRun([]string{"kamailio-go", "does-not-exist"})
	if code == 0 {
		t.Errorf("unknown command exit = %d, want non-zero", code)
	}
}

func TestCLI_ParseAndRun_SmokeTest(t *testing.T) {
	cli := buildCLI()
	code := cli.ParseAndRun([]string{"kamailio-go", "test"})
	if code != 0 {
		t.Errorf("test exit = %d, want 0", code)
	}
}

func TestCLI_ParseAndRun_CheckConfigDefault(t *testing.T) {
	cli := buildCLI()
	code := cli.ParseAndRun([]string{"kamailio-go", "check-config"})
	if code != 0 {
		t.Errorf("check-config with defaults exit = %d, want 0", code)
	}
}

package app

import (
	"fmt"
	"os"
	"strings"
)

// Command represents a subcommand that can be invoked from the CLI.
type Command struct {
	Name        string
	Aliases     []string
	Description string
	Usage       string
	Run         func(args []string) int
}

// CLI is the top-level command dispatcher.
type CLI struct {
	AppName  string
	Version  string
	Commit   string
	Commands map[string]*Command
}

// NewCLI constructs a CLI with the standard command set.
func NewCLI(appName, version, commit string) *CLI {
	return &CLI{
		AppName:  appName,
		Version:  version,
		Commit:   commit,
		Commands: make(map[string]*Command),
	}
}

// Register adds a command to the dispatcher. The command is keyed by
// its canonical Name and any Aliases.
func (c *CLI) Register(cmd *Command) {
	c.Commands[cmd.Name] = cmd
	for _, alias := range cmd.Aliases {
		c.Commands[alias] = cmd
	}
}

// ParseAndRun is the primary entry point: it parses os.Args-style
// arguments and dispatches to the matching command.
//
// The first non-flag argument is treated as a subcommand. If none is
// provided, the CLI falls back to a "run" command when registered.
func (c *CLI) ParseAndRun(args []string) int {
	if len(args) < 2 {
		if cmd, ok := c.Commands["run"]; ok {
			return cmd.Run(nil)
		}
		c.PrintUsage()
		return 0
	}

	sub := args[1]

	switch {
	case sub == "-h" || sub == "--help" || sub == "help":
		c.PrintUsage()
		return 0
	case sub == "-v" || sub == "--version" || sub == "version":
		if cmd, ok := c.Commands["version"]; ok {
			return cmd.Run(nil)
		}
		fmt.Printf("%s version %s (git: %s)\n", c.AppName, c.Version, c.Commit)
		return 0
	}

	cmd, ok := c.Commands[sub]
	if !ok {
		fmt.Fprintf(os.Stderr, "Unknown command: %s\n\n", sub)
		c.PrintUsage()
		return 2
	}

	var rest []string
	if len(args) > 2 {
		rest = args[2:]
	}
	return cmd.Run(rest)
}

// PrintUsage prints a simple but informative usage line.
func (c *CLI) PrintUsage() {
	fmt.Printf("%s - a modular SIP server written in Go\n\n", c.AppName)
	fmt.Printf("Usage:\n  %s <command> [options]\n\n", c.AppName)
	fmt.Println("Available commands:")

	seen := make(map[*Command]bool)
	for _, cmd := range c.Commands {
		if seen[cmd] {
			continue
		}
		seen[cmd] = true
		aliases := ""
		aliasList := make([]string, 0)
		for n, other := range c.Commands {
			if other == cmd && n != cmd.Name {
				aliasList = append(aliasList, n)
			}
		}
		if len(aliasList) > 0 {
			aliases = " (aliases: " + strings.Join(aliasList, ", ") + ")"
		}
		fmt.Printf("  %-16s %s%s\n", cmd.Name, cmd.Description, aliases)
	}
	fmt.Printf("\nRun '%s <command> -h' for command-specific help.\n", c.AppName)
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Script routing engine tests
 *
 * Exercises parser, executor, and proxy integration end-to-end.
 */

package script

import (
	"net"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// buildSIP constructs a SIP request raw message for parser.ParseMsg.
func buildSIP(method, ruri, from, to string) []byte {
	return []byte(method + " " + ruri + " SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP 10.0.0.1:5060;branch=z9hG4bK1\r\n" +
		"From: " + from + "\r\n" +
		"To: " + to + "\r\n" +
		"Call-ID: test-call-1@example.com\r\n" +
		"CSeq: 1 " + method + "\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
}

// mustParse fails the test if the script text does not parse.
func mustParse(t *testing.T, text string) *Script {
	t.Helper()
	sc, err := ParseScript(text)
	if err != nil {
		t.Fatalf("ParseScript unexpected error: %v", err)
	}
	if sc == nil {
		t.Fatalf("ParseScript returned nil script")
	}
	return sc
}

// mustRun parses and executes the script against ctx, failing on error.
func mustRun(t *testing.T, text string, ctx *ExecContext) {
	t.Helper()
	sc := mustParse(t, text)
	if err := sc.Execute(ctx); err != nil {
		t.Fatalf("Execute unexpected error: %v", err)
	}
}

func TestParseScript_Empty(t *testing.T) {
	sc, err := ParseScript("")
	if err != nil {
		t.Fatalf("empty script should parse, got err: %v", err)
	}
	if sc == nil {
		t.Fatalf("empty script should produce non-nil Script")
	}
	if len(sc.Root) != 0 {
		t.Errorf("expected empty root block, got %d actions", len(sc.Root))
	}
	if len(sc.Routes) != 0 {
		t.Errorf("expected no named routes, got %d", len(sc.Routes))
	}
}

func TestParseScript_RequestRoute(t *testing.T) {
	const src = `
request_route {
    xlog("hello");
    drop;
}
`
	sc := mustParse(t, src)
	if len(sc.Root) != 2 {
		t.Fatalf("expected 2 actions in root block, got %d", len(sc.Root))
	}
	if sc.Root[0].Type != ActLog || sc.Root[0].Arg != "hello" {
		t.Errorf("first action should be xlog('hello'), got type=%d arg=%q", sc.Root[0].Type, sc.Root[0].Arg)
	}
	if sc.Root[1].Type != ActDrop {
		t.Errorf("second action should be drop, got %d", sc.Root[1].Type)
	}
}

func TestParseScript_NamedRoute(t *testing.T) {
	const src = `
request_route {
    route(RELAY);
}
route[RELAY] {
    $du = "sip:10.0.0.2:5060";
    forward();
}
`
	sc := mustParse(t, src)
	if _, ok := sc.Routes["RELAY"]; !ok {
		t.Fatalf("missing named route RELAY")
	}
	if len(sc.Routes["RELAY"]) != 2 {
		t.Fatalf("RELAY should have 2 actions, got %d", len(sc.Routes["RELAY"]))
	}
	ctx := NewExecContext(nil, nil, "example.com")
	if err := sc.Execute(ctx); err != nil {
		t.Fatalf("execute err: %v", err)
	}
	if ctx.DstURI != "sip:10.0.0.2:5060" {
		t.Errorf("expected DstURI sip:10.0.0.2:5060, got %q", ctx.DstURI)
	}
}

func TestParseScript_SendReply(t *testing.T) {
	const src = `
request_route {
    sl_send_reply("200", "OK");
}
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if ctx.Reply == nil {
		t.Fatalf("expected Reply to be set")
	}
	if ctx.Reply.Status != 200 || ctx.Reply.Reason != "OK" {
		t.Errorf("expected 200/OK, got %d/%q", ctx.Reply.Status, ctx.Reply.Reason)
	}
}

func TestParseScript_Forward(t *testing.T) {
	const src = `
request_route {
    forward("sip:10.0.0.3:5060");
}
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if ctx.DstURI != "sip:10.0.0.3:5060" {
		t.Errorf("expected DstURI sip:10.0.0.3:5060, got %q", ctx.DstURI)
	}
}

func TestParseScript_Drop(t *testing.T) {
	const src = `
request_route {
    drop;
    xlog("after drop");
}
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if !ctx.Drop {
		t.Errorf("expected ctx.Drop true")
	}
	if len(ctx.Logs) != 0 {
		t.Errorf("expected no logs after drop, got %+v", ctx.Logs)
	}
}

func TestParseScript_Flags(t *testing.T) {
	const src = `
request_route {
    setflag(1);
    if (flag(1)) {
        xlog("set");
    }
    resetflag(1);
    if (!flag(1)) {
        xlog("cleared");
    }
}
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if len(ctx.Logs) != 2 || ctx.Logs[0] != "set" || ctx.Logs[1] != "cleared" {
		t.Errorf("expected [set, cleared], got %+v", ctx.Logs)
	}
}

func TestParseScript_PVMethod(t *testing.T) {
	const src = `
request_route {
    if (method == "INVITE") {
        xlog("invite");
    }
    if (method != "REGISTER") {
        xlog("not-register");
    }
}
`
	msg, err := parser.ParseMsg(buildSIP("INVITE", "sip:bob@example.com", "<sip:alice@example.com>", "<sip:bob@example.com>"))
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	ctx := NewExecContext(msg, nil, "example.com")
	mustRun(t, src, ctx)
	if len(ctx.Logs) != 2 {
		t.Fatalf("expected 2 log lines, got %d (%+v)", len(ctx.Logs), ctx.Logs)
	}
	if ctx.Logs[0] != "invite" || ctx.Logs[1] != "not-register" {
		t.Errorf("unexpected log lines: %+v", ctx.Logs)
	}
}

func TestParseScript_PVReqUser(t *testing.T) {
	const src = `
request_route {
    if ($rU == "bob") {
        xlog("bob");
    }
    if ($rd != "bad.example.com") {
        xlog("rd-ok");
    }
}
`
	raw := buildSIP("INVITE", "sip:bob@example.com", "<sip:alice@example.com>", "<sip:bob@example.com>")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	if err := parser.ParseMsgURI(msg); err != nil {
		t.Fatalf("ParseMsgURI: %v", err)
	}
	ctx := NewExecContext(msg, nil, "example.com")
	mustRun(t, src, ctx)
	if len(ctx.Logs) < 1 || ctx.Logs[0] != "bob" {
		t.Errorf("expected first log 'bob', got %+v", ctx.Logs)
	}
}

func TestParseScript_VarAssign(t *testing.T) {
	const src = `
request_route {
    $var(name) = "alice";
    if ($var(name) == "alice") {
        xlog("matched");
    }
}
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if len(ctx.Logs) != 1 || ctx.Logs[0] != "matched" {
		t.Errorf("expected [matched], got %+v", ctx.Logs)
	}
}

func TestParseScript_NestedIf(t *testing.T) {
	const src = `
request_route {
    setflag(1);
    if (flag(1)) {
        if (flag(2)) {
            xlog("two");
        } else {
            xlog("not-two");
        }
    } else {
        xlog("never");
    }
}
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if len(ctx.Logs) != 1 || ctx.Logs[0] != "not-two" {
		t.Errorf("expected [not-two], got %+v", ctx.Logs)
	}
}

func TestParseScript_RouteCall(t *testing.T) {
	const src = `
request_route {
    route(A);
    route(B);
    route(MISSING);
    xlog("done");
}
route[A] { xlog("A"); }
route[B] { xlog("B"); }
`
	ctx := NewExecContext(nil, nil, "example.com")
	mustRun(t, src, ctx)
	if len(ctx.Logs) != 3 || ctx.Logs[0] != "A" || ctx.Logs[1] != "B" || ctx.Logs[2] != "done" {
		t.Errorf("expected [A, B, done], got %+v", ctx.Logs)
	}
}

func TestParseScript_Invalid(t *testing.T) {
	// Missing closing brace / broken syntax should surface an error.
	const src = `
request_route {
    sl_send_reply("200", "OK");
`
	if _, err := ParseScript(src); err == nil {
		t.Fatalf("expected parse error for unclosed block, got nil")
	}
}

func TestParseScript_Integration(t *testing.T) {
	const scriptText = `
request_route {
    if (method == "INVITE") {
        sl_send_reply("200", "OK");
    }
}
`
	msg, err := parser.ParseMsg(buildSIP("INVITE", "sip:bob@example.com",
		"<sip:alice@example.com>", "<sip:bob@example.com>"))
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}
	sc := mustParse(t, scriptText)
	src := &net.UDPAddr{IP: net.ParseIP("10.0.0.1"), Port: 5060}
	ctx := NewExecContext(msg, src, "example.com")
	if err := sc.Execute(ctx); err != nil {
		t.Fatalf("Execute: %v", err)
	}
	if ctx.Reply == nil {
		t.Fatalf("expected Reply from integration script")
	}
	if ctx.Reply.Status != 200 || ctx.Reply.Reason != "OK" {
		t.Errorf("expected 200/OK from integration script, got %d/%q", ctx.Reply.Status, ctx.Reply.Reason)
	}
}

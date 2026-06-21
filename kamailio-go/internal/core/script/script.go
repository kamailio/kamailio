// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Script routing engine
 *
 * AST type definitions and a recursive-descent parser for a minimal
 * subset of Kamailio-native configuration language. Supported grammar:
 *
 *   request_route { action ... }
 *   route[NAME]    { action ... }
 *
 *   if (expr) { action ... } [ else { action ... } ]
 *   forward(uri); forward();
 *   sl_send_reply(CODE, "Reason");
 *   drop;
 *   xlog("text");
 *   setflag(N); resetflag(N);
 *   record_route();
 *   append_branch(uri);
 *   route("NAME"); route(NAME);
 *   return;
 *   $du = "sip:...";
 *   $ruri = "sip:...";
 *   $var(name) = "value";
 *
 * Expressions inside `if ( ... )`:
 *   method == "INVITE" / method != "REGISTER"
 *   uri == "sip:..."
 *   $rU == "alice", $fd != "bad.domain", $rm == "INVITE", $ru == ...
 *   flag(N) / !flag(N)
 *   $var(name) == "value"
 */

package script

import (
	"fmt"
	"strconv"
	"strings"
	"unicode"
)

// ActionType enumerates the script primitives we support in Phase 27.
type ActionType int

const (
	ActForward ActionType = iota
	ActSendReply
	ActDrop
	ActLog
	ActSetFlag
	ActResetFlag
	ActIf
	ActRecordRoute
	ActAppendBranch
	ActSetRURI
	ActSetDstURI
	ActSetVar
	ActRoute
	ActReturn
)

// Action is one script instruction. Actions form a linked list inside a
// block. An `if` Action owns two sub-blocks via IfTrue / IfFalse.
type Action struct {
	Type      ActionType
	Arg       string // textual argument: URI, Reason, route name, var name
	ArgNum    int    // numeric argument: flag number, status code
	Arg2      string // secondary text argument, e.g. reason phrase
	IfTrue    []*Action
	IfFalse   []*Action
	RouteName string
	Expr      *Expr
}

// Script is the parsed, executable program.
type Script struct {
	Root   []*Action           // default `request_route { ... }` block
	Routes map[string][]*Action // named routes keyed by route name
}

// ParseScript parses script text into a Script AST.
func ParseScript(text string) (*Script, error) {
	tokens, err := tokenize(text)
	if err != nil {
		return nil, err
	}
	p := &parserState{tokens: tokens, pos: 0}
	return p.parseTopLevel()
}

// Parse is a convenience alias for ParseScript so callers can write
// script.Parse(text) without remembering the longer name.
func Parse(text string) (*Script, error) { return ParseScript(text) }

// ---------------------------------------------------------------------------
// tokenizer
// ---------------------------------------------------------------------------

type tokKind int

const (
	tokIdent tokKind = iota
	tokString
	tokNumber
	tokSymbol
	tokEOF
)

type tok struct {
	kind tokKind
	text string
	line int
}

// tokenize scans the input text into a stream of typed tokens.
func tokenize(text string) ([]tok, error) {
	var tokens []tok
	line := 1
	i := 0
	n := len(text)
	for i < n {
		c := text[i]
		switch {
		case c == '\n':
			line++
			i++
		case unicode.IsSpace(rune(c)):
			i++
		case c == '#':
			for i < n && text[i] != '\n' {
				i++
			}
		case c == '"':
			i++
			start := i
			var sb strings.Builder
			for i < n && text[i] != '"' {
				if text[i] == '\\' && i+1 < n {
					sb.WriteByte(text[i+1])
					i += 2
					continue
				}
				if text[i] == '\n' {
					line++
				}
				sb.WriteByte(text[i])
				i++
			}
			if i >= n {
				return nil, fmt.Errorf("unterminated string starting at line %d near %q", line, truncate(text[start:], 40))
			}
			tokens = append(tokens, tok{kind: tokString, text: sb.String(), line: line})
			i++
		case c == '$':
			start := i
			i++
			for i < n {
				cc := text[i]
				if isPVChar(cc) {
					i++
					continue
				}
				break
			}
			tokens = append(tokens, tok{kind: tokIdent, text: text[start:i], line: line})
		case unicode.IsDigit(rune(c)):
			start := i
			for i < n && unicode.IsDigit(rune(text[i])) {
				i++
			}
			tokens = append(tokens, tok{kind: tokNumber, text: text[start:i], line: line})
		case isSymChar(c):
			if i+1 < n && isTwoCharSym(text[i:i+2]) {
				tokens = append(tokens, tok{kind: tokSymbol, text: text[i : i+2], line: line})
				i += 2
			} else {
				tokens = append(tokens, tok{kind: tokSymbol, text: string(c), line: line})
				i++
			}
		case unicode.IsLetter(rune(c)) || c == '_':
			start := i
			for i < n && (unicode.IsLetter(rune(text[i])) || unicode.IsDigit(rune(text[i])) || text[i] == '_' || text[i] == '-') {
				i++
			}
			tokens = append(tokens, tok{kind: tokIdent, text: text[start:i], line: line})
		default:
			i++
		}
	}
	tokens = append(tokens, tok{kind: tokEOF, text: "", line: line})
	return tokens, nil
}

func isSymChar(c byte) bool {
	switch c {
	case '{', '}', '(', ')', ';', ',', '=', '!', '<', '>', '[', ']':
		return true
	}
	return false
}

func isTwoCharSym(s string) bool {
	switch s {
	case "==", "!=", "<=", ">=":
		return true
	}
	return false
}

// isPVChar reports whether c is a valid character inside a PV token.
// PV tokens terminate at whitespace and at the special delimiters that
// surround them in assignments ("$du = ...") and expressions.
func isPVChar(c byte) bool {
	if unicode.IsLetter(rune(c)) || unicode.IsDigit(rune(c)) || c == '_' {
		return true
	}
	if c == '(' || c == ')' {
		return true
	}
	return false
}

func truncate(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n] + "..."
}

// ---------------------------------------------------------------------------
// recursive-descent parser
// ---------------------------------------------------------------------------

type parserState struct {
	tokens []tok
	pos    int
}

func (p *parserState) cur() tok {
	if p.pos >= len(p.tokens) {
		return tok{kind: tokEOF, line: 0}
	}
	return p.tokens[p.pos]
}

func (p *parserState) advance() tok {
	t := p.cur()
	p.pos++
	return t
}

func (p *parserState) expect(kind tokKind, text string) (tok, error) {
	t := p.cur()
	if t.kind != kind {
		return t, fmt.Errorf("line %d: expected token kind %d (%q), got kind=%d text=%q", t.line, kind, text, t.kind, t.text)
	}
	if text != "" && t.text != text {
		return t, fmt.Errorf("line %d: expected token %q, got %q", t.line, text, t.text)
	}
	p.pos++
	return t, nil
}

func (p *parserState) parseTopLevel() (*Script, error) {
	sc := &Script{Routes: make(map[string][]*Action)}
	for {
		t := p.cur()
		if t.kind == tokEOF {
			break
		}
		if t.kind != tokIdent {
			return nil, fmt.Errorf("line %d: expected top-level block, got %q", t.line, t.text)
		}
		name := t.text
		p.advance()
		switch name {
		case "request_route":
			if _, err := p.expect(tokSymbol, "{"); err != nil {
				return nil, err
			}
			actions, err := p.parseBlock()
			if err != nil {
				return nil, err
			}
			sc.Root = actions
		case "route":
			if _, err := p.expect(tokSymbol, "["); err != nil {
				return nil, err
			}
			rtTok := p.cur()
			if rtTok.kind != tokIdent && rtTok.kind != tokString && rtTok.kind != tokNumber {
				return nil, fmt.Errorf("line %d: expected route name, got %q", rtTok.line, rtTok.text)
			}
			p.advance()
			if _, err := p.expect(tokSymbol, "]"); err != nil {
				return nil, err
			}
			if _, err := p.expect(tokSymbol, "{"); err != nil {
				return nil, err
			}
			actions, err := p.parseBlock()
			if err != nil {
				return nil, err
			}
			sc.Routes[rtTok.text] = actions
		default:
			return nil, fmt.Errorf("line %d: unknown top-level block %q", t.line, name)
		}
	}
	return sc, nil
}

// parseBlock reads actions until the matching closing '}'. The opening
// '{' is consumed by the caller.
func (p *parserState) parseBlock() ([]*Action, error) {
	var actions []*Action
	for {
		t := p.cur()
		if t.kind == tokEOF {
			return nil, fmt.Errorf("line %d: unexpected end of script inside block", t.line)
		}
		if t.kind == tokSymbol && t.text == "}" {
			p.advance()
			return actions, nil
		}
		a, err := p.parseAction()
		if err != nil {
			return nil, err
		}
		if a != nil {
			actions = append(actions, a)
		}
	}
}

// parseAction parses one statement inside a block. Statements end with
// either ';' or, for `if ... else` constructs, after the closing '}'.
func (p *parserState) parseAction() (*Action, error) {
	t := p.cur()
	if t.kind != tokIdent {
		return nil, fmt.Errorf("line %d: expected action, got %q", t.line, t.text)
	}

	// PV assignment form: $x = "..." ;
	if strings.HasPrefix(t.text, "$") {
		return p.parseAssignment(t)
	}

	// Keyword statements.
	switch t.text {
	case "if":
		return p.parseIf()
	case "forward":
		return p.parseForward()
	case "sl_send_reply":
		return p.parseSendReply()
	case "drop":
		p.advance()
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActDrop}, nil
	case "xlog", "log":
		p.advance()
		if _, err := p.expect(tokSymbol, "("); err != nil {
			return nil, err
		}
		msgTok, err := p.expect(tokString, "")
		if err != nil {
			return nil, err
		}
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActLog, Arg: msgTok.text}, nil
	case "setflag":
		p.advance()
		if _, err := p.expect(tokSymbol, "("); err != nil {
			return nil, err
		}
		n, err := p.parseNumeric()
		if err != nil {
			return nil, err
		}
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActSetFlag, ArgNum: n}, nil
	case "resetflag":
		p.advance()
		if _, err := p.expect(tokSymbol, "("); err != nil {
			return nil, err
		}
		n, err := p.parseNumeric()
		if err != nil {
			return nil, err
		}
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActResetFlag, ArgNum: n}, nil
	case "record_route":
		p.advance()
		if p.cur().kind == tokSymbol && p.cur().text == "(" {
			p.advance()
			if _, err := p.expect(tokSymbol, ")"); err != nil {
				return nil, err
			}
		}
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActRecordRoute}, nil
	case "append_branch":
		p.advance()
		if _, err := p.expect(tokSymbol, "("); err != nil {
			return nil, err
		}
		uriTok, err := p.expect(tokString, "")
		if err != nil {
			return nil, err
		}
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActAppendBranch, Arg: uriTok.text}, nil
	case "route":
		p.advance()
		if _, err := p.expect(tokSymbol, "("); err != nil {
			return nil, err
		}
		nameTok := p.cur()
		if nameTok.kind != tokString && nameTok.kind != tokIdent && nameTok.kind != tokNumber {
			return nil, fmt.Errorf("line %d: expected route name, got %q", nameTok.line, nameTok.text)
		}
		p.advance()
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActRoute, RouteName: nameTok.text}, nil
	case "return":
		p.advance()
		if _, err := p.maybeSemicolon(); err != nil {
			return nil, err
		}
		return &Action{Type: ActReturn}, nil
	}
	return nil, fmt.Errorf("line %d: unknown action %q", t.line, t.text)
}

// maybeSemicolon consumes an optional trailing ';'.
func (p *parserState) maybeSemicolon() (tok, error) {
	if p.cur().kind == tokSymbol && p.cur().text == ";" {
		t := p.cur()
		p.advance()
		return t, nil
	}
	return tok{}, nil
}

// parseNumeric reads a number token, accepting either a bare number or a
// quoted numeric string (used as a liberal fallback for script writers).
func (p *parserState) parseNumeric() (int, error) {
	t := p.cur()
	if t.kind == tokNumber {
		n, err := strconv.Atoi(t.text)
		if err != nil {
			return 0, fmt.Errorf("line %d: invalid number %q: %v", t.line, t.text, err)
		}
		p.advance()
		return n, nil
	}
	if t.kind == tokString {
		n, err := strconv.Atoi(t.text)
		if err != nil {
			return 0, fmt.Errorf("line %d: expected number, got %q", t.line, t.text)
		}
		p.advance()
		return n, nil
	}
	return 0, fmt.Errorf("line %d: expected number, got %q", t.line, t.text)
}

// parseAssignment handles $x = "value"; at the start of a statement.
func (p *parserState) parseAssignment(start tok) (*Action, error) {
	p.advance()
	if _, err := p.expect(tokSymbol, "="); err != nil {
		return nil, err
	}
	valTok, err := p.expect(tokString, "")
	if err != nil {
		// Allow bare PV-to-PV assignments? Keep it strict for now.
		return nil, err
	}
	if _, err := p.maybeSemicolon(); err != nil {
		return nil, err
	}
	lhs := strings.ToLower(start.text)
	switch lhs {
	case "$du", "$dsturi":
		return &Action{Type: ActSetDstURI, Arg: valTok.text}, nil
	case "$ruri", "$ru":
		return &Action{Type: ActSetRURI, Arg: valTok.text}, nil
	}
	if strings.HasPrefix(lhs, "$var(") && strings.HasSuffix(lhs, ")") {
		name := start.text[len("$var(") : len(start.text)-1]
		return &Action{Type: ActSetVar, Arg: name, Arg2: valTok.text}, nil
	}
	return nil, fmt.Errorf("line %d: unsupported assignment target %q", start.line, start.text)
}

// parseIf handles `if (expr) { ... } [ else { ... } ]`.
func (p *parserState) parseIf() (*Action, error) {
	p.advance() // consume "if"
	if _, err := p.expect(tokSymbol, "("); err != nil {
		return nil, err
	}
	expr, err := p.parseExpr()
	if err != nil {
		return nil, err
	}
	if _, err := p.expect(tokSymbol, ")"); err != nil {
		return nil, err
	}
	if _, err := p.expect(tokSymbol, "{"); err != nil {
		return nil, err
	}
	trueBlock, err := p.parseBlock()
	if err != nil {
		return nil, err
	}
	act := &Action{Type: ActIf, Expr: expr, IfTrue: trueBlock}
	if p.cur().kind == tokIdent && strings.EqualFold(p.cur().text, "else") {
		p.advance()
		if _, err := p.expect(tokSymbol, "{"); err != nil {
			return nil, err
		}
		falseBlock, err := p.parseBlock()
		if err != nil {
			return nil, err
		}
		act.IfFalse = falseBlock
	}
	return act, nil
}

// parseExpr parses one boolean expression.
func (p *parserState) parseExpr() (*Expr, error) {
	t := p.cur()
	// flag(N) / !flag(N)
	if t.kind == tokSymbol && t.text == "!" {
		p.advance()
		next := p.cur()
		if next.kind == tokIdent && strings.ToLower(next.text) == "flag" {
			p.advance()
			if _, err := p.expect(tokSymbol, "("); err != nil {
				return nil, err
			}
			n, err := p.parseNumeric()
			if err != nil {
				return nil, err
			}
			if _, err := p.expect(tokSymbol, ")"); err != nil {
				return nil, err
			}
			return &Expr{IsFlag: true, FlagN: n, Negate: true}, nil
		}
		return nil, fmt.Errorf("line %d: unexpected '!' before %q", next.line, next.text)
	}
	if t.kind == tokIdent && strings.ToLower(t.text) == "flag" {
		p.advance()
		if _, err := p.expect(tokSymbol, "("); err != nil {
			return nil, err
		}
		n, err := p.parseNumeric()
		if err != nil {
			return nil, err
		}
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
		return &Expr{IsFlag: true, FlagN: n}, nil
	}

	// left-hand side: a PV or a keyword (method / uri / $var(name)).
	var leftStr string
	var leftPV PVRef
	if t.kind == tokIdent {
		lhs := strings.ToLower(t.text)
		switch lhs {
		case "method", "uri":
			leftStr = lhs
			p.advance()
		default:
			if pv := ParsePV(t.text); pv != PVNone {
				leftPV = pv
				p.advance()
			} else if strings.HasPrefix(lhs, "$var(") && strings.HasSuffix(lhs, ")") {
				leftStr = t.text
				p.advance()
			} else {
				return nil, fmt.Errorf("line %d: unexpected expression token %q", t.line, t.text)
			}
		}
	} else {
		return nil, fmt.Errorf("line %d: expected expression, got %q", t.line, t.text)
	}

	// operator
	opTok := p.cur()
	if opTok.kind != tokSymbol || (opTok.text != "==" && opTok.text != "!=") {
		return nil, fmt.Errorf("line %d: expected == or !=, got %q", opTok.line, opTok.text)
	}
	p.advance()

	// right-hand side: a quoted string literal (or bare identifier for
	// convenience — we accept both forms).
	rhsTok := p.cur()
	var rhs string
	switch rhsTok.kind {
	case tokString:
		rhs = rhsTok.text
		p.advance()
	case tokIdent, tokNumber:
		rhs = rhsTok.text
		p.advance()
	default:
		return nil, fmt.Errorf("line %d: expected literal on right-hand side, got %q", rhsTok.line, rhsTok.text)
	}

	return &Expr{
		LeftPV:  leftPV,
		LeftStr: leftStr,
		Op:      opTok.text,
		Right:   rhs,
	}, nil
}

// parseForward handles `forward(uri);` and `forward();`.
func (p *parserState) parseForward() (*Action, error) {
	p.advance() // consume "forward"
	if _, err := p.expect(tokSymbol, "("); err != nil {
		return nil, err
	}
	act := &Action{Type: ActForward}
	t := p.cur()
	if t.kind == tokSymbol && t.text == ")" {
		p.advance()
	} else if t.kind == tokString || t.kind == tokIdent {
		act.Arg = t.text
		p.advance()
		if _, err := p.expect(tokSymbol, ")"); err != nil {
			return nil, err
		}
	} else {
		return nil, fmt.Errorf("line %d: expected string or ')' inside forward(...), got %q", t.line, t.text)
	}
	if _, err := p.maybeSemicolon(); err != nil {
		return nil, err
	}
	return act, nil
}

// parseSendReply handles `sl_send_reply(CODE, "Reason");`.
func (p *parserState) parseSendReply() (*Action, error) {
	p.advance()
	if _, err := p.expect(tokSymbol, "("); err != nil {
		return nil, err
	}
	code, err := p.parseNumeric()
	if err != nil {
		return nil, err
	}
	if _, err := p.expect(tokSymbol, ","); err != nil {
		return nil, err
	}
	reasonTok, err := p.expect(tokString, "")
	if err != nil {
		return nil, err
	}
	if _, err := p.expect(tokSymbol, ")"); err != nil {
		return nil, err
	}
	if _, err := p.maybeSemicolon(); err != nil {
		return nil, err
	}
	return &Action{Type: ActSendReply, ArgNum: code, Arg2: reasonTok.text}, nil
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Script routing engine
 *
 * Executor walks an AST produced by ParseScript, updating a
 * per-request ExecContext with side effects (replies, forwards, flag
 * bits, variables, branches). Proxy.ProcessRequest then reads the
 * ExecContext to decide how to finalize the SIP request.
 */

package script

import (
	"net"
	"strings"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// ExecContext carries mutable per-request state for the script runtime.
// It bridges the script with the proxy core — fields like Reply /
// DstURI / Drop tell ProcessRequest what to do after the script runs.
type ExecContext struct {
	Msg     *parser.SIPMsg
	SrcAddr net.Addr
	Realm   string

	Flags    uint32
	RURI     string
	DstURI   string
	Branches []string

	Reply  *ReplyAction
	Drop   bool
	Return bool

	// $var(name) store — protected by mu.
	mu   *sync.RWMutex
	Vars map[string]string
	Logs []string
}

// ReplyAction records a script's decision to reply to a request with a
// given status / reason / extra headers.
type ReplyAction struct {
	Status  int
	Reason  string
	Headers []string
}

// NewExecContext creates a fresh execution context for a request.
// msg may be nil (tests); otherwise RURI is pre-seeded from the message.
func NewExecContext(msg *parser.SIPMsg, src net.Addr, realm string) *ExecContext {
	ctx := &ExecContext{
		Msg:     msg,
		SrcAddr: src,
		Realm:   realm,
		mu:      &sync.RWMutex{},
		Vars:    make(map[string]string),
	}
	if msg != nil && msg.FirstLine != nil && msg.FirstLine.Req != nil {
		ctx.RURI = msg.FirstLine.Req.URI.String()
	}
	return ctx
}

// Execute runs the Script against ctx.
func (s *Script) Execute(ctx *ExecContext) error {
	if s == nil || ctx == nil {
		return nil
	}
	return s.runBlock(s.Root, ctx)
}

// runBlock executes a slice of actions sequentially. It stops early if
// the script requests a reply, a drop, or a return.
func (s *Script) runBlock(actions []*Action, ctx *ExecContext) error {
	for _, a := range actions {
		if ctx.Reply != nil || ctx.Drop || ctx.Return {
			return nil
		}
		if err := s.runOne(a, ctx); err != nil {
			return err
		}
	}
	return nil
}

// runOne dispatches a single action to its handler.
func (s *Script) runOne(a *Action, ctx *ExecContext) error {
	if a == nil {
		return nil
	}
	switch a.Type {
	case ActForward:
		if a.Arg != "" {
			ctx.DstURI = a.Arg
		}
	case ActSendReply:
		status := a.ArgNum
		if status == 0 {
			status = 200
		}
		reason := a.Arg2
		if reason == "" {
			reason = "OK"
		}
		ctx.Reply = &ReplyAction{Status: status, Reason: reason}
	case ActDrop:
		ctx.Drop = true
	case ActLog:
		ctx.Logs = append(ctx.Logs, a.Arg)
		log.Info("script: " + a.Arg)
	case ActSetFlag:
		ctx.Flags |= 1 << uint(a.ArgNum)
	case ActResetFlag:
		ctx.Flags &= ^(1 << uint(a.ArgNum))
	case ActIf:
		ok, err := s.evalExpr(a.Expr, ctx)
		if err != nil {
			return err
		}
		if ok {
			return s.runBlock(a.IfTrue, ctx)
		}
		return s.runBlock(a.IfFalse, ctx)
	case ActRecordRoute:
		ctx.mu.Lock()
		ctx.Vars["__record_route"] = "1"
		ctx.mu.Unlock()
	case ActAppendBranch:
		ctx.Branches = append(ctx.Branches, a.Arg)
	case ActSetRURI:
		ctx.RURI = a.Arg
	case ActSetDstURI:
		ctx.DstURI = a.Arg
	case ActSetVar:
		ctx.mu.Lock()
		ctx.Vars[a.Arg] = a.Arg2
		ctx.mu.Unlock()
	case ActRoute:
		block, ok := s.Routes[a.RouteName]
		if !ok {
			return nil
		}
		return s.runBlock(block, ctx)
	case ActReturn:
		ctx.Return = true
	}
	return nil
}

// evalExpr evaluates one expression against the context.
func (s *Script) evalExpr(e *Expr, ctx *ExecContext) (bool, error) {
	if e == nil {
		return false, nil
	}
	if e.IsFlag {
		set := (ctx.Flags & (1 << uint(e.FlagN))) != 0
		if e.Negate {
			return !set, nil
		}
		return set, nil
	}
	var lhs string
	switch strings.ToLower(e.LeftStr) {
	case "method":
		if ctx.Msg != nil && ctx.Msg.FirstLine != nil && ctx.Msg.FirstLine.Req != nil {
			lhs = ctx.Msg.FirstLine.Req.Method.String()
		}
	case "uri":
		lhs = ctx.RURI
	default:
		if e.LeftPV != PVNone {
			lhs, _ = resolvePV(e.LeftPV, ctx.Msg, ctx)
		} else if len(e.LeftStr) > 0 {
			lower := strings.ToLower(e.LeftStr)
			if strings.HasPrefix(lower, "$var(") && strings.HasSuffix(lower, ")") {
				name := e.LeftStr[len("$var(") : len(e.LeftStr)-1]
				ctx.mu.RLock()
				lhs = ctx.Vars[name]
				ctx.mu.RUnlock()
			}
		}
	}
	switch e.Op {
	case "==":
		return lhs == e.Right, nil
	case "!=":
		return lhs != e.Right, nil
	}
	return false, nil
}

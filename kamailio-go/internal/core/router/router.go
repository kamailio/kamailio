// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Routing script engine - matching C route.c / route_struct.h
 *
 * The routing engine executes a tree of actions defined in the
 * configuration script. Each action can modify the message,
 * forward it, send replies, or call module functions.
 */

package router

import (
	"context"
	"fmt"
	"strconv"
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/forward"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// Action types - matching C action_type_t
type ActionType int

const (
	ActionForward ActionType = iota
	ActionSend
	ActionDrop
	ActionLog
	ActionAppendBranch
	ActionRemoveBranch
	ActionSetURI
	ActionSetHost
	ActionSetPort
	ActionSetUser
	ActionSetUserPass
	ActionSetMethod
	ActionSetHostPort
	ActionSetHostPortTrans
	ActionSetBranch
	ActionSetFlag
	ActionResetFlag
	ActionIsFlagSet
	ActionSetAVP
	ActionDelAVP
	ActionReturn
	ActionExit
	ActionIf
	ActionSwitch
	ActionWhile
	ActionExec
	ActionModule
	ActionRoute
)

// Action represents a single routing action
// C: struct action
type Action struct {
	Type     ActionType
	Elements []*ActionElem
	Next     *Action
}

// ActionElem represents an action element
// C: struct action_elem
type ActionElem struct {
	Type  ElemType
	Value interface{}
}

// ElemType represents element types
type ElemType int

const (
	ElemNone ElemType = iota
	ElemString
	ElemNumber
	ElemVar
	ElemExpr
	ElemAction
)

// RouteBlock represents a named routing block
// C: struct route_list
type RouteBlock struct {
	Name    string
	ID      int
	Actions *Action
}

// Router represents the routing engine
type Router struct {
	routes     map[string]*RouteBlock
	routeByID  map[int]*RouteBlock
	vars       map[string]interface{}
	flags      uint32
	avps       map[string][]interface{}
	forwarder  *forward.Forwarder // 新增
	defaultDst string             // 默认下一跳 "host:port"
}

// NewRouter creates a new routing engine
func NewRouter() *Router {
	return &Router{
		routes:    make(map[string]*RouteBlock),
		routeByID: make(map[int]*RouteBlock),
		vars:      make(map[string]interface{}),
		avps:      make(map[string][]interface{}),
	}
}

// SetForwarder sets the forwarder for the router
func (r *Router) SetForwarder(f *forward.Forwarder) {
	r.forwarder = f
}

// SetDefaultDestination sets the default destination for forwarding
func (r *Router) SetDefaultDestination(dst string) {
	r.defaultDst = dst
}

// AddRoute adds a routing block
func (r *Router) AddRoute(name string, id int, actions *Action) {
	rb := &RouteBlock{
		Name:    name,
		ID:      id,
		Actions: actions,
	}
	r.routes[name] = rb
	r.routeByID[id] = rb
}

// GetRoute returns a route by name
func (r *Router) GetRoute(name string) *RouteBlock {
	return r.routes[name]
}

// GetRouteByID returns a route by ID
func (r *Router) GetRouteByID(id int) *RouteBlock {
	return r.routeByID[id]
}

// RunResult represents the result of running a route
type RunResult int

const (
	ResultContinue RunResult = iota
	ResultDrop
	ResultReturn
	ResultExit
)

// Run executes a routing block for a message
func (r *Router) Run(ctx context.Context, routeName string, msg *parser.SIPMsg) RunResult {
	route := r.GetRoute(routeName)
	if route == nil {
		return ResultContinue
	}

	return r.runActions(ctx, route.Actions, msg)
}

// RunByID executes a routing block by ID
func (r *Router) RunByID(ctx context.Context, routeID int, msg *parser.SIPMsg) RunResult {
	route := r.GetRouteByID(routeID)
	if route == nil {
		return ResultContinue
	}

	return r.runActions(ctx, route.Actions, msg)
}

// runActions executes a chain of actions
func (r *Router) runActions(ctx context.Context, actions *Action, msg *parser.SIPMsg) RunResult {
	for act := actions; act != nil; act = act.Next {
		result := r.runAction(ctx, act, msg)
		if result != ResultContinue {
			return result
		}
	}
	return ResultContinue
}

// runAction executes a single action
func (r *Router) runAction(ctx context.Context, act *Action, msg *parser.SIPMsg) RunResult {
	switch act.Type {
	case ActionDrop:
		return ResultDrop

	case ActionReturn:
		return ResultReturn

	case ActionExit:
		return ResultExit

	case ActionIf:
		return r.runIf(ctx, act, msg)

	case ActionRoute:
		return r.runRouteCall(ctx, act, msg)

	case ActionSetFlag:
		r.setFlag(act, msg)

	case ActionResetFlag:
		r.resetFlag(act, msg)

	case ActionLog:
		r.logMessage(act, msg)

	case ActionForward:
		r.runActionForward(act, msg)

	case ActionSend:
		r.runActionSend(act, msg)
	}

	return ResultContinue
}

// runIf executes an if action
func (r *Router) runIf(ctx context.Context, act *Action, msg *parser.SIPMsg) RunResult {
	if len(act.Elements) < 2 {
		return ResultContinue
	}

	// Evaluate condition
	cond := act.Elements[0]
	if cond.Type != ElemExpr {
		return ResultContinue
	}

	expr, ok := cond.Value.(*Expression)
	if !ok {
		return ResultContinue
	}

	if r.evalExpr(expr, msg) {
		// Execute true branch
		trueBranch := act.Elements[1]
		if trueBranch.Type == ElemAction {
			if actions, ok := trueBranch.Value.(*Action); ok {
				return r.runActions(ctx, actions, msg)
			}
		}
	} else if len(act.Elements) > 2 {
		// Execute false branch
		falseBranch := act.Elements[2]
		if falseBranch.Type == ElemAction {
			if actions, ok := falseBranch.Value.(*Action); ok {
				return r.runActions(ctx, actions, msg)
			}
		}
	}

	return ResultContinue
}

// runRouteCall executes a route() call
func (r *Router) runRouteCall(ctx context.Context, act *Action, msg *parser.SIPMsg) RunResult {
	if len(act.Elements) == 0 {
		return ResultContinue
	}

	// Get route name or ID
	elem := act.Elements[0]
	var routeName string
	var routeID int

	switch elem.Type {
	case ElemString:
		routeName = elem.Value.(string)
	case ElemNumber:
		routeID = int(elem.Value.(int64))
	}

	var result RunResult
	if routeName != "" {
		result = r.Run(ctx, routeName, msg)
	} else {
		result = r.RunByID(ctx, routeID, msg)
	}

	// Only propagate exit, not return
	if result == ResultExit {
		return ResultExit
	}
	return ResultContinue
}

// setFlag sets a message flag
func (r *Router) setFlag(act *Action, msg *parser.SIPMsg) {
	if len(act.Elements) == 0 {
		return
	}

	flag, ok := act.Elements[0].Value.(int64)
	if !ok {
		return
	}

	msg.Flags |= uint32(1 << flag)
}

// resetFlag resets a message flag
func (r *Router) resetFlag(act *Action, msg *parser.SIPMsg) {
	if len(act.Elements) == 0 {
		return
	}

	flag, ok := act.Elements[0].Value.(int64)
	if !ok {
		return
	}

	msg.Flags &^= uint32(1 << flag)
}

// runActionForward executes ActionForward
func (r *Router) runActionForward(act *Action, msg *parser.SIPMsg) RunResult {
	if r.forwarder == nil {
		return ResultContinue
	}

	var nextHopURI string
	if len(act.Elements) > 0 {
		nextHopURI, _ = act.Elements[0].Value.(string)
	}
	if nextHopURI == "" {
		nextHopURI = r.defaultDst
	}

	if nextHopURI == "" {
		return ResultContinue
	}

	_ = r.forwarder.ForwardRequest(msg, nextHopURI, nil)
	return ResultContinue
}

// runActionSend executes ActionSend
func (r *Router) runActionSend(act *Action, msg *parser.SIPMsg) RunResult {
	if r.forwarder == nil {
		return ResultContinue
	}

	if len(act.Elements) == 0 {
		return ResultContinue
	}

	dst, _ := act.Elements[0].Value.(string)
	if dst == "" {
		return ResultContinue
	}

	host, port := parseHostPortForSend(dst)

	data, err := parser.BuildMessage(msg)
	if err != nil {
		return ResultContinue
	}

	_ = r.forwarder.SendToUDP(host, uint16(port), data)
	return ResultContinue
}

// parseHostPortForSend parses "host:port" or "host" for send action
func parseHostPortForSend(s string) (string, int) {
	s = strings.TrimSpace(s)
	if strings.HasPrefix(s, "[") {
		end := strings.Index(s, "]")
		if end == -1 {
			return s, 5060
		}
		host := s[1:end]
		rest := s[end+1:]
		if strings.HasPrefix(rest, ":") {
			if p, err := strconv.Atoi(rest[1:]); err == nil {
				return host, p
			}
		}
		return host, 5060
	}
	if colon := strings.LastIndex(s, ":"); colon != -1 {
		host := s[:colon]
		portStr := s[colon+1:]
		if p, err := strconv.Atoi(portStr); err == nil {
			return host, p
		}
	}
	return s, 5060
}

// logMessage logs a message
func (r *Router) logMessage(act *Action, msg *parser.SIPMsg) {
	if len(act.Elements) == 0 {
		return
	}

	level, ok := act.Elements[0].Value.(int64)
	if !ok {
		return
	}

	var message string
	if len(act.Elements) > 1 {
		message, _ = act.Elements[1].Value.(string)
	}

	// Simple logging
	fmt.Printf("[%d] %s\n", level, message)
}

// Expression types
type ExprType int

const (
	ExprNone ExprType = iota
	ExprString
	ExprNumber
	ExprVar
	ExprMethod
	ExprURI
	ExprSrcIP
	ExprDstIP
	ExprSrcPort
	ExprDstPort
	ExprProto
	ExprAF
	ExprMsgLen
	ExprAnd
	ExprOr
	ExprNot
	ExprEqual
	ExprNotEqual
	ExprMatch
	ExprLT
	ExprGT
	ExprLTE
	ExprGTE
	ExprPlus
	ExprMinus
	ExprMul
	ExprDiv
	ExprMod
	ExprBAnd
	ExprBOr
	ExprBXor
	ExprBNot
	ExprRShift
	ExprLShift
)

// Expression represents an expression
// C: struct expr
type Expression struct {
	Type     ExprType
	Left     *Expression
	Right    *Expression
	Operand  interface{}
	Operands []*Expression
}

// evalExpr evaluates an expression
func (r *Router) evalExpr(expr *Expression, msg *parser.SIPMsg) bool {
	if expr == nil {
		return false
	}

	switch expr.Type {
	case ExprAnd:
		return r.evalExpr(expr.Left, msg) && r.evalExpr(expr.Right, msg)

	case ExprOr:
		return r.evalExpr(expr.Left, msg) || r.evalExpr(expr.Right, msg)

	case ExprNot:
		return !r.evalExpr(expr.Left, msg)

	case ExprEqual:
		left := r.evalValue(expr.Left, msg)
		right := r.evalValue(expr.Right, msg)
		return left == right

	case ExprNotEqual:
		left := r.evalValue(expr.Left, msg)
		right := r.evalValue(expr.Right, msg)
		return left != right

	case ExprMethod:
		if msg == nil || msg.FirstLine == nil {
			return false
		}
		method := msg.Method()
		if expr.Operand != nil {
			return method == expr.Operand.(parser.RequestMethod)
		}
		return method != parser.MethodUndefined

	case ExprProto:
		if msg == nil || msg.Via1 == nil {
			return false
		}
		proto := msg.Via1.Proto
		if expr.Operand != nil {
			return proto == expr.Operand.(int16)
		}
		return true

	case ExprSrcIP:
		// Source IP check: compare against the first Via header's received
		// parameter (if present) or the host part of the sent-by.
		if msg == nil || msg.Via1 == nil {
			return false
		}
		srcIP := ""
		if msg.Via1.Received != nil && msg.Via1.Received.Value.Len > 0 {
			srcIP = msg.Via1.Received.Value.String()
		} else if msg.Via1.Host.Len > 0 {
			srcIP = msg.Via1.Host.String()
		}
		if expr.Operand != nil {
			return srcIP == expr.Operand.(string)
		}
		return srcIP != ""

	case ExprString:
		return expr.Operand != nil && expr.Operand.(string) != ""
	}

	return false
}

// evalValue evaluates an expression to a value
func (r *Router) evalValue(expr *Expression, msg *parser.SIPMsg) interface{} {
	if expr == nil {
		return nil
	}

	switch expr.Type {
	case ExprString:
		return expr.Operand

	case ExprNumber:
		return expr.Operand

	case ExprVar:
		if name, ok := expr.Operand.(string); ok {
			return r.vars[name]
		}

	case ExprMethod:
		if msg != nil {
			return msg.Method()
		}
	}

	return nil
}

// ScriptBuilder helps build routing scripts programmatically
type ScriptBuilder struct {
	router *Router
}

// NewScriptBuilder creates a new script builder
func NewScriptBuilder() *ScriptBuilder {
	return &ScriptBuilder{
		router: NewRouter(),
	}
}

// Route starts a new route block
func (b *ScriptBuilder) Route(name string) *RouteBuilder {
	return &RouteBuilder{
		builder: b,
		name:    name,
	}
}

// Build returns the built router
func (b *ScriptBuilder) Build() *Router {
	return b.router
}

// RouteBuilder helps build a single route
type RouteBuilder struct {
	builder   *ScriptBuilder
	name      string
	actions   *Action
	last      *Action
}

// Drop adds a drop action
func (rb *RouteBuilder) Drop() *RouteBuilder {
	rb.addAction(&Action{Type: ActionDrop})
	return rb
}

// Return adds a return action
func (rb *RouteBuilder) Return() *RouteBuilder {
	rb.addAction(&Action{Type: ActionReturn})
	return rb
}

// Exit adds an exit action
func (rb *RouteBuilder) Exit() *RouteBuilder {
	rb.addAction(&Action{Type: ActionExit})
	return rb
}

// If adds an if action
func (rb *RouteBuilder) If(cond *Expression) *IfBuilder {
	return &IfBuilder{
		routeBuilder: rb,
		condition:    cond,
	}
}

// Route calls another route
func (rb *RouteBuilder) Route(name string) *RouteBuilder {
	rb.addAction(&Action{
		Type: ActionRoute,
		Elements: []*ActionElem{
			{Type: ElemString, Value: name},
		},
	})
	return rb
}

// Log adds a log action
func (rb *RouteBuilder) Log(level int, message string) *RouteBuilder {
	rb.addAction(&Action{
		Type: ActionLog,
		Elements: []*ActionElem{
			{Type: ElemNumber, Value: int64(level)},
			{Type: ElemString, Value: message},
		},
	})
	return rb
}

// Forward adds a forward action
func (rb *RouteBuilder) Forward(dst string) *RouteBuilder {
	rb.addAction(&Action{
		Type: ActionForward,
		Elements: []*ActionElem{
			{Type: ElemString, Value: dst},
		},
	})
	return rb
}

// Send adds a send action
func (rb *RouteBuilder) Send(dst string) *RouteBuilder {
	rb.addAction(&Action{
		Type: ActionSend,
		Elements: []*ActionElem{
			{Type: ElemString, Value: dst},
		},
	})
	return rb
}

// SetFlag sets a message flag
func (rb *RouteBuilder) SetFlag(flag int) *RouteBuilder {
	rb.addAction(&Action{
		Type: ActionSetFlag,
		Elements: []*ActionElem{
			{Type: ElemNumber, Value: int64(flag)},
		},
	})
	return rb
}

// EndRoute finishes the route block
func (rb *RouteBuilder) EndRoute() *ScriptBuilder {
	rb.builder.router.AddRoute(rb.name, len(rb.builder.router.routes), rb.actions)
	return rb.builder
}

// addAction adds an action to the chain
func (rb *RouteBuilder) addAction(act *Action) {
	if rb.actions == nil {
		rb.actions = act
		rb.last = act
	} else {
		rb.last.Next = act
		rb.last = act
	}
}

// IfBuilder helps build an if statement
type IfBuilder struct {
	routeBuilder *RouteBuilder
	condition    *Expression
	trueActions  *Action
	falseActions *Action
}

// Then specifies the true branch
func (ib *IfBuilder) Then(actions *Action) *IfBuilder {
	ib.trueActions = actions
	return ib
}

// Else specifies the false branch
func (ib *IfBuilder) Else(actions *Action) *IfBuilder {
	ib.falseActions = actions
	return ib
}

// EndIf finishes the if statement
func (ib *IfBuilder) EndIf() *RouteBuilder {
	act := &Action{
		Type: ActionIf,
		Elements: []*ActionElem{
			{Type: ElemExpr, Value: ib.condition},
		},
	}

	if ib.trueActions != nil {
		act.Elements = append(act.Elements, &ActionElem{
			Type: ElemAction, Value: ib.trueActions,
		})
	}

	if ib.falseActions != nil {
		act.Elements = append(act.Elements, &ActionElem{
			Type: ElemAction, Value: ib.falseActions,
		})
	}

	ib.routeBuilder.addAction(act)
	return ib.routeBuilder
}

// Expr helpers

// Method creates a method expression
func Method(m parser.RequestMethod) *Expression {
	return &Expression{
		Type:    ExprMethod,
		Operand: m,
	}
}

// Equals creates an equality expression
func Equals(left, right *Expression) *Expression {
	return &Expression{
		Type:  ExprEqual,
		Left:  left,
		Right: right,
	}
}

// String creates a string expression
func String(s string) *Expression {
	return &Expression{
		Type:    ExprString,
		Operand: s,
	}
}

// Number creates a number expression
func Number(n int64) *Expression {
	return &Expression{
		Type:    ExprNumber,
		Operand: n,
	}
}

// And creates an AND expression
func And(left, right *Expression) *Expression {
	return &Expression{
		Type:  ExprAnd,
		Left:  left,
		Right: right,
	}
}

// Or creates an OR expression
func Or(left, right *Expression) *Expression {
	return &Expression{
		Type:  ExprOr,
		Left:  left,
		Right: right,
	}
}

// Not creates a NOT expression
func Not(expr *Expression) *Expression {
	return &Expression{
		Type:  ExprNot,
		Left:  expr,
	}
}

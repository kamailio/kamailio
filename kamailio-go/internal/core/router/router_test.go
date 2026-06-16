// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Router engine tests
 */

package router

import (
	"context"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

func TestRouterAddRoute(t *testing.T) {
	r := NewRouter()

	// Add a simple route
	actions := &Action{Type: ActionDrop}
	r.AddRoute("test", 1, actions)

	// Verify route was added
	route := r.GetRoute("test")
	if route == nil {
		t.Fatal("Expected route to be added")
	}

	if route.Name != "test" {
		t.Errorf("Expected route name 'test', got %q", route.Name)
	}

	if route.ID != 1 {
		t.Errorf("Expected route ID 1, got %d", route.ID)
	}

	// Verify lookup by ID
	routeByID := r.GetRouteByID(1)
	if routeByID == nil {
		t.Fatal("Expected route to be found by ID")
	}

	if routeByID.Name != "test" {
		t.Errorf("Expected route name 'test', got %q", routeByID.Name)
	}
}

func TestRouterRunDrop(t *testing.T) {
	r := NewRouter()

	// Add a route that drops
	actions := &Action{Type: ActionDrop}
	r.AddRoute("drop_route", 1, actions)

	// Run the route
	msg := &parser.SIPMsg{}
	result := r.Run(context.Background(), "drop_route", msg)

	if result != ResultDrop {
		t.Errorf("Expected ResultDrop, got %v", result)
	}
}

func TestRouterRunReturn(t *testing.T) {
	r := NewRouter()

	// Add a route that returns
	actions := &Action{Type: ActionReturn}
	r.AddRoute("return_route", 1, actions)

	// Run the route
	msg := &parser.SIPMsg{}
	result := r.Run(context.Background(), "return_route", msg)

	if result != ResultReturn {
		t.Errorf("Expected ResultReturn, got %v", result)
	}
}

func TestRouterRunExit(t *testing.T) {
	r := NewRouter()

	// Add a route that exits
	actions := &Action{Type: ActionExit}
	r.AddRoute("exit_route", 1, actions)

	// Run the route
	msg := &parser.SIPMsg{}
	result := r.Run(context.Background(), "exit_route", msg)

	if result != ResultExit {
		t.Errorf("Expected ResultExit, got %v", result)
	}
}

func TestRouterRunContinue(t *testing.T) {
	r := NewRouter()

	// Add a route that continues (no terminal action)
	actions := &Action{Type: ActionLog}
	r.AddRoute("continue_route", 1, actions)

	// Run the route
	msg := &parser.SIPMsg{}
	result := r.Run(context.Background(), "continue_route", msg)

	if result != ResultContinue {
		t.Errorf("Expected ResultContinue, got %v", result)
	}
}

func TestRouterRunNotFound(t *testing.T) {
	r := NewRouter()

	// Run a non-existent route
	msg := &parser.SIPMsg{}
	result := r.Run(context.Background(), "nonexistent", msg)

	if result != ResultContinue {
		t.Errorf("Expected ResultContinue for non-existent route, got %v", result)
	}
}

func TestRouterSetFlag(t *testing.T) {
	r := NewRouter()

	// Add a route that sets a flag
	actions := &Action{
		Type: ActionSetFlag,
		Elements: []*ActionElem{
			{Type: ElemNumber, Value: int64(0)},
		},
	}
	r.AddRoute("set_flag", 1, actions)

	// Run the route
	msg := &parser.SIPMsg{}
	r.Run(context.Background(), "set_flag", msg)

	// Check flag was set
	if msg.Flags&1 == 0 {
		t.Error("Expected flag 0 to be set")
	}
}

func TestRouterResetFlag(t *testing.T) {
	r := NewRouter()

	// Add a route that resets a flag
	actions := &Action{
		Type: ActionResetFlag,
		Elements: []*ActionElem{
			{Type: ElemNumber, Value: int64(0)},
		},
	}
	r.AddRoute("reset_flag", 1, actions)

	// Run the route with flag already set
	msg := &parser.SIPMsg{Flags: 1}
	r.Run(context.Background(), "reset_flag", msg)

	// Check flag was reset
	if msg.Flags&1 != 0 {
		t.Error("Expected flag 0 to be reset")
	}
}

func TestRouterRunByID(t *testing.T) {
	r := NewRouter()

	// Add a route
	actions := &Action{Type: ActionDrop}
	r.AddRoute("test", 42, actions)

	// Run by ID
	msg := &parser.SIPMsg{}
	result := r.RunByID(context.Background(), 42, msg)

	if result != ResultDrop {
		t.Errorf("Expected ResultDrop, got %v", result)
	}
}

func TestRouterActionChain(t *testing.T) {
	r := NewRouter()

	// Create a chain of actions
	act1 := &Action{Type: ActionLog}
	act2 := &Action{Type: ActionLog}
	act3 := &Action{Type: ActionDrop}
	act1.Next = act2
	act2.Next = act3

	r.AddRoute("chain", 1, act1)

	// Run the route
	msg := &parser.SIPMsg{}
	result := r.Run(context.Background(), "chain", msg)

	if result != ResultDrop {
		t.Errorf("Expected ResultDrop, got %v", result)
	}
}

func TestScriptBuilder(t *testing.T) {
	// Build a simple script
	router := NewScriptBuilder().
		Route("main").
			Log(1, "Processing request").
			Drop().
			EndRoute().
		Build()

	// Verify route was added
	route := router.GetRoute("main")
	if route == nil {
		t.Fatal("Expected route to be added")
	}

	// Run the route
	msg := &parser.SIPMsg{}
	result := router.Run(context.Background(), "main", msg)

	if result != ResultDrop {
		t.Errorf("Expected ResultDrop, got %v", result)
	}
}

func TestScriptBuilderWithRouteCall(t *testing.T) {
	// Build a script with route calls
	router := NewScriptBuilder().
		Route("sub").
			Return().
			EndRoute().
		Route("main").
			Route("sub").
			Drop().
			EndRoute().
		Build()

	// Run the main route
	msg := &parser.SIPMsg{}
	result := router.Run(context.Background(), "main", msg)

	if result != ResultDrop {
		t.Errorf("Expected ResultDrop, got %v", result)
	}
}

func TestExpressionMethod(t *testing.T) {
	r := NewRouter()

	// Create expression for INVITE method
	expr := Method(parser.MethodInvite)

	// Test with INVITE message
	msg := &parser.SIPMsg{
		FirstLine: &parser.MsgStart{
			Req: &parser.RequestLine{
				MethodValue: parser.MethodInvite,
			},
		},
	}

	if !r.evalExpr(expr, msg) {
		t.Error("Expected expression to be true for INVITE")
	}

	// Test with non-INVITE message
	msg2 := &parser.SIPMsg{
		FirstLine: &parser.MsgStart{
			Req: &parser.RequestLine{
				MethodValue: parser.MethodBye,
			},
		},
	}

	if r.evalExpr(expr, msg2) {
		t.Error("Expected expression to be false for BYE")
	}
}

func TestExpressionEquals(t *testing.T) {
	r := NewRouter()

	// Create equality expression
	expr := Equals(String("test"), String("test"))

	if !r.evalExpr(expr, nil) {
		t.Error("Expected 'test' == 'test' to be true")
	}

	// Test inequality
	expr2 := Equals(String("test"), String("other"))

	if r.evalExpr(expr2, nil) {
		t.Error("Expected 'test' == 'other' to be false")
	}
}

func TestExpressionAnd(t *testing.T) {
	r := NewRouter()

	// Create AND expression
	expr := And(
		Equals(String("a"), String("a")),
		Equals(String("b"), String("b")),
	)

	if !r.evalExpr(expr, nil) {
		t.Error("Expected true AND true to be true")
	}

	// Test with false
	expr2 := And(
		Equals(String("a"), String("a")),
		Equals(String("b"), String("c")),
	)

	if r.evalExpr(expr2, nil) {
		t.Error("Expected true AND false to be false")
	}
}

func TestExpressionOr(t *testing.T) {
	r := NewRouter()

	// Create OR expression
	expr := Or(
		Equals(String("a"), String("b")),
		Equals(String("c"), String("c")),
	)

	if !r.evalExpr(expr, nil) {
		t.Error("Expected false OR true to be true")
	}

	// Test both false
	expr2 := Or(
		Equals(String("a"), String("b")),
		Equals(String("c"), String("d")),
	)

	if r.evalExpr(expr2, nil) {
		t.Error("Expected false OR false to be false")
	}
}

func TestExpressionNot(t *testing.T) {
	r := NewRouter()

	// Create NOT expression
	expr := Not(Equals(String("a"), String("b")))

	if !r.evalExpr(expr, nil) {
		t.Error("Expected NOT false to be true")
	}

	// Test NOT true
	expr2 := Not(Equals(String("a"), String("a")))

	if r.evalExpr(expr2, nil) {
		t.Error("Expected NOT true to be false")
	}
}

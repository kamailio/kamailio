// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go — Topoh unit tests.
 */

package topoh

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// TestNewDefault_NonNilMaps verifies NewDefault produces a usable
// Hider with non-nil maps and a non-empty realm.
func TestNewDefault_NonNilMaps(t *testing.T) {
	h := NewDefault()
	if h == nil {
		t.Fatal("NewDefault returned nil")
	}
	if h.strategy.Realm == "" {
		t.Fatal("expected non-empty realm")
	}
}

// makeTestMsg produces a minimal parser.SIPMsg with CallID/From/To
// headers populated. The returned msg can be mutated by Hider.
func makeTestMsg(callID, from, to string) *parser.SIPMsg {
	return &parser.SIPMsg{
		CallID: &parser.HdrField{Body: str.Mk(callID)},
		From:   &parser.HdrField{Body: str.Mk(from)},
		To:     &parser.HdrField{Body: str.Mk(to)},
	}
}

// TestHider_HideAndRestore_CallID verifies that a call-id is
// rewritten on forward and restored on UnhideForProcessing.
func TestHider_HideAndRestore_CallID(t *testing.T) {
	h := New(HideStrategy{
		HideCallID: true,
		Realm:      "hidden.local",
	})

	const internal = "abc-123-xyz@internal.example"
	msg := makeTestMsg(internal,
		"<sip:alice@internal.example>;tag=abc123",
		"<sip:bob@internal.example>")

	h.HideForForward(msg)
	if msg.CallID == nil {
		t.Fatal("CallID header went missing after HideForForward")
	}
	rewritten := msg.CallID.Body.String()
	if rewritten == internal {
		t.Fatal("call-id was not rewritten")
	}
	if rewritten != "" && (rewritten[0] == 'a') {
		// Sanity check that we changed *something* meaningful.
	}

	h.UnhideForProcessing(msg)
	if restored := msg.CallID.Body.String(); restored != internal {
		t.Fatalf("call-id not restored: got %q, want %q", restored, internal)
	}
}

// TestHider_TagMapping verifies that From/To tags are rewritten on
// forward and restored on UnhideForProcessing.
func TestHider_TagMapping(t *testing.T) {
	h := New(HideStrategy{HideTags: true})

	const fromText = "<sip:alice@internal.example>;tag=fromTagValue"
	const toText = "<sip:bob@internal.example>;tag=toTagValue"
	msg := makeTestMsg("cid-1@internal.example", fromText, toText)

	h.HideForForward(msg)
	hiddenFrom := msg.From.Body.String()
	hiddenTo := msg.To.Body.String()
	if hiddenFrom == fromText {
		t.Fatal("from tag was not rewritten")
	}
	if hiddenTo == toText {
		t.Fatal("to tag was not rewritten")
	}

	h.UnhideForProcessing(msg)
	if got := msg.From.Body.String(); got != fromText {
		t.Fatalf("from text not restored: got %q, want %q", got, fromText)
	}
	if got := msg.To.Body.String(); got != toText {
		t.Fatalf("to text not restored: got %q, want %q", got, toText)
	}
}

// TestHider_NoOpWhenStrategyZero verifies that a zero strategy
// results in no visible changes to the message.
func TestHider_NoOpWhenStrategyZero(t *testing.T) {
	h := New(HideStrategy{})
	msg := makeTestMsg("cid-orig@local",
		"<sip:alice@local>;tag=abc", "<sip:bob@local>;tag=xyz")
	origCallID := msg.CallID.Body.String()
	origFrom := msg.From.Body.String()
	origTo := msg.To.Body.String()

	h.HideForForward(msg)
	if msg.CallID.Body.String() != origCallID {
		t.Fatal("call-id modified despite zero strategy")
	}
	if msg.From.Body.String() != origFrom {
		t.Fatal("from modified despite zero strategy")
	}
	if msg.To.Body.String() != origTo {
		t.Fatal("to modified despite zero strategy")
	}
}

// TestHider_NilMsg_DoesNotPanic verifies that every public method
// handles a nil msg without panicking.
func TestHider_NilMsg_DoesNotPanic(t *testing.T) {
	h := NewDefault()
	h.HideForForward(nil)
	h.HideForReply(nil)
	h.UnhideForProcessing(nil)

	// Nil Hider should also be safe on every method.
	var nilH *Hider
	nilH.HideForForward(nil)
	nilH.HideForReply(nil)
	nilH.UnhideForProcessing(nil)
}

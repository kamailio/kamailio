package main

import (
	"fmt"
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

func TestDebug_ACKDialog3(t *testing.T) {
	s := newTestServer()

	invite, err := parser.ParseMsg(sampleINVITE())
	if err != nil {
		t.Fatal(err)
	}

	d, err := dialog.CreateUASDialog(invite, "<sip:proxy@example.com>")
	if err != nil {
		t.Fatal(err)
	}
	if err := s.dialogs.Add(d); err != nil {
		t.Fatal(err)
	}
	d.Confirm()

	fmt.Printf("stored: Local=%q Remote=%q CallID=%q State=%v\n",
		d.LocalTag, d.RemoteTag, d.CallID, d.State)

	ack, err := parser.ParseMsg(sampleACK(d.LocalTag))
	if err != nil {
		t.Fatal(err)
	}
	s.handleRequest(ack, fakeSrcAddr())

	fromTag := ""
	if invite.From != nil {
		fromTag = extractTagFrom(invite.From.Body.String())
	}
	got := s.dialogs.Lookup(d.CallID, fromTag, d.LocalTag)
	if got == nil {
		fmt.Printf("NOT FOUND\n")
		t.Fatal("missing")
	}
	fmt.Printf("after ACK: State=%v IsConfirmed=%v IsTerminated=%v\n",
		got.State, got.IsConfirmed(), got.IsTerminated())
}

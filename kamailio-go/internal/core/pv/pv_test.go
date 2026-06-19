// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Pseudo Variables system tests
 */

package pv

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

func init() {
	InitCorePVs()
}

func TestPV_GetRU(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-pv@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, err := parser.ParseMsg(raw)
	if err != nil {
		t.Fatalf("ParseMsg: %v", err)
	}

	ctx := NewPVContext(msg)
	val := GlobalRegistry().Get("ru", ctx)
	if !val.OK {
		t.Fatal("expected $ru to be resolved")
	}
	if val.Str != "sip:bob@example.com" {
		t.Errorf("$ru = %q, want sip:bob@example.com", val.Str)
	}
}

func TestPV_GetRD(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com:5060 SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-rd@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("rd", ctx)
	if !val.OK {
		t.Fatal("expected $rd to be resolved")
	}
	if val.Str != "example.com" {
		t.Errorf("$rd = %q, want example.com", val.Str)
	}
}

func TestPV_GetCI(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-ci@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("ci", ctx)
	if !val.OK {
		t.Fatal("expected $ci to be resolved")
	}
	if val.Str != "test-ci@example.com" {
		t.Errorf("$ci = %q", val.Str)
	}
}

func TestPV_GetRM(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-rm@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("rm", ctx)
	if !val.OK || val.Str != "INVITE" {
		t.Errorf("$rm = %q, want INVITE", val.Str)
	}
}

func TestPV_GetFU(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-fu@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("fU", ctx)
	if !val.OK || val.Str != "alice" {
		t.Errorf("$fU = %q, want alice", val.Str)
	}
}

func TestPV_GetFD(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-fd@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("fD", ctx)
	if !val.OK || val.Str != "example.com" {
		t.Errorf("$fD = %q, want example.com", val.Str)
	}
}

func TestPV_GetFT(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=myfromtag\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-ft@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("ft", ctx)
	if !val.OK || val.Str != "myfromtag" {
		t.Errorf("$ft = %q, want myfromtag", val.Str)
	}
}

func TestPV_GetCS(t *testing.T) {
	raw := []byte("INVITE sip:bob@example.com SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-cs@example.com\r\n" +
		"CSeq: 42 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	val := GlobalRegistry().Get("cs", ctx)
	if !val.OK || val.Int != 42 {
		t.Errorf("$cs = %d, want 42", val.Int)
	}
}

func TestPV_SetDU(t *testing.T) {
	ctx := NewPVContext(nil)
	err := GlobalRegistry().Set("du", ctx, PVValue{Str: "sip:proxy.example.com"})
	if err != nil {
		t.Fatalf("Set $du: %v", err)
	}
	val := GlobalRegistry().Get("du", ctx)
	if !val.OK || val.Str != "sip:proxy.example.com" {
		t.Errorf("$du = %q", val.Str)
	}
}

func TestTransform_Len(t *testing.T) {
	result, err := ApplyTransformation("hello", "s.len")
	if err != nil {
		t.Fatalf("s.len: %v", err)
	}
	if result != "5" {
		t.Errorf("s.len = %q, want 5", result)
	}
}

func TestTransform_Trim(t *testing.T) {
	result, err := ApplyTransformation("  hello  ", "s.trim")
	if err != nil {
		t.Fatalf("s.trim: %v", err)
	}
	if result != "hello" {
		t.Errorf("s.trim = %q, want hello", result)
	}
}

func TestTransform_Substr(t *testing.T) {
	result, err := ApplyTransformation("hello world", "s.substr.0.5")
	if err != nil {
		t.Fatalf("s.substr: %v", err)
	}
	if result != "hello" {
		t.Errorf("s.substr = %q, want hello", result)
	}
}

func TestTransform_Tolower(t *testing.T) {
	result, err := ApplyTransformation("HELLO", "s.tolower")
	if err != nil {
		t.Fatalf("s.tolower: %v", err)
	}
	if result != "hello" {
		t.Errorf("s.tolower = %q, want hello", result)
	}
}

func TestTransform_MD5(t *testing.T) {
	result, err := ApplyTransformation("test", "s.md5")
	if err != nil {
		t.Fatalf("s.md5: %v", err)
	}
	if len(result) != 32 {
		t.Errorf("md5 length = %d, want 32", len(result))
	}
}

func TestTransform_HexEncode(t *testing.T) {
	result, err := ApplyTransformation("AB", "s.encode.hexa")
	if err != nil {
		t.Fatalf("s.encode.hexa: %v", err)
	}
	if result != "4142" {
		t.Errorf("s.encode.hexa = %q, want 4142", result)
	}
}

func TestParseTransformations(t *testing.T) {
	name, transforms := ParseTransformations("$ru{s.tolower}{s.substr.0.3}")
	if name != "ru" {
		t.Errorf("name = %q, want ru", name)
	}
	if len(transforms) != 2 {
		t.Fatalf("expected 2 transforms, got %d", len(transforms))
	}
	if transforms[0] != "s.tolower" {
		t.Errorf("transform[0] = %q", transforms[0])
	}
}

func TestPV_Resolve(t *testing.T) {
	raw := []byte("INVITE sip:BOB@Example.COM SIP/2.0\r\n" +
		"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776\r\n" +
		"From: Alice <sip:alice@example.com>;tag=abc\r\n" +
		"To: Bob <sip:bob@example.com>\r\n" +
		"Call-ID: test-resolve@example.com\r\n" +
		"CSeq: 1 INVITE\r\n" +
		"Content-Length: 0\r\n" +
		"\r\n")
	msg, _ := parser.ParseMsg(raw)
	ctx := NewPVContext(msg)

	result, err := Resolve("$rU{s.tolower}", ctx)
	if err != nil {
		t.Fatalf("Resolve: %v", err)
	}
	if result != "bob" {
		t.Errorf("resolved = %q, want bob", result)
	}
}

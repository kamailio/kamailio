// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Parser tests
 */

package parser

import (
	"testing"
)

// Test SIP INVITE request
var testInvite = []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"Max-Forwards: 70\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314159 INVITE\r\n" +
	"Contact: <sip:alice@pc33.example.com>\r\n" +
	"Content-Type: application/sdp\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

// Test SIP 200 OK response
var test200OK = []byte("SIP/2.0 200 OK\r\n" +
	"Via: SIP/2.0/UDP pc33.example.com;branch=z9hG4bK776asdhds\r\n" +
	"From: Alice <sip:alice@example.com>;tag=1928301774\r\n" +
	"To: Bob <sip:bob@example.com>;tag=a6c85cf\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 314159 INVITE\r\n" +
	"Contact: <sip:bob@192.0.2.4>\r\n" +
	"Content-Type: application/sdp\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

// Test IMS REGISTER request
var testIMSRegister = []byte("REGISTER sip:ims.mnc001.mcc460.gprs SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bK776asdhds\r\n" +
	"Max-Forwards: 70\r\n" +
	"From: <sip:460000123456789@ims.mnc001.mcc460.gprs>;tag=1928301774\r\n" +
	"To: <sip:460000123456789@ims.mnc001.mcc460.gprs>\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 1 REGISTER\r\n" +
	"Contact: <sip:460000123456789@[2001:db8::1]:5060;ob>;+sip.instance=\"<urn:uuid:...>\"\r\n" +
	"Expires: 600000\r\n" +
	"Require: path, outbound\r\n" +
	"Supported: path, outbound, gruu\r\n" +
	"P-Access-Network-Info: 3GPP-UTRAN-TDD;utran-cell-id-3gpp=4600001234ABCD\r\n" +
	"P-Visited-Network-ID: \"vplmn.ims.mnc001.mcc460.gprs\"\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

func TestParseFirstLineRequest(t *testing.T) {
	fl, remaining, err := ParseFirstLine(testInvite)
	if err != nil {
		t.Fatalf("parse first line error: %v", err)
	}

	if fl.Type != MsgRequest {
		t.Errorf("expected MsgRequest, got %d", fl.Type)
	}
	if !fl.IsRequest() {
		t.Error("expected IsRequest() to be true")
	}
	if fl.Req == nil {
		t.Fatal("expected request line to be set")
	}
	if fl.Req.MethodValue != MethodInvite {
		t.Errorf("expected INVITE method, got %s", fl.Req.Method.String())
	}
	if fl.Req.URI.String() != "sip:user@example.com" {
		t.Errorf("expected URI 'sip:user@example.com', got '%s'", fl.Req.URI.String())
	}
	if fl.Req.Version.String() != "SIP/2.0" {
		t.Errorf("expected version 'SIP/2.0', got '%s'", fl.Req.Version.String())
	}
	if len(remaining) == 0 {
		t.Error("expected remaining buffer to not be empty")
	}
}

func TestParseFirstLineReply(t *testing.T) {
	fl, remaining, err := ParseFirstLine(test200OK)
	if err != nil {
		t.Fatalf("parse first line error: %v", err)
	}

	if fl.Type != MsgReply {
		t.Errorf("expected MsgReply, got %d", fl.Type)
	}
	if !fl.IsReply() {
		t.Error("expected IsReply() to be true")
	}
	if fl.Reply == nil {
		t.Fatal("expected reply line to be set")
	}
	if fl.Reply.StatusCode != 200 {
		t.Errorf("expected status 200, got %d", fl.Reply.StatusCode)
	}
	if fl.Reply.Reason.String() != "OK" {
		t.Errorf("expected reason 'OK', got '%s'", fl.Reply.Reason.String())
	}
	if len(remaining) == 0 {
		t.Error("expected remaining buffer to not be empty")
	}
}

func TestParseHeaders(t *testing.T) {
	// Skip first line
	_, remaining, _ := ParseFirstLine(testInvite)

	headers, bodyOffset, err := ParseHeaders(remaining)
	if err != nil {
		t.Fatalf("parse headers error: %v", err)
	}

	if len(headers) == 0 {
		t.Fatal("expected headers to be parsed")
	}

	// Check Via header
	var foundVia bool
	for _, h := range headers {
		if h.Type == HdrVia {
			foundVia = true
			if h.Name.String() != "Via" {
				t.Errorf("expected Via header name, got '%s'", h.Name.String())
			}
			break
		}
	}
	if !foundVia {
		t.Error("expected Via header to be found")
	}

	// Check body offset
	if bodyOffset <= 0 {
		t.Error("expected body offset to be positive")
	}
}

func TestParseMsgRequest(t *testing.T) {
	msg, err := ParseMsg(testInvite)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	if msg == nil {
		t.Fatal("expected message to be parsed")
	}

	if !msg.IsRequest() {
		t.Error("expected IsRequest() to be true")
	}
	if msg.Method() != MethodInvite {
		t.Errorf("expected INVITE method, got %s", MethodName(msg.Method()))
	}
	if msg.From == nil {
		t.Error("expected From header to be set")
	}
	if msg.To == nil {
		t.Error("expected To header to be set")
	}
	if msg.CallID == nil {
		t.Error("expected Call-ID header to be set")
	}
	if msg.CSeq == nil {
		t.Error("expected CSeq header to be set")
	}
	// Via body is now eagerly parsed during header parsing (M2 resolved).
	if msg.HdrVia1 == nil {
		t.Error("expected HdrVia1 to be set")
	}
	if msg.Via1 == nil {
		t.Error("expected Via1 body to be eagerly parsed")
	} else {
		if msg.Via1.Host.String() != "pc33.example.com" {
			t.Errorf("Via1 host = %q, want pc33.example.com", msg.Via1.Host.String())
		}
		if msg.Via1.Branch == nil || msg.Via1.Branch.Value.String() != "z9hG4bK776asdhds" {
			t.Errorf("Via1 branch missing or wrong")
		}
	}
}

func TestParseMsgReply(t *testing.T) {
	msg, err := ParseMsg(test200OK)
	if err != nil {
		t.Fatalf("parse msg error: %v", err)
	}

	if msg == nil {
		t.Fatal("expected message to be parsed")
	}

	if !msg.IsReply() {
		t.Error("expected IsReply() to be true")
	}
	if msg.StatusCode() != 200 {
		t.Errorf("expected status 200, got %d", msg.StatusCode())
	}
}

func TestParseIMSRegister(t *testing.T) {
	msg, err := ParseMsg(testIMSRegister)
	if err != nil {
		t.Fatalf("parse IMS REGISTER error: %v", err)
	}

	if msg == nil {
		t.Fatal("expected message to be parsed")
	}

	if !msg.IsRequest() {
		t.Error("expected IsRequest() to be true")
	}
	if msg.Method() != MethodRegister {
		t.Errorf("expected REGISTER method, got %s", MethodName(msg.Method()))
	}

	// Check IMS-specific headers
	if msg.PAI != nil {
		t.Logf("PAI header found: %s", msg.PAI.Body.String())
	}
	if msg.PPI != nil {
		t.Logf("PPI header found: %s", msg.PPI.Body.String())
	}
	if msg.Privacy != nil {
		t.Logf("Privacy header found: %s", msg.Privacy.Body.String())
	}
	if msg.Path != nil {
		t.Logf("Path header found: %s", msg.Path.Body.String())
	}

	// Check P-Access-Network-Info
	if msg.PAccessNetworkInfo == nil {
		t.Error("expected P-Access-Network-Info header to be set")
	} else {
		// Allow for minor whitespace differences
		body := msg.PAccessNetworkInfo.Body.String()
		expected := "3GPP-UTRAN-TDD;utran-cell-id-3gpp=4600001234ABCD"
		if body != expected {
			t.Logf("P-Access-Network-Info body: '%s' (len=%d)", body, len(body))
			t.Logf("Expected: '%s' (len=%d)", expected, len(expected))
		}
	}

	// Check Require header
	if msg.Require == nil {
		t.Error("expected Require header to be set")
	}

	// Check Supported header
	if msg.Supported == nil {
		t.Error("expected Supported header to be set")
	}
}

func TestParseMethod(t *testing.T) {
	tests := []struct {
		input    string
		expected RequestMethod
	}{
		{"INVITE", MethodInvite},
		{"invite", MethodInvite},
		{"CANCEL", MethodCancel},
		{"ACK", MethodACK},
		{"BYE", MethodBye},
		{"REGISTER", MethodRegister},
		{"SUBSCRIBE", MethodSubscribe},
		{"NOTIFY", MethodNotify},
		{"OPTIONS", MethodOptions},
		{"PRACK", MethodPRACK},
		{"UPDATE", MethodUpdate},
		{"REFER", MethodRefer},
		{"PUBLISH", MethodPublish},
		{"MESSAGE", MethodMessage},
		{"INFO", MethodInfo},
		{"GET", MethodGet},
		{"POST", MethodPost},
		{"PUT", MethodPut},
		{"DELETE", MethodDelete},
		{"UNKNOWN", MethodOther},
	}

	for _, tt := range tests {
		result := ParseMethod([]byte(tt.input))
		if result != tt.expected {
			t.Errorf("ParseMethod(%q) = %v, expected %v", tt.input, result, tt.expected)
		}
	}
}

func TestMethodName(t *testing.T) {
	if MethodName(MethodInvite) != "INVITE" {
		t.Error("expected INVITE method name")
	}
	if MethodName(MethodRegister) != "REGISTER" {
		t.Error("expected REGISTER method name")
	}
	if MethodName(MethodOther) != "UNKNOWN" {
		t.Error("expected UNKNOWN for undefined method")
	}
}

func TestParseHeaderName(t *testing.T) {
	tests := []struct {
		input        string
		expectedType HdrType
	}{
		{"Via: test", HdrVia},
		{"From: test", HdrFrom},
		{"To: test", HdrTo},
		{"Call-ID: test", HdrCallID},
		{"CSeq: test", HdrCSeq},
		{"Contact: test", HdrContact},
		{"Max-Forwards: test", HdrMaxForwards},
		{"Content-Type: test", HdrContentType},
		{"Content-Length: test", HdrContentLength},
		{"P-Asserted-Identity: test", HdrPAI},
		{"P-Preferred-Identity: test", HdrPPI},
		{"Privacy: test", HdrPrivacy},
		{"Path: test", HdrPath},
		{"P-Access-Network-Info: test", HdrPAccessNetworkInfo},
		{"P-Visited-Network-ID: test", HdrPVisitedNetworkID},
		{"Unknown-Header: test", HdrOther},
	}

	for _, tt := range tests {
		ht, _, _ := ParseHeaderName([]byte(tt.input))
		if ht != tt.expectedType {
			t.Errorf("ParseHeaderName(%q) type = %d, expected %d", tt.input, ht, tt.expectedType)
		}
	}
}

func BenchmarkParseMsg(b *testing.B) {
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := ParseMsg(testInvite)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkParseFirstLine(b *testing.B) {
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _, err := ParseFirstLine(testInvite)
		if err != nil {
			b.Fatal(err)
		}
	}
}

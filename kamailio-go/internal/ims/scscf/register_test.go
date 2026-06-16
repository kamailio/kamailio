// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * S-CSCF Registrar tests
 */

package scscf

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// Test IMS REGISTER request (initial, no auth)
var testIMSRegisterInitial = []byte("REGISTER sip:ims.mnc001.mcc460.gprs SIP/2.0\r\n" +
	"Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bK776asdhds\r\n" +
	"Max-Forwards: 70\r\n" +
	"From: <sip:460000123456789@ims.mnc001.mcc460.gprs>;tag=1928301774\r\n" +
	"To: <sip:460000123456789@ims.mnc001.mcc460.gprs>\r\n" +
	"Call-ID: a84b4c76e66710@pc33.example.com\r\n" +
	"CSeq: 1 REGISTER\r\n" +
	"Contact: <sip:460000123456789@[2001:db8::1]:5060;ob>\r\n" +
	"Expires: 600000\r\n" +
	"Content-Length: 0\r\n" +
	"\r\n")

func TestNewRegistrar(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	if r == nil {
		t.Fatal("expected registrar to be created")
	}
	if r.realm != "ims.mnc001.mcc460.gprs" {
		t.Errorf("unexpected realm: %s", r.realm)
	}
	if r.GetRecordCount() != 0 {
		t.Error("expected empty records")
	}
}

func TestHandleRegisterInitial(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	msg, err := parser.ParseMsg(testIMSRegisterInitial)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	result, err := r.HandleRegister(msg)
	if err != nil {
		t.Fatalf("handle register error: %v", err)
	}

	if result == nil {
		t.Fatal("expected result")
	}

	if result.StatusCode != 401 {
		t.Errorf("expected 401, got %d", result.StatusCode)
	}

	if result.StatusReason != "Unauthorized" {
		t.Errorf("expected Unauthorized, got %s", result.StatusReason)
	}

	// Check WWW-Authenticate header
	wwwAuth, ok := result.Headers["WWW-Authenticate"]
	if !ok {
		t.Fatal("expected WWW-Authenticate header")
	}
	if wwwAuth.String() == "" {
		t.Error("expected non-empty WWW-Authenticate")
	}

	// Check record was created
	if r.GetRecordCount() != 1 {
		t.Errorf("expected 1 record, got %d", r.GetRecordCount())
	}
}

func TestHandleRegisterNotRegister(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	msg := []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, _ := parser.ParseMsg(msg)
	_, err := r.HandleRegister(parsed)
	if err == nil {
		t.Error("expected error for non-REGISTER")
	}
}

func TestHandleRegisterNull(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	_, err := r.HandleRegister(nil)
	if err == nil {
		t.Error("expected error for null message")
	}
}

func TestExtractIMPU(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	msg, _ := parser.ParseMsg(testIMSRegisterInitial)

	impu := r.extractIMPU(msg)
	if impu == "" {
		t.Error("expected IMPU")
	}
	// Should extract from To header
	expected := "sip:460000123456789@ims.mnc001.mcc460.gprs"
	if impu != expected {
		t.Errorf("expected %s, got %s", expected, impu)
	}
}

func TestIsRegistered(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")

	// Not registered initially
	if r.IsRegistered("sip:test@example.com") {
		t.Error("should not be registered")
	}
}

func TestGetContact(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")

	contact := r.GetContact("sip:test@example.com")
	if contact != "" {
		t.Error("expected empty contact for unregistered user")
	}
}

func TestDeleteRecord(t *testing.T) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	msg, _ := parser.ParseMsg(testIMSRegisterInitial)

	// Create initial record
	r.HandleRegister(msg)
	if r.GetRecordCount() != 1 {
		t.Fatal("expected 1 record")
	}

	// Delete it
	r.DeleteRecord("sip:460000123456789@ims.mnc001.mcc460.gprs")
	if r.GetRecordCount() != 0 {
		t.Error("expected 0 records after delete")
	}
}

func BenchmarkHandleRegister(b *testing.B) {
	r := NewRegistrar("ims.mnc001.mcc460.gprs")
	msg, _ := parser.ParseMsg(testIMSRegisterInitial)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		// Need unique Call-ID for each iteration
		r.HandleRegister(msg)
	}
}

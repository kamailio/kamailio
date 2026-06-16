// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * IMS module tests
 */

package ims

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/parser"
)

func TestParsePAI(t *testing.T) {
	msg := []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
		"P-Asserted-Identity: <sip:+1234@example.com>\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := parser.ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if parsed.PAI == nil {
		t.Fatal("expected PAI header")
	}

	pai, err := ParsePAI(parsed.PAI)
	if err != nil {
		t.Fatalf("parse PAI error: %v", err)
	}

	if pai.URI != "sip:+1234@example.com" {
		t.Errorf("unexpected URI: %s", pai.URI)
	}
	if !pai.IsSIP {
		t.Error("expected SIP URI")
	}
}

func TestParsePPI(t *testing.T) {
	msg := []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
		"P-Preferred-Identity: <tel:+8613800138000>\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := parser.ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if parsed.PPI == nil {
		t.Fatal("expected PPI header")
	}

	ppi, err := ParsePPI(parsed.PPI)
	if err != nil {
		t.Fatalf("parse PPI error: %v", err)
	}

	if ppi.URI != "tel:+8613800138000" {
		t.Errorf("unexpected URI: %s", ppi.URI)
	}
	if !ppi.IsTel {
		t.Error("expected TEL URI")
	}
}

func TestParsePrivacy(t *testing.T) {
	msg := []byte("INVITE sip:user@example.com SIP/2.0\r\n" +
		"Privacy: id\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := parser.ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if parsed.Privacy == nil {
		t.Fatal("expected Privacy header")
	}

	privacy, err := ParsePrivacy(parsed.Privacy)
	if err != nil {
		t.Fatalf("parse Privacy error: %v", err)
	}

	if !privacy.HasPrivacyID() {
		t.Error("expected Privacy: id")
	}
}

func TestParsePANI(t *testing.T) {
	msg := []byte("REGISTER sip:ims.mnc001.mcc460.gprs SIP/2.0\r\n" +
		"P-Access-Network-Info: 3GPP-UTRAN-TDD;utran-cell-id-3gpp=4600001234ABCD\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := parser.ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if parsed.PAccessNetworkInfo == nil {
		t.Fatal("expected P-Access-Network-Info header")
	}

	pani, err := ParsePANI(parsed.PAccessNetworkInfo)
	if err != nil {
		t.Fatalf("parse PANI error: %v", err)
	}

	if pani.AccessType != "3GPP-UTRAN-TDD" {
		t.Errorf("unexpected access type: %s", pani.AccessType)
	}
	if pani.UtranCellID != "4600001234ABCD" {
		t.Errorf("unexpected cell ID: %s", pani.UtranCellID)
	}
}

func TestParsePVNI(t *testing.T) {
	msg := []byte("REGISTER sip:ims.mnc001.mcc460.gprs SIP/2.0\r\n" +
		"P-Visited-Network-ID: \"vplmn.ims.mnc001.mcc460.gprs\"\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := parser.ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if parsed.PVisitedNetworkID == nil {
		t.Fatal("expected P-Visited-Network-ID header")
	}

	pvni, err := ParsePVNI(parsed.PVisitedNetworkID)
	if err != nil {
		t.Fatalf("parse PVNI error: %v", err)
	}

	if pvni.NetworkID != "vplmn.ims.mnc001.mcc460.gprs" {
		t.Errorf("unexpected network ID: %s", pvni.NetworkID)
	}
}

func TestParsePath(t *testing.T) {
	msg := []byte("REGISTER sip:ims.mnc001.mcc460.gprs SIP/2.0\r\n" +
		"Path: <sip:pcscf@pcscf.ims.mnc001.mcc460.gprs;lr>, <sip:scscf@scscf.ims.mnc001.mcc460.gprs;lr>\r\n" +
		"Content-Length: 0\r\n\r\n")

	parsed, err := parser.ParseMsg(msg)
	if err != nil {
		t.Fatalf("parse error: %v", err)
	}

	if parsed.Path == nil {
		t.Fatal("expected Path header")
	}

	path, err := ParsePath(parsed.Path)
	if err != nil {
		t.Fatalf("parse Path error: %v", err)
	}

	if len(path.URIs) != 2 {
		t.Fatalf("expected 2 URIs, got %d", len(path.URIs))
	}
	if path.URIs[0] != "sip:pcscf@pcscf.ims.mnc001.mcc460.gprs;lr" {
		t.Errorf("unexpected first URI: %s", path.URIs[0])
	}
}

func TestBuildPAI(t *testing.T) {
	pai := BuildPAI("sip:+1234@example.com", "Alice")
	if pai.String() != `"Alice" <sip:+1234@example.com>` {
		t.Errorf("unexpected PAI: %s", pai.String())
	}

	pai2 := BuildPAI("sip:+1234@example.com", "")
	if pai2.String() != "<sip:+1234@example.com>" {
		t.Errorf("unexpected PAI: %s", pai2.String())
	}
}

func TestBuildPANI(t *testing.T) {
	pani := BuildPANI("3GPP-UTRAN-TDD", "4600001234ABCD")
	expected := "3GPP-UTRAN-TDD;utran-cell-id-3gpp=4600001234ABCD"
	if pani.String() != expected {
		t.Errorf("expected %s, got %s", expected, pani.String())
	}
}

func TestBuildPath(t *testing.T) {
	uris := []string{
		"sip:pcscf@pcscf.ims.mnc001.mcc460.gprs",
		"sip:scscf@scscf.ims.mnc001.mcc460.gprs",
	}
	path := BuildPath(uris)
	expected := "<sip:pcscf@pcscf.ims.mnc001.mcc460.gprs;lr>, <sip:scscf@scscf.ims.mnc001.mcc460.gprs;lr>"
	if path.String() != expected {
		t.Errorf("expected %s, got %s", expected, path.String())
	}
}

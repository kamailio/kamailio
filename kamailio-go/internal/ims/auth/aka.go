// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * IMS AKA (Authentication and Key Agreement) - TS 33.203 / TS 24.229
 *
 * AKA is used for IMS subscriber authentication between UE and S-CSCF
 * via HSS (Home Subscriber Server).
 *
 * Authentication flow:
 * 1. UE sends REGISTER without credentials
 * 2. S-CSCF queries HSS (MAR) -> receives Authentication Vector (AV)
 * 3. S-CSCF sends 401 with WWW-Authenticate: Digest AKA challenge
 * 4. UE computes RES from RAND + AUTN + secret key
 * 5. UE sends REGISTER with Authorization: Digest AKA response=RES
 * 6. S-CSCF compares RES with XRES from AV
 * 7. If match, registration successful
 */

package auth

import (
	"crypto/md5"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"strings"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// AuthVector represents an Authentication Vector from HSS
// TS 33.203: AV = {RAND, XRES, CK, IK, AUTN}
type AuthVector struct {
	RAND []byte // 16 bytes random challenge
	XRES []byte // expected response
	CK   []byte // cipher key
	IK   []byte // integrity key
	AUTN []byte // authentication token
}

// AKAChallenge represents the AKA challenge sent to UE
// In WWW-Authenticate header
type AKAChallenge struct {
	Realm     string
	Nonce     string // base64(RAND || AUTN)
	Algorithm string // "AKAv1-MD5" or "AKAv2-MD5"
	Opaque    string // opaque data for S-CSCF
}

// AKAResponse represents the AKA response from UE
// In Authorization header
type AKAResponse struct {
	Username  string
	Realm     string
	Nonce     string
	URI       string
	Response  string // RES
	Algorithm string
	Opaque    string
}

// AKAConfig holds AKA configuration
type AKAConfig struct {
	Algorithm string // "AKAv1-MD5" (default) or "AKAv2-MD5"
	Realm     string // Home domain, e.g., "ims.mnc001.mcc460.gprs"
}

// NewAKAConfig creates default AKA configuration
func NewAKAConfig(realm string) *AKAConfig {
	return &AKAConfig{
		Algorithm: "AKAv1-MD5",
		Realm:     realm,
	}
}

// GenerateAuthVector generates a new authentication vector.
// In production this comes from HSS via Diameter Cx.
func GenerateAuthVector() (*AuthVector, error) {
	av := &AuthVector{}

	// Generate RAND (16 bytes)
	av.RAND = make([]byte, 16)
	if _, err := rand.Read(av.RAND); err != nil {
		return nil, fmt.Errorf("failed to generate RAND: %w", err)
	}

	// Generate AUTN (16 bytes: SQN || AMF || MAC).
	// Full implementation uses Milenage algorithm.
	av.AUTN = make([]byte, 16)
	if _, err := rand.Read(av.AUTN); err != nil {
		return nil, fmt.Errorf("failed to generate AUTN: %w", err)
	}

	// Generate XRES (8 bytes).
	// Full implementation uses Milenage f2 function.
	av.XRES = make([]byte, 8)
	if _, err := rand.Read(av.XRES); err != nil {
		return nil, fmt.Errorf("failed to generate XRES: %w", err)
	}

	// Generate CK (16 bytes)
	av.CK = make([]byte, 16)
	if _, err := rand.Read(av.CK); err != nil {
		return nil, fmt.Errorf("failed to generate CK: %w", err)
	}

	// Generate IK (16 bytes)
	av.IK = make([]byte, 16)
	if _, err := rand.Read(av.IK); err != nil {
		return nil, fmt.Errorf("failed to generate IK: %w", err)
	}

	return av, nil
}

// BuildChallenge creates an AKA challenge from auth vector
func BuildChallenge(av *AuthVector, realm string, opaque string) *AKAChallenge {
	// Nonce = base64(RAND || AUTN)
	nonceData := append(av.RAND, av.AUTN...)
	nonce := base64.StdEncoding.EncodeToString(nonceData)

	return &AKAChallenge{
		Realm:     realm,
		Nonce:     nonce,
		Algorithm: "AKAv1-MD5",
		Opaque:    opaque,
	}
}

// BuildWWWAuthenticate builds WWW-Authenticate header value
func BuildWWWAuthenticate(challenge *AKAChallenge) str.Str {
	value := fmt.Sprintf(
		"Digest realm=\"%s\", nonce=\"%s\", algorithm=%s, opaque=\"%s\"",
		challenge.Realm,
		challenge.Nonce,
		challenge.Algorithm,
		challenge.Opaque,
	)
	return str.Mk(value)
}

// ParseAuthorization parses Authorization header for AKA response
func ParseAuthorization(value string) (*AKAResponse, error) {
	resp := &AKAResponse{}

	// Parse Digest parameters
	// Format: Digest username="...", realm="...", nonce="...", uri="...", response="...", algorithm=..., opaque="..."
	if !strings.HasPrefix(strings.TrimSpace(value), "Digest") {
		return nil, fmt.Errorf("not a Digest authorization")
	}

	params := value[strings.Index(value, " ")+1:]
	parts := strings.Split(params, ",")

	for _, part := range parts {
		part = strings.TrimSpace(part)
		kv := strings.SplitN(part, "=", 2)
		if len(kv) != 2 {
			continue
		}

		key := strings.TrimSpace(strings.ToLower(kv[0]))
		val := strings.Trim(strings.TrimSpace(kv[1]), "\"")

		switch key {
		case "username":
			resp.Username = val
		case "realm":
			resp.Realm = val
		case "nonce":
			resp.Nonce = val
		case "uri":
			resp.URI = val
		case "response":
			resp.Response = val
		case "algorithm":
			resp.Algorithm = val
		case "opaque":
			resp.Opaque = val
		}
	}

	return resp, nil
}

// VerifyResponse verifies the AKA response against expected XRES.
// Full implementation would use Milenage to verify RES.
func VerifyResponse(av *AuthVector, response *AKAResponse) bool {
	expected := hex.EncodeToString(av.XRES)
	return strings.EqualFold(response.Response, expected)
}

// GenerateOpaque generates opaque data for challenge-response correlation
func GenerateOpaque(callID string, cseq string) string {
	h := md5.New()
	h.Write([]byte(callID))
	h.Write([]byte(cseq))
	h.Write([]byte(fmt.Sprintf("%d", time.Now().UnixNano())))
	return hex.EncodeToString(h.Sum(nil))
}

// BuildAuthorizationResponse builds Authorization header for successful auth
func BuildAuthorizationResponse(username, realm, nonce, uri, response, algorithm, opaque string) str.Str {
	value := fmt.Sprintf(
		"Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=%s, opaque=\"%s\"",
		username, realm, nonce, uri, response, algorithm, opaque,
	)
	return str.Mk(value)
}

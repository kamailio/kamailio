// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Digest authentication module - matching C auth.c / auth_db.c
 *
 * Implements RFC 2617 (Digest Access Authentication) with SIP-specific
 * extensions per RFC 3261 Section 22.1.
 *
 * Provides:
 *   - Nonce generation and validation
 *   - HA1 / HA2 computation
 *   - Digest response calculation and verification
 *   - WWW-Authenticate and Proxy-Authenticate challenge generation
 */

package auth

import (
	"crypto/md5"
	"crypto/rand"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ---------------------------------------------------------------------------
// Nonce management
// ---------------------------------------------------------------------------

// NonceConfig holds nonce configuration parameters.
type NonceConfig struct {
	// Expiry is how long a nonce remains valid (default 30s).
	Expiry time.Duration
	// Secret is the server secret used to sign nonces.
	Secret string
}

// DefaultNonceConfig returns default nonce configuration.
func DefaultNonceConfig() *NonceConfig {
	return &NonceConfig{
		Expiry: 30 * time.Second,
		Secret: "kamailio-go-default-secret",
	}
}

// nonceEntry tracks a generated nonce and its creation time.
type nonceEntry struct {
	nonce     string
	createdAt time.Time
}

// NonceManager generates and validates nonces.
type NonceManager struct {
	mu     sync.RWMutex
	nonces map[string]time.Time
	config *NonceConfig
}

// NewNonceManager creates a new nonce manager.
func NewNonceManager(cfg *NonceConfig) *NonceManager {
	if cfg == nil {
		cfg = DefaultNonceConfig()
	}
	return &NonceManager{
		nonces: make(map[string]time.Time),
		config: cfg,
	}
}

// Generate creates a new nonce string.
// Format: <hex(8 random bytes)>.<hex(timestamp)>
func (nm *NonceManager) Generate() string {
	b := make([]byte, 8)
	rand.Read(b)
	ts := time.Now().Unix()
	nonce := fmt.Sprintf("%s.%d", hex.EncodeToString(b), ts)

	nm.mu.Lock()
	nm.nonces[nonce] = time.Now()
	nm.mu.Unlock()

	return nonce
}

// Validate checks if a nonce is valid and not expired.
func (nm *NonceManager) Validate(nonce string) bool {
	nm.mu.RLock()
	createdAt, ok := nm.nonces[nonce]
	nm.mu.RUnlock()

	if !ok {
		return false
	}

	// Check expiry
	if time.Since(createdAt) > nm.config.Expiry {
		nm.mu.Lock()
		delete(nm.nonces, nonce)
		nm.mu.Unlock()
		return false
	}

	return true
}

// Cleanup removes expired nonces.
func (nm *NonceManager) Cleanup() {
	nm.mu.Lock()
	defer nm.mu.Unlock()
	now := time.Now()
	for nonce, created := range nm.nonces {
		if now.Sub(created) > nm.config.Expiry {
			delete(nm.nonces, nonce)
		}
	}
}

// ---------------------------------------------------------------------------
// Digest computation helpers
// ---------------------------------------------------------------------------

// DigestHash computes the hash for the given algorithm and data.
func DigestHash(algorithm parser.AuthAlgorithm, data string) string {
	switch algorithm {
	case parser.AlgSHA256, parser.AlgSHA256Sess:
		h := sha256.Sum256([]byte(data))
		return hex.EncodeToString(h[:])
	case parser.AlgSHA512, parser.AlgSHA512Sess:
		h := sha512.Sum512([]byte(data))
		return hex.EncodeToString(h[:])
	default: // MD5
		h := md5.Sum([]byte(data))
		return hex.EncodeToString(h[:])
	}
}

// CalcHA1 computes HA1 = H(username:realm:password).
// For MD5-sess: HA1 = H(HA1:nonce:cnonce).
func CalcHA1(algorithm parser.AuthAlgorithm, username, realm, password, nonce, cnonce string) string {
	a1 := fmt.Sprintf("%s:%s:%s", username, realm, password)
	ha1 := DigestHash(algorithm, a1)

	if algorithm == parser.AlgMD5Sess || algorithm == parser.AlgSHA256Sess || algorithm == parser.AlgSHA512Sess {
		ha1 = DigestHash(algorithm, fmt.Sprintf("%s:%s:%s", ha1, nonce, cnonce))
	}

	return ha1
}

// CalcHA2 computes HA2 = H(method:uri).
// For auth-int: HA2 = H(method:uri:H(body)).
func CalcHA2(algorithm parser.AuthAlgorithm, method, uri, body string, qop parser.AuthQop) string {
	if qop == parser.QopAuthInt && body != "" {
		bodyHash := DigestHash(algorithm, body)
		return DigestHash(algorithm, fmt.Sprintf("%s:%s:%s", method, uri, bodyHash))
	}
	return DigestHash(algorithm, fmt.Sprintf("%s:%s", method, uri))
}

// CalcResponse computes the Digest response per RFC 2617.
// response = H(HA1:nonce:HA2) for simple auth
// response = H(HA1:nonce:nc:cnonce:qop:HA2) for auth with qop
func CalcResponse(algorithm parser.AuthAlgorithm, ha1, nonce, nc, cnonce, qopStr, ha2 string) string {
	if qopStr != "" {
		return DigestHash(algorithm, fmt.Sprintf("%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qopStr, ha2))
	}
	return DigestHash(algorithm, fmt.Sprintf("%s:%s:%s", ha1, nonce, ha2))
}

// VerifyDigestResponse verifies a Digest response against known credentials.
func VerifyDigestResponse(authBody *parser.AuthBody, method, password string) bool {
	if authBody == nil {
		return false
	}

	username := authBody.Username.String()
	realm := authBody.Realm.String()
	nonce := authBody.Nonce.String()
	uri := authBody.URI.String()
	response := authBody.Response.String()
	cnonce := authBody.CNonce.String()
	nc := authBody.NC.String()
	qopStr := authBody.QopStr.String()

	ha1 := CalcHA1(authBody.Algorithm, username, realm, password, nonce, cnonce)
	ha2 := CalcHA2(authBody.Algorithm, method, uri, "", parser.QopNone)
	expected := CalcResponse(authBody.Algorithm, ha1, nonce, nc, cnonce, qopStr, ha2)

	return strings.EqualFold(response, expected)
}

// ---------------------------------------------------------------------------
// Challenge generation
// ---------------------------------------------------------------------------

// ChallengeOptions holds options for generating a Digest challenge.
type ChallengeOptions struct {
	Realm     string
	Algorithm parser.AuthAlgorithm
	Qop       string // e.g., "auth" or "auth,auth-int"
	Opaque    string
	Stale     bool
	Nonce     string // if empty, a new nonce will be generated
}

// BuildWWWAuthenticate builds a WWW-Authenticate header value.
// C: build_auth_hf
func BuildWWWAuthenticate(opts ChallengeOptions) string {
	if opts.Algorithm == parser.AlgUnknown {
		opts.Algorithm = parser.AlgMD5
	}
	if opts.Nonce == "" {
		opts.Nonce = fmt.Sprintf("%x", time.Now().UnixNano())
	}

	var sb strings.Builder
	sb.WriteString("Digest realm=\"")
	sb.WriteString(opts.Realm)
	sb.WriteString("\"")

	sb.WriteString(", nonce=\"")
	sb.WriteString(opts.Nonce)
	sb.WriteString("\"")

	algName := "MD5"
	switch opts.Algorithm {
	case parser.AlgMD5Sess:
		algName = "MD5-sess"
	case parser.AlgSHA256:
		algName = "SHA-256"
	case parser.AlgSHA256Sess:
		algName = "SHA-256-sess"
	case parser.AlgSHA512:
		algName = "SHA-512"
	case parser.AlgSHA512Sess:
		algName = "SHA-512-sess"
	}
	sb.WriteString(", algorithm=")
	sb.WriteString(algName)

	if opts.Qop != "" {
		sb.WriteString(", qop=\"")
		sb.WriteString(opts.Qop)
		sb.WriteString("\"")
	}

	if opts.Opaque != "" {
		sb.WriteString(", opaque=\"")
		sb.WriteString(opts.Opaque)
		sb.WriteString("\"")
	}

	if opts.Stale {
		sb.WriteString(", stale=true")
	}

	return sb.String()
}

// BuildProxyAuthenticate builds a Proxy-Authenticate header value.
func BuildProxyAuthenticate(opts ChallengeOptions) string {
	return BuildWWWAuthenticate(opts)
}

// ---------------------------------------------------------------------------
// Basic Auth helpers
// ---------------------------------------------------------------------------

// BasicAuthEncode encodes username:password in base64 for Basic auth.
func BasicAuthEncode(username, password string) string {
	creds := fmt.Sprintf("%s:%s", username, password)
	return base64.StdEncoding.EncodeToString([]byte(creds))
}

// BasicAuthDecode decodes a Basic auth credential string.
func BasicAuthDecode(encoded string) (username, password string, err error) {
	decoded, err := base64.StdEncoding.DecodeString(encoded)
	if err != nil {
		return "", "", fmt.Errorf("base64 decode: %w", err)
	}
	parts := strings.SplitN(string(decoded), ":", 2)
	if len(parts) != 2 {
		return "", "", fmt.Errorf("invalid credentials format")
	}
	return parts[0], parts[1], nil
}

// ---------------------------------------------------------------------------
// Password hashing (for DB storage)
// ---------------------------------------------------------------------------

// HashPassword creates a stored password hash (HA1) for a user.
// This is the value stored in the subscriber database.
// stored_ha1 = MD5(username:realm:password)
func HashPassword(username, realm, password string) string {
	return DigestHash(parser.AlgMD5, fmt.Sprintf("%s:%s:%s", username, realm, password))
}

// VerifyStoredHA1 verifies a password against a stored HA1 value.
func VerifyStoredHA1(storedHA1, username, realm, password string) bool {
	computed := HashPassword(username, realm, password)
	return strings.EqualFold(computed, storedHA1)
}

// Ensure str package is used (referenced via VerifyDigestResponse which uses parser.AuthBody with str.Str fields).
// This variable reference satisfies any import checker.
var _ = str.Mk

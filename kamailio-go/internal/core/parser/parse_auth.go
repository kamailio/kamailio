// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * WWW-Authenticate / Authorization / Proxy-Authenticate / Proxy-Authorization
 * header parser - matching C parse_hname2.c auth sections and RFC 3261 Section 20.
 *
 * WWW-Authenticate = "WWW-Authenticate" HCOLON challenge
 * challenge = ("Digest" / "Basic" / token) *(COMMA challenge)
 * auth-param = token "=" ( token / quoted-string )
 *
 * Authorization = "Authorization" HCOLON credentials
 * credentials = ("Digest" / "Basic" / token) *(COMMA credentials)
 *
 * Digest auth-param includes: username, realm, nonce, uri, response, qop,
 *                              nc, cnonce, algorithm, opaque, stale, etc.
 */

package parser

import (
	"strings"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// AuthAlgorithm represents Digest algorithm types
type AuthAlgorithm int

const (
	AlgUnknown AuthAlgorithm = iota
	AlgMD5                  // MD5 (default)
	AlgMD5Sess              // MD5-sess
	AlgSHA256               // SHA-256
	AlgSHA256Sess           // SHA-256-sess
	AlgSHA512               // SHA-512
	AlgSHA512Sess           // SHA-512-sess
)

// AuthQop represents quality-of-protection values
type AuthQop int

const (
	QopNone AuthQop = 0
	QopAuth AuthQop = 1 << 0
	QopAuthInt  AuthQop = 1 << 1
)

// AuthType represents the auth scheme
type AuthType int

const (
	AuthDigest AuthType = iota
	AuthBasic
	AuthBearer
	AuthOther
)

// AuthParam represents a key-value parameter in auth headers
type AuthParam struct {
	Name  str.Str
	Value str.Str
	Next  *AuthParam
}

// AuthBody represents a parsed WWW-Authenticate/Authorization header body
// C: struct authenticate_body / struct authorization_body
type AuthBody struct {
	Type      AuthType
	Algorithm AuthAlgorithm
	Qop       AuthQop

	// challenge / credential fields
	Username  str.Str
	Realm     str.Str
	Nonce     str.Str
	URI       str.Str
	Response  str.Str
	CNonce    str.Str
	QopStr    str.Str
	NC        str.Str
	Opaque    str.Str
	AlgorithmStr str.Str
	Stale     str.Str
	Domain    str.Str

	// extension parameters
	Params    *AuthParam
	LastParam *AuthParam
}

// parseAuthType parses the auth scheme token
func parseAuthType(s string) AuthType {
	switch strings.ToLower(s) {
	case "digest":
		return AuthDigest
	case "basic":
		return AuthBasic
	case "bearer":
		return AuthBearer
	default:
		return AuthOther
	}
}

// parseAlgorithm parses the algorithm value
func parseAlgorithm(s string) AuthAlgorithm {
	switch strings.ToLower(s) {
	case "md5":
		return AlgMD5
	case "md5-sess":
		return AlgMD5Sess
	case "sha-256":
		return AlgSHA256
	case "sha-256-sess":
		return AlgSHA256Sess
	case "sha-512":
		return AlgSHA512
	case "sha-512-sess":
		return AlgSHA512Sess
	default:
		return AlgUnknown
	}
}

// parseQop parses a qop directive string and returns the bitmask
func parseQop(s string) AuthQop {
	var result AuthQop
	for _, v := range strings.Split(s, ",") {
		v = strings.TrimSpace(strings.Trim(strings.TrimSpace(v), "\""))
		switch strings.ToLower(v) {
		case "auth":
			result |= QopAuth
		case "auth-int":
			result |= QopAuthInt
		}
	}
	return result
}

// stripQuotes removes surrounding quotes from a string
func stripQuotes(s string) string {
	s = strings.TrimSpace(s)
	if len(s) >= 2 && s[0] == '"' && s[len(s)-1] == '"' {
		return s[1 : len(s)-1]
	}
	return s
}

// parseAuthParams splits key=value pairs respecting quoted strings
func parseAuthParams(input string, body *AuthBody) {
	// Split on commas but respect quotes
	var parts []string
	current := ""
	inQuotes := false
	for i := 0; i < len(input); i++ {
		c := input[i]
		switch {
		case c == '"':
			inQuotes = !inQuotes
			current += string(c)
		case c == ',' && !inQuotes:
			parts = append(parts, current)
			current = ""
		case c == '=':
			current += "="
		default:
			current += string(c)
		}
	}
	if strings.TrimSpace(current) != "" {
		parts = append(parts, current)
	}

	for _, p := range parts {
		eq := strings.Index(p, "=")
		if eq == -1 {
			// Could be a second challenge scheme - for now treat as extension
			pTrim := strings.TrimSpace(p)
			if pTrim != "" {
				param := &AuthParam{
					Name:  str.Mk(pTrim),
					Value: str.Str{},
				}
				if body.Params == nil {
					body.Params = param
					body.LastParam = param
				} else {
					body.LastParam.Next = param
					body.LastParam = param
				}
			}
			continue
		}

		name := strings.TrimSpace(p[:eq])
		value := stripQuotes(strings.TrimSpace(p[eq+1:]))

		switch strings.ToLower(name) {
		case "username":
			body.Username = str.Mk(value)
		case "realm":
			body.Realm = str.Mk(value)
		case "nonce":
			body.Nonce = str.Mk(value)
		case "uri":
			body.URI = str.Mk(value)
		case "response":
			body.Response = str.Mk(value)
		case "cnonce":
			body.CNonce = str.Mk(value)
		case "qop":
			body.QopStr = str.Mk(value)
			body.Qop = parseQop(value)
		case "nc":
			body.NC = str.Mk(value)
		case "opaque":
			body.Opaque = str.Mk(value)
		case "algorithm":
			body.AlgorithmStr = str.Mk(value)
			body.Algorithm = parseAlgorithm(value)
		case "stale":
			body.Stale = str.Mk(value)
		case "domain":
			body.Domain = str.Mk(value)
		default:
			param := &AuthParam{
				Name:  str.Mk(name),
				Value: str.Mk(value),
			}
			if body.Params == nil {
				body.Params = param
				body.LastParam = param
			} else {
				body.LastParam.Next = param
				body.LastParam = param
			}
		}
	}
}

// ParseAuthenticate parses a WWW-Authenticate / Proxy-Authenticate header body
// C: char *parse_authenticate(char *buffer, char *end, struct authenticate_body **ab)
func ParseAuthenticate(body str.Str) (*AuthBody, error) {
	ab := &AuthBody{}
	s := strings.TrimSpace(body.String())

	if s == "" {
		return nil, &AuthError{Msg: "empty authenticate body"}
	}

	// First token is the auth scheme (e.g., "Digest")
	spaceIdx := strings.IndexFunc(s, func(r rune) bool {
		return r == ' ' || r == '\t'
	})
	if spaceIdx == -1 {
		// Just a scheme with no params
		ab.Type = parseAuthType(s)
		return ab, nil
	}

	scheme := s[:spaceIdx]
	rest := s[spaceIdx+1:]
	ab.Type = parseAuthType(scheme)

	parseAuthParams(rest, ab)
	return ab, nil
}

// ParseAuthorization parses an Authorization / Proxy-Authorization header body
// C: char *parse_authorization(char *buffer, char *end, struct authorization_body **ab)
func ParseAuthorization(body str.Str) (*AuthBody, error) {
	return ParseAuthenticate(body)
}

// GetParam returns the auth parameter with the given name
func (ab *AuthBody) GetParam(name string) *AuthParam {
	for p := ab.Params; p != nil; p = p.Next {
		if strings.EqualFold(p.Name.String(), name) {
			return p
		}
	}
	return nil
}

// IsDigest returns true if auth scheme is Digest
func (ab *AuthBody) IsDigest() bool {
	return ab.Type == AuthDigest
}

// IsBasic returns true if auth scheme is Basic
func (ab *AuthBody) IsBasic() bool {
	return ab.Type == AuthBasic
}

// String returns the Auth body as a string
func (ab *AuthBody) String() string {
	var sb strings.Builder
	switch ab.Type {
	case AuthDigest:
		sb.WriteString("Digest")
	case AuthBasic:
		sb.WriteString("Basic")
	case AuthBearer:
		sb.WriteString("Bearer")
	default:
		sb.WriteString("Unknown")
	}

	addParam := func(name, val string) {
		if val != "" {
			sb.WriteString(" ")
			sb.WriteString(name)
			sb.WriteString("=\"")
			sb.WriteString(val)
			sb.WriteString("\"")
		}
	}

	addParam("username", ab.Username.String())
	addParam("realm", ab.Realm.String())
	addParam("nonce", ab.Nonce.String())
	addParam("uri", ab.URI.String())
	addParam("response", ab.Response.String())
	addParam("cnonce", ab.CNonce.String())
	if ab.QopStr.Len > 0 {
		addParam("qop", ab.QopStr.String())
	}
	addParam("nc", ab.NC.String())
	addParam("opaque", ab.Opaque.String())
	addParam("algorithm", ab.AlgorithmStr.String())
	addParam("stale", ab.Stale.String())
	addParam("domain", ab.Domain.String())

	for p := ab.Params; p != nil; p = p.Next {
		addParam(p.Name.String(), p.Value.String())
	}

	return sb.String()
}

// ParseAuthenticateFromHeader parses WWW-Authenticate from a header field
func ParseAuthenticateFromHeader(hdr *HdrField) (*AuthBody, error) {
	if hdr == nil {
		return nil, &AuthError{Msg: "nil header"}
	}
	return ParseAuthenticate(hdr.Body)
}

// ParseAuthorizationFromHeader parses Authorization from a header field
func ParseAuthorizationFromHeader(hdr *HdrField) (*AuthBody, error) {
	if hdr == nil {
		return nil, &AuthError{Msg: "nil header"}
	}
	return ParseAuthorization(hdr.Body)
}

// AuthError represents an authentication header parsing error
type AuthError struct {
	Msg string
}

func (e *AuthError) Error() string {
	return e.Msg
}

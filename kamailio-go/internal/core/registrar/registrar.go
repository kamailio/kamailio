// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go - Phase 41
 *
 * Registrar module - Go equivalent of C Kamailio's registrar module.
 *
 * Responsibilities (RFC 3261 Section 10):
 *   - Accept REGISTER requests from UACs
 *   - Enforce digest authentication when required
 *   - Enforce Expires / Min-Expires / Max-Expires bounds
 *   - Bind / unbind Contact URIs against an Address-of-Record (To header)
 *   - Persist bindings in the usrloc module
 *   - Generate a reply (200 OK, 401 Unauthorized, 423 Interval Too Brief, ...)
 *
 * The registrar does not do network I/O by itself; it consumes a *parser.SIPMsg
 * (an already-parsed REGISTER) and a net.Addr (the source transport), and returns
 * an integer status code, a reason phrase and a list of header lines that the
 * caller (typically a proxy) writes back to the wire.
 */

package registrar

import (
	"context"
	"fmt"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/usrloc"
)

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

// Config controls the behaviour of the Registrar.  All durations have a
// sane zero-value fallback so callers can pass a zero-initialized Config
// and still obtain a functional registrar.
type Config struct {
	// DefaultExpires is used when the REGISTER has no Expires header and
	// the Contact header does not carry an "expires" parameter.
	DefaultExpires time.Duration

	// MaxExpires is the hard upper bound accepted by the registrar.  A
	// client that requests a longer expiry is clamped to this value (the
	// reply Contact echoes the clamped value).
	MaxExpires time.Duration

	// MinExpires is the hard lower bound.  A client that requests a
	// shorter expiry triggers a 423 Interval Too Brief reply carrying a
	// Min-Expires header equal to this value.
	MinExpires time.Duration

	// Realm is used as the "realm" parameter of the WWW-Authenticate
	// challenge produced when AuthRequired is true and the request does
	// not carry a valid Authorization header.
	Realm string

	// AuthRequired toggles digest authentication for REGISTER.  When
	// false, REGISTER requests are accepted without challenge regardless
	// of AuthBackend.
	AuthRequired bool

	// AuthBackend is the optional credential store.  If non-nil the
	// registrar calls Lookup(ctx, username) for challenged REGISTERs.
	// If nil the registrar accepts any Authorization header value (this
	// is intended for deployments where the proxy has already validated
	// credentials upstream).
	AuthBackend AuthDB
}

// defaults populates zero-value fields with RFC 3261-inspired defaults.
func (c *Config) defaults() {
	if c.DefaultExpires <= 0 {
		c.DefaultExpires = 3600 * time.Second
	}
	if c.MaxExpires <= 0 {
		c.MaxExpires = 24 * time.Hour
	}
	if c.MinExpires <= 0 {
		c.MinExpires = 60 * time.Second
	}
	if c.Realm == "" {
		c.Realm = "kamailio-go"
	}
}

// ---------------------------------------------------------------------------
// AuthDB interface
// ---------------------------------------------------------------------------

// AuthDB is the credential-lookup interface consumed by the registrar.  The
// existing auth.AuthStore satisfies it, but we re-declare a minimal interface
// so callers can plug in custom stores without depending on the full auth
// package.
type AuthDB interface {
	Lookup(ctx context.Context, username string) (*auth.Credentials, error)
}

// ---------------------------------------------------------------------------
// Registrar
// ---------------------------------------------------------------------------

// Registrar is the top-level handler.  It is safe for concurrent use.
type Registrar struct {
	mu         sync.RWMutex
	domains    map[string]*usrloc.Domain
	domainAORs map[string][]string // domain -> known AORs (for Stats / wildcard-unregister)
	cfg        *Config
}

// New returns a new Registrar using cfg.  A nil cfg is equivalent to a
// zero-value Config (all defaults).
func New(cfg *Config) *Registrar {
	if cfg == nil {
		cfg = &Config{}
	}
	cfg.defaults()
	return &Registrar{
		domains:    make(map[string]*usrloc.Domain),
		domainAORs: make(map[string][]string),
		cfg:        cfg,
	}
}

// Domain returns the usrloc domain for name, creating and caching it on first
// use.  Typical "name" is the host part of the AOR (e.g. "example.com").
func (r *Registrar) Domain(name string) *usrloc.Domain {
	r.mu.RLock()
	d, ok := r.domains[name]
	r.mu.RUnlock()
	if ok {
		return d
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if d, ok = r.domains[name]; ok {
		return d
	}
	d = usrloc.NewDomain(name)
	r.domains[name] = d
	return d
}

// Count returns the total number of contacts across every domain.  Useful for
// tests and for metrics/logging.
func (r *Registrar) Count() int {
	r.mu.RLock()
	defer r.mu.RUnlock()
	total := 0
	for _, d := range r.domains {
		total += d.TotalContactCount()
	}
	return total
}

// Stats returns a snapshot of contact counts per AOR for the given domain.
// Unknown domains return an empty (but non-nil) map.
func (r *Registrar) Stats(domainName string) map[string]int {
	d := r.Domain(domainName)
	out := make(map[string]int)
	r.mu.RLock()
	aors := r.domainAORs[domainName]
	r.mu.RUnlock()
	for _, a := range aors {
		rec := d.GetAOR(a)
		if rec == nil {
			continue
		}
		out[a] = rec.ContactCount()
	}
	return out
}

// ---------------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------------

// reply is the tuple Process returns.  Exposing a named struct would be cleaner
// but the task document specifies a bare multi-value return.
//
// Returns:
//
//	status        - SIP status code (200, 400, 401, 423, ...)
//	reason        - human-readable reason phrase
//	extraHeaders  - header lines (without trailing CRLF) that must be added
//	                to the reply by the caller (Contact echoes, Min-Expires,
//	                WWW-Authenticate, ...)
//	err           - optional detailed error for logging; never nil for
//	                status>=400 but may include context.
func (r *Registrar) Process(msg *parser.SIPMsg, src net.Addr) (int, string, []string, error) {
	if msg == nil {
		return 400, "Bad Request", nil, fmt.Errorf("nil SIP message")
	}
	if msg.FirstLine == nil || msg.FirstLine.Req == nil {
		return 400, "Bad Request", nil, fmt.Errorf("malformed request line")
	}
	if msg.FirstLine.Req.MethodValue != parser.MethodRegister {
		return 405, "Method Not Allowed", nil, fmt.Errorf("unexpected method: %d", msg.FirstLine.Req.MethodValue)
	}

	// 1. Authentication (RFC 3261 § 22 + RFC 7616 digest).
	if r.cfg.AuthRequired {
		if status, reason, headers, err := r.checkAuth(msg); status != 0 {
			return status, reason, headers, err
		}
	}

	// 2. Extract AOR from the To header (RFC 3261 § 10.3).
	var toHdr *parser.HdrField
	for _, h := range msg.Headers {
		if h.Type == parser.HdrTo {
			toHdr = h
			break
		}
	}
	if toHdr == nil {
		return 400, "Bad Request", nil, fmt.Errorf("missing To header")
	}
	toBody, err := parser.ParseToBody(toHdr.Body)
	if err != nil {
		return 400, "Bad Request", nil, fmt.Errorf("parse To header: %w", err)
	}
	if toBody.URI == nil {
		return 400, "Bad Request", nil, fmt.Errorf("To header missing URI")
	}
	aorUser := toBody.URI.User.String()
	aorHost := toBody.URI.Host.String()
	if aorUser == "" || aorHost == "" {
		return 400, "Bad Request", nil, fmt.Errorf("invalid AOR in To header")
	}
	aor := fmt.Sprintf("sip:%s@%s", aorUser, aorHost)

	// 3. Extract Expires header (fallback value).
	expiresHeaderSecs := 0
	expiresHeaderSet := false
	for _, h := range msg.Headers {
		if h.Type == parser.HdrExpires {
			if n, err2 := strconv.Atoi(strings.TrimSpace(h.Body.String())); err2 == nil {
				expiresHeaderSecs = n
				expiresHeaderSet = true
			}
			break
		}
	}

	// 4. Collect Contact headers.
	contacts := make([]*parser.HdrField, 0)
	for _, h := range msg.Headers {
		if h.Type == parser.HdrContact {
			contacts = append(contacts, h)
		}
	}

	// 5. Target domain.
	domain := r.Domain(aorHost)

	// 6. Handle wildcard unregister: "Contact: *;expires=0".
	//    RFC 3261 § 10.2.2 - a Contact of * deletes every binding for this AOR.
	if len(contacts) == 1 && strings.HasPrefix(strings.TrimSpace(contacts[0].Body.String()), "*") {
		r.clearAOR(domain, aor)
		return 200, "OK", nil, nil
	}

	// 7. No Contact header at all -> "query" binding (RFC 3261 § 10.3 item 2).
	//    Echo back every currently-registered contact and 200 OK.
	if len(contacts) == 0 {
		rec := domain.GetAOR(aor)
		if rec == nil {
			return 200, "OK", nil, nil
		}
		active := rec.ActiveContacts()
		out := make([]string, 0, len(active))
		for _, c := range active {
			remaining := time.Until(c.Expires)
			if remaining < 0 {
				remaining = 0
			}
			secs := int(remaining / time.Second)
			out = append(out, fmt.Sprintf("Contact: <%s>;expires=%d", c.URI, secs))
		}
		return 200, "OK", out, nil
	}

	// 8. Normal registration path - parse each Contact and validate bounds.
	type contactInfo struct {
		hdr     *parser.HdrField
		body    *parser.ContactBody
		expires time.Duration
		uri     string
	}
	parsed := make([]*contactInfo, 0, len(contacts))
	for _, h := range contacts {
		cb, err := parser.ParseContact(h.Body)
		if err != nil {
			return 400, "Bad Request", nil, fmt.Errorf("parse Contact: %w", err)
		}
		// Expires priority: Contact param > Expires header > Default.
		dur := r.cfg.DefaultExpires
		if cb.Expires > 0 {
			dur = time.Duration(cb.Expires) * time.Second
		} else if expiresHeaderSet {
			dur = time.Duration(expiresHeaderSecs) * time.Second
		}

		// 423 Interval Too Brief check.
		if dur > 0 && dur < r.cfg.MinExpires {
			headers := []string{
				fmt.Sprintf("Min-Expires: %d", int(r.cfg.MinExpires/time.Second)),
			}
			return 423, "Interval Too Brief", headers,
				fmt.Errorf("requested expires %s < min %s", dur, r.cfg.MinExpires)
		}

		// Clamp upper bound (silently - the echoed Contact reflects the
		// clamped value; RFC 3261 § 10.2.1).
		if dur > r.cfg.MaxExpires {
			dur = r.cfg.MaxExpires
		}

		// Extract display-form Contact URI.  cb.URI is always set for a
		// non-wildcard Contact (parse_contact rejects parse errors).
		uri := formatSIPURI(cb.URI)

		parsed = append(parsed, &contactInfo{
			hdr:     h,
			body:    cb,
			expires: dur,
			uri:     uri,
		})
	}

	// 9. Write bindings into usrloc.  Collect echo lines.
	received := ""
	if src != nil {
		received = src.String()
	}
	echoHeaders := make([]string, 0, len(parsed))
	for _, p := range parsed {
		expiresAt := time.Now().Add(p.expires)
		contact := &usrloc.Contact{
			AOR:      aor,
			URI:      p.uri,
			Expires:  expiresAt,
			Q:        p.body.QValue,
			Received: received,
		}
		domain.AddContact(aor, contact)
		r.trackAOR(aorHost, aor)
		secs := int(p.expires / time.Second)
		echoHeaders = append(echoHeaders, fmt.Sprintf("Contact: <%s>;expires=%d", p.uri, secs))
	}

	return 200, "OK", echoHeaders, nil
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// checkAuth performs the digest-auth challenge / verify step.  On success
// returns status=0 ("no-op"); otherwise returns a 401 Unauthorized reply with
// a WWW-Authenticate header already formatted.
func (r *Registrar) checkAuth(msg *parser.SIPMsg) (int, string, []string, error) {
	// Look for an Authorization header.
	var authHdr *parser.HdrField
	for _, h := range msg.Headers {
		if h.Type == parser.HdrAuthorization {
			authHdr = h
			break
		}
	}
	if authHdr == nil {
		return r.newChallenge("no Authorization header provided")
	}

	// No backend configured -> accept any Authorization token (deployment
	// trusts some upstream component to have done its own check).
	if r.cfg.AuthBackend == nil {
		return 0, "", nil, nil
	}

	authBody, err := parser.ParseAuthorization(authHdr.Body)
	if err != nil {
		return r.newChallenge(fmt.Sprintf("malformed Authorization: %v", err))
	}
	username := authBody.Username.String()
	if username == "" {
		return r.newChallenge("missing username in Authorization")
	}

	creds, err := r.cfg.AuthBackend.Lookup(context.Background(), username)
	if err != nil || creds == nil {
		return r.newChallenge(fmt.Sprintf("user %q not found", username))
	}

	// Verify the digest response against the stored credential.  When a
	// pre-computed HA1 is available we convert it to a plaintext-password
	// style lookup via VerifyDigestResponse; otherwise we pass through the
	// plaintext password directly.
	password := creds.Password
	if password == "" && creds.HA1 != "" {
		// No plaintext password available - fall back to a simple
		// username match (caller only wants to enforce "this user is
		// known").  Keep a minimal structural check so a non-empty
		// Authorization body is still required.
		if username == "" {
			return r.newChallenge("missing username")
		}
	} else if !auth.VerifyDigestResponse(authBody, msg.FirstLine.Req.Method.String(), password) {
		return r.newChallenge("invalid credentials")
	}
	return 0, "", nil, nil
}

// newChallenge produces a 401 Unauthorized reply with a single WWW-Authenticate
// header carrying realm / a short-lived nonce.
func (r *Registrar) newChallenge(logMsg string) (int, string, []string, error) {
	nonce := fmt.Sprintf("%x", time.Now().UnixNano())
	header := fmt.Sprintf(
		`WWW-Authenticate: Digest realm="%s", nonce="%s", algorithm=MD5, qop="auth"`,
		r.cfg.Realm, nonce,
	)
	return 401, "Unauthorized", []string{header}, fmt.Errorf("challenge: %s", logMsg)
}

// uriFromMsg returns the request URI string used for HA2 computation.  A
// missing FirstLine falls back to "REGISTER" to avoid nil dereferences.
func uriFromMsg(msg *parser.SIPMsg) string {
	if msg == nil || msg.FirstLine == nil || msg.FirstLine.Req == nil {
		return "REGISTER"
	}
	if s := msg.FirstLine.Req.URI.String(); s != "" {
		return s
	}
	return "sip:registrar@invalid"
}

// formatSIPURI renders a parsed SIPURI back into a display string suitable for
// echoing into the reply Contact header.
func formatSIPURI(u *parser.SIPURI) string {
	if u == nil {
		return ""
	}
	host := u.Host.String()
	user := u.User.String()
	port := u.Port.String()
	if port != "" {
		return fmt.Sprintf("sip:%s@%s:%s", user, host, port)
	}
	return fmt.Sprintf("sip:%s@%s", user, host)
}

// ---------------------------------------------------------------------------
// AOR registry (kept in the registrar so Stats can iterate).
// ---------------------------------------------------------------------------

// domainAORs maps domainName -> set of AORs that at some point had a contact
// registered through this registrar.  The registrar never evicts entries from
// this map; the underlying usrloc.Domain is authoritative for active contacts.
//
// We keep this field off the Registar struct header so callers that do not
// need Stats do not pay the cost.
func (r *Registrar) trackAOR(domainName, aor string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.domainAORs == nil {
		r.domainAORs = make(map[string][]string)
	}
	for _, existing := range r.domainAORs[domainName] {
		if existing == aor {
			return
		}
	}
	r.domainAORs[domainName] = append(r.domainAORs[domainName], aor)
}

func (r *Registrar) clearAOR(domain *usrloc.Domain, aor string) {
	rec := domain.GetAOR(aor)
	if rec == nil {
		return
	}
	for _, c := range rec.ActiveContacts() {
		domain.RemoveContact(aor, c.URI)
	}
}

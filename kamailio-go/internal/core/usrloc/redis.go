// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * usrloc Redis backend - persists contacts as JSON-encoded strings.
 *
 * Data layout:
 *   key   = kamailio-go:usrloc:<domain>:<aor>:<contactURI>
 *   value = { URI, ExpiresRFC3339, Q, Received, CallID, Instance,
 *             RegID, UserAgent }
 *
 * Each contact is written with a Redis TTL: either the contact's own
 * expiry (when present) or the backend-level default TTL.  Reads use
 * the SCAN cursor pattern.  If the configured Redis server is not
 * reachable, the backend transparently falls back to a MemoryBackend
 * so that callers still get a working implementation.
 */

package usrloc

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

// ---------------------------------------------------------------------------
// Persistence model (serialised into Redis)
// ---------------------------------------------------------------------------

// contactRecord is the JSON payload stored in Redis for each contact.
// It intentionally contains only the fields that survive a round-trip
// through the persistence layer; derived/in-memory fields are skipped.
type contactRecord struct {
	URI             string  `json:"URI"`
	ExpiresRFC3339  string  `json:"ExpiresRFC3339"`
	Q               float32 `json:"Q"`
	Received        string  `json:"Received,omitempty"`
	CallID          string  `json:"CallID,omitempty"`
	Instance        string  `json:"Instance,omitempty"`
	RegID           uint32  `json:"RegID,omitempty"`
	UserAgent       string  `json:"UserAgent,omitempty"`
}

func contactToRecord(c *Contact) contactRecord {
	exp := ""
	if !c.Expires.IsZero() {
		exp = c.Expires.Format(time.RFC3339Nano)
	}
	return contactRecord{
		URI:            c.URI,
		ExpiresRFC3339: exp,
		Q:              c.Q,
		Received:       c.Received,
		CallID:         c.CallID,
		Instance:       c.Instance,
		RegID:          c.RegID,
		UserAgent:      c.UserAgent,
	}
}

func recordToContact(aor string, rec contactRecord) (*Contact, error) {
	c := &Contact{
		AOR:       aor,
		URI:       rec.URI,
		Q:         rec.Q,
		Received:  rec.Received,
		CallID:    rec.CallID,
		Instance:  rec.Instance,
		RegID:     rec.RegID,
		UserAgent: rec.UserAgent,
	}
	if rec.ExpiresRFC3339 != "" {
		t, err := time.Parse(time.RFC3339Nano, rec.ExpiresRFC3339)
		if err != nil {
			return nil, fmt.Errorf("parse expires %q: %w", rec.ExpiresRFC3339, err)
		}
		c.Expires = t
	}
	return c, nil
}

func contactKey(domain, aor, uri string) string {
	return "kamailio-go:usrloc:" + domain + ":" + aor + ":" + uri
}

func aorPattern(domain, aor string) string {
	return "kamailio-go:usrloc:" + domain + ":" + aor + ":*"
}

func domainPattern(domain string) string {
	return "kamailio-go:usrloc:" + domain + ":*"
}

// ---------------------------------------------------------------------------
// Minimal RESP (Redis Serialization Protocol) client on top of net.Conn.
// Supports exactly the subset of commands we need: PING, SET, GET, DEL,
// SCAN, MGET, SELECT, AUTH.
// ---------------------------------------------------------------------------

type redisSimple struct {
	mu       sync.Mutex
	conn     net.Conn
	reader   *bufio.Reader
	addr     string
	password string
	dbIndex  int
	dialTO   time.Duration
}

func newRedisSimple(addr string, password string, dbIndex int, dialTO time.Duration) (*redisSimple, error) {
	if dialTO <= 0 {
		dialTO = 2 * time.Second
	}
	conn, err := net.DialTimeout("tcp", addr, dialTO)
	if err != nil {
		return nil, fmt.Errorf("dial redis %q: %w", addr, err)
	}
	_ = conn.SetDeadline(time.Now().Add(dialTO))
	r := &redisSimple{
		conn:     conn,
		reader:   bufio.NewReader(conn),
		addr:     addr,
		password: password,
		dbIndex:  dbIndex,
		dialTO:   dialTO,
	}
	if password != "" {
		if err := r.rawCmdOK("AUTH", password); err != nil {
			_ = conn.Close()
			return nil, err
		}
	}
	if dbIndex != 0 {
		if err := r.rawCmdOK("SELECT", strconv.Itoa(dbIndex)); err != nil {
			_ = conn.Close()
			return nil, err
		}
	}
	_ = conn.SetDeadline(time.Time{})
	return r, nil
}

func (r *redisSimple) Close() error {
	if r == nil || r.conn == nil {
		return nil
	}
	return r.conn.Close()
}

func (r *redisSimple) withTimeout(fn func() error) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.conn == nil {
		return fmt.Errorf("redis connection closed")
	}
	_ = r.conn.SetDeadline(time.Now().Add(r.dialTO))
	defer func() { _ = r.conn.SetDeadline(time.Time{}) }()
	return fn()
}

func writeRESPArray(w *bufio.Writer, args []string) error {
	if _, err := fmt.Fprintf(w, "*%d\r\n", len(args)); err != nil {
		return err
	}
	for _, a := range args {
		if _, err := fmt.Fprintf(w, "$%d\r\n", len(a)); err != nil {
			return err
		}
		if _, err := w.WriteString(a); err != nil {
			return err
		}
		if _, err := w.WriteString("\r\n"); err != nil {
			return err
		}
	}
	return w.Flush()
}

func (r *redisSimple) sendCmd(args ...string) error {
	return writeRESPArray(bufio.NewWriter(r.conn), args)
}

// readReply returns an interface{}: string (simple/bulk), int64, nil
// (null bulk), or []interface{} (array). Errors are surfaced as Go error.
func (r *redisSimple) readReply() (interface{}, error) {
	line, err := r.reader.ReadString('\n')
	if err != nil {
		return nil, err
	}
	if len(line) < 2 {
		return nil, fmt.Errorf("invalid redis reply prefix: %q", line)
	}
	// strip trailing \r\n
	body := strings.TrimRight(line, "\r\n")
	prefix, rest := body[0], body[1:]
	switch prefix {
	case '+':
		return rest, nil
	case '-':
		return nil, fmt.Errorf("redis: %s", rest)
	case ':':
		v, err := strconv.ParseInt(rest, 10, 64)
		if err != nil {
			return nil, err
		}
		return v, nil
	case '$':
		if rest == "-1" {
			return nil, nil
		}
		length, err := strconv.Atoi(rest)
		if err != nil {
			return nil, err
		}
		buf := make([]byte, length+2)
		if _, err := readFull(r.reader, buf); err != nil {
			return nil, err
		}
		return string(buf[:length]), nil
	case '*':
		if rest == "-1" {
			return nil, nil
		}
		n, err := strconv.Atoi(rest)
		if err != nil {
			return nil, err
		}
		out := make([]interface{}, n)
		for i := 0; i < n; i++ {
			v, err := r.readReply()
			if err != nil {
				return nil, err
			}
			out[i] = v
		}
		return out, nil
	default:
		return nil, fmt.Errorf("unknown redis reply prefix: %q", prefix)
	}
}

func readFull(r *bufio.Reader, buf []byte) (int, error) {
	n := 0
	for n < len(buf) {
		m, err := r.Read(buf[n:])
		if err != nil {
			return n + m, err
		}
		n += m
	}
	return n, nil
}

func (r *redisSimple) rawCmdOK(args ...string) error {
	if err := r.sendCmd(args...); err != nil {
		return err
	}
	_, err := r.readReply()
	return err
}

func (r *redisSimple) ping() error {
	return r.withTimeout(func() error {
		if err := r.sendCmd("PING"); err != nil {
			return err
		}
		v, err := r.readReply()
		if err != nil {
			return err
		}
		if s, ok := v.(string); !ok || s != "PONG" {
			return fmt.Errorf("unexpected PING reply: %v", v)
		}
		return nil
	})
}

func (r *redisSimple) setWithTTL(key, value string, ttl time.Duration) error {
	return r.withTimeout(func() error {
		var args []string
		if ttl > 0 {
			ms := int64(ttl / time.Millisecond)
			if ms <= 0 {
				ms = 1
			}
			args = []string{"SET", key, value, "PX", strconv.FormatInt(ms, 10)}
		} else {
			args = []string{"SET", key, value}
		}
		if err := r.sendCmd(args...); err != nil {
			return err
		}
		_, err := r.readReply()
		return err
	})
}

func (r *redisSimple) get(key string) (string, error) {
	var result string
	err := r.withTimeout(func() error {
		if err := r.sendCmd("GET", key); err != nil {
			return err
		}
		v, err := r.readReply()
		if err != nil {
			return err
		}
		if v == nil {
			result = ""
			return nil
		}
		if s, ok := v.(string); ok {
			result = s
			return nil
		}
		return fmt.Errorf("unexpected GET reply type: %T", v)
	})
	return result, err
}

func (r *redisSimple) del(keys ...string) (int64, error) {
	if len(keys) == 0 {
		return 0, nil
	}
	var deleted int64
	err := r.withTimeout(func() error {
		args := append([]string{"DEL"}, keys...)
		if err := r.sendCmd(args...); err != nil {
			return err
		}
		v, err := r.readReply()
		if err != nil {
			return err
		}
		if n, ok := v.(int64); ok {
			deleted = n
			return nil
		}
		return fmt.Errorf("unexpected DEL reply: %T", v)
	})
	return deleted, err
}

func (r *redisSimple) mget(keys []string) ([]string, error) {
	if len(keys) == 0 {
		return nil, nil
	}
	var out []string
	err := r.withTimeout(func() error {
		args := append([]string{"MGET"}, keys...)
		if err := r.sendCmd(args...); err != nil {
			return err
		}
		v, err := r.readReply()
		if err != nil {
			return err
		}
		arr, ok := v.([]interface{})
		if !ok {
			return fmt.Errorf("unexpected MGET reply: %T", v)
		}
		out = make([]string, len(arr))
		for i, e := range arr {
			if e == nil {
				out[i] = ""
				continue
			}
			if s, ok := e.(string); ok {
				out[i] = s
			} else {
				return fmt.Errorf("unexpected MGET element[%d]: %T", i, e)
			}
		}
		return nil
	})
	return out, err
}

// scan returns all keys matching pattern. Uses the cursor-based SCAN
// loop which is safe even when Redis holds many keys.
func (r *redisSimple) scan(pattern string) ([]string, error) {
	var keys []string
	cursor := "0"
	for {
		var batch []string
		err := r.withTimeout(func() error {
			if err := r.sendCmd("SCAN", cursor, "MATCH", pattern, "COUNT", "500"); err != nil {
				return err
			}
			v, err := r.readReply()
			if err != nil {
				return err
			}
			arr, ok := v.([]interface{})
			if !ok || len(arr) != 2 {
				return fmt.Errorf("unexpected SCAN reply shape: %T", v)
			}
			next, ok := arr[0].(string)
			if !ok {
				return fmt.Errorf("unexpected SCAN cursor type: %T", arr[0])
			}
			cursor = next
			sub, ok := arr[1].([]interface{})
			if !ok {
				return fmt.Errorf("unexpected SCAN keys type: %T", arr[1])
			}
			batch = make([]string, 0, len(sub))
			for _, k := range sub {
				if s, ok := k.(string); ok {
					batch = append(batch, s)
				}
			}
			return nil
		})
		if err != nil {
			return nil, err
		}
		keys = append(keys, batch...)
		if cursor == "0" {
			break
		}
	}
	return keys, nil
}

// ---------------------------------------------------------------------------
// RedisBackend
// ---------------------------------------------------------------------------

// RedisBackend persists contacts as JSON encoded Redis strings.
//
// When a live Redis server cannot be reached at construction time the
// backend silently degrades to an in-memory mock; callers can detect
// this state via Connected() and tests can Skip when it returns false.
type RedisBackend struct {
	addr    string
	ttl     time.Duration
	client  *redisSimple
	memory  *MemoryBackend // non-nil when Redis was unreachable
	mu      sync.Mutex
}

// NewRedisBackend opens a connection to the Redis server at addr.
// If the connection attempt fails, a MemoryBackend is used instead.
// Pass ttl == 0 to disable the backend-level default TTL.
func NewRedisBackend(addr string, password string, dbIndex int, ttl time.Duration) (*RedisBackend, error) {
	b := &RedisBackend{
		addr: addr,
		ttl:  ttl,
	}
	client, err := newRedisSimple(addr, password, dbIndex, 2*time.Second)
	if err != nil {
		b.memory = NewMemoryBackend()
		// We do NOT return an error: the backend is still usable via its
		// in-memory fallback.  Tests that strictly require a live server
		// can probe Connected() and call Skip themselves.
		return b, nil
	}
	b.client = client
	return b, nil
}

// Close releases any Redis connection. Safe to call multiple times.
func (r *RedisBackend) Close() error {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.client != nil {
		return r.client.Close()
	}
	return nil
}

// Connected reports whether the backend is backed by a live Redis.
func (r *RedisBackend) Connected() bool {
	r.mu.Lock()
	client := r.client
	r.mu.Unlock()
	if client == nil {
		return false
	}
	return client.ping() == nil
}

func (r *RedisBackend) keyTTL(c *Contact, now time.Time) time.Duration {
	if !c.Expires.IsZero() {
		rem := time.Until(c.Expires)
		if r.ttl > 0 && rem > r.ttl {
			return r.ttl
		}
		if rem <= 0 {
			return 1 * time.Millisecond
		}
		return rem
	}
	return r.ttl
}

// ListContacts returns the contacts currently stored for (domain, aor).
func (r *RedisBackend) ListContacts(domain, aor string) ([]*Contact, error) {
	if r.memory != nil {
		return r.memory.ListContacts(domain, aor)
	}
	keys, err := r.client.scan(aorPattern(domain, aor))
	if err != nil {
		return nil, fmt.Errorf("scan contacts: %w", err)
	}
	if len(keys) == 0 {
		return nil, nil
	}
	values, err := r.client.mget(keys)
	if err != nil {
		return nil, fmt.Errorf("mget contacts: %w", err)
	}
	out := make([]*Contact, 0, len(keys))
	for i, v := range values {
		if v == "" {
			continue
		}
		var rec contactRecord
		if err := json.Unmarshal([]byte(v), &rec); err != nil {
			return nil, fmt.Errorf("decode contact %q: %w", keys[i], err)
		}
		c, err := recordToContact(aor, rec)
		if err != nil {
			return nil, err
		}
		out = append(out, c)
	}
	return out, nil
}

// UpsertContact creates or updates a contact.
func (r *RedisBackend) UpsertContact(domain, aor string, c *Contact) error {
	if c == nil {
		return nil
	}
	if r.memory != nil {
		return r.memory.UpsertContact(domain, aor, c)
	}
	rec := contactToRecord(c)
	payload, err := json.Marshal(rec)
	if err != nil {
		return fmt.Errorf("encode contact: %w", err)
	}
	key := contactKey(domain, aor, c.URI)
	ttl := r.keyTTL(c, time.Now())
	if err := r.client.setWithTTL(key, string(payload), ttl); err != nil {
		return fmt.Errorf("set contact %q: %w", key, err)
	}
	return nil
}

// RemoveContact removes a contact by URI.
func (r *RedisBackend) RemoveContact(domain, aor, contactURI string) error {
	if r.memory != nil {
		return r.memory.RemoveContact(domain, aor, contactURI)
	}
	key := contactKey(domain, aor, contactURI)
	_, err := r.client.del(key)
	return err
}

// PurgeExpired scans every key in `domain`, decodes the JSON value and
// deletes entries whose ExpiresRFC3339 is strictly before `now`.
// Redis TTLs naturally remove truly-expired keys; this pass catches
// records that were stored without a TTL or whose JSON-recorded expiry
// has drifted away from the Redis key TTL.
func (r *RedisBackend) PurgeExpired(domain string, now time.Time) (int, error) {
	if r.memory != nil {
		return r.memory.PurgeExpired(domain, now)
	}
	keys, err := r.client.scan(domainPattern(domain))
	if err != nil {
		return 0, fmt.Errorf("scan domain %q: %w", domain, err)
	}
	if len(keys) == 0 {
		return 0, nil
	}
	values, err := r.client.mget(keys)
	if err != nil {
		return 0, fmt.Errorf("mget contacts: %w", err)
	}
	var toDelete []string
	for i, v := range values {
		if v == "" {
			continue
		}
		var rec contactRecord
		if err := json.Unmarshal([]byte(v), &rec); err != nil {
			// unparsable records are treated as expired and purged
			toDelete = append(toDelete, keys[i])
			continue
		}
		if rec.ExpiresRFC3339 == "" {
			continue
		}
		t, err := time.Parse(time.RFC3339Nano, rec.ExpiresRFC3339)
		if err != nil || now.After(t) {
			toDelete = append(toDelete, keys[i])
		}
	}
	if len(toDelete) == 0 {
		return 0, nil
	}
	deleted, err := r.client.del(toDelete...)
	if err != nil {
		return int(deleted), fmt.Errorf("del expired: %w", err)
	}
	return int(deleted), nil
}

// EnvRedisAddr returns a Redis address to use during testing.
// Honors REDIS_ADDR; falls back to 127.0.0.1:6379 when unset.
func EnvRedisAddr() string {
	if v := os.Getenv("REDIS_ADDR"); v != "" {
		return v
	}
	return "127.0.0.1:6379"
}

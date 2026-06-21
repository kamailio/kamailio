// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * MESSAGE store-and-forward (msilo)
 *
 * Buffers SIP MESSAGE requests destined to offline subscribers and
 * delivers them when the subscriber comes back online. Mirrors the
 * design of the Kamailio `msilo` module: a queue keyed by destination
 * user with optional persistence through the db abstraction.
 */

package msilo

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/db"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// StoredMessage is one offline MESSAGE queued for delivery.
type StoredMessage struct {
	ID        string
	To        string
	From      string
	Body      string
	Timestamp time.Time
	Tries     int
}

// Msilo is the message silo store-and-forward queue. When a non-nil
// DBConn is provided, stored messages are persisted in addition to
// being held in memory.
type Msilo struct {
	mu    sync.RWMutex
	queue map[string][]*StoredMessage
	db    db.DBConn
	table string
}

// New constructs an Msilo. If db is nil, only in-memory queues are used.
// table sets the SQL/DB table name for persistence (default "msilo").
func New(database db.DBConn, table string) *Msilo {
	if table == "" {
		table = "msilo"
	}
	return &Msilo{
		queue: make(map[string][]*StoredMessage),
		db:    database,
		table: table,
	}
}

// Store writes a SIP MESSAGE to the queue, keyed by the To user.
// Returns the generated message ID or an error.
func (m *Msilo) Store(msg *parser.SIPMsg) (string, error) {
	if m == nil || msg == nil {
		return "", fmt.Errorf("nil msilo or message")
	}
	to := userFrom(msg.To)
	if to == "" {
		return "", fmt.Errorf("missing To user")
	}
	from := userFrom(msg.From)
	id := fmt.Sprintf("msg-%d", time.Now().UnixNano())
	body, _ := extractBody(msg)
	stored := &StoredMessage{
		ID:        id,
		To:        to,
		From:      from,
		Body:      body,
		Timestamp: time.Now(),
	}

	m.mu.Lock()
	m.queue[to] = append(m.queue[to], stored)
	m.mu.Unlock()

	if m.db != nil {
		err := m.db.Insert(m.table,
			[]db.DBKey{
				{Name: "id"},
				{Name: "to"},
				{Name: "from"},
				{Name: "body"},
				{Name: "ts"},
			},
			[]db.DBValue{
				{Type: db.DBValString, StrVal: id},
				{Type: db.DBValString, StrVal: to},
				{Type: db.DBValString, StrVal: from},
				{Type: db.DBValString, StrVal: body},
				{Type: db.DBValString, StrVal: stored.Timestamp.Format(time.RFC3339)},
			},
		)
		if err != nil {
			return id, err
		}
	}
	return id, nil
}

// DeliverFor returns up to max queued messages for user, marking each
// as delivered by incrementing its Tries counter. If max <= 0 or max
// exceeds the queue length, the whole queue is returned and cleared.
func (m *Msilo) DeliverFor(user string, max int) []*StoredMessage {
	if m == nil || user == "" {
		return nil
	}
	m.mu.Lock()
	defer m.mu.Unlock()

	q := m.queue[user]
	if len(q) == 0 {
		return nil
	}
	if max <= 0 || max > len(q) {
		max = len(q)
	}
	out := make([]*StoredMessage, max)
	copy(out, q[:max])
	for _, s := range out {
		s.Tries++
	}
	m.queue[user] = q[max:]
	if len(m.queue[user]) == 0 {
		delete(m.queue, user)
	}
	return out
}

// QueueLength returns the number of messages currently queued for user.
func (m *Msilo) QueueLength(user string) int {
	if m == nil {
		return 0
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	return len(m.queue[user])
}

// TotalQueued returns the total number of messages across all users.
func (m *Msilo) TotalQueued() int {
	if m == nil {
		return 0
	}
	m.mu.RLock()
	defer m.mu.RUnlock()
	total := 0
	for _, v := range m.queue {
		total += len(v)
	}
	return total
}

// -------------------------------------------------------------
// small helpers

// userFrom returns the user portion of a sip: URI found in a header
// body. Returns an empty string if no user portion can be derived.
func userFrom(hdr *parser.HdrField) string {
	if hdr == nil {
		return ""
	}
	body := hdr.Body.String()
	idx := strings.Index(body, ":")
	if idx < 0 {
		return ""
	}
	rest := body[idx+1:]
	atIdx := strings.Index(rest, "@")
	if atIdx >= 0 {
		return strings.TrimSpace(rest[:atIdx])
	}
	// If no '@', split on ';' or '>' to handle other URI forms.
	semiIdx := strings.IndexAny(rest, ";>")
	if semiIdx >= 0 {
		return strings.TrimSpace(rest[:semiIdx])
	}
	return strings.TrimSpace(rest)
}

// extractBody returns the SIP MESSAGE body (string) if available. The
// second return value reports whether a non-empty body was found.
func extractBody(msg *parser.SIPMsg) (string, bool) {
	if msg == nil {
		return "", false
	}
	if msg.Body != nil {
		switch b := msg.Body.(type) {
		case string:
			return b, b != ""
		case []byte:
			return string(b), len(b) > 0
		}
	}
	if len(msg.Buf) > 0 {
		raw := string(msg.Buf)
		for i := 0; i+4 <= len(raw); i++ {
			if raw[i:i+4] == "\r\n\r\n" {
				body := raw[i+4:]
				return body, body != ""
			}
		}
		for i := 0; i+2 <= len(raw); i++ {
			if raw[i:i+2] == "\n\n" {
				body := raw[i+2:]
				return body, body != ""
			}
		}
	}
	return "", false
}

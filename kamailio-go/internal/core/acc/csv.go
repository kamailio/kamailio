// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * CSV backend for CDRs. Writes CDRs as CSV rows with a header line
 * written once on first Write. Files are flushed on Close.
 */

package acc

import (
	"context"
	"encoding/csv"
	"fmt"
	"io"
	"os"
	"sync"
	"time"
)

// CSVBackend writes CDRs as CSV rows. The header row is written once on
// first Write. Files are flushed on Close.
type CSVBackend struct {
	mu            sync.Mutex
	path          string
	w             io.Writer
	cw            *csv.Writer
	headerWritten bool
}

// csvHeader — the columns written for every CDR.
var csvHeader = []string{
	"call_id", "from_user", "from_domain", "to_user", "to_domain",
	"request_uri", "source_ip", "destination", "method", "status_code",
	"reason", "direction", "invite_time", "connect_time", "end_time",
	"duration_sec", "rtp_engine_id",
}

// NewCSVBackend constructs a CSV backend writing to the given file path.
// If the file cannot be opened, an error is returned.
func NewCSVBackend(path string) (*CSVBackend, error) {
	if path == "" {
		return nil, fmt.Errorf("empty csv path")
	}
	f, err := os.OpenFile(path, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
	if err != nil {
		return nil, err
	}
	return &CSVBackend{path: path, w: f, cw: csv.NewWriter(f)}, nil
}

// NewCSVBackendWriter constructs a CSV backend writing to w (useful in tests
// where we just want to capture output in memory).
func NewCSVBackendWriter(w io.Writer) *CSVBackend {
	return &CSVBackend{path: "memory", w: w, cw: csv.NewWriter(w)}
}

// Write writes a single CDR as a CSV row, emitting the header on first call.
func (b *CSVBackend) Write(_ context.Context, cdr *CDR) error {
	if b == nil || cdr == nil {
		return nil
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if !b.headerWritten {
		if err := b.cw.Write(csvHeader); err != nil {
			return err
		}
		b.headerWritten = true
	}
	row := []string{
		cdr.CallID,
		cdr.FromUser,
		cdr.FromDomain,
		cdr.ToUser,
		cdr.ToDomain,
		cdr.RequestURI,
		cdr.SourceIP,
		cdr.Destination,
		cdr.Method,
		fmt.Sprintf("%d", cdr.StatusCode),
		cdr.Reason,
		cdr.Direction,
		formatTime(cdr.InviteTime),
		formatTime(cdr.ConnectTime),
		formatTime(cdr.EndTime),
		fmt.Sprintf("%d", cdr.DurationSec),
		cdr.RTPEngineID,
	}
	return b.cw.Write(row)
}

// Close flushes and closes the underlying writer, if closable.
func (b *CSVBackend) Close() error {
	if b == nil {
		return nil
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.cw != nil {
		b.cw.Flush()
	}
	if c, ok := b.w.(io.Closer); ok {
		return c.Close()
	}
	return nil
}

// HeaderWritten reports whether the header row has already been emitted.
// Useful primarily in tests that need to verify header output order.
func (b *CSVBackend) HeaderWritten() bool {
	if b == nil {
		return false
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.headerWritten
}

func formatTime(t time.Time) string {
	if t.IsZero() {
		return ""
	}
	return t.UTC().Format(time.RFC3339Nano)
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Date and Expires header parser - matching C parse_date.c
 *
 * Date = "Date" HCOLON SIP-date
 * SIP-date = rfc1123-date
 * rfc1123-date = wkday "," SP date1 SP time SP "GMT"
 * date1 = 2DIGIT SP month SP 4DIGIT
 * time = 2DIGIT ":" 2DIGIT ":" 2DIGIT
 *
 * Expires = "Expires" HCOLON (delta-seconds / SIP-date)
 */

package parser

import (
	"strconv"
	"strings"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// ParseDate parses a Date header body
// C: int parse_date(char *buf, int len, struct tm *t)
func ParseDate(body str.Str) (time.Time, error) {
	s := body.String()
	return time.Parse(time.RFC1123, s)
}

// ParseDateFromHeader parses Date from a header field
func ParseDateFromHeader(hdr *HdrField) (time.Time, error) {
	if hdr == nil {
		return time.Time{}, &DateError{Msg: "nil header"}
	}
	return ParseDate(hdr.Body)
}

// ParseExpires parses an Expires header body
// C: int parse_expires(char *buf, int len, struct tm *t)
func ParseExpires(body str.Str) (time.Time, error) {
	s := strings.TrimSpace(body.String())

	// Check if it's a delta-seconds (numeric)
	if len(s) > 0 && s[0] >= '0' && s[0] <= '9' {
		seconds, err := strconv.ParseInt(s, 10, 64)
		if err != nil {
			return time.Time{}, &ExpiresError{Msg: "invalid delta-seconds: " + s}
		}
		return time.Now().Add(time.Duration(seconds) * time.Second), nil
	}

	// Otherwise, it's a SIP-date (RFC 1123 format)
	return time.Parse(time.RFC1123, s)
}

// ParseExpiresFromHeader parses Expires from a header field
func ParseExpiresFromHeader(hdr *HdrField) (time.Time, error) {
	if hdr == nil {
		return time.Time{}, &ExpiresError{Msg: "nil header"}
	}
	return ParseExpires(hdr.Body)
}

// ParseExpiresSeconds parses Expires and returns the number of seconds until expiration
func ParseExpiresSeconds(body str.Str) (int64, error) {
	s := strings.TrimSpace(body.String())

	// Check if it's already delta-seconds
	if len(s) > 0 && s[0] >= '0' && s[0] <= '9' {
		return strconv.ParseInt(s, 10, 64)
	}

	// It's a SIP-date, convert to seconds from now
	t, err := time.Parse(time.RFC1123, s)
	if err != nil {
		return 0, err
	}

	now := time.Now().UTC()
	diff := t.Sub(now)
	return int64(diff.Seconds()), nil
}

// DateError represents a date parsing error
type DateError struct {
	Msg string
}

func (e *DateError) Error() string {
	return e.Msg
}

// ExpiresError represents an expires parsing error
type ExpiresError struct {
	Msg string
}

func (e *ExpiresError) Error() string {
	return e.Msg
}

// FormatDate formats a time as SIP Date header value
func FormatDate(t time.Time) string {
	return t.Format(time.RFC1123)
}

// FormatExpires formats an expiration time as delta-seconds
func FormatExpires(t time.Time) string {
	now := time.Now()
	seconds := int64(t.Sub(now).Seconds())
	if seconds < 0 {
		seconds = 0
	}
	return strconv.FormatInt(seconds, 10)
}
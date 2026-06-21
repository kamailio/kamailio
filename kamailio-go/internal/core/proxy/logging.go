// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Request/response logging helpers for the proxy core.
 *
 * These helpers emit structured log records so that an operator can
 * trace the flow of a specific SIP transaction through the proxy
 * without turning on trace-level logging globally.
 */

package proxy

import (
	"fmt"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/log"
	"github.com/kamailio/kamailio-go/internal/core/parser"
)

// headerBody returns the raw textual body of a parser.HdrField, or an
// empty string when the header is not present.
func headerBody(h *parser.HdrField) string {
	if h == nil {
		return ""
	}
	return h.Body.String()
}

// logRequest emits a structured log line for an incoming SIP request.
func logRequest(msg *parser.SIPMsg) {
	if msg == nil {
		return
	}
	from := headerBody(msg.From)
	to := headerBody(msg.To)
	callID := headerBody(msg.CallID)
	cseq := headerBody(msg.CSeq)
	log.Info("SIP request received",
		log.String("method", parser.MethodName(msg.Method())),
		log.String("from", from),
		log.String("to", to),
		log.String("call_id", callID),
		log.String("cseq", cseq),
	)
}

// logResponse emits a structured log line for an outgoing SIP reply.
func logResponse(msg *parser.SIPMsg, status int, reason string) {
	if msg == nil {
		return
	}
	log.Info("SIP response sent",
		log.Int("status", status),
		log.String("reason", reason),
		log.String("method", parser.MethodName(msg.Method())),
		log.String("call_id", headerBody(msg.CallID)),
	)
}

// logForwardedRequest emits a structured log line when a request is
// forwarded to a downstream target.
func logForwardedRequest(msg *parser.SIPMsg, target string) {
	if msg == nil {
		return
	}
	log.Info("SIP request forwarded",
		log.String("method", parser.MethodName(msg.Method())),
		log.String("target", target),
		log.String("call_id", headerBody(msg.CallID)),
	)
}

// logLatency emits a log line that captures the processing time of a
// single phase (e.g. "request_dispatch" or "nat_detection").
func logLatency(phase string, start time.Time) {
	elapsed := time.Since(start)
	log.Info("Processing latency",
		log.String("phase", phase),
		log.String("elapsed", fmt.Sprintf("%.3fms", float64(elapsed.Nanoseconds())/1e6)),
	)
}

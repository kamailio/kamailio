// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * PIDF/XPIDF document handling - matching C pidf.c
 *
 * PIDF (Presence Information Data Format) - RFC 3863
 * XPIDF (XML Profile for Instant Messaging) - older format.
 */

package presence

import (
	"fmt"
	"strings"
	"time"
)

// PIDFDocument represents a parsed PIDF document.
type PIDFDocument struct {
	URI       string
	Entity    string
	TupleID   string
	Status    PresenceState
	Note      string
	Contact   string
	Priority  float64
	Timestamp time.Time
}

func GeneratePIDF(doc *PIDFDocument) string {
	statusStr := PresenceStatusString(doc.Status)
	if statusStr == "" {
		statusStr = "open"
	}

	entity := doc.Entity
	if entity == "" {
		entity = "pres:" + doc.URI
	}

	tupleID := doc.TupleID
	if tupleID == "" {
		tupleID = fmt.Sprintf("pidf-%d", time.Now().UnixNano())
	}

	note := ""
	if doc.Note != "" {
		note = fmt.Sprintf("    <note xml:lang=\"en\">%s</note>\n", escapeXML(doc.Note))
	}

	contact := ""
	if doc.Contact != "" {
		priority := doc.Priority
		if priority <= 0 {
			priority = 0.8
		}
		contact = fmt.Sprintf("    <contact priority=\"%.1f\">%s</contact>\n", priority, doc.Contact)
	}

	ts := doc.Timestamp
	if ts.IsZero() {
		ts = time.Now().UTC()
	}

	return fmt.Sprintf(
		`<?xml version="1.0" encoding="UTF-8"?>
<presence xmlns="urn:ietf:params:xml:ns:pidf"
          entity="%s">
  <tuple id="%s">
    <status>
      <basic>%s</basic>
    </status>
%s%s    <timestamp>%s</timestamp>
  </tuple>
</presence>`,
		entity, tupleID, statusStr, contact, note, ts.Format(time.RFC3339))
}

func ParsePIDF(xmlContent string) (*PIDFDocument, error) {
	doc := &PIDFDocument{}

	if idx := strings.Index(xmlContent, `entity="`); idx >= 0 {
		rest := xmlContent[idx+len(`entity="`):]
		if endIdx := strings.Index(rest, `"`); endIdx >= 0 {
			doc.Entity = rest[:endIdx]
		}
	}

	if idx := strings.Index(xmlContent, "<basic>"); idx >= 0 {
		rest := xmlContent[idx+len("<basic>"):]
		if endIdx := strings.Index(rest, "</basic>"); endIdx >= 0 {
			doc.Status = ParsePresenceState(strings.TrimSpace(rest[:endIdx]))
		}
	}

	if idx := strings.Index(xmlContent, "<contact"); idx >= 0 {
		rest := xmlContent[idx:]
		if startIdx := strings.Index(rest, ">"); startIdx >= 0 {
			content := rest[startIdx+1:]
			if endIdx := strings.Index(content, "</contact>"); endIdx >= 0 {
				doc.Contact = strings.TrimSpace(content[:endIdx])
			}
		}
	}

	if idx := strings.Index(xmlContent, "<note"); idx >= 0 {
		rest := xmlContent[idx:]
		if startIdx := strings.Index(rest, ">"); startIdx >= 0 {
			content := rest[startIdx+1:]
			if endIdx := strings.Index(content, "</note>"); endIdx >= 0 {
				doc.Note = strings.TrimSpace(content[:endIdx])
			}
		}
	}

	return doc, nil
}

func GenerateXPIDF(status PresenceState, uri, contactURI string, priority float64) string {
	statusStr := "online"
	switch status {
	case PresenceStateClosed:
		statusStr = "offline"
	case PresenceStateOpen:
		statusStr = "online"
	case PresenceStateBusy:
		statusStr = "busy"
	case PresenceStateAway, PresenceStateXAway:
		statusStr = "away"
	case PresenceStateDND:
		statusStr = "busy"
	}

	pri := priority
	if pri <= 0 {
		pri = 0.8
	}

	return fmt.Sprintf(
		`<?xml version="1.0" encoding="UTF-8"?>
<xpidf version="1.0">
  <presence status="%s">
    <atom uri="pres:%s" address="%s" priority="%.1f">
      <status status="%s"/>
    </atom>
  </presence>
</xpidf>`,
		statusStr, uri, contactURI, pri, statusStr)
}

func ParseXPIDF(xmlContent string) (*PIDFDocument, error) {
	doc := &PIDFDocument{}

	if idx := strings.Index(xmlContent, `<presence status="`); idx >= 0 {
		rest := xmlContent[idx+len(`<presence status="`):]
		if endIdx := strings.Index(rest, `"`); endIdx >= 0 {
			doc.Status = ParsePresenceState(rest[:endIdx])
		}
	}

	if idx := strings.Index(xmlContent, `uri="pres:`); idx >= 0 {
		rest := xmlContent[idx+len(`uri="pres:`):]
		if endIdx := strings.Index(rest, `"`); endIdx >= 0 {
			doc.URI = rest[:endIdx]
		}
	}

	if idx := strings.Index(xmlContent, `address="`); idx >= 0 {
		rest := xmlContent[idx+len(`address="`):]
		if endIdx := strings.Index(rest, `"`); endIdx >= 0 {
			doc.Contact = rest[:endIdx]
		}
	}

	return doc, nil
}

func GenerateDialogInfo(entity, dialogID, direction, dialogState, remoteID, remoteTarget, localID string, version int, durationSec int) string {
	return fmt.Sprintf(
		`<?xml version="1.0" encoding="UTF-8"?>
<dialog-info xmlns="urn:ietf:params:xml:ns:dialog-info"
             version="%d" state="full"
             entity="sip:%s">
  <dialog id="%s" direction="%s">
    <state>%s</state>
    <remote>
      <identity>sip:%s</identity>
      <target uri="sip:%s"/>
    </remote>
    <local>
      <identity>sip:%s</identity>
    </local>
    <duration>%d</duration>
  </dialog>
</dialog-info>`,
		version, entity, dialogID, direction, dialogState, remoteID, remoteTarget, localID, durationSec)
}

func GenerateMWI(account string, newMsg, oldMsg, newUrgent, oldUrgent int) string {
	return fmt.Sprintf(
		`<?xml version="1.0" encoding="UTF-8"?>
<message-summary xmlns="urn:ietf:params:xml:ns:message-summary">
  <messages-account>sip:%s</messages-account>
  <msg-count new="%d" old="%d" new-urgent="%d" old-urgent="%d">voice-message</msg-count>
</message-summary>`,
		account, newMsg, oldMsg, newUrgent, oldUrgent)
}

func escapeXML(s string) string {
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	s = strings.ReplaceAll(s, "\"", "&quot;")
	return s
}

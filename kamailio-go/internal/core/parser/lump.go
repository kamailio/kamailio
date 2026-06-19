// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Lump operations for message modification - matching C data_lump.h
 *
 * Lumps are used to track modifications to SIP messages without
 * actually modifying the original buffer. This allows efficient
 * message manipulation and reconstruction.
 */

package parser

import (
	"github.com/kamailio/kamailio-go/internal/core/str"
)

// LumpOp represents lump operation types
// C: enum lump_op
type LumpOp int

const (
	LumpNOP LumpOp = 0 // no operation
	LumpDel LumpOp = 1 // delete
	LumpAdd LumpOp = 2 // add/insert
)

// LumpFlag represents lump flags
// C: enum lump_flags
type LumpFlag uint32

const (
	LumpFlagNone     LumpFlag = 0
	LumpFlagBody     LumpFlag = 1 << 0 // modify body
	LumpFlagHeader   LumpFlag = 1 << 1 // modify header
	LumpFlagShmem    LumpFlag = 1 << 2 // in shared memory
	LumpFlagIsDel    LumpFlag = 1 << 3 // delete operation
	LumpFlagIsAdd    LumpFlag = 1 << 4 // add operation
	LumpFlagIsBefore LumpFlag = 1 << 5 // insert before offset
	LumpFlagIsAfter  LumpFlag = 1 << 6 // insert after offset
)

// LumpAnchor represents where to anchor the lump
// C: enum lump_anchor
type LumpAnchor int

const (
	AnchorStart LumpAnchor = 0 // anchor at start of message
	AnchorEnd   LumpAnchor = 1 // anchor at end of message
)

// Lump represents a single modification operation
// C: struct lump
type Lump struct {
	Op       LumpOp
	Flags    LumpFlag
	Offset   int    // offset in original buffer
	Len      int    // length to delete (for LUMP_DEL)
	NewValue []byte // new value to insert (for LUMP_ADD)
	Next     *Lump  // next lump in list
	Before   *Lump  // lumps to insert before this one
	After    *Lump  // lumps to insert after this one
}

// LumpList represents a list of lump operations
// C: struct lump_head
type LumpList struct {
	Head *Lump
	Tail *Lump
}

// NewLump creates a new lump operation
func NewLump(op LumpOp, offset, len int, value []byte) *Lump {
	return &Lump{
		Op:       op,
		Offset:   offset,
		Len:      len,
		NewValue: value,
	}
}

// Append adds a lump to the end of the list
func (ll *LumpList) Append(lump *Lump) {
	if ll.Head == nil {
		ll.Head = lump
		ll.Tail = lump
	} else {
		ll.Tail.Next = lump
		ll.Tail = lump
	}
}

// Prepend adds a lump to the beginning of the list
func (ll *LumpList) Prepend(lump *Lump) {
	if ll.Head == nil {
		ll.Head = lump
		ll.Tail = lump
	} else {
		lump.Next = ll.Head
		ll.Head = lump
	}
}

// InsertBefore inserts a lump before another lump
func (l *Lump) InsertBefore(newLump *Lump) {
	if l.Before == nil {
		l.Before = newLump
	} else {
		// Add to end of before list
		curr := l.Before
		for curr.Next != nil {
			curr = curr.Next
		}
		curr.Next = newLump
	}
}

// InsertAfter inserts a lump after another lump
func (l *Lump) InsertAfter(newLump *Lump) {
	if l.After == nil {
		l.After = newLump
	} else {
		// Add to end of after list
		curr := l.After
		for curr.Next != nil {
			curr = curr.Next
		}
		curr.Next = newLump
	}
}

// AnchorLump represents an anchored lump for header/body modifications
// C: struct lump_anchor
type AnchorLump struct {
	Before *LumpList // lumps before anchor
	After  *LumpList // lumps after anchor
}

// MsgLumps contains all lump lists for a message
// C: struct sip_msg.lumps
type MsgLumps struct {
	AddRM       LumpList // add/remove lumps
	BodyLumps   LumpList // body modifications
	ReplyLumps  LumpList // reply modifications
	HeadLumps   LumpList // head modifications
}

// DeleteLump creates a delete lump operation
func DeleteLump(offset, len int) *Lump {
	return &Lump{
		Op:     LumpDel,
		Flags:  LumpFlagIsDel,
		Offset: offset,
		Len:    len,
	}
}

// AddLump creates an add lump operation
func AddLump(offset int, value []byte, before bool) *Lump {
	flags := LumpFlagIsAdd
	if before {
		flags |= LumpFlagIsBefore
	} else {
		flags |= LumpFlagIsAfter
	}
	return &Lump{
		Op:       LumpAdd,
		Flags:    flags,
		Offset:   offset,
		NewValue: value,
	}
}

// ReplaceLump creates a replace operation (del + add)
func ReplaceLump(offset, len int, value []byte) *Lump {
	return &Lump{
		Op:       LumpAdd,
		Flags:    LumpFlagIsAdd | LumpFlagIsDel,
		Offset:   offset,
		Len:      len,
		NewValue: value,
	}
}

// LumpManager manages lump operations for a message
type LumpManager struct {
	AddRM      *LumpList
	BodyLumps  *LumpList
	ReplyLumps *LumpList
	HeadLumps  *LumpList
}

// NewLumpManager creates a new lump manager
func NewLumpManager() *LumpManager {
	return &LumpManager{
		AddRM:      &LumpList{},
		BodyLumps:  &LumpList{},
		ReplyLumps: &LumpList{},
		HeadLumps:  &LumpList{},
	}
}

// DelLump adds a delete lump to the message
// C: del_lump
func (lm *LumpManager) DelLump(offset, len int, flags LumpFlag) *Lump {
	lump := DeleteLump(offset, len)
	lump.Flags |= flags
	lm.AddRM.Append(lump)
	return lump
}

// AddLumpToMsg adds an insert lump to the message
// C: insert_new_lump
func (lm *LumpManager) AddLumpToMsg(offset int, value []byte, flags LumpFlag) *Lump {
	lump := AddLump(offset, value, false)
	lump.Flags |= flags
	lm.AddRM.Append(lump)
	return lump
}

// AddLumpBefore adds a lump before the given offset
// C: insert_new_lump_before
func (lm *LumpManager) AddLumpBefore(offset int, value []byte, flags LumpFlag) *Lump {
	lump := AddLump(offset, value, true)
	lump.Flags |= flags
	lm.AddRM.Append(lump)
	return lump
}

// AddBodyLump adds a lump for body modification
// C: add_body_lump
func (lm *LumpManager) AddBodyLump(offset int, value []byte, flags LumpFlag) *Lump {
	lump := AddLump(offset, value, false)
	lump.Flags |= flags | LumpFlagBody
	lm.BodyLumps.Append(lump)
	return lump
}

// DelBodyLump adds a delete lump for body modification
func (lm *LumpManager) DelBodyLump(offset, len int, flags LumpFlag) *Lump {
	lump := DeleteLump(offset, len)
	lump.Flags |= flags | LumpFlagBody
	lm.BodyLumps.Append(lump)
	return lump
}

// AnchorHeader returns an anchor lump for header modifications
// C: anchor_lump
func (lm *LumpManager) AnchorHeader(msg *SIPMsg, hdr *HdrField) *AnchorLump {
	return &AnchorLump{
		Before: &LumpList{},
		After:  &LumpList{},
	}
}

// DelHeaderLump deletes a header using lumps
// C: del_lump
func (lm *LumpManager) DelHeaderLump(hdr *HdrField) *Lump {
	if hdr == nil {
		return nil
	}
	// Delete from header name to end of header (including CRLF)
	offset := hdr.Offset
	len := hdr.Len
	return lm.DelLump(offset, len, LumpFlagHeader)
}

// ReplaceHeaderLump replaces a header value
func (lm *LumpManager) ReplaceHeaderLump(hdr *HdrField, newValue string) *Lump {
	if hdr == nil {
		return nil
	}
	// Delete old header and add new one
	lump := lm.DelHeaderLump(hdr)
	if lump != nil {
		// Add new header after deletion
		newLump := AddLump(hdr.Offset, []byte(newValue+"\r\n"), false)
		newLump.Flags |= LumpFlagHeader
		lump.InsertAfter(newLump)
	}
	return lump
}

// ToMsgLumps converts the LumpManager's collections into a MsgLumps
// suitable for RebuildMsg.
func (lm *LumpManager) ToMsgLumps() *MsgLumps {
	return &MsgLumps{
		AddRM:      *lm.AddRM,
		BodyLumps:  *lm.BodyLumps,
		ReplyLumps: *lm.ReplyLumps,
		HeadLumps:  *lm.HeadLumps,
	}
}

// AppendBodyLump appends data to the message body using a lump.
// C: append_body_lump
func (lm *LumpManager) AppendBodyLump(data []byte) *Lump {
	lump := AddLump(0, data, false)
	lump.Flags |= LumpFlagBody
	lm.BodyLumps.Append(lump)
	return lump
}

// ReplaceBodyLump replaces the entire message body.
func (lm *LumpManager) ReplaceBodyLump(offset, len int, newBody []byte) *Lump {
	lump := ReplaceLump(offset, len, newBody)
	lump.Flags |= LumpFlagBody
	lm.BodyLumps.Append(lump)
	return lump
}

// InsertHeaderLumpAfter inserts a header lump after a specific header.
// C: insert_new_lump_after
func (lm *LumpManager) InsertHeaderLumpAfter(hdr *HdrField, value []byte) *Lump {
	if hdr == nil {
		return nil
	}
	offset := hdr.Offset + hdr.Len
	lump := AddLump(offset, value, false)
	lump.Flags |= LumpFlagHeader | LumpFlagIsAfter
	lm.AddRM.Append(lump)
	return lump
}

// InsertHeaderLumpBefore inserts a header lump before a specific header.
// C: insert_new_lump_before
func (lm *LumpManager) InsertHeaderLumpBefore(hdr *HdrField, value []byte) *Lump {
	if hdr == nil {
		return nil
	}
	lump := AddLump(hdr.Offset, value, true)
	lump.Flags |= LumpFlagHeader | LumpFlagIsBefore
	lm.AddRM.Prepend(lump)
	return lump
}

// AddHeaderLump adds a new header
func (lm *LumpManager) AddHeaderLump(msg *SIPMsg, header string, after bool) *Lump {
	if msg == nil {
		return nil
	}

	// Find insertion point (after last header, before body)
	offset := 0
	if len(msg.Headers) > 0 {
		lastHdr := msg.Headers[len(msg.Headers)-1]
		offset = lastHdr.Offset + lastHdr.Len
	}

	lump := AddLump(offset, []byte(header+"\r\n"), false)
	lump.Flags |= LumpFlagHeader
	if after {
		lm.AddRM.Append(lump)
	} else {
		lm.AddRM.Prepend(lump)
	}
	return lump
}

// Count returns the total number of lumps
func (ll *LumpList) Count() int {
	count := 0
	for l := ll.Head; l != nil; l = l.Next {
		count++
	}
	return count
}

// Clear removes all lumps from the list
func (ll *LumpList) Clear() {
	ll.Head = nil
	ll.Tail = nil
}

// IsEmpty returns true if the list is empty
func (ll *LumpList) IsEmpty() bool {
	return ll.Head == nil
}

// First returns the first lump
func (ll *LumpList) First() *Lump {
	return ll.Head
}

// Last returns the last lump
func (ll *LumpList) Last() *Lump {
	return ll.Tail
}

// Clone creates a copy of the lump list
func (ll *LumpList) Clone() *LumpList {
	newList := &LumpList{}
	for l := ll.Head; l != nil; l = l.Next {
		newLump := &Lump{
			Op:       l.Op,
			Flags:    l.Flags,
			Offset:   l.Offset,
			Len:      l.Len,
			NewValue: append([]byte(nil), l.NewValue...),
		}
		// Clone before/after lists
		if l.Before != nil {
			beforeList := &LumpList{}
			for b := l.Before; b != nil; b = b.Next {
				beforeList.Append(&Lump{
					Op:       b.Op,
					Flags:    b.Flags,
					Offset:   b.Offset,
					Len:      b.Len,
					NewValue: append([]byte(nil), b.NewValue...),
				})
			}
			newLump.Before = beforeList.Head
		}
		if l.After != nil {
			afterList := &LumpList{}
			for a := l.After; a != nil; a = a.Next {
				afterList.Append(&Lump{
					Op:       a.Op,
					Flags:    a.Flags,
					Offset:   a.Offset,
					Len:      a.Len,
					NewValue: append([]byte(nil), a.NewValue...),
				})
			}
			newLump.After = afterList.Head
		}
		newList.Append(newLump)
	}
	return newList
}

// Helper functions for common operations

// DelHeader deletes a header from a message
func DelHeader(msg *SIPMsg, hdr *HdrField) error {
	if msg == nil || hdr == nil {
		return nil
	}

	// Create lump manager if not exists
	lm := NewLumpManager()

	// Add delete lump
	lm.DelHeaderLump(hdr)

	return nil
}

// AddHeader adds a new header to a message
func AddHeader(msg *SIPMsg, header string) error {
	if msg == nil {
		return nil
	}

	lm := NewLumpManager()
	lm.AddHeaderLump(msg, header, true)

	return nil
}

// ReplaceHeader replaces a header in a message
func ReplaceHeader(msg *SIPMsg, hdr *HdrField, newValue string) error {
	if msg == nil || hdr == nil {
		return nil
	}

	lm := NewLumpManager()
	lm.ReplaceHeaderLump(hdr, newValue)

	return nil
}

// AppendHeader appends a header value to existing header
func AppendHeader(msg *SIPMsg, hdr *HdrField, value str.Str) error {
	if msg == nil || hdr == nil {
		return nil
	}

	// Create replacement header with appended value
	newValue := string(hdr.Body.S) + ", " + string(value.S)

	lm := NewLumpManager()
	lm.ReplaceHeaderLump(hdr, newValue)

	return nil
}

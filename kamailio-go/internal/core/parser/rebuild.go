// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Message rebuild from lumps - matching C msg_translator.c
 *
 * This module reconstructs a SIP message by applying lump operations
 * to the original message buffer.
 */

package parser

import (
	"bytes"
	"sort"
)

// RebuildResult contains the result of message rebuilding
type RebuildResult struct {
	Buffer     []byte
	Len        int
	Modified   bool
	Operations int
}

// LumpEntry represents a lump operation for sorting
type LumpEntry struct {
	Offset int
	Len    int
	Op     LumpOp
	Value  []byte
	Flags  LumpFlag
}

// RebuildMsg reconstructs a message by applying all lump operations
// C: build_req_buf_from_sip_req / build_res_buf_from_sip_res
func RebuildMsg(msg *SIPMsg, lumps *MsgLumps) (*RebuildResult, error) {
	if msg == nil {
		return nil, nil
	}

	result := &RebuildResult{
		Modified: false,
	}

	// Collect all lump operations
	entries := collectLumpEntries(lumps)
	if len(entries) == 0 {
		// No modifications, return original buffer
		result.Buffer = msg.Buf
		result.Len = msg.Len
		return result, nil
	}

	// Sort by offset (descending for deletions, ascending for insertions)
	sort.Slice(entries, func(i, j int) bool {
		return entries[i].Offset < entries[j].Offset
	})

	// Apply lumps to build new buffer
	var newBuf bytes.Buffer
	originalBuf := msg.Buf
	lastPos := 0

	// Process entries in reverse order (from end to start)
	// This ensures offsets remain valid
	for i := len(entries) - 1; i >= 0; i-- {
		e := entries[i]

		// Copy data before this lump
		if e.Offset > lastPos {
			newBuf.Write(originalBuf[lastPos:e.Offset])
		}

		switch e.Op {
		case LumpDel:
			// Skip the deleted portion
			lastPos = e.Offset + e.Len
			result.Modified = true
			result.Operations++

		case LumpAdd:
			// Insert new value
			newBuf.Write(e.Value)
			lastPos = e.Offset
			result.Modified = true
			result.Operations++

		default:
			// NOP, just copy
			lastPos = e.Offset
		}
	}

	// Copy remaining data after last lump
	if lastPos < len(originalBuf) {
		newBuf.Write(originalBuf[lastPos:])
	}

	result.Buffer = newBuf.Bytes()
	result.Len = len(result.Buffer)

	return result, nil
}

// collectLumpEntries collects all lump operations into a sorted list
func collectLumpEntries(lumps *MsgLumps) []LumpEntry {
	var entries []LumpEntry

	if lumps == nil {
		return entries
	}

	// Collect from AddRM list
	for l := lumps.AddRM.Head; l != nil; l = l.Next {
		entries = append(entries, LumpEntry{
			Offset: l.Offset,
			Len:    l.Len,
			Op:     l.Op,
			Value:  l.NewValue,
			Flags:  l.Flags,
		})
		// Collect before lumps
		for b := l.Before; b != nil; b = b.Next {
			entries = append(entries, LumpEntry{
				Offset: b.Offset,
				Len:    b.Len,
				Op:     b.Op,
				Value:  b.NewValue,
				Flags:  b.Flags,
			})
		}
		// Collect after lumps
		for a := l.After; a != nil; a = a.Next {
			entries = append(entries, LumpEntry{
				Offset: a.Offset,
				Len:    a.Len,
				Op:     a.Op,
				Value:  a.NewValue,
				Flags:  a.Flags,
			})
		}
	}

	// Collect from BodyLumps list
	for l := lumps.BodyLumps.Head; l != nil; l = l.Next {
		entries = append(entries, LumpEntry{
			Offset: l.Offset,
			Len:    l.Len,
			Op:     l.Op,
			Value:  l.NewValue,
			Flags:  l.Flags,
		})
	}

	// Collect from ReplyLumps list
	for l := lumps.ReplyLumps.Head; l != nil; l = l.Next {
		entries = append(entries, LumpEntry{
			Offset: l.Offset,
			Len:    l.Len,
			Op:     l.Op,
			Value:  l.NewValue,
			Flags:  l.Flags,
		})
	}

	// Collect from HeadLumps list
	for l := lumps.HeadLumps.Head; l != nil; l = l.Next {
		entries = append(entries, LumpEntry{
			Offset: l.Offset,
			Len:    l.Len,
			Op:     l.Op,
			Value:  l.NewValue,
			Flags:  l.Flags,
		})
	}

	return entries
}

// RebuildMsgSimple is a simpler rebuild function for basic modifications
func RebuildMsgSimple(msg *SIPMsg, lumps *LumpList) ([]byte, error) {
	if msg == nil {
		return nil, nil
	}

	if lumps == nil || lumps.IsEmpty() {
		return msg.Buf, nil
	}

	// Collect lumps
	var entries []LumpEntry
	for l := lumps.Head; l != nil; l = l.Next {
		entries = append(entries, LumpEntry{
			Offset: l.Offset,
			Len:    l.Len,
			Op:     l.Op,
			Value:  l.NewValue,
			Flags:  l.Flags,
		})
	}

	// Sort by offset (descending)
	sort.Slice(entries, func(i, j int) bool {
		return entries[i].Offset > entries[j].Offset
	})

	// Apply lumps
	result := make([]byte, len(msg.Buf))
	copy(result, msg.Buf)

	for _, e := range entries {
		switch e.Op {
		case LumpDel:
			// Delete portion
			if e.Offset+e.Len <= len(result) {
				result = append(result[:e.Offset], result[e.Offset+e.Len:]...)
			}
		case LumpAdd:
			// Insert new value
			if e.Offset <= len(result) {
				newResult := make([]byte, 0, len(result)+len(e.Value))
				newResult = append(newResult, result[:e.Offset]...)
				newResult = append(newResult, e.Value...)
				newResult = append(newResult, result[e.Offset:]...)
				result = newResult
			}
		}
	}

	return result, nil
}

// ApplyLump applies a single lump operation to a buffer
func ApplyLump(buf []byte, lump *Lump) []byte {
	if lump == nil {
		return buf
	}

	switch lump.Op {
	case LumpDel:
		if lump.Offset+lump.Len <= len(buf) {
			newBuf := make([]byte, 0, len(buf)-lump.Len)
			newBuf = append(newBuf, buf[:lump.Offset]...)
			newBuf = append(newBuf, buf[lump.Offset+lump.Len:]...)
			return newBuf
		}

	case LumpAdd:
		if lump.Offset <= len(buf) {
			newBuf := make([]byte, 0, len(buf)+len(lump.NewValue))
			newBuf = append(newBuf, buf[:lump.Offset]...)
			newBuf = append(newBuf, lump.NewValue...)
			newBuf = append(newBuf, buf[lump.Offset:]...)
			return newBuf
		}
	}

	return buf
}

// ApplyLumpList applies a list of lump operations to a buffer
func ApplyLumpList(buf []byte, lumps *LumpList) []byte {
	if lumps == nil || lumps.IsEmpty() {
		return buf
	}

	// Collect and sort lumps
	var entries []LumpEntry
	for l := lumps.Head; l != nil; l = l.Next {
		entries = append(entries, LumpEntry{
			Offset: l.Offset,
			Len:    l.Len,
			Op:     l.Op,
			Value:  l.NewValue,
			Flags:  l.Flags,
		})
	}

	// Sort by offset (descending to maintain correct offsets)
	sort.Slice(entries, func(i, j int) bool {
		return entries[i].Offset > entries[j].Offset
	})

	// Apply each lump
	result := buf
	for _, e := range entries {
		switch e.Op {
		case LumpDel:
			if e.Offset+e.Len <= len(result) {
				newResult := make([]byte, 0, len(result)-e.Len)
				newResult = append(newResult, result[:e.Offset]...)
				newResult = append(newResult, result[e.Offset+e.Len:]...)
				result = newResult
			}

		case LumpAdd:
			if e.Offset <= len(result) {
				newResult := make([]byte, 0, len(result)+len(e.Value))
				newResult = append(newResult, result[:e.Offset]...)
				newResult = append(newResult, e.Value...)
				newResult = append(newResult, result[e.Offset:]...)
				result = newResult
			}
		}
	}

	return result
}

// CalculateNewLen calculates the new message length after applying lumps
func CalculateNewLen(msg *SIPMsg, lumps *MsgLumps) int {
	if msg == nil {
		return 0
	}

	newLen := msg.Len

	if lumps == nil {
		return newLen
	}

	// Sum up all changes
	for l := lumps.AddRM.Head; l != nil; l = l.Next {
		switch l.Op {
		case LumpDel:
			newLen -= l.Len
		case LumpAdd:
			newLen += len(l.NewValue)
		}
	}

	for l := lumps.BodyLumps.Head; l != nil; l = l.Next {
		switch l.Op {
		case LumpDel:
			newLen -= l.Len
		case LumpAdd:
			newLen += len(l.NewValue)
		}
	}

	return newLen
}

// ValidateLumps validates that all lump operations are within bounds
func ValidateLumps(msg *SIPMsg, lumps *MsgLumps) bool {
	if msg == nil || lumps == nil {
		return true
	}

	// Check AddRM lumps
	for l := lumps.AddRM.Head; l != nil; l = l.Next {
		if l.Offset < 0 || l.Offset > msg.Len {
			return false
		}
		if l.Op == LumpDel && l.Offset+l.Len > msg.Len {
			return false
		}
	}

	// Check BodyLumps
	for l := lumps.BodyLumps.Head; l != nil; l = l.Next {
		if l.Offset < 0 || l.Offset > msg.Len {
			return false
		}
		if l.Op == LumpDel && l.Offset+l.Len > msg.Len {
			return false
		}
	}

	return true
}

// MergeLumpLists merges multiple lump lists into one
func MergeLumpLists(lists ...*LumpList) *LumpList {
	result := &LumpList{}
	for _, list := range lists {
		if list == nil {
			continue
		}
		for l := list.Head; l != nil; l = l.Next {
			result.Append(&Lump{
				Op:       l.Op,
				Flags:    l.Flags,
				Offset:   l.Offset,
				Len:      l.Len,
				NewValue: l.NewValue,
			})
		}
	}
	return result
}

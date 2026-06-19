// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * TM module type definitions - matching C h_table.h
 */

package tm

import (
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/parser"
	"github.com/kamailio/kamailio-go/internal/core/str"
	"github.com/kamailio/kamailio-go/internal/core/transport"
)

// Transaction states
// C: enum tstates
type TState int

const (
	TStateUndefined TState = iota
	// UAS states
	TStateTrying      // 100 trying sent
	TStateProceeding  // 1xx sent
	TStateCompleted   // final response sent
	TStateConfirmed   // ACK received (INVITE only)
	// UAC states
	TStateCalling     // request sent, no response
	TStateProceedingUAC // 1xx received
	TStateCompletedUAC  // final response received
	// Additional UAS states
	TStateACKCalled          // ACK sent for non-2xx (UAS only)
	TStateDestroyed          // transaction destroyed (waiting for GC)
	// Additional UAC states
	TStateCompletedUACNoACK  // UAC received non-2xx, waiting for ACK timer
)

// Transaction flags
// C: T_IS_INVITE_FLAG, T_CANCELED, etc.
type TFlags uint32

const (
	TIsInvite     TFlags = 1 << 0
	TIsLocal      TFlags = 1 << 1
	TNoisyCTimer  TFlags = 1 << 2
	TCanceled     TFlags = 1 << 3
	T6xxReceived  TFlags = 1 << 4
	TInAgony      TFlags = 1 << 5
	TAutoInv100   TFlags = 1 << 6
	TNoAutoACK    TFlags = 1 << 7
	TDisable6xx   TFlags = 1 << 8
	TDisableFailover TFlags = 1 << 9
	TPassProvisional TFlags = 1 << 10
	TAsyncContinue   TFlags = 1 << 11
	TDisableInternalReply TFlags = 1 << 12
	TAdminReply    TFlags = 1 << 13
	TAsyncSuspended TFlags = 1 << 14
)

// RetransmitBuffer represents a retransmission buffer
// C: struct retr_buf
type RetransmitBuffer struct {
	Type       int16 // TYPE_REQUEST, TYPE_LOCAL_CANCEL, TYPE_LOCAL_ACK, or status code
	Flags      uint16
	TimerActive uint16
	Branch     uint16
	Buffer     []byte
	BufferLen  int
	MyT        *Cell
	Dest       *transport.DestInfo
	RetrExpire time.Time
	FRExpire   time.Time
}

// UAServer represents User Agent Server content
// C: struct ua_server
type UAServer struct {
	Request      *parser.SIPMsg
	Response     *RetransmitBuffer
	LocalToTag   str.Str
	Status       uint16
	CancelReason string
}

// UAClient represents User Agent Client content
// C: struct ua_client
type UAClient struct {
	Reply        *parser.SIPMsg
	Request      *RetransmitBuffer
	LocalCancel  *RetransmitBuffer
	LocalACK     *RetransmitBuffer
	URI          str.Str
	DstURI       str.Str
	Path         str.Str
	LastReceived int
	Flags        uint32
	BranchFlags  uint32
	ICode        int
	OnFailure    int
	OnBranchFailure int
	OnReply      int
}

// ToTagElem represents a to-tag element for ACK matching
// C: struct totag_elem
type ToTagElem struct {
	Next  *ToTagElem
	Tag   str.Str
	Acked int32
}

// Cell represents a transaction cell
// C: struct cell
type Cell struct {
	sync.RWMutex

	// Hash table linkage
	NextC      *Cell
	PrevC      *Cell
	HashIndex  uint32
	Label      uint32

	// Transaction info
	Flags      TFlags
	NrOfOutgoings int

	// Reference counting
	RefCount   int32

	// Key fields for matching
	FromHdr    str.Str
	CallIDHdr  str.Str
	CSeqHdrN   str.Str
	ToHdr      str.Str
	CallIDVal  str.Str
	CSeqNum    str.Str
	CSeqMet    str.Str
	Method     str.Str
	MethodValue parser.RequestMethod

	// Via branch for transaction matching
	ViaBranch  str.Str

	// UAS and UAC
	UAS        UAServer
	UAC        []*UAClient

	// To-tags for 200/INVITE ACK matching
	FwdedToTags *ToTagElem

	// Timers
	FRTimeout      time.Duration
	FRInvTimeout   time.Duration
	RTT1Timeout    time.Duration
	RTT2Timeout    time.Duration
	EndOfLife      time.Time
	WaitStart      time.Time

	// State
	State          TState

	// Reply info
	RelayedReplyBranch int
	OnFailure      int
	OnBranchFailure int
	OnReply        int
	OnBranch       int

	// Created timestamp
	CreatedAt      time.Time
}

// IsInvite returns true if the transaction is for an INVITE
func (c *Cell) IsInvite() bool {
	return c.Flags&TIsInvite != 0
}

// IsCanceled returns true if the transaction was canceled
func (c *Cell) IsCanceled() bool {
	return c.Flags&TCanceled != 0
}

// IsLocal returns true if this is a locally generated transaction
func (c *Cell) IsLocal() bool {
	return c.Flags&TIsLocal != 0
}

// StateString returns a human-readable name for the transaction state.
func (c *Cell) StateString() string {
	switch c.State {
	case TStateTrying:
		return "Trying"
	case TStateProceeding:
		return "Proceeding"
	case TStateCompleted:
		return "Completed"
	case TStateConfirmed:
		return "Confirmed"
	case TStateCalling:
		return "Calling"
	case TStateProceedingUAC:
		return "ProceedingUAC"
	case TStateCompletedUAC:
		return "CompletedUAC"
	case TStateACKCalled:
		return "ACKCalled"
	case TStateDestroyed:
		return "Destroyed"
	default:
		return "Undefined"
	}
}

// SetState sets the transaction state directly (used by timer callbacks).
// C: t_set_state()
func (c *Cell) SetState(s TState) {
	c.State = s
}

// Ref increments the reference count
func (c *Cell) Ref() {
	atomic.AddInt32(&c.RefCount, 1)
}

// Unref decrements the reference count and returns true if the cell should be deleted
func (c *Cell) Unref() bool {
	newCount := atomic.AddInt32(&c.RefCount, -1)
	return newCount <= 0
}

// Entry represents a hash table entry (collision list)
// C: struct entry
type Entry struct {
	sync.RWMutex
	NextC      *Cell
	PrevC      *Cell
	NextLabel  uint32
}

// Table represents the transaction table
// C: struct s_table
type Table struct {
	sync.RWMutex
	Entries    []*Entry
	Size       uint32
}

// NewTable creates a new transaction table
func NewTable(size uint32) *Table {
	entries := make([]*Entry, size)
	for i := range entries {
		entries[i] = &Entry{}
	}
	return &Table{
		Entries: entries,
		Size:    size,
	}
}

// Hash computes the hash for a transaction key
func (t *Table) Hash(callID str.Str, cseq str.Str, viaBranch str.Str) uint32 {
	// Simple hash combining callid, cseq, and branch
	var h uint32
	for i := 0; i < callID.Len; i++ {
		h = h*31 + uint32(callID.S[i])
	}
	for i := 0; i < cseq.Len; i++ {
		h = h*31 + uint32(cseq.S[i])
	}
	for i := 0; i < viaBranch.Len; i++ {
		h = h*31 + uint32(viaBranch.S[i])
	}
	return h % t.Size
}

// Insert adds a cell to the transaction table
func (t *Table) Insert(cell *Cell) {
	hash := cell.HashIndex
	entry := t.Entries[hash]

	entry.Lock()
	defer entry.Unlock()

	// Generate label
	cell.Label = atomic.AddUint32(&entry.NextLabel, 1)
	cell.Ref() // reference for being in the hash table

	// Add to list
	if entry.NextC == nil {
		entry.NextC = cell
		entry.PrevC = cell
		cell.NextC = cell
		cell.PrevC = cell
	} else {
		cell.NextC = entry.NextC
		cell.PrevC = entry.PrevC
		entry.PrevC.NextC = cell
		entry.PrevC = cell
	}
}

// Remove removes a cell from the transaction table
func (t *Table) Remove(cell *Cell) {
	hash := cell.HashIndex
	entry := t.Entries[hash]

	entry.Lock()
	defer entry.Unlock()

	// Unlink from list
	if cell.NextC == cell {
		// Last cell in list
		entry.NextC = nil
		entry.PrevC = nil
	} else {
		cell.PrevC.NextC = cell.NextC
		cell.NextC.PrevC = cell.PrevC
		if entry.NextC == cell {
			entry.NextC = cell.NextC
		}
		if entry.PrevC == cell {
			entry.PrevC = cell.PrevC
		}
	}

	cell.NextC = nil
	cell.PrevC = nil

	// Unref for hash table reference
	if cell.Unref() {
		// Cell should be deleted - but we need to be careful about concurrent access
		// In Go, we let GC handle this when no more references exist
	}
}

// Lookup finds a transaction by key
func (t *Table) Lookup(callID str.Str, cseq str.Str, viaBranch str.Str) *Cell {
	hash := t.Hash(callID, cseq, viaBranch)
	entry := t.Entries[hash]

	entry.RLock()
	defer entry.RUnlock()

	// Search in collision list
	for cell := entry.NextC; cell != nil; cell = cell.NextC {
		if cell.CallIDVal.Equal(callID) && cell.CSeqNum.Equal(cseq) {
			// Additional check for branch if provided
			if viaBranch.Len > 0 && !cell.ViaBranch.Equal(viaBranch) {
				continue // Branch doesn't match, skip this cell
			}
			cell.Ref()
			return cell
		}
		if cell.NextC == entry.NextC {
			break // wrapped around
		}
	}

	return nil
}

// LookupByCallIDCSeq searches all entries in the table for a transaction
// matching the given Call-ID and CSeq, ignoring the Via branch.
// This is used as a fallback when the branch-aware hash lookup fails
// (e.g., in proxy scenarios where the response's Via branch differs
// from the stored transaction's branch).
func (t *Table) LookupByCallIDCSeq(callID str.Str, cseq str.Str) *Cell {
	for i := range t.Entries {
		entry := t.Entries[i]
		entry.RLock()
		for cell := entry.NextC; cell != nil; cell = cell.NextC {
			if cell.CallIDVal.Equal(callID) && cell.CSeqNum.Equal(cseq) {
				cell.Ref()
				entry.RUnlock()
				return cell
			}
			if cell.NextC == entry.NextC {
				break
			}
		}
		entry.RUnlock()
	}
	return nil
}

// LookupByMsg finds a transaction from a SIP message.
// It uses branch-aware lookup to match NewTransaction's storage hash.
func (t *Table) LookupByMsg(msg *parser.SIPMsg) *Cell {
	if msg == nil || msg.CallID == nil || msg.CSeq == nil {
		return nil
	}

	callID := msg.CallID.Body
	cseqBody, err := parser.ParseCSeqHeader(msg.CSeq)
	if err != nil {
		return nil
	}
	// Convert cseq number to str
	cseqStr := str.Mk(fmt.Sprintf("%d", cseqBody.Number))

	// Extract Via branch for branch-aware lookup (same as NewTransaction)
	var viaBranch str.Str
	if vb, err := msg.GetParsedVia(); err == nil && vb != nil && vb.Branch != nil {
		viaBranch = vb.Branch.Value
	} else if msg.Via1 != nil && msg.Via1.Branch != nil {
		viaBranch = msg.Via1.Branch.Value
	}

	return t.Lookup(callID, cseqStr, viaBranch)
}

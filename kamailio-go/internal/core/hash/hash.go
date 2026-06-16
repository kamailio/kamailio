// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Hash functions - matching C hash_func.c for SIP message hashing
 *
 * Used for Call-ID, branch parameter, and transaction hashing
 * to maintain compatibility with C implementation behavior.
 */

package hash

import (
	"crypto/md5"
	"hash"
)

// Generic hash table interface
// C: hash functions in hash_func.c

// HashFunc is a generic hash function type
type HashFunc func(data []byte) uint32

// table entry for hash table implementation
type Entry struct {
	Key   string
	Value interface{}
	Next  *Entry
}

// Table is a generic hash table implementation
type Table struct {
	entries []*Entry
	size   uint32
	used   uint32
}

// NewTable creates a new hash table with the given size
func NewTable(size uint32) *Table {
	return &Table{
		entries: make([]*Entry, size),
		size:    size,
		used:    0,
	}
}

// hash1 is the first hash function (from C hash_func.c)
func hash1(key []byte, size uint32) uint32 {
	var h uint32
	for i := 0; i < len(key); i++ {
		h = h*33 + uint32(key[i])
	}
	return h % size
}

// hash2 is the second hash function (from C hash_func.c)
func hash2(key []byte, size uint32) uint32 {
	var h uint32
	h = ((h >> 16) ^ h) * 0x45d9f3b
	h = ((h >> 16) ^ h) * 0x45d9f3b
	h = (h >> 16) ^ h
	return h % size
}

// HashString hashes a string key
func HashString(key string) uint32 {
	var h uint32
	for i := 0; i < len(key); i++ {
		h = h*33 + uint32(key[i])
	}
	return h
}

// HashBytes hashes a byte slice
func HashBytes(data []byte) uint32 {
	var h uint32
	for i := 0; i < len(data); i++ {
		h = h*33 + uint32(data[i])
	}
	return h
}

// hash1_mod is the modular hash function for hash table
func hash1_mod(key []byte, size uint32) uint32 {
	return hash1(key, size)
}

// hash2_mod is the secondary hash for linear probing
func hash2_mod(key []byte, size uint32) uint32 {
	return hash2(key, size)
}

// MemberHash computes hash for Call-ID / branch parameter
// Matches C's member_hash function
func MemberHash(callID []byte, cseq []byte, viaBranch []byte) uint32 {
	h := md5.New()
	h.Write(callID)
	h.Write(cseq)
	if len(viaBranch) > 0 {
		h.Write(viaBranch)
	}
	sum := h.Sum(nil)

	// Convert MD5 hash to uint32 (same as C)
	return (uint32(sum[0]) << 24) |
		(uint32(sum[1]) << 16) |
		(uint32(sum[2]) << 8) |
		uint32(sum[3])
}

// ParamHash computes hash for HDR_PARAMETER (used in hdr_field)
// Matches C's param_hash
func ParamHash(param []byte) uint32 {
	var h uint32
	for i := 0; i < len(param); i++ {
		h = h*33 + uint32(param[i])
	}
	return h
}

// NewHashFunc creates a hash.Hash for general use
func NewHashFunc() hash.Hash {
	return md5.New()
}

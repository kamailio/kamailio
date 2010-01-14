/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#ifndef __BINRPC_HT_H__
#define __BINRPC_HT_H__

#ifdef _LIBBINRPC_BUILD

#include <inttypes.h>
#include <stdlib.h>

#include "list.h"
#include "misc.h"
#include "lock.h"
#include "mem.h"
#include "errnr.h"


typedef uint32_t	hval_t;

/* link to be embedded into custom structure */
typedef struct {
	struct brpc_list_head cell;
	hval_t hval;
	unsigned long label;
} ht_lnk_t;

/* HT is an array of slots. this is how one looks like */
struct ht_slot {
	struct brpc_list_head head;
	unsigned long cnt;
	brpc_lock_t *lock;
};

/* HT */
typedef struct {
	uint32_t sz;
	hval_t hmask; /* save one substraction op per index retrieving */
	struct ht_slot **slots;
} ht_t;


/**
 * API
 */

/* New hash table allocated&inited. */
__LOCAL ht_t *ht_new(size_t size);
/* Purge HT. */
__LOCAL void ht_del(ht_t *ht);

/**
 * Non thread safe methods 
 */
/* Insert a HT link into the HT. */
__LOCAL bool ht_ins(ht_t *ht, ht_lnk_t *lnk);
/* Delete HT link from HT. */
__LOCAL void ht_rem(ht_lnk_t *lnk);
/* Lookup a HT link, given the hash value and link label */
__LOCAL ht_lnk_t *ht_lnk_lkup(ht_t *ht, hval_t hval, unsigned long label);
/**
 * Iterate over the links having a given hash value. Not safe over pos
 * removal!
 * @param pos ht_lnk_t* : current position
 * @param _ht_ Hash Table reference
 * @param hval The hash value
 * @param tmp struct brpc_list_head * : temporary variable (internal place holder)
 */
/*
#define ht_for_hval(pos, _ht_, hval, tmp) 
(definition below)
*/

/**
 * Mutual eXclusive methods (thread safe).
 */
/* Insert a HT link into the HT. */
__LOCAL bool ht_ins_mx(ht_t *ht, ht_lnk_t *lnk);
/* Delete HT link from HT. */
__LOCAL bool ht_rem_mx(ht_t *ht, ht_lnk_t *lnk);
/* Lookup in and remove from a HT a link, starting from hash value and label.
 * The found link reference is returned (or NULL). */
__LOCAL ht_lnk_t *ht_lnk_lkup_rem_mx(ht_t *ht, hval_t hval, 
		unsigned long label);

/* obtain the wrapping structure where a ht_lnk_t is embedded */
#define ht_entry(ptr, type, member)	brpc_container_of(ptr, type, member)
/* hash a string */
__LOCAL hval_t hash_str(const char *val, size_t len);
/* initialize a link */
#define HT_LINK_INIT(_lnk_, _hval) \
	do { \
		INIT_LIST_HEAD(&(_lnk_)->cell); \
		(_lnk_)->hval = _hval; \
	} while (0)
/* initialize a hash link for a string (brpc_str_t) */
#define HT_LINK_INIT_STR(_lnk_, _cstr_, _len) \
	do { \
		hval_t hval = hash_str(_cstr_, _len); \
		HT_LINK_INIT(_lnk_, hval); \
	} while (0)
/* retrieve the hash value of a link */
#define HT_LINK_HVAL(_lnk_)	(_lnk_)->hval
/* retrieve the label of a link (only consistent after HT insertion!) */
#define HT_LINK_LABEL(_lnk_)	(_lnk_)->label
/* retrieve HT slot, given a hash value */
#define HT_SLOT4HASH(_ht_, hval)	(_ht_)->slots[hval & (_ht_)->hmask]
/* lock/unlock a whole slot, given the hash; op status 'returned' */
#define HT_LOCK_GET(_ht_, hval) brpc_lock_get(HT_SLOT4HASH(_ht_, hval)->lock)
#define HT_LOCK_LET(_ht_, hval) brpc_lock_let(HT_SLOT4HASH(_ht_, hval)->lock)


/**
 * Implementation
 */

__LOCAL ht_t *ht_new(size_t size)
{
	ht_t *ht;
	int i;
	uint32_t sz;

	/* first 2^k greater or equal to size */
	for (sz = 1; sz < size; sz <<= 1)
		;
#ifndef NDEBUG
	DBG("HT size %zd%s.\n", sz, (sz != size) ? " (adjusted)" : "");
#endif

	ht = brpc_calloc(1, sizeof(ht_t));
	if (! ht) {
		WERRNO(ENOMEM);
		return NULL;
	}
	ht->sz = sz;
	ht->hmask = sz - 1;

#ifdef FIX_FALSE_GCC_WARNS
	i = 0;
#endif

	ht->slots = (struct ht_slot **)brpc_malloc(sz * sizeof(struct ht_slot *));
	if (! ht->slots) {
		WERRNO(ENOMEM);
		goto enomem;
	}

	for (i = 0; i < sz; i ++) {
		ht->slots[i] = (struct ht_slot *)brpc_calloc(1, 
				sizeof(struct ht_slot));
		if (! ht->slots[i]) {
			WERRNO(ENOMEM);
			goto enomem;
		}
		INIT_LIST_HEAD(&ht->slots[i]->head);
		ht->slots[i]->lock = brpc_lock_new();
		if (! ht->slots[i]->lock)
			goto elock;
	}

	return ht;

elock:
	brpc_free(ht->slots[i]);
enomem:
	if (ht->slots) {
		brpc_free(ht->slots);
		for (i = i - 1; 0 < i; i --) {
			brpc_lock_del(ht->slots[i]->lock);
			brpc_free(ht->slots[i]);
		}
	}
	brpc_free(ht);
	return NULL;
}

__LOCAL void ht_del(ht_t *ht)
{
	int i;

	for (i = 0; i < ht->sz; i ++) {
		brpc_lock_del(ht->slots[i]->lock);
		brpc_free(ht->slots[i]);
	}
	brpc_free(ht->slots);
	brpc_free(ht);
}


__LOCAL bool ht_ins(ht_t *ht, ht_lnk_t *lnk)
{
	struct ht_slot *slot;
#ifndef NDEBUG
	if (! list_empty(&lnk->cell)) {
		ERR("ht lnk already inserted.\n");
		WERRNO(EINVAL);
		return false;
	}
#endif /* NDEBUG */
	slot = HT_SLOT4HASH(ht, lnk->hval);
	list_add_tail(&lnk->cell, &slot->head);
	lnk->label = slot->cnt ++;
	return true;
}

__LOCAL void ht_rem(ht_lnk_t *lnk)
{
	list_del(&lnk->cell);
}

/* walks the list of a slot, stopping at entries matching a given hash val */
__LOCAL ht_lnk_t *_ht_slot_walk(struct brpc_list_head *end, 
		struct brpc_list_head *prev, hval_t hval)
{
	struct brpc_list_head *pos;
	ht_lnk_t *lnk;
	for (pos = prev->next; pos != end; pos = pos->next) {
		lnk = brpc_list_entry(pos, ht_lnk_t, cell);
		if (lnk->hval == hval)
			return lnk;
	}
	return NULL;
}

#if 0
#define _ht_slot_walk(end, last, hval) \
({ \
	ht_lnk_t *kpos; \
	struct brpc_list_head *pos; \
	for (pos = (last)->next; pos != end; pos = pos->next) { \
		kpos = brpc_list_entry(pos, ht_lnk_t, cell); \
		if (kpos->hval == hval) \
			break; \
	} \
	if (pos == end) \
		kpos = NULL; \
	kpos; \
})
#endif

#define ht_for_hval(pos, _ht_, hval, tmp) \
	for ( \
		/* head of list of cells belonging to determined slot */ \
		tmp = &HT_SLOT4HASH(_ht_, hval)->head, \
		/* first match */ \
		pos = _ht_slot_walk(tmp, tmp, hval); \
		pos; \
		/* next match */ \
		pos = _ht_slot_walk(tmp, &pos->cell, hval))

__LOCAL ht_lnk_t *ht_lnk_lkup(ht_t *ht, hval_t hval, unsigned long label)
{
	struct brpc_list_head *head, *pos;
	ht_lnk_t *lnk;
	head = &HT_SLOT4HASH(ht, hval)->head;
	list_for_each(pos, head) {
		lnk = brpc_list_entry(pos, ht_lnk_t, cell);
		if (lnk->label == label)
			return lnk;
	}
	return NULL;
}


#define _SLOT_LOCK_GET(_slot_) \
	if (brpc_lock_get((_slot_)->lock) != 0) { \
		WERRNO(ELOCK); \
		ERR("failed to acquire lock for slot.\n"); \
		goto elock; \
	}
#define _SLOT_LOCK_LET(_slot_) \
	if (brpc_lock_let((_slot_)->lock) != 0) { \
		WERRNO(ELOCK); \
		ERR("failed to release lock for slot.\n"); \
		abort(); \
		/* formal, if no SIGABRT handling */ \
		goto elock; \
	}

__LOCAL bool ht_ins_mx(ht_t *ht, ht_lnk_t *lnk)
{
	struct ht_slot *slot;

#ifndef NDEBUG
	if (! list_empty(&lnk->cell)) {
		ERR("ht lnk already inserted.\n");
		WERRNO(EINVAL);
		return false;
	}
#endif /* NDEBUG */

	slot = HT_SLOT4HASH(ht, lnk->hval);
	_SLOT_LOCK_GET(slot);
	list_add_tail(&lnk->cell, &slot->head);
	lnk->label = slot->cnt ++;
	_SLOT_LOCK_LET(slot);
	
	return true;
elock:
	return false;
}

__LOCAL bool ht_rem_mx(ht_t *ht, ht_lnk_t *lnk)
{
	struct ht_slot *slot = HT_SLOT4HASH(ht, lnk->hval);
	_SLOT_LOCK_GET(slot);
	list_del(&lnk->cell);
	_SLOT_LOCK_LET(slot);
	return true;
elock:
	return false;
}

__LOCAL ht_lnk_t *ht_lnk_lkup_rem_mx(ht_t *ht, hval_t hval, 
		unsigned long label)
{
	struct brpc_list_head *head, *pos;
	ht_lnk_t *lnk;
	struct ht_slot *slot;

	slot = HT_SLOT4HASH(ht, hval);
	head = &HT_SLOT4HASH(ht, hval)->head;
	_SLOT_LOCK_GET(slot);
	list_for_each(pos, head) {
		lnk = brpc_list_entry(pos, ht_lnk_t, cell);
		if (lnk->label == label) {
			ht_rem(lnk);
			_SLOT_LOCK_LET(slot);
			return lnk;
		}
	}
	_SLOT_LOCK_LET(slot);
elock:
	return NULL;
}


__LOCAL hval_t hash_str(const char *str, size_t len)
{
    const char* p;
    register hval_t v;
    register hval_t h;

    h=0;
    for (p = str; p <= str + len - 4; p += 4){
        v = rd32(p);
        h += v ^ (v>>3);
    }
    v=0;
    for (; p < str + len; p++) {
        v<<=8;
        v+=*p;
    }
    h += v ^ (v>>3);

    h=((h) + (h>>11)) + ((h>>13) + (h>>23));

    return h;
}


#endif /* _LIBBINRPC_BUILD */

#endif /* __BINRPC_HT_H__ */

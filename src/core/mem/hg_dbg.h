/*
 * slab allocator with elastic arenas - debug checks
 *
 * Copyright (C) 2026 Yury Kirsanov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef hg_dbg_h
#define hg_dbg_h

#include "hg_malloc.h"

/* debug checks (hb->dbg): a trailer and red zone after the requested size,
 * and freed cells poisoned and held in a per-thread quarantine */
#define HG_DBG_TAIL (sizeof(unsigned long))
#define HG_DBG_REDZONE 0xcc
#define HG_DBG_POISON 0x6b
#define HG_QUAR_CELLS 256 /* cells one thread holds per arena */
#define HG_QUAR_MIN (16UL << 10)
#define HG_QUAR_MAX (1UL << 20)

/* stamp / check the trailer and red zone of cell @cs (@cap payload bytes,
 * @size asked for); check returns 0 if intact */
void hg_dbg_tail_set(void *cs, unsigned long cap, unsigned long size);
int hg_dbg_tail_check(struct hg_block *hb, void *cs, unsigned long cap);

/* per-thread quarantine: put poisons a freed cell, drain checks and releases
 * them, reset empties it without releasing (after fork) */
void hg_quar_put(
		struct hg_block *hb, struct hg_palloc *pl, unsigned int c, char *cs);
unsigned int hg_quar_drain(struct hg_block *hb, struct hg_palloc *pl);
void hg_quar_reset(struct hg_quar *q);
/* thread exit: free an emptied quarantine's mapping */
void hg_quar_unmap(struct hg_quar *q);

#endif /* hg_dbg_h */

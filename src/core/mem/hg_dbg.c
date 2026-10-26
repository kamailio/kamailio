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

#ifdef HG_MALLOC

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "hg_malloc.h"
#include "hg_arena.h"
#include "hg_dbg.h"
#include "../dprint.h"

/* debug arenas (hgdbg, hb->dbg set): tail red zone on every allocation and a
 * poisoned quarantine for freed cells; the normal paths only test the flag */

/* the trailer is stored XORed with this, so neither the red zone's own
 * bytes nor a stray small integer reads back as a plausible size */
static inline unsigned long tail_key(const void *cs)
{
	unsigned long long x =
			((unsigned long long)(unsigned long)cs ^ hg_hdr_secret)
			* 0x9e3779b97f4a7c15ULL;

	return (unsigned long)(x ^ (x >> 32));
}

/* the first of @len bytes that is not @byte, or @len */
static unsigned long first_unlike(
		const unsigned char *b, unsigned long len, unsigned char byte)
{
	unsigned long i;

	for(i = 0; i < len; i++)
		if(b[i] != byte)
			break;
	return i;
}

/* @size + HG_DBG_TAIL <= @cap: the allocation was sized for it */
void hg_dbg_tail_set(void *cs, unsigned long cap, unsigned long size)
{
	char *pay = (char *)cs + HG_CELL_HDR;
	unsigned long enc = size ^ tail_key(cs);

	memset(pay + size, HG_DBG_REDZONE, cap - HG_DBG_TAIL - size);
	memcpy(pay + cap - HG_DBG_TAIL, &enc, HG_DBG_TAIL);
}

int hg_dbg_tail_check(struct hg_block *hb, void *cs, unsigned long cap)
{
	const unsigned char *pay = (const unsigned char *)cs + HG_CELL_HDR;
	unsigned long enc, size, i;
	char what[64];

	memcpy(&enc, pay + cap - HG_DBG_TAIL, HG_DBG_TAIL);
	size = enc ^ tail_key(cs);
	if(size <= cap - HG_DBG_TAIL) {
		i = size
			+ first_unlike(
					pay + size, cap - HG_DBG_TAIL - size, HG_DBG_REDZONE);
		if(i == cap - HG_DBG_TAIL)
			return 0;
		snprintf(what, sizeof what, "the %lu bytes asked for (byte %lu)", size,
				i);
	} else {
		/* the trailer itself was overwritten: the size is lost */
		snprintf(what, sizeof what, "its size (trailer lost)");
	}
#ifdef DBG_HG_MALLOC
	LM_CRIT("%s: %p (%lu bytes) was written past %s - allocated at "
			"%s: %s(%lu)\n",
			hb->name, (const void *)pay, cap, what,
			HG_CELL_AT(cs)->file ? HG_CELL_AT(cs)->file : "?",
			HG_CELL_AT(cs)->func ? HG_CELL_AT(cs)->func : "?",
			HG_CELL_AT(cs)->line);
#else
	LM_CRIT("%s: %p (%lu bytes) was written past %s\n", hb->name,
			(const void *)pay, cap, what);
#endif
	hg_corrupt(hb, HG_C_OVERFLOW);
	return -1;
}

/* freed cells are poisoned and parked here, oldest out first, before reuse;
 * per thread and per arena, bounded by HG_QUAR_CELLS and hb->quar_bytes */
struct hg_quar
{
	unsigned int head;				  /* oldest entry */
	unsigned int n;					  /* entries held */
	unsigned long bytes;			  /* cell bytes held */
	char *cell[HG_QUAR_CELLS];		  /* cell starts */
	unsigned char cls[HG_QUAR_CELLS]; /* their classes */
};

/* a cell leaving quarantine must still be all poison under an intact FREE
 * header; a cell written after its free is leaked, never reused */
static int quar_check(struct hg_block *hb, char *cs, unsigned int c)
{
	const unsigned char *pay = (const unsigned char *)cs + HG_CELL_HDR;
	unsigned long cap = hg_cell_total_size(c) - HG_CELL_HDR, i;

	if(hg_hdr_state(cs) != HG_ST_FREE || HG_CELL_AT(cs)->cls != c) {
		LM_CRIT("%s: the header of %p was overwritten while the cell sat "
				"in quarantine - leaking it\n",
				hb->name, (const void *)pay);
		hg_corrupt(hb, HG_C_HEADER);
		return -1;
	}
	i = first_unlike(pay, cap, HG_DBG_POISON);
	if(i == cap)
		return 0;
#ifdef DBG_HG_MALLOC
	LM_CRIT("%s: %p (%lu bytes) was written after it was freed - byte %lu "
			"is 0x%02x; freed at %s: %s(%lu) - leaking it\n",
			hb->name, (const void *)pay, cap, i, pay[i],
			HG_CELL_AT(cs)->file ? HG_CELL_AT(cs)->file : "?",
			HG_CELL_AT(cs)->func ? HG_CELL_AT(cs)->func : "?",
			HG_CELL_AT(cs)->line);
#else
	LM_CRIT("%s: %p (%lu bytes) was written after it was freed - byte %lu "
			"is 0x%02x - leaking it\n",
			hb->name, (const void *)pay, cap, i, pay[i]);
#endif
	hg_corrupt(hb, HG_C_USE_AFTER_FREE);
	return -1;
}

static void quar_evict(
		struct hg_block *hb, struct hg_palloc *pl, struct hg_quar *q)
{
	char *cs = q->cell[q->head];
	unsigned int c = q->cls[q->head];

	q->head = (q->head + 1) % HG_QUAR_CELLS;
	q->n--;
	q->bytes -= hg_cell_total_size(c);
	hg_san_open(hb, cs, hg_cell_total_size(c) - HG_CELL_HDR);
	if(quar_check(hb, cs, c) == 0) {
		hg_san_take(hb, cs, hg_cell_total_size(c) - HG_CELL_HDR);
		hg_cell_push_private(hb, pl, c, cs);
	} else {
		/* leaked: closed, so a further stale access is reported too */
		hg_san_close(hb, cs, hg_cell_total_size(c) - HG_CELL_HDR);
	}
}

void hg_quar_put(
		struct hg_block *hb, struct hg_palloc *pl, unsigned int c, char *cs)
{
	struct hg_quar *q = pl->quar;
	unsigned int i;

	if(!q) {
		void *m = mmap(NULL, sizeof *q, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if(m == MAP_FAILED) {
			/* no quarantine: the cell goes back at once, unchecked */
			hg_san_take(hb, cs, hg_cell_total_size(c) - HG_CELL_HDR);
			hg_cell_push_private(hb, pl, c, cs);
			return;
		}
		q = pl->quar = m;
	}
	memset(cs + HG_CELL_HDR, HG_DBG_POISON,
			hg_cell_total_size(c) - HG_CELL_HDR);
	hg_san_close(hb, cs, hg_cell_total_size(c) - HG_CELL_HDR);
	while(q->n == HG_QUAR_CELLS
			|| (q->n && q->bytes + hg_cell_total_size(c) > hb->quar_bytes))
		quar_evict(hb, pl, q);
	i = (q->head + q->n) % HG_QUAR_CELLS;
	q->cell[i] = cs;
	q->cls[i] = (unsigned char)c;
	q->n++;
	q->bytes += hg_cell_total_size(c);
}

void hg_quar_reset(struct hg_quar *q)
{
	q->head = q->n = 0;
	q->bytes = 0;
}

void hg_quar_unmap(struct hg_quar *q)
{
	if(q)
		munmap(q, sizeof *q);
}

unsigned int hg_quar_drain(struct hg_block *hb, struct hg_palloc *pl)
{
	struct hg_quar *q = pl->quar;
	unsigned int n = 0;

	for(; q && q->n; n++)
		quar_evict(hb, pl, q);
	return n;
}

#endif /* HG_MALLOC */

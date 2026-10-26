/*
 * slab allocator with elastic arenas - arena growth and shrink
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

#include <string.h>
#include <unistd.h>

#include "hg_malloc.h"
#include "hg_buddy_int.h"
#include "../dprint.h"

/* invariant: committed_bytes + punched_pages * hps == span_end; a mismatch
 * is an accounting slip, counted as internal corruption */
static void span_check(struct hg_block *hb, const char *after)
{
	unsigned long holes = hb->punched_pages << hb->hps_shift;

	if(hb->committed_bytes + holes == hb->span_end)
		return;
	LM_CRIT("%s: after %s committed_bytes %lu + %lu punched != span_end "
			"%lu\n",
			hb->name, after, hb->committed_bytes, holes, hb->span_end);
	hg_corrupt(hb, HG_C_INTERNAL);
}

/* reserve floor at 1/16 of the committed pages, not the span: holes are
 * not carvable */
static inline void floor_recompute(struct hg_block *hb)
{
	hb->reserve_floor =
			((hb->npages - hb->punched_pages) * hg_leaves_per_page(hb)) / 16;
}

/* --- grow ------------------------------------------------------------- */

/* a host-side refusal: arm on the first, latch if refused again after a
 * full GC pass; warns then, and the maintenance process repeats it for shm */
static int grow_resource_refused(struct hg_block *hb)
{
	hb->grow_refused++;

	if(hb->grow_blocked)
		return -1;

	if(!hb->grow_blocked_mark) {
		/* gc_passes + 1 is never 0, so it doubles as the armed flag */
		hb->grow_blocked_mark = hb->gc_passes + 1;
		hb->grow_blocked_refuse0 = hb->grow_refused;
	} else if(hb->gc_passes >= hb->grow_blocked_mark) {
		hb->grow_blocked = 1;
		hb->grow_event_due = 1;
		LM_WARN("%s: GROW-BLOCKED latched - the arena cannot grow and "
				"a GC pass did not change that (%lu refusals so far). "
				"Alert on %s; details precede this line.\n",
				hb->name, hb->grow_refused,
				hb->shared ? "hg_shm_grow_blocked" : "hg_pkg_grow_blocked");
	}
	return -1;
}

/* the blocked state ends two ways; both say so if there is anything to
 * end, and both re-arm the once-per-episode messages */
void hg_grow_unblock(struct hg_block *hb, const char *how)
{
	if(hb->grow_blocked)
		LM_NOTICE("%s: GROW-BLOCKED cleared - %s\n", hb->name, how);
	hb->grow_blocked = 0;
	hb->grow_blocked_mark = 0;
	hb->grow_blocked_refuse0 = 0;
	hb->grow_event_due = 0;
	hb->grow_refuse_said = 0;
	hb->grow_refuse_mlock = 0;
}

/* sweep half of the latch: latch if an armed episode still refuses one
 * interval later; hb->lock held */
void hg_grow_blocked_tick(struct hg_block *hb)
{
	if(hb->grow_blocked || !hb->grow_blocked_mark)
		return;
	if(hb->grow_refused > hb->grow_blocked_refuse0) {
		hb->grow_blocked = 1;
		hb->grow_event_due = 1;
		LM_WARN("%s: GROW-BLOCKED latched - the arena cannot grow and "
				"a full sweep interval did not change that (%lu refusals "
				"so far). Alert on %s; details precede this line.\n",
				hb->name, hb->grow_refused,
				hb->shared ? "hg_shm_grow_blocked" : "hg_pkg_grow_blocked");
	} else {
		/* armed but quiet for a whole interval: the spike passed */
		hb->grow_blocked_mark = 0;
		hb->grow_blocked_refuse0 = 0;
	}
}

/* max wait, lock released, for another worker's grow in flight */
#define HG_GROW_WAIT_MAX_NS 2000000000UL
#define HG_GROW_WAIT_SLICE_US 100

static int hg_buddy_regrow_holes_unlocks(
		struct hg_block *hb, unsigned long need, unsigned long room);

/* MADV_COLLAPSE for pages a worker's grow left on 4 KB: one pass per
 * maintenance tick, lock released, HG_COLLAPSE_BUDGET_NS per tick */
#define HG_COLLAPSE_BUDGET_NS (100UL * 1000000UL)

void hg_collapse_deferred(struct hg_block *hb)
{
	unsigned long start = hg_now_ns(), i;
	struct hg_page *pg;
	int rc;

	if(!__atomic_load_n(&hb->collapse_pending, __ATOMIC_ACQUIRE))
		return;
	while(hg_now_ns() - start < HG_COLLAPSE_BUDGET_NS) {
		hg_lock_enter(hb, HG_LK_POLICY);
		pg = NULL;
		for(i = hb->collapse_scan; i < hb->npages; i++)
			if(hb->pages[i].want_collapse && !hb->pages[i].releasing
					&& !hb->pages[i].punched) {
				pg = &hb->pages[i];
				break;
			}
		if(!pg) {
			/* end of this tick's pass; the next tick starts over */
			hb->collapse_scan = 0;
			if(!hb->collapse_wanting)
				hb->collapse_pending = 0;
			hg_lock_leave(hb);
			return;
		}
		hb->collapse_scan = i + 1;
		hg_lock_leave(hb);

		rc = hg_mem_collapse(hb, pg->base, hb->hps);

		hg_lock_enter(hb, HG_LK_POLICY);
		if(!pg->want_collapse) {
			/* released meanwhile: not counted either way */
		} else if(rc > 0) {
			if(pg->tier == HG_MEM_4K) {
				if(hb->tier_bytes[HG_MEM_4K] >= hb->hps)
					hb->tier_bytes[HG_MEM_4K] -= hb->hps;
				hb->tier_bytes[HG_MEM_THP_COLLAPSE] += hb->hps;
				pg->tier = HG_MEM_THP_COLLAPSE;
			}
			pg->want_collapse = 0;
			hb->collapse_wanting--;
			hb->collapsed_late++;
		} else if(rc < 0 && --pg->want_collapse) {
			hb->collapse_retries++;
			if(!hb->collapse_fail_said) {
				hb->collapse_fail_said = 1;
				LM_INFO("%s: deferred collapse of a huge page failed (%s) "
						"- retrying it on later ticks\n",
						hb->name, strerror(-rc));
			}
		} else {
			/* tries used up, or the host forbids collapse now */
			pg->want_collapse = 0;
			hb->collapse_wanting--;
			hb->collapse_late_failed++;
			LM_NOTICE("%s: a huge page range stays on 4 KB pages - %s\n",
					hb->name,
					rc < 0 ? strerror(-rc)
						   : "the host no longer allows collapse");
		}
		hg_lock_leave(hb);
	}
}

static int buddy_grow_body(
		struct hg_block *hb, unsigned long need, enum hg_grow_why why)
{
	unsigned long delta, room, old_pages, i, limit, t0, off, commit_ns;
	struct hg_room_check rc;
	int tier, reason, later;

	if(!hb->buddy_ready)
		return -1;

	/* another worker's grow is in flight: wait for it unlocked, then retry */
	if(hb->grow_inflight) {
		unsigned long gen = hb->grow_gen, waited = 0;

		hb->grow_waits++;
		reason = hb->lk_cur;
		t0 = hg_now_ns();
		/* the token can be retaken between the poll and the relock: wait again
		 * within the same budget */
		do {
			hg_lock_leave(hb);
			do {
				usleep(HG_GROW_WAIT_SLICE_US);
				waited = hg_now_ns() - t0;
			} while(__atomic_load_n(&hb->grow_inflight, __ATOMIC_ACQUIRE)
					&& waited < HG_GROW_WAIT_MAX_NS);
			hg_lock_enter(hb, (enum hg_lock_reason)reason);
			/* pages published: retry; a trim or punch holding it is not */
			if(hb->grow_gen != gen)
				return 0;
		} while(hb->grow_inflight && waited < HG_GROW_WAIT_MAX_NS);
		if(hb->grow_inflight) {
			hb->grow_wait_timeouts++; /* still populating - give up */
			hb->grow_refused++;
			hb->grow_refuse_why = HG_GR_WAIT_TIMEOUT;
			return -1;
		}
		/* the in-flight operation went away; fall through and try ours */
	}

	limit = hb->hcap;
	/* room in committed bytes; holes are refilled first (below) */
	room = limit > hb->committed_bytes ? limit - hb->committed_bytes : 0;
	if(room == 0) {
		hb->grow_refused++;
		hb->grow_refuse_why = HG_GR_CAP;
		/* said once per episode, and only for an arena that grew: a fixed arena
		 * lives at room 0 */
		if(hb->grows && !hb->grow_refuse_said) {
			hb->grow_refuse_said = 1;
			LM_NOTICE("%s: at the %lu MB growth ceiling (%s), "
					  "a %lu byte request must fail - counting further "
					  "refusals in grow_refused\n",
					hb->name, limit >> 20, "the -m/-M reservation", need);
		}
		return -1;
	}

	/* holes first: their address is already ours, a refill costs a pin */
	{
		int r = hg_buddy_regrow_holes_unlocks(hb, need, room);

		if(r > 0)
			return 0; /* served: the caller retries */
		if(r < 0)
			return -1; /* refused by the host RAM check */
	}

	delta = hb->grow_granule;
	if(need > delta)
		delta = (need + hb->grow_granule - 1) / hb->grow_granule
				* hb->grow_granule;
	if(delta > room)
		delta = room;

	/* phase 1, locked: reserve the granule's range */
	off = hb->span_end;
	hb->grow_inflight = 1;
	reason = hb->lk_cur;
	hg_lock_leave(hb);

	/* phase 2, unlocked: the host RAM check (it reads /proc), then
	 * populate, pin and verify the backing */
	if(hg_grow_room_short(hb, delta, &rc)) {
		hg_lock_enter(hb, (enum hg_lock_reason)reason);
		__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
		hg_grow_room_refused(hb, delta, &rc);
		return grow_resource_refused(hb);
	}
	t0 = hg_now_ns();
	later = 0;
	tier = hg_mem_commit(hb, off, delta,
			(why == HG_GROW_EXHAUSTION && hb->shared
					&& __atomic_load_n(&hb->maint_active, __ATOMIC_ACQUIRE))
					? &later
					: NULL);
	commit_ns = hg_now_ns() - t0;

	/* phase 3, locked: publish */
	hg_lock_enter(hb, (enum hg_lock_reason)reason);
	__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
	if(tier < 0)
		return grow_resource_refused(hb); /* the commit rolled itself back */

	hb->tier_bytes[tier] += delta;
	/* hb->size feeds the totals and free_to_carve = size - real_used */
	hb->size += delta;
	old_pages = hb->npages;
	hb->npages = (unsigned long)(hb->hbase + off + delta - hb->pbase)
				 >> hb->hps_shift;

	for(i = old_pages; i < hb->npages; i++) {
		hb->pages[i].tier = (unsigned char)tier;
		hb->pages[i].want_collapse = later ? HG_COLLAPSE_TRIES : 0;
		page_publish_whole(hb, &hb->pages[i]);
	}
	if(later) {
		hb->collapse_deferred += hb->npages - old_pages;
		hb->collapse_wanting += hb->npages - old_pages;
		__atomic_store_n(&hb->collapse_pending, 1, __ATOMIC_RELEASE);
	}
	/* the published sizes move last, with release semantics: nothing that
	 * reads them lock-free may see the new figure before the pages exist */
	__atomic_store_n(&hb->span_end, off + delta, __ATOMIC_RELEASE);
	__atomic_store_n(&hb->committed_bytes, hb->committed_bytes + delta,
			__ATOMIC_RELEASE);
	span_check(hb, "a grow");

	/* keep the floor at 1/16 of the grid it now guards */
	floor_recompute(hb);

	hb->grows++;
	hb->grow_gen++;
	hb->grow_bytes += delta;
	if(why == HG_GROW_PROACTIVE)
		hb->grows_proactive++;
	else
		hb->grows_exhaustion++;
	hb->shrink_quiet = 0; /* fresh demand voids any quiet window */
	hb->draining = 0;
	hg_grow_unblock(hb, "the arena grew, the resource came back");

	LM_NOTICE("%s arena grew %s by %lu MB to %lu MB (%lu new pages on %s; "
			  "commit %lu us with the lock released; %lu MB headroom left)\n",
			hb->name,
			why == HG_GROW_PROACTIVE ? "proactively" : "on exhaustion",
			delta >> 20, hb->committed_bytes >> 20, hb->npages - old_pages,
			hg_mem_tier_str((enum hg_mem_tier)tier), commit_ns / 1000,
			(hb->hcap - hb->committed_bytes) >> 20);
	return 0;
}

int hg_buddy_grow_unlocks(
		struct hg_block *hb, unsigned long need, enum hg_grow_why why)
{
	int r;

	hg_lock_assert_held(hb, "hg_buddy_grow_unlocks entry");
	r = buddy_grow_body(hb, need, why);
	hg_lock_assert_held(hb, "hg_buddy_grow_unlocks return");
	return r;
}

/* --- shrink ----------------------------------------------------------- */

/* release whole-free pages from the top of the span, never below
 * committed_min: taken off the list locked, released unlocked, booked locked */
static unsigned long buddy_shrink_body(struct hg_block *hb)
{
	unsigned long lpp = hg_leaves_per_page(hb);
	unsigned long limit = hb->shrink_step >> hb->hps_shift;
	unsigned long n = 0, i, off, len, backed = 0, t0, rel_ns;
	unsigned int top = hb->buddy_top;
	int reason, rc;

	/* charge the floor only for backed pages; punched ones already left */
	while(n < limit) {
		struct hg_page *pg = &hb->pages[hb->npages - 1 - n];
		unsigned long cost = pg->punched ? 0 : hb->hps;
		unsigned long bpages = (backed + cost) >> hb->hps_shift;
		unsigned long floor_after, free_after;

		if(pg->releasing || !page_is_whole_free(hb, pg))
			break;
		if(hb->committed_bytes - backed < hb->committed_min + cost)
			break;
		/* stay above the headroom rule's line, or the next tick regrows it */
		if(hb->buddy_free_leaves < bpages * lpp)
			break;
		free_after = hb->buddy_free_leaves - bpages * lpp;
		floor_after = ((hb->npages - hb->punched_pages - bpages) * lpp) / 16;
		if(free_after < 2 * floor_after)
			break;
		backed += cost;
		n++;
	}
	if(!n)
		return 0;

	len = n << hb->hps_shift;
	off = (unsigned long)(hb->pages[hb->npages - n].base - hb->hbase);
	if(off + len != hb->span_end) {
		/* the grid does not end at the span end: decline */
		return 0;
	}

	/* phase 1, locked: off the free list first, so nothing carves them */
	for(i = hb->npages - n; i < hb->npages; i++) {
		struct hg_page *pg = &hb->pages[i];

		if(!pg->punched)
			fl_unlink(hb, pg->base, top);
		pg->releasing = 1;
	}
	hb->grow_inflight = 1;
	reason = hb->lk_cur;
	hg_lock_leave(hb);

	/* phase 2, unlocked: the syscalls */
	t0 = hg_now_ns();
	rc = hg_mem_release(hb, off, len);
	rel_ns = hg_now_ns() - t0;

	/* phase 3, locked: publish, or put back */
	hg_lock_enter(hb, (enum hg_lock_reason)reason);
	__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
	if(rc != 0) {
		for(i = hb->npages - n; i < hb->npages; i++) {
			struct hg_page *pg = &hb->pages[i];

			pg->releasing = 0;
			if(!pg->punched)
				fl_push(hb, pg->base, top);
		}
		return 0;
	}

	for(i = hb->npages - n; i < hb->npages; i++) {
		struct hg_page *pg = &hb->pages[i];

		pg->releasing = 0;
		bit_clear(pg->bitmap, node_id(top, top, 0));
		memset(pg->leaforder, HG_LEAF_NONE, (size_t)lpp);
		pg->free_leaves = 0;
		pg->run_len = 0;
		if(pg->punched) {
			/* the hole leaves the span with its page; its bytes and
			 * its leaves left the counts at the punch */
			pg->punched = 0;
			if(hb->punched_pages)
				hb->punched_pages--;
		} else {
			hb->buddy_free_leaves -= lpp;
			if(hb->tier_bytes[pg->tier] >= hb->hps)
				hb->tier_bytes[pg->tier] -= hb->hps;
		}
		if(pg->want_collapse) { /* released: nothing left to collapse */
			pg->want_collapse = 0;
			hb->collapse_wanting--;
		}
	}
	hb->npages -= n;
	hb->span_end -= len;
	hb->committed_bytes -= backed;
	hb->size -= len;
	hb->shrinks++;
	hb->shrink_bytes += backed;
	floor_recompute(hb);
	span_check(hb, "a shrink");

	LM_NOTICE("%s arena shrank by %lu MB to %lu MB (%lu pages released "
			  "to the %s in %lu us with the lock released; %lu MB of growth "
			  "still held)\n",
			hb->name, len >> 20, hb->committed_bytes >> 20, n,
			hb->tier == HG_MEM_HUGETLB ? "hugetlb pool" : "host", rel_ns / 1000,
			(hb->committed_bytes - hb->committed_min) >> 20);
	return n;
}

static unsigned long hg_buddy_shrink_unlocks(struct hg_block *hb)
{
	unsigned long r;

	hg_lock_assert_held(hb, "hg_buddy_shrink_unlocks entry");
	r = buddy_shrink_body(hb);
	hg_lock_assert_held(hb, "hg_buddy_shrink_unlocks return");
	return r;
}

/* interior release: punch a whole-free page anywhere in the span
 * (MADV_REMOVE); the page leaves the free list until re-committed */
#define HG_PAGES_PER_EVENT 128 /* per release or re-commit event */
static unsigned long buddy_punch_body(struct hg_block *hb)
{
	unsigned long sel[HG_PAGES_PER_EVENT];
	unsigned long limit = hb->shrink_step >> hb->hps_shift;
	unsigned long lpp = hg_leaves_per_page(hb);
	unsigned long i, got = 0, ok, t0, rel_ns, free_after, floor_after;
	unsigned int top = hb->buddy_top;
	int reason;

	if(!hg_interior_release || hb->shrink_unsupported || !limit)
		return 0;
	if(limit > HG_PAGES_PER_EVENT)
		limit = HG_PAGES_PER_EVENT;

	/* phase 1, locked: choose from the bottom, off the free list first, keep
	 * committed bytes at or above committed_min */
	for(i = 0; i + 1 < hb->npages && got < limit; i++) {
		struct hg_page *pg = &hb->pages[i];

		if(pg->punched || pg->releasing || !page_is_whole_free(hb, pg))
			continue;
		if(hb->committed_bytes
				< hb->committed_min + ((got + 1) << hb->hps_shift))
			break;
		/* the headroom rule's line, as in the trim */
		if(hb->buddy_free_leaves < (got + 1) * lpp)
			break;
		free_after = hb->buddy_free_leaves - (got + 1) * lpp;
		floor_after = ((hb->npages - hb->punched_pages - got - 1) * lpp) / 16;
		if(free_after < 2 * floor_after)
			break;
		fl_unlink(hb, pg->base, top);
		pg->releasing = 1;
		sel[got++] = i;
	}
	if(!got)
		return 0;
	hb->grow_inflight = 1;
	reason = hb->lk_cur;
	hg_lock_leave(hb);

	/* phase 2, unlocked: the syscalls; stop at the first refusal */
	t0 = hg_now_ns();
	for(ok = 0; ok < got; ok++) {
		struct hg_page *pg = &hb->pages[sel[ok]];

		if(hg_mem_release(hb, (unsigned long)(pg->base - hb->hbase), hb->hps)
				!= 0)
			break;
	}
	rel_ns = hg_now_ns() - t0;

	/* phase 3, locked: publish what went, put back what did not */
	hg_lock_enter(hb, (enum hg_lock_reason)reason);
	__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
	for(i = 0; i < got; i++) {
		struct hg_page *pg = &hb->pages[sel[i]];

		pg->releasing = 0;
		if(i >= ok) {
			fl_push(hb, pg->base, top); /* still backed */
			continue;
		}
		pg->punched = 1;
		hb->buddy_free_leaves -= lpp;
		hb->committed_bytes -= hb->hps;
		hb->punches++;
		hb->punch_bytes += hb->hps;
		hb->punched_pages++;
		if(hb->tier_bytes[pg->tier] >= hb->hps)
			hb->tier_bytes[pg->tier] -= hb->hps;
		if(pg->want_collapse) { /* punched: nothing left to collapse */
			pg->want_collapse = 0;
			hb->collapse_wanting--;
		}
	}
	if(ok) {
		floor_recompute(hb);
		span_check(hb, "a punch");
		LM_NOTICE("%s arena punched %lu interior page%s (%lu MB) back "
				  "to the %s in %lu us with the lock released; %lu MB "
				  "committed of a %lu MB span, %lu page%s held as "
				  "holes\n",
				hb->name, ok, ok == 1 ? "" : "s", (ok << hb->hps_shift) >> 20,
				hb->tier == HG_MEM_HUGETLB ? "hugetlb pool" : "host",
				rel_ns / 1000, hb->committed_bytes >> 20, hb->span_end >> 20,
				hb->punched_pages, hb->punched_pages == 1 ? "" : "s");
	}
	return ok;
}

static unsigned long hg_buddy_punch_unlocks(struct hg_block *hb)
{
	unsigned long r;

	hg_lock_assert_held(hb, "hg_buddy_punch_unlocks entry");
	r = buddy_punch_body(hb);
	hg_lock_assert_held(hb, "hg_buddy_punch_unlocks return");
	return r;
}

/* re-commit holes (fault in and re-pin, in this process) before the span
 * grows. 1 served, 0 no holes, -1 refused */
static int buddy_regrow_holes_body(
		struct hg_block *hb, unsigned long need, unsigned long room)
{
	unsigned long sel[HG_PAGES_PER_EVENT];
	unsigned long want, i, got = 0, ok, t0, pin_ns;
	unsigned long lpp = hg_leaves_per_page(hb);
	unsigned int top = hb->buddy_top;
	struct hg_room_check rc;
	int reason;

	if(!hb->punched_pages)
		return 0;

	/* at least a granule, like a span grow, and at least what the
	 * request needs - one round trip, not one per page */
	want = (need + hb->hps - 1) >> hb->hps_shift;
	if(want < hb->grow_granule >> hb->hps_shift)
		want = hb->grow_granule >> hb->hps_shift;
	if(want < 1)
		want = 1;
	if(want > hb->punched_pages)
		want = hb->punched_pages;
	if(want > room >> hb->hps_shift)
		want = room >> hb->hps_shift;
	if(want > HG_PAGES_PER_EVENT)
		want = HG_PAGES_PER_EVENT;
	if(!want)
		return 0;

	/* phase 1, locked: select, lowest first */
	for(i = 0; i < hb->npages && got < want; i++)
		if(hb->pages[i].punched && !hb->pages[i].releasing)
			sel[got++] = i;
	hb->grow_inflight = 1;
	reason = hb->lk_cur;
	hg_lock_leave(hb);

	/* phase 2, unlocked: the host RAM check first (it reads /proc); then
	 * the re-pins - a hugetlb hole may not come back; it stays a hole */
	if(hg_grow_room_short(hb, got << hb->hps_shift, &rc)) {
		hg_lock_enter(hb, (enum hg_lock_reason)reason);
		__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
		hg_grow_room_refused(hb, got << hb->hps_shift, &rc);
		return grow_resource_refused(hb);
	}
	t0 = hg_now_ns();
	for(i = 0, ok = 0; i < got; i++) {
		struct hg_page *pg = &hb->pages[sel[i]];

		if(hg_mem_repin(
				   hb, (unsigned long)(pg->base - hb->hbase), hb->hps, pg->tier)
						== 0
				|| pg->tier != HG_MEM_HUGETLB)
			sel[ok++] = sel[i];
	}
	got = ok;
	pin_ns = hg_now_ns() - t0;

	/* phase 3, locked: publish what came back */
	hg_lock_enter(hb, (enum hg_lock_reason)reason);
	if(!got) {
		__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
		hb->grow_refuse_why = HG_GR_HOLE_REPIN;
		return grow_resource_refused(hb);
	}
	for(i = 0; i < got; i++) {
		struct hg_page *pg = &hb->pages[sel[i]];

		pg->punched = 0;
		fl_push(hb, pg->base, top);
		hb->buddy_free_leaves += lpp;
		hb->tier_bytes[pg->tier] += hb->hps;
		if(hb->punched_pages)
			hb->punched_pages--;
	}
	/* the count moves last, with release semantics: a waiter polling it
	 * lock-free must not see it before the pages are back on the list */
	__atomic_store_n(&hb->committed_bytes,
			hb->committed_bytes + (got << hb->hps_shift), __ATOMIC_RELEASE);
	__atomic_store_n(&hb->grow_inflight, 0, __ATOMIC_RELEASE);
	hb->repins += got;
	hb->repin_bytes += got << hb->hps_shift;
	hb->hole_regrows++;
	hb->grow_gen++;
	floor_recompute(hb);
	hb->shrink_quiet = 0; /* fresh demand voids any quiet window */
	hb->draining = 0;
	span_check(hb, "a re-commit");

	LM_NOTICE("%s arena re-committed %lu hole%s (%lu MB) instead of "
			  "growing the span; pin %lu us with the lock released; %lu MB "
			  "committed of a %lu MB span, %lu hole%s left\n",
			hb->name, got, got == 1 ? "" : "s", (got << hb->hps_shift) >> 20,
			pin_ns / 1000, hb->committed_bytes >> 20, hb->span_end >> 20,
			hb->punched_pages, hb->punched_pages == 1 ? "" : "s");
	return 1;
}

static int hg_buddy_regrow_holes_unlocks(
		struct hg_block *hb, unsigned long need, unsigned long room)
{
	int r;

	hg_lock_assert_held(hb, "hg_buddy_regrow_holes_unlocks entry");
	r = buddy_regrow_holes_body(hb, need, room);
	hg_lock_assert_held(hb, "hg_buddy_regrow_holes_unlocks return");
	return r;
}

/* quiet sweep ticks before a drain starts (2 minutes at the 30 s sweep) */
#define HG_SHRINK_QUIET_TICKS 4

/* shrink gate, once per sweep, hb->lock held: a tick is quiet when nothing
 * is starved and free space passes the drain's own gate */
static void shrink_tick_body(struct hg_block *hb)
{
	unsigned long lpp = hg_leaves_per_page(hb);
	unsigned int need_ticks;

	if(!hb->buddy_ready || hb->shrink_unsupported)
		return;
	if(hb->committed_bytes <= hb->committed_min) {
		hb->shrink_quiet = 0;
		return;
	}
	/* never move the top while a grow is populating above it */
	if(hb->grow_inflight) {
		hb->shrink_quiet = 0;
		return;
	}
	/* never shrink a starved or latched arena; the top-page test is for the
	 * trim only, the punch exists for a pinned top page */
	if(hb->below_floor || hb->grow_blocked
			|| (!hg_interior_release
					&& !page_is_whole_free(hb, &hb->pages[hb->npages - 1]))) {
		hb->shrink_quiet = 0;
		return;
	}
	if(hb->buddy_free_leaves < lpp + 4 * hb->reserve_floor) {
		hb->shrink_quiet = 0;
		return;
	}
	need_ticks = HG_SHRINK_QUIET_TICKS;
	if(++hb->shrink_quiet < need_ticks)
		return;
	hb->shrink_quiet = 0;
	/* decided once per cycle; released one step per maintenance tick */
	hb->draining = 1;
	hg_drain_tick(hb);
}

void hg_shrink_tick(struct hg_block *hb)
{
	hg_lock_assert_held(hb, "hg_shrink_tick entry");
	shrink_tick_body(hb);
	hg_lock_assert_held(hb, "hg_shrink_tick return");
}

/* one drain step, hb->lock held: top trim, then interior punch; the gates
 * are re-tested, abundance one page above 4x the floor */
void hg_drain_tick(struct hg_block *hb)
{
	unsigned long lpp = hg_leaves_per_page(hb), released;

	if(!hb->draining || !hb->buddy_ready)
		return;
	if(hb->shrink_unsupported || hb->below_floor || hb->grow_blocked
			|| hb->committed_bytes <= hb->committed_min) {
		hb->draining = 0;
		return;
	}
	if(hb->grow_inflight)
		return; /* a release or a grow is in flight */
	if(hb->buddy_free_leaves < 4 * hb->reserve_floor + lpp) {
		hb->draining = 0;
		return;
	}

	released = hg_buddy_shrink_unlocks(hb);
	/* the punch reaches what the top-only trim cannot */
	released += hg_buddy_punch_unlocks(hb);
	if(!released)
		hb->draining = 0;
}

/* headroom rule: keep the free grid above twice the floor while the cap
 * allows. Every second from maintenance, else each sweep. 1 if it grew */
static int grow_headroom_tick_body(struct hg_block *hb)
{
	if(!hb->buddy_ready || !hg_grow_ahead)
		return 0;
	if(hb->span_end < hb->hcap && !hb->grow_inflight
			&& hb->buddy_free_leaves < 2 * hb->reserve_floor)
		return hg_buddy_grow_unlocks(hb, hb->grow_granule, HG_GROW_PROACTIVE)
			   == 0;
	return 0;
}

int hg_grow_headroom_tick(struct hg_block *hb)
{
	int r;

	hg_lock_assert_held(hb, "hg_grow_headroom_tick entry");
	r = grow_headroom_tick_body(hb);
	hg_lock_assert_held(hb, "hg_grow_headroom_tick return");
	return r;
}

static void grow_tick_body(struct hg_block *hb)
{
	if(hb->buddy_ready && !hb->maint_active)
		hg_grow_headroom_tick(hb);
}

void hg_grow_tick(struct hg_block *hb)
{
	hg_lock_assert_held(hb, "hg_grow_tick entry");
	grow_tick_body(hb);
	hg_lock_assert_held(hb, "hg_grow_tick return");
}

#endif /* HG_MALLOC */

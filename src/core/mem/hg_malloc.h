/*
 * slab allocator with elastic arenas
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

#ifndef hg_malloc_h
#define hg_malloc_h

#include "hg_block.h"

/* reserve a region of @size bytes (huge pages where possible) and lay the
 * block header at its start; NULL on failure */
/* @shared: MAP_SHARED shm, MAP_PRIVATE pkg. HG_INIT_INHERITED: the pre-fork
 * pkg arena, which every child inherits copy-on-write */
#define HG_INIT_INHERITED (1U << 0)

struct hg_block *hg_malloc_init(unsigned long size, char *name, int shared,
		const char *proc_desc, unsigned int flags);
void hg_malloc_destroy(struct hg_block *hb);

/* the -m/-M caps (0 = fixed); the pkg cap is per process */

/* commit @delta bytes at @off: populate, pin, verify the backing; the tier
 * or -1 rolled back. Called without hb->lock; see @collapse_later */
int hg_mem_commit(struct hg_block *hb, unsigned long off, unsigned long delta,
		int *collapse_later);
int hg_mem_collapse(struct hg_block *hb, char *base, unsigned long len);
void hg_collapse_deferred(struct hg_block *hb);

/* the host figures behind a grow refusal: read by hg_grow_room_short()
 * with hb->lock released, reported by hg_grow_room_refused() under it */
struct hg_room_check
{
	int host_short, cg_short;
	long floor_mb, avail_kb;
	unsigned long long effective; /* @delta, x nproc for pkg */
	unsigned long nproc;
	int cg_version;
	unsigned long cg_limit_mb, cg_used_mb, cg_floor_mb;
};
/* would @delta (x nproc for pkg) leave the host below the RAM floor, or the
 * memory cgroup below its own? Nonzero means refuse; hugetlb always passes */
int hg_grow_room_short(
		struct hg_block *hb, unsigned long delta, struct hg_room_check *rc);
void hg_grow_room_refused(struct hg_block *hb, unsigned long delta,
		const struct hg_room_check *rc);

const char *hg_grow_refusal_str(unsigned int why);

/* why mlock() refused: the memlock limit or the hugetlb pool; pure */
enum hg_grow_refusal hg_mlock_refusal_why(unsigned long lim, long locked_kb,
		unsigned long delta, int hugetlb, long free_pg, long rsvd_pg,
		long page_kb);

/* pkg arena capacity checks: hg_capacity_check() before the first fork,
 * hg_fork_fits() before each fork */
int hg_capacity_check(unsigned int nprocs);
int hg_fork_fits(void);

/* per-process pkg growth refusals, published and summed */
/* allocation failure throttle, hb->lock held */
static inline int hg_oom_first(struct hg_block *hb, unsigned long req)
{
	hb->oom_fails++;
	if(req > hb->oom_max_req)
		hb->oom_max_req = req;
	if(hb->oom_episode)
		return 0;
	hb->oom_episode = 1;
	return 1;
}

static inline unsigned long hg_oom_take(
		struct hg_block *hb, unsigned long *max_req)
{
	unsigned long n = hb->oom_fails - hb->oom_fails_said;

	*max_req = hb->oom_max_req;
	hb->oom_fails_said = hb->oom_fails;
	hb->oom_max_req = 0;
	if(!n)
		hb->oom_episode = 0;
	return n;
}

void hg_oom_report(struct hg_block *hb, unsigned long n, unsigned long max_req);
void hg_oom_report_shared(void);
void hg_sample_live_peaks(void);

int hg_pkg_pub_init(unsigned int nproc);
void hg_pkg_publish(struct hg_block *hb);
unsigned long hg_pkg_pub_stat(void *ctx);
void hg_pkg_pub_sum(unsigned int *n, unsigned long *committed,
		unsigned long *committed_max, unsigned long *grows,
		unsigned long *shrinks);
unsigned int hg_pkg_pub_peak(unsigned long *peak);

/* release the backing of a range: MADV_REMOVE when shared (hugetlb too),
 * MADV_DONTNEED when private. 0, or -1 with shrink_unsupported latched */
int hg_mem_release(struct hg_block *hb, unsigned long off, unsigned long len);
int hg_mem_repin(
		struct hg_block *hb, unsigned long off, unsigned long len, int tier);

/* after the config, before fork: private-cache budget, grow granules,
 * shrink step */
int hg_post_cfg(void);

/* fork reset for every shared arena in the registry */
void hg_malloc_child_init_all(void);

/* the public entry points (hg_malloc_dyn.h), with the memory manager api
 * signatures of memapi.h: @hbp is the struct hg_block of the arena */
#ifdef DBG_HG_MALLOC
void *hg_malloc(void *hbp, size_t size, const char *file, const char *func,
		unsigned int line, const char *mname);
void *hg_mallocxz(void *hbp, size_t size, const char *file, const char *func,
		unsigned int line, const char *mname);
void *hg_mallocxn(void *hbp, size_t size, const char *file, const char *func,
		unsigned int line, const char *mname);
void hg_free(void *hbp, void *p, const char *file, const char *func,
		unsigned int line, const char *mname);
void *hg_realloc(void *hbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname);
void *hg_reallocxf(void *hbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname);
void *hg_resize(void *hbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname);
#else
void *hg_malloc(void *hbp, size_t size);
void *hg_mallocxz(void *hbp, size_t size);
void *hg_mallocxn(void *hbp, size_t size);
void hg_free(void *hbp, void *p);
void *hg_realloc(void *hbp, void *p, size_t size);
void *hg_reallocxf(void *hbp, void *p, size_t size);
void *hg_resize(void *hbp, void *p, size_t size);
#endif

/* status dump at the memlog level; with DBG_HG_MALLOC also every live cell
 * and a summary by allocation site */
void hg_status(struct hg_block *hb);
void hg_info(struct hg_block *hb, struct mem_info *info);
#ifdef DBG_HG_MALLOC
/* live memory by allocation site: logged at memlog, or collected into a
 * mem_counter list for the per-module statistics */
void hg_sums(struct hg_block *hb);
void hg_mod_get_stats(struct hg_block *hb, void **rootp);
void hg_mod_free_stats(void *rootp);
#endif

/* defined in hg_arena.c; table lookup, no lock needed (immutable) */
unsigned int hg_cell_total_size(unsigned char cls);

/* @frag is HG_HDR(p) - HG_LFRAG_HDR; opaque to avoid including hg_large.h */
unsigned long hg_large_frag_size_at(const void *frag);

/* ownership: hg_owns() for one known arena, hg_owns_any() over the arena
 * registry (for callers without a block); they decline, not repair */
static inline int hg_owns(struct hg_block *hb, void *cell_start)
{
	/* against the cap: it never changes, span_end does */
	return (char *)cell_start >= hb->hbase
		   && (char *)cell_start < hb->hbase + hb->hcap;
}

/* --- lock instrumentation helpers -------------------------------------- */
/* hg_lock_stall_us (default 1000 us) is in hg_kam.c */
extern const char *const hg_lk_reason_str[HG_LK_REASONS];

static inline unsigned long hg_now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long)ts.tv_sec * 1000000000UL + (unsigned long)ts.tv_nsec;
}

/* the stall bookkeeping and rate-limited WARN; out of line (hg_stats.c) so
 * the header stays free of dprint */
void hg_lock_stall_note(struct hg_block *hb, int reason, unsigned long held_ns);

static inline void hg_lock_enter(struct hg_block *hb, enum hg_lock_reason r)
{
	lock_get(&hb->lock);
	HG_TSAN_ACQUIRE(&hb->lock);
	hb->lk_t0 = hg_now_ns();
	hb->lk_cur = r;
}

static inline void hg_lock_leave(struct hg_block *hb)
{
	unsigned long held = hg_now_ns() - hb->lk_t0;
	int r = hb->lk_cur;
	int stalled = 0;

	if(held > hb->lk_worst_ns) {
		hb->lk_worst_ns = held;
		hb->lk_worst_reason = r;
		hb->lk_worst_proc = process_no;
	}
	if(hg_lock_stall_us > 0
			&& held >= (unsigned long)hg_lock_stall_us * 1000UL) {
		hb->lk_stalls++;
		hb->lk_last_stall_ns = held;
		hb->lk_last_stall_reason = r;
		hb->lk_last_stall_proc = process_no;
		hb->lk_stall_event_due = 1;
		stalled = 1;
	}
	/* mark the lock free before releasing it, so a callee that must return
	 * with the lock held can be checked (hg_lock_assert_held) */
	hb->lk_cur = HG_LK_NONE;
	HG_TSAN_RELEASE(&hb->lock);
	lock_release(&hb->lock);
	if(stalled)
		hg_lock_stall_note(hb, r, held);
}

/* lock contract check for the *_unlocks functions */
void hg_lock_assert_held(struct hg_block *hb, const char *where);

#define HG_ARENA_REG_MAX 8
struct hg_arena_range
{
	char *base;
	unsigned long size;
	struct hg_block *hb;
};
/* process-local; a child keeps the parent's entries */
extern struct hg_arena_range hg_arena_reg[HG_ARENA_REG_MAX];

/* the arena a pointer belongs to: a child may free what its parent
 * allocated before fork, which is redirected to that arena */
static inline struct hg_block *hg_owner(const void *p)
{
	int i;

	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		if(!hg_arena_reg[i].base)
			continue;
		if((const char *)p >= hg_arena_reg[i].base
				&& (const char *)p
						   < hg_arena_reg[i].base + hg_arena_reg[i].size)
			return hg_arena_reg[i].hb;
	}
	return NULL;
}

static inline int hg_owns_any(const void *p)
{
	return hg_owner(p) != NULL;
}

static inline unsigned long hg_frag_size(void *p)
{
	unsigned char c;

	if(!p)
		return 0;

	/* check before reading the header; 0 = unknown size */
	if(!hg_owns_any(HG_HDR(p)))
		return 0;

	c = HG_CLASS(p);
	if(c == HG_LARGE_MARKER)
		return hg_large_frag_size_at(HG_HDR(p) - HG_LFRAG_HDR_SIZE);
	if(c >= HG_NCLASSES)
		return 0;

	return hg_cell_total_size(c);
}

/* this thread's stats slot (hg_thread_stats_init()), -1 = the process slot */
extern __thread int hg_pstat_slot;

static inline struct hg_pstat *hg_pstat_mine(struct hg_block *hb)
{
	int s = hg_pstat_slot;

	if(s < 0)
		s = (int)(((unsigned int)(process_no + 1)) % HG_STAT_SLOTS);
	return &hb->pstat[s];
}

/* claim a stats slot for this thread (TCP reactor pool), from the top */
void hg_thread_stats_init(void);

/* summed on read, clamped at 0 */
static inline unsigned long hg_used(struct hg_block *hb)
{
	long total = 0;
	int i;

	for(i = 0; i < HG_STAT_SLOTS; i++)
		total += hb->pstat[i].used;
	return total < 0 ? 0 : (unsigned long)total;
}

static inline unsigned long hg_fragments(struct hg_block *hb)
{
	long total = 0;
	int i;

	for(i = 0; i < HG_STAT_SLOTS; i++)
		total += hb->pstat[i].fragments;
	return total < 0 ? 0 : (unsigned long)total;
}

/* carved-but-idle cell capacity, in hg_arena.c */
unsigned long hg_slab_recycled(struct hg_block *hb);

/* the shm arena's figures as "hgmem" counters; pkg arenas are visible
 * through the usual per-process pkg status */
#ifdef HG_MALLOC
int hg_register_stats(void);
#else
/* no-op without HG_MALLOC */
static inline int hg_register_stats(void)
{
	return 0;
}
#endif

/* mem_safety = 0: log and abort() - in hg_arena.c */
void hg_corrupt_abort(struct hg_block *hb, enum hg_corrupt_kind k)
		__attribute__((noreturn, cold));

/* bump @hb's corruption counter; every site logs first, so a
 * mem_safety = 0 abort comes after the message */
static inline void hg_corrupt(struct hg_block *hb, enum hg_corrupt_kind k)
{
	hb->corrupt[k]++;
	if(cfg_get(core, core_cfg, mem_safety) == 0)
		hg_corrupt_abort(hb, k);
}

/* mem_guard: new cells of an arena without the debug checks get a tail
 * guard; read per allocation, the setting may change at runtime */
static inline int hg_guard_on(const struct hg_block *hb)
{
	return !hb->dbg && cfg_get(core, core_cfg, mem_guard);
}

/* a guarded cell failed hg_guard_ok(): logged and counted - in hg_arena.c */
void hg_guard_bad(struct hg_block *hb, void *cs, unsigned long cap,
		unsigned long size) __attribute__((cold));

static inline unsigned long hg_corrupt_total(struct hg_block *hb)
{
	unsigned long t = 0;
	int i;

	for(i = 0; i < HG_CORRUPT_KINDS; i++)
		t += hb->corrupt[i];
	return t;
}

/* total cell-slot bytes handed out across every process */
static inline unsigned long hg_cell_live(struct hg_block *hb)
{
	long total = 0;
	int i;

	for(i = 0; i < HG_STAT_SLOTS; i++)
		total += hb->pstat[i].cell_live;
	return total < 0 ? 0 : (unsigned long)total;
}

/* large tier backing not handed out; O(1) */
static inline unsigned long hg_large_recycled(struct hg_block *hb)
{
	return hb->large_backing > hb->large_live
				   ? hb->large_backing - hb->large_live
				   : 0;
}
static inline unsigned long hg_get_real_used(struct hg_block *hb)
{
	/* both tiers' idle held bytes come out of real_used */
	unsigned long recycled = hg_slab_recycled(hb) + hg_large_recycled(hb);
	unsigned long live =
			hb->real_used > recycled ? hb->real_used - recycled : 0;

	if(live > hb->max_live_used)
		hb->max_live_used = live;
	return live;
}

#endif /* hg_malloc_h */

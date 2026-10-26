/*
 * slab allocator with elastic arenas - core integration
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

/* hg as a Kamailio memory manager: shm/pkg vtables, child init, idle-cache
 * sweep and the pre-fork checks */

#ifdef HG_MALLOC

#include <string.h>
#include <stdlib.h>
#include <time.h> /* hg.advise: uptime */

#include "../globals.h"
#include "../events.h"
#include "../pt.h"
#include "../rpc.h"
#include "../rpc_lookup.h"
#include "hg_malloc.h"
#include "hg_arena.h"

int hg_shm_mode = HG_MODE_NONE;
int hg_pkg_mode = HG_MODE_NONE;
void *hg_shm_block = NULL;
void *hg_pkg_block = NULL;

unsigned long hg_shm_cap_bytes = 0;
unsigned long hg_pkg_cap_bytes = 0;

/* host free memory no commit may take the host below, MB; 0 = MemTotal/20,
 * at least the smaller of 256 MB and MemTotal/8 */
int hg_ram_floor_mb = 0;
/* one grow step of the shm arena / of each process's pkg arena; 0 = an
 * eighth of the cap, within one page and 16 MB */
unsigned long hg_shm_grow_granule = 0;
unsigned long hg_pkg_grow_granule = 0;
/* an arena lock hold at or above this is counted as a stall, us */
int hg_lock_stall_us = 1000;
/* keep the free part of the arena above twice its reserve floor */
int hg_grow_ahead = 1;
/* seconds per GC and shrink cycle of the shared arenas */
int hg_scaling_cycle = 30;
/* the most a shared arena releases per maintenance tick */
unsigned long hg_shrink_step = 64UL << 20;
/* 1 = give back whole free pages from anywhere in the arena */
int hg_interior_release = 0;

static char *_hg_mem_name = "hg_malloc";

unsigned int hg_nprocs(void)
{
	return pt ? (unsigned int)get_max_procs() : 0;
}

int hg_io_threads(void)
{
	return ksr_tcp_main_threads == 2 ? ksr_tcp_reactor_threads : 0;
}

/* --- vtable members that the arena code does not provide as they are -- */

static void hg_kam_status(void *hbp)
{
	hg_status((struct hg_block *)hbp);
}

static void hg_kam_info(void *hbp, struct mem_info *info)
{
	hg_info((struct hg_block *)hbp, info);
}

/* what an allocation can still get: memory not yet cut into blocks, plus
 * the room the arena may still grow into */
static unsigned long hg_kam_available(void *hbp)
{
	struct hg_block *hb = (struct hg_block *)hbp;
	unsigned long avail;

	avail = hb->size > hb->real_used ? hb->size - hb->real_used : 0;
	if(hb->hcap > hb->committed_bytes)
		avail += hb->hcap - hb->committed_bytes;
	return avail;
}

/* the figures the counters hold; no fragment walk */
static void hg_kam_report(void *hbp, mem_report_t *mrep)
{
	struct hg_block *hb = (struct hg_block *)hbp;
	struct mem_info mi;

	memset(mrep, 0, sizeof(mem_report_t));
	if(!hb)
		return;
	hg_info(hb, &mi);
	mrep->total_size = mi.total_size;
	mrep->free_size_s = mi.free_size;
	mrep->used_size_s = mi.used_size;
	mrep->real_used_s = mi.real_used;
	mrep->max_used_s = mi.max_used;
	mrep->used_frags = mi.total_frags;
	mrep->total_frags = mi.total_frags;
}

static void hg_kam_sums(void *hbp)
{
#ifdef DBG_HG_MALLOC
	hg_sums((struct hg_block *)hbp);
#else
	LOG_FP(DEFAULT_FACILITY, cfg_get(core, core_cfg, memlog),
			"hg_sums: ", "not available (%p)\n", hbp);
#endif
}

static void hg_kam_mod_get_stats(void *hbp, void **rootp)
{
#ifdef DBG_HG_MALLOC
	hg_mod_get_stats((struct hg_block *)hbp, rootp);
#else
	LM_WARN("build with memory debugging (MEMDBG=1) for module statistics\n");
#endif
}

static void hg_kam_mod_free_stats(void *rootp)
{
#ifdef DBG_HG_MALLOC
	hg_mod_free_stats(rootp);
#endif
}

/* hg locks internally; tm's free_cell() batches under shm_global_lock(),
 * so these are no-ops */
static void hg_kam_glock(void *hbp)
{
}

static void hg_kam_gunlock(void *hbp)
{
}

static void hg_kam_destroy_shm(void)
{
	if(hg_shm_block) {
		hg_malloc_destroy((struct hg_block *)hg_shm_block);
		hg_shm_block = NULL;
	}
}

static void hg_kam_destroy_pkg(void)
{
	if(hg_pkg_block) {
		hg_malloc_destroy((struct hg_block *)hg_pkg_block);
		hg_pkg_block = NULL;
	}
}

/* --- manager init --------------------------------------------------- */

int hg_malloc_init_shm_manager(int dbg)
{
	sr_shm_api_t ma;

	hg_shm_mode = dbg ? HG_MODE_DBG : HG_MODE_ON;
	hg_shm_block = hg_malloc_init(shm_mem_size, "shm", 1, NULL, 0);
	if(!hg_shm_block) {
		LM_CRIT("could not initialize the hg shm arena\n");
		hg_shm_mode = HG_MODE_NONE;
		return -1;
	}

	memset(&ma, 0, sizeof(sr_shm_api_t));
	ma.mname = _hg_mem_name;
	ma.mem_pool = ((struct hg_block *)hg_shm_block)->hbase;
	ma.mem_block = hg_shm_block;
	ma.xmalloc = hg_malloc;
	ma.xmallocxz = hg_mallocxz;
	ma.xmallocxn = hg_mallocxn;
	ma.xmalloc_unsafe = hg_malloc;
	ma.xfree = hg_free;
	ma.xfree_unsafe = hg_free;
	ma.xrealloc = hg_realloc;
	ma.xreallocxf = hg_reallocxf;
	ma.xresize = hg_resize;
	ma.xstatus = hg_kam_status;
	ma.xinfo = hg_kam_info;
	ma.xreport = hg_kam_report;
	ma.xavailable = hg_kam_available;
	ma.xsums = hg_kam_sums;
	ma.xdestroy = hg_kam_destroy_shm;
	ma.xmodstats = hg_kam_mod_get_stats;
	ma.xfmodstats = hg_kam_mod_free_stats;
	ma.xglock = hg_kam_glock;
	ma.xgunlock = hg_kam_gunlock;

	return shm_init_api(&ma);
}

static void hg_kam_pkg_api(sr_pkg_api_t *ma)
{
	memset(ma, 0, sizeof(sr_pkg_api_t));
	ma->mname = _hg_mem_name;
	ma->mem_pool = ((struct hg_block *)hg_pkg_block)->hbase;
	ma->mem_block = hg_pkg_block;
	ma->xmalloc = hg_malloc;
	ma->xmallocxz = hg_mallocxz;
	ma->xmallocxn = hg_mallocxn;
	ma->xfree = hg_free;
	ma->xrealloc = hg_realloc;
	ma->xreallocxf = hg_reallocxf;
	ma->xstatus = hg_kam_status;
	ma->xinfo = hg_kam_info;
	ma->xreport = hg_kam_report;
	ma->xavailable = hg_kam_available;
	ma->xsums = hg_kam_sums;
	ma->xdestroy = hg_kam_destroy_pkg;
	ma->xmodstats = hg_kam_mod_get_stats;
	ma->xfmodstats = hg_kam_mod_free_stats;
}

int hg_malloc_init_pkg_manager(int dbg)
{
	sr_pkg_api_t ma;

	hg_pkg_mode = dbg ? HG_MODE_DBG : HG_MODE_ON;
	/* kept after fork for pre-fork allocations; children get their own */
	hg_pkg_block =
			hg_malloc_init(pkg_mem_size, "pkg", 0, NULL, HG_INIT_INHERITED);
	if(!hg_pkg_block) {
		LM_CRIT("could not initialize the hg pkg arena\n");
		hg_pkg_mode = HG_MODE_NONE;
		return -1;
	}
	hg_kam_pkg_api(&ma);
	return pkg_init_api(&ma);
}

/* is @p in the shm arena's reservation (shm_address_in()) */
int hg_shm_address_in(void *p)
{
	return hg_shm_block && hg_owns((struct hg_block *)hg_shm_block, p);
}

/* --- processes ------------------------------------------------------ */

/* first thing in a forked child: drop the inherited private caches of shared
 * arenas and switch to a pkg arena of its own */
void hg_kam_child_init(const char *desc)
{
	struct hg_block *hb;
	sr_pkg_api_t ma;

	if(!HG_SERVES_SHM() && !HG_SERVES_PKG())
		return;
	hg_malloc_child_init_all();
	if(!HG_SERVES_PKG())
		return;
	hb = hg_malloc_init(pkg_mem_size, "pkg", 0, desc, 0);
	if(!hb) {
		LM_CRIT("failed to init this child's own pkg memory (%lu bytes)\n",
				pkg_mem_size);
		exit(-1);
	}
	hg_pkg_block = hb;
	hg_kam_pkg_api(&ma);
	pkg_init_api(&ma);
	/* counted from the start: its sweep publishes only once it allocates */
	hg_pkg_publish(hb);
}

/* --- RPC ------------------------------------------------------------ */

static const char *hg_rpc_stats_doc[2] = {
		"hg memory manager: the shm arena, and every process's pkg arena "
		"summed (as each last published it: when created, then at its "
		"idle-cache sweeps)",
		0};

/* hg.stats keys for enum hg_corrupt_kind */
static const char *const hg_rpc_corrupt_keys[HG_CORRUPT_KINDS] = {
		[HG_C_CLASS_MISMATCH] = "class_mismatch",
		[HG_C_DOUBLE_FREE] = "double_free",
		[HG_C_NFREE_UNDERFLOW] = "free_count_underflow",
		[HG_C_BAD_CLASS] = "bad_class",
		[HG_C_FOREIGN_PTR] = "foreign_pointer",
		[HG_C_BUDDY_BAD_FREE] = "bad_buddy_free",
		[HG_C_INTERNAL] = "internal",
		[HG_C_HEADER] = "damaged_header",
		[HG_C_USE_AFTER_FREE] = "use_after_free",
		[HG_C_OVERFLOW] = "overflow"};

static void hg_rpc_stats(rpc_t *rpc, void *ctx)
{
	struct hg_block *hb = (struct hg_block *)hg_shm_block;
	void *th, *sh, *ph, *xh;
	unsigned int n;
	unsigned long pc, pcmax, pg, ps;
	int k;

	if(rpc->add(ctx, "{", &th) < 0)
		return;
	if(hb && HG_SERVES_SHM()) {
		if(rpc->struct_add(th, "{", "shm", &sh) < 0)
			return;
		rpc->struct_add(sh, "sjjjjjjj", "tier", hg_mem_tier_str(hb->tier),
				"committed", hb->committed_bytes, "cap", hb->hcap,
				"committed_min", hb->committed_min, "live",
				hg_get_real_used(hb), "live_peak", hb->max_live_used, "carved",
				hb->real_used, "carved_peak", hb->max_real_used);
		/* committed bytes by the backing each grow achieved */
		if(rpc->struct_add(sh, "{", "tier_bytes", &xh) < 0)
			return;
		rpc->struct_add(xh, "jjjj", "hugetlb", hb->tier_bytes[HG_MEM_HUGETLB],
				"thp_advise", hb->tier_bytes[HG_MEM_THP_ADVISE], "thp_collapse",
				hb->tier_bytes[HG_MEM_THP_COLLAPSE], "4k",
				hb->tier_bytes[HG_MEM_4K]);
		rpc->struct_add(sh, "jjjjdjjjjj", "grows", hb->grows, "grows_proactive",
				hb->grows_proactive, "grows_exhaustion", hb->grows_exhaustion,
				"grow_refused", hb->grow_refused, "grow_blocked",
				(int)hb->grow_blocked, "shrinks", hb->shrinks, "shrink_bytes",
				hb->shrink_bytes, "punches", hb->punches, "punch_bytes",
				hb->punch_bytes, "whole_free_pages", hb->nfree[hb->buddy_top]);
		rpc->struct_add(sh, "sjjjjjj", "grow_refuse_reason",
				hg_grow_refusal_str(hb->grow_refuse_why), "grow_waits",
				hb->grow_waits, "grow_wait_timeouts", hb->grow_wait_timeouts,
				"collapse_deferred", hb->collapse_deferred, "collapse_done",
				hb->collapsed_late, "collapse_retries", hb->collapse_retries,
				"collapse_failed", hb->collapse_late_failed);
		rpc->struct_add(sh, "j", "corruption", hg_corrupt_total(hb));
		if(rpc->struct_add(sh, "{", "corruption_kinds", &xh) < 0)
			return;
		for(k = 0; k < HG_CORRUPT_KINDS; k++)
			rpc->struct_add(xh, "j", hg_rpc_corrupt_keys[k], hb->corrupt[k]);
		rpc->struct_add(sh, "jjjjj", "cross_arena_frees", hb->xarena_frees,
				"lock_stalls", hb->lk_stalls, "lock_hold_max_us",
				hb->lk_worst_ns / 1000, "cache_flushes", hb->cache_flushes,
				"pressure_flushes", hb->pressure_flushes);
	}
	if(HG_SERVES_PKG()) {
		hg_pkg_pub_sum(&n, &pc, &pcmax, &pg, &ps);
		if(rpc->struct_add(th, "{", "pkg", &ph) < 0)
			return;
		rpc->struct_add(ph, "djjjjjj", "processes", (int)n, "committed", pc,
				"committed_max", pcmax, "grows", pg, "shrinks", ps,
				"grow_refused", hg_pkg_pub_stat((void *)0L), "grow_blocked",
				hg_pkg_pub_stat((void *)1L));
	}
}

static const char *hg_rpc_advise_doc[2] = {
		"hg memory manager: the -m and -M caps the carve peaks reached so far "
		"ask for, with a verdict per arena",
		0};

/* hg.advise works from each arena's carve peak: what had to fit, which can
 * sit well above live. shm exhaustion fails an allocation, pkg exhaustion
 * ends the process, so pkg gets the wider margin */
#define HG_ADVISE_SHM_MARGIN 2
#define HG_ADVISE_PKG_MARGIN 3
/* floors: low enough to stop only a silly answer, not to become it */
#define HG_ADVISE_SHM_FLOOR_MB 8
#define HG_ADVISE_PKG_FLOOR_MB 2
/* verdict bands, the peak in percent of the cap; 80 was too late to warn */
#define HG_ADVISE_TIGHT_PCT 70
#define HG_ADVISE_WATCH_PCT 50
#define HG_ADVISE_LOOSE_PCT 20

/* peak x margin in MB, at least the floor, whole huge pages; 64-bit
 * arithmetic, as a 32-bit build's peak x margin can pass 4 GB */
static unsigned long hg_advise_mb(unsigned long peak, unsigned int margin,
		unsigned long floor_mb, unsigned long hps)
{
	unsigned long mb = (unsigned long)(((unsigned long long)peak * margin
											   + (1ULL << 20) - 1)
									   >> 20);
	unsigned long step = hps >> 20;

	if(mb < floor_mb)
		mb = floor_mb;
	if(step > 1)
		mb = (mb + step - 1) / step * step;
	return mb;
}

static const char *hg_advise_verdict(unsigned long pct)
{
	if(pct > HG_ADVISE_TIGHT_PCT)
		return "TIGHT - raise it before the next busy period";
	if(pct > HG_ADVISE_WATCH_PCT)
		return "watch - fine now, no room for a bad day";
	if(pct < HG_ADVISE_LOOSE_PCT)
		return "oversized - the surplus is pinned and unusable elsewhere";
	return "reasonable";
}

/* one arena's advice as a struct under @parent; @nproc > 0 adds the pkg
 * totals over every process */
static int hg_rpc_advise_one(rpc_t *rpc, void *parent, const char *name,
		struct hg_block *hb, unsigned long peak, unsigned int margin,
		unsigned long floor_mb, unsigned int nproc)
{
	void *o;
	unsigned long pct =
			hb->hcap
					? (unsigned long)((unsigned long long)peak * 100 / hb->hcap)
					: 0;
	unsigned long rec = hg_advise_mb(peak, margin, floor_mb, hb->hps);

	if(rpc->struct_add(parent, "{", name, &o) < 0)
		return -1;
	if(rpc->struct_add(o, "jjjjdjs", "init_mb", hb->committed_min >> 20,
			   "cap_mb", hb->hcap >> 20, "peak_bytes", peak, "peak_pct_of_cap",
			   pct, "margin", (int)margin, "recommended_cap_mb", rec, "verdict",
			   hg_advise_verdict(pct))
			< 0)
		return -1;
	if(nproc
			&& rpc->struct_add(o, "djj", "processes", (int)nproc,
					   "cap_total_mb", nproc * (hb->hcap >> 20),
					   "recommended_total_mb", nproc * rec)
					   < 0)
		return -1;
	return 0;
}

static void hg_rpc_advise(rpc_t *rpc, void *ctx)
{
	struct hg_block *hb;
	void *th, *notes;
	unsigned long uptime = (unsigned long)(time(NULL) - up_since), peak;
	unsigned int nproc;

	if(rpc->add(ctx, "{", &th) < 0)
		return;
	if(rpc->struct_add(th, "j", "uptime_s", uptime) < 0)
		return;
	hb = (struct hg_block *)hg_shm_block;
	if(hb && HG_SERVES_SHM()
			&& hg_rpc_advise_one(rpc, th, "shm", hb, hb->max_real_used,
					   HG_ADVISE_SHM_MARGIN, HG_ADVISE_SHM_FLOOR_MB, 0)
					   < 0)
		return;
	hb = (struct hg_block *)hg_pkg_block;
	if(hb && HG_SERVES_PKG()) {
		/* the worst process's peak, from the published slots; this
		 * process's own when none has published yet */
		nproc = hg_pkg_pub_peak(&peak);
		if(!nproc)
			peak = hb->max_real_used;
		if(hg_rpc_advise_one(rpc, th, "pkg", hb, peak, HG_ADVISE_PKG_MARGIN,
				   HG_ADVISE_PKG_FLOOR_MB, nproc)
				< 0)
			return;
	}
	if(rpc->struct_add(th, "[", "notes", &notes) < 0)
		return;
	rpc->array_add(notes, "s",
			"the advice is based on the peaks reached so far; a longer or "
			"busier period can only raise them");
	if(uptime < 7200)
		rpc->array_add(notes, "s",
				"uptime is under 2 hours, too short to have seen a busy "
				"period: treat the numbers as provisional");
	if(HG_SERVES_PKG())
		rpc->array_add(notes, "s",
				"the pkg peak is the worst single process's, as published at "
				"its last idle-cache sweep; -M has to cover it, and every "
				"process gets an arena of that size");
}

static rpc_export_t hg_rpc_cmds[] = {
		{"hg.stats", hg_rpc_stats, hg_rpc_stats_doc, 0},
		{"hg.advise", hg_rpc_advise, hg_rpc_advise_doc, 0}, {0, 0, 0, 0}};

/* after the config is parsed, before the process table is sized */
int hg_kam_post_cfg(void)
{
	if(!HG_SERVES_SHM() && !HG_SERVES_PKG())
		return 0;
	if(hg_post_cfg() < 0)
		return -1;
	if(hg_register_stats() < 0)
		return -1;
	if(rpc_register_array(hg_rpc_cmds) != 0) {
		LM_ERR("failed to register the hg RPC commands\n");
		return -1;
	}
	return 0;
}

/* before forking one more process: can its pkg arena commit its INIT? */
int hg_kam_fork_fits(void)
{
	return hg_fork_fits();
}

/* after the process table is sized, before the first fork: check that all
 * pkg arenas fit, and set up the shared pkg state slots */
int hg_kam_prefork(void)
{
	unsigned int n = hg_nprocs();

	if(!HG_SERVES_SHM() && !HG_SERVES_PKG())
		return 0;
	if(hg_capacity_check(n) != 0)
		return -1;
	if(n + 1 >= HG_STAT_SLOTS)
		LM_WARN("%u processes, but %d per-process statistics slots: some "
				"share one, and their hg used/fragments figures drift\n",
				n, HG_STAT_SLOTS - 1);
	if(hg_pkg_pub_init(n) != 0)
		return -1;
	/* counted from the start, also if it never allocates again */
	if(HG_SERVES_PKG())
		hg_pkg_publish((struct hg_block *)hg_pkg_block);
	return 0;
}

/* --- the idle-cache sweep ------------------------------------------- */

/* each process flushes its own thread-local caches every HG_SWEEP_INTERVAL s
 * from hg_sweep_point(), in a slot hashed from its process index */
#define HG_SWEEP_INTERVAL 30
/* every 4th interval sweeps a process with clean shared caches too, for
 * its pkg caches */
#define HG_SWEEP_FORCE_EVERY 4

static unsigned long hg_sweep_last = (unsigned long)-1;

void hg_sweep_point(void)
{
	unsigned long now, period, offset;

	if(!HG_SERVES_SHM() && !HG_SERVES_PKG())
		return;
	offset = (unsigned long)((unsigned int)(process_no + 1) * 2654435761u
							 % HG_SWEEP_INTERVAL);
	now = hg_now_ns() / 1000000000UL;
	period = (now + offset) / HG_SWEEP_INTERVAL;
	if(likely(period == hg_sweep_last))
		return;
	if(hg_sweep_last == (unsigned long)-1) {
		/* the first call only starts the clock */
		hg_sweep_last = period;
		return;
	}
	hg_sweep_last = period;
	hg_sweep_gen++;
	if((period % HG_SWEEP_FORCE_EVERY) && !hg_proc_cache_dirty(process_no)
			&& !HG_SERVES_PKG()) {
		if(hg_shm_block)
			((struct hg_block *)hg_shm_block)->sweeps_skipped++;
		return;
	}
	hg_cache_flush_self();
	if(HG_SERVES_PKG())
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
}

#endif /* HG_MALLOC */

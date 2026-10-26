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

#ifdef HG_MALLOC

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h> /* getrlimit(RLIMIT_MEMLOCK) */
#include <pthread.h>	  /* pthread_atfork() */

#include "hg_version.h"
#include "hg_malloc.h"
#include "hg_arena.h"
#include "hg_large.h"
#include "hg_dbg.h"
#include "hg_backing.h"
#include "hg_buddy.h" /* hg_backed(), for hg_malloc_dyn.h */
#include "../dprint.h"

void hg_oom_report(struct hg_block *hb, unsigned long n, unsigned long max_req)
{
	LM_ERR("%s: %lu allocations failed in the last 30 s for lack of arena "
		   "memory (largest %lu %s) - the arena is full; increase it or find "
		   "what fills it (hg.stats)\n",
			hb->name, n, HG_SZ_VAL(max_req), HG_SZ_UNIT(max_req));
}

/* the shared arena (shm), once per sweep interval from the maintenance
 * process: every process sees it, so no per-process report */
void hg_oom_report_shared(void)
{
	unsigned long n, max_req;
	struct hg_block *hb;
	int i;

	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		hb = hg_arena_reg[i].hb;
		if(!hb || !hb->shared)
			continue;
		hg_lock_enter(hb, HG_LK_STATS);
		n = hg_oom_take(hb, &max_req);
		hg_lock_leave(hb);
		if(n)
			hg_oom_report(hb, n, max_req);
	}
}

/* sample the shared arenas' live peak every second, lock-free */
void hg_sample_live_peaks(void)
{
	struct hg_block *hb;
	int i;

	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		hb = hg_arena_reg[i].hb;
		if(hb && hb->shared)
			(void)hg_get_real_used(hb);
	}
}

int hg_post_cfg(void)
{
	int hg_shm = HG_SERVES_SHM();
#ifdef PKG_MALLOC
	int hg_pkg = HG_SERVES_PKG();
#else
	int hg_pkg = 0; /* pkg memory is the system malloc */
#endif

	/* the TCP reactor pool size is known now: re-derive the cache divisor */
	if(hg_shm && hg_shm_block)
		hg_private_caps_retune((struct hg_block *)hg_shm_block);
	if(hg_pkg && hg_pkg_block)
		hg_private_caps_retune((struct hg_block *)hg_pkg_block);

	if(hg_shm_grow_granule && hg_shm && hg_shm_block) {
		struct hg_block *hb = (struct hg_block *)hg_shm_block;

		hg_lock_enter(hb, HG_LK_POLICY);
		hb->grow_granule = hg_page_round(hg_shm_grow_granule, hb->hps);
		hg_lock_leave(hb);
		LM_NOTICE("shm grow granule set to %lu %s per step\n",
				HG_SZ_VAL(hb->grow_granule), HG_SZ_UNIT(hb->grow_granule));
	}
	if(hg_pkg_grow_granule && hg_pkg)
		LM_NOTICE("pkg grow granule %lu %s per step will apply to every "
				  "process's arena\n",
				HG_SZ_VAL(hg_pkg_grow_granule),
				HG_SZ_UNIT(hg_pkg_grow_granule));
	if(hg_shm && hg_shm_block) {
		struct hg_block *hb = (struct hg_block *)hg_shm_block;
		unsigned long step = hg_page_round(hg_shrink_step, hb->hps);

		if(step < hb->hps)
			step = hb->hps;
		if(step != hb->shrink_step) {
			hg_lock_enter(hb, HG_LK_POLICY);
			hb->shrink_step = step;
			hg_lock_leave(hb);
		}
		LM_NOTICE("shm shrink step %lu %s per drain tick, policy cycle "
				  "%d s\n",
				HG_SZ_VAL(hb->shrink_step), HG_SZ_UNIT(hb->shrink_step),
				hg_scaling_cycle);
	}
	return 0;
}

const char *hg_grow_refusal_str(unsigned int why)
{
	static const char *const str[HG_GR_REASONS] = {
			[HG_GR_NONE] = "none",
			[HG_GR_CAP] = "cap",
			[HG_GR_WAIT_TIMEOUT] = "grow_wait_timeout",
			[HG_GR_HOST_FLOOR] = "host_ram_floor",
			[HG_GR_CGROUP] = "cgroup_limit",
			[HG_GR_MEMLOCK] = "memlock_limit",
			[HG_GR_HUGETLB_POOL] = "hugetlb_pool",
			[HG_GR_MLOCK] = "mlock_failed",
			[HG_GR_HOLE_REPIN] = "hole_repin_failed",
	};

	return why < HG_GR_REASONS ? str[why] : "?";
}

const char *hg_mem_tier_str(enum hg_mem_tier tier)
{
	switch(tier) {
		case HG_MEM_HUGETLB:
			return "MAP_HUGETLB huge pages";
		case HG_MEM_THP_ADVISE:
			return "THP via MADV_HUGEPAGE (huge at fault)";
		case HG_MEM_THP_COLLAPSE:
			return "THP via MADV_COLLAPSE (post-fill retrofit)";
		case HG_MEM_4K:
			return "plain 4K pages";
	}
	return "unknown";
}

/* live arena ranges for hg_owns_any(); a static array, since the allocator
 * cannot allocate through itself. A child keeps the parent's entries */
struct hg_arena_range hg_arena_reg[HG_ARENA_REG_MAX];

static void hg_arena_reg_add(struct hg_block *hb)
{
	int i;

	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		if(!hg_arena_reg[i].base) {
			hg_arena_reg[i].base = hb->hbase;
			/* the cap, not the span: growth must not invalidate the entry */
			hg_arena_reg[i].size = hb->hcap;
			hg_arena_reg[i].hb = hb;
			return;
		}
	}
	/* not fatal: hg_owns_any() then declines pointers it cannot vouch for,
	 * which costs diagnostics, never correctness */
	LM_WARN("%s: more than %d live HG_MALLOC arenas in one process - "
			"ownership checks will be incomplete\n",
			hb->name, HG_ARENA_REG_MAX);
}

/* every fork, fork_process() or not: the parent hands its shared-arena
 * caches back first and the child drops its copy of the slots */
static void hg_atfork_register(void)
{
	static int done;

	if(done)
		return;
	done = 1;
	if(pthread_atfork(hg_cache_fork_prepare, NULL, hg_malloc_child_init_all)
			!= 0)
		LM_WARN("pthread_atfork() failed - a fork that bypasses "
				"fork_process() keeps the parent's cached shm cells\n");
}

static void hg_arena_reg_del(struct hg_block *hb)
{
	int i;

	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		if(hg_arena_reg[i].base == hb->hbase) {
			hg_arena_reg[i].base = NULL;
			hg_arena_reg[i].size = 0;
			hg_arena_reg[i].hb = NULL;
			return;
		}
	}
}

/* header checksum secret: random, else clock and pid */
static void hg_hdr_secret_init(void)
{
	unsigned long long s = 0;
	int fd;

	fd = open("/dev/urandom", O_RDONLY);
	if(fd >= 0) {
		if(read(fd, &s, sizeof s) != (ssize_t)sizeof s)
			s = 0;
		close(fd);
	}
	if(!s) {
		struct timeval tv;

		gettimeofday(&tv, NULL);
		s = ((unsigned long long)tv.tv_sec << 32)
			^ (unsigned long long)tv.tv_usec
			^ ((unsigned long long)getpid() << 16)
			^ (unsigned long long)(unsigned long)&s;
	}
	hg_hdr_secret = s | 1;
}

/* the block header lives at the front of the arena it manages */
struct hg_block *hg_malloc_init(unsigned long size, char *name, int shared,
		const char *proc_desc, unsigned int flags)
{
	enum hg_mem_tier tier;
	unsigned long locked_b;
	unsigned long cap, apage;
	char *base;
	struct hg_block *hb;
	int tier_decides;
	unsigned long cap_asked, mv;

	/* before the first arena, so before any fork: every process then
	 * checks headers against the same secret */
	if(!hg_hdr_secret)
		hg_hdr_secret_init();

	/* the cap (-m/-M INIT:CAP) may exceed INIT; a hugetlb pool too small for
	 * it gives a fixed arena */
	if(!strcmp(name, "shm"))
		cap = hg_shm_cap_bytes;
	else if(!strcmp(name, "pkg"))
		cap = hg_pkg_cap_bytes;
	else
		cap = 0;
	apage = hg_arena_page(size, cap);
	/* INIT under a huge page with a cap above it: the tier decides the page */
	tier_decides = (apage == HG_HPS && size < HG_HPS);
	/* a small-mode arena with room above it can start on one page: what
	 * does not fit grows the arena instead of failing */
	mv = (apage < HG_HPS && cap > hg_page_round(size, apage))
				 ? apage
				 : hg_min_viable(apage);
	if(!tier_decides && size < mv) {
		LM_ERR("%s arena: %lu %s is below the %lu %s minimum viable "
			   "arena\n",
				name, HG_SZ_VAL(size), HG_SZ_UNIT(size), HG_SZ_VAL(mv),
				HG_SZ_UNIT(mv));
		return NULL;
	}
	/* the INIT is committed and pinned at once: refuse one the host
	 * cannot hold rather than let the kernel's OOM killer decide */
	if(!hg_init_fits(name, size, cap, shared))
		return NULL;
	cap_asked = cap;
	base = hg_mem_reserve(size, &cap, &tier, &locked_b, shared, apage);
	if(!base) {
		LM_ERR("failed to reserve the %lu %s cap of the %s HG_MALLOC arena "
			   "(%lu %s to commit now): the host refused the mapping - with "
			   "vm.overcommit_memory=2 the caps must fit under CommitLimit\n",
				HG_SZ_VAL(cap_asked), HG_SZ_UNIT(cap_asked), name,
				HG_SZ_VAL(size), HG_SZ_UNIT(size));
		return NULL;
	}

	if(tier_decides && tier != HG_MEM_4K) {
		/* a huge page is the minimum INIT; refuse if the cap leaves no room */
		if(hg_page_round(cap_asked, HG_HPS) <= HG_HPS) {
			LM_ERR("%s arena: on huge pages INIT is at least one huge "
				   "page (%lu %s), which leaves no room to grow under the "
				   "%lu %s cap - raise the cap, or give INIT as %lu %s for "
				   "a fixed arena\n",
					name, HG_SZ_VAL(HG_HPS), HG_SZ_UNIT(HG_HPS),
					HG_SZ_VAL(cap_asked), HG_SZ_UNIT(cap_asked),
					HG_SZ_VAL(HG_HPS), HG_SZ_UNIT(HG_HPS));
			munmap(base, cap);
			return NULL;
		}
		LM_NOTICE("%s arena: INIT raised from %lu %s to %lu %s - on huge "
				  "pages an arena holds at least one whole page\n",
				name, HG_SZ_VAL(size), HG_SZ_UNIT(size), HG_SZ_VAL(HG_HPS),
				HG_SZ_UNIT(HG_HPS));
		size = HG_HPS;
	} else if(tier_decides) {
		/* 4K backing: small pages to the cap; the pre-faulted rest goes back */
		unsigned long keep = hg_page_round(size, HG_SMALL_PAGE);

		/* one small page is enough: the cap leaves room to grow */
		if(locked_b > keep) {
			munlock(base + keep, locked_b - keep);
			locked_b = keep;
		}
#ifdef __OS_linux
		/* off Linux these pages simply stay committed: there is no
		 * portable release, see hg_mem_release() */
		madvise(base + keep, HG_HPS - keep,
				shared ? MADV_REMOVE : MADV_DONTNEED);
#endif
		apage = HG_SMALL_PAGE;
	}

	/* the block header itself lives inside the reservation it describes */
	if(size < ROUNDUP_TO(sizeof(struct hg_block))) {
		LM_ERR("%s arena of %lu bytes too small for the block header "
			   "(%zu bytes)\n",
				name, size, sizeof(struct hg_block));
		munmap(base, cap);
		return NULL;
	}

	hb = (struct hg_block *)(void *)base;
	memset(hb, 0, sizeof *hb);
	hb->name = name;
	hb->hbase = base;
	/* one figure assigned to both: the span and the committed count part ways
	 * only when a page is punched */
	hb->span_end = hg_page_round(size, apage);
	hb->committed_bytes = hb->span_end;
	/* the arena is what the grid spans: the page-rounded INIT, not INIT */
	hb->size = hb->span_end;
	hb->committed_min = hb->committed_bytes;
	hb->hcap = cap;
	/* grow granule: an eighth of the cap, within one page and 16 MB */
	hb->grow_granule = hg_page_round(cap / 8, apage);
	if(hb->grow_granule > hg_page_round(16UL << 20, apage))
		hb->grow_granule = hg_page_round(16UL << 20, apage);
	if(hb->grow_granule < apage)
		hb->grow_granule = apage;
	/* override (hg_*_grow_granule), page rounded; the core shm arena takes
	 * it again in hg_post_cfg() */
	if(!shared && hg_pkg_grow_granule)
		hb->grow_granule = hg_page_round(hg_pkg_grow_granule, apage);
	else if(shared && !strcmp(name, "shm") && hg_shm_grow_granule)
		hb->grow_granule = hg_page_round(hg_shm_grow_granule, apage);
	/* shrink step, page rounded; re-applied to shm in hg_post_cfg() */
	hb->shrink_step = hg_page_round(hg_shrink_step, apage);
	if(hb->shrink_step < apage)
		hb->shrink_step = apage;
	/* the page unit of this arena's grid */
	hb->hps = apage;
	hb->tier = tier;
	hb->locked_mb = locked_b >> 20;
	hb->unpinned = (tier != HG_MEM_HUGETLB && locked_b == 0);
	hb->tier_bytes[tier] = hb->committed_bytes;
	hb->shared = shared;
	/* debug checks follow the manager chosen for shm or pkg; quarantine is
	 * 1/1024 of the reservation per thread, 16 KB to 1 MB */
	hb->dbg = shared ? hg_shm_mode == HG_MODE_DBG : hg_pkg_mode == HG_MODE_DBG;
	hb->quar_bytes = cap >> 10;
	if(hb->quar_bytes < HG_QUAR_MIN)
		hb->quar_bytes = HG_QUAR_MIN;
	else if(hb->quar_bytes > HG_QUAR_MAX)
		hb->quar_bytes = HG_QUAR_MAX;

	if(!lock_init(&hb->lock)) {
		LM_ERR("failed to init the %s arena lock\n", name);
		munmap(base, hb->hcap);
		return NULL;
	}

	hg_arena_reg_add(hb);
	if(shared)
		hg_atfork_register();

	/* the region right after the block header is the first thing chunks
	 * bump-carve from - hg_arena_init() sets hoff past it */
	if(hg_arena_init(hb, ROUNDUP_TO(sizeof(struct hg_block))) < 0) {
		LM_ERR("failed to init the %s arena\n", name);
		hg_arena_reg_del(hb);
		lock_destroy(&hb->lock);
		munmap(base, hb->hcap);
		return NULL;
	}

	/* hugetlb pages are not swappable, tiers 2-4 are mlock()ed: both pinned */
	if(proc_desc)
		LM_NOTICE("%s " HG_MALLOC_NAME " arena (%s): %lu %s on %s, %lu %s "
				  "pinned from swapping%s\n",
				name, proc_desc, HG_SZ_VAL(size), HG_SZ_UNIT(size),
				hg_mem_tier_str(tier), HG_SZ_VAL(locked_b),
				HG_SZ_UNIT(locked_b),
				tier == HG_MEM_4K ? hg_4k_reason(shared) : "");
	else
		LM_NOTICE("%s " HG_MALLOC_NAME " arena: %lu %s on %s, %lu %s "
				  "pinned from swapping%s%s\n",
				name, HG_SZ_VAL(size), HG_SZ_UNIT(size), hg_mem_tier_str(tier),
				HG_SZ_VAL(locked_b), HG_SZ_UNIT(locked_b),
				tier == HG_MEM_4K ? hg_4k_reason(shared) : "",
				(flags & HG_INIT_INHERITED) ? " (pre-fork arena, inherited "
											  "copy-on-write by every child)"
											: "");
	if(hb->hcap > hb->committed_bytes)
		LM_NOTICE("%s arena can grow to %lu %s (%lu %s headroom "
				  "reserved, uncommitted)\n",
				name, HG_SZ_VAL(hb->hcap), HG_SZ_UNIT(hb->hcap),
				HG_SZ_VAL(hb->hcap - hb->committed_bytes),
				HG_SZ_UNIT(hb->hcap - hb->committed_bytes));

	/* warn at start when the memlock limit cannot cover the cap: hugetlb grows
	 * would fail, other tiers would grow unpinned */
	if(hb->hcap > hb->committed_bytes) {
		unsigned long lim = hg_memlock_limit();
		unsigned long need =
				(tier == HG_MEM_HUGETLB) ? hb->grow_granule : hb->hcap;

		if(lim && lim < need && tier == HG_MEM_HUGETLB)
			LM_WARN("%s arena: RLIMIT_MEMLOCK is %lu KB, but growing "
					"on hugetlb pages needs %lu KB locked (one grow "
					"granule) - grows past the limit will be refused. Set "
					"LimitMEMLOCK=infinity in the service unit (or ulimit "
					"-l unlimited) before relying on the %lu %s cap.\n",
					name, lim >> 10, need >> 10, HG_SZ_VAL(hb->hcap),
					HG_SZ_UNIT(hb->hcap));
		else if(lim && lim < need)
			LM_WARN("%s arena: RLIMIT_MEMLOCK is %lu KB, less than the "
					"%lu %s cap - %s, so the arena can be swapped. Set "
					"LimitMEMLOCK=infinity in the service unit (or ulimit "
					"-l unlimited) to keep it pinned.\n",
					name, lim >> 10, HG_SZ_VAL(hb->hcap), HG_SZ_UNIT(hb->hcap),
					hb->unpinned ? "it runs unpinned "
								   "and grows without pinning"
								 : "grows past the limit "
								   "continue unpinned");
	}

	return hb;
}

void hg_malloc_destroy(struct hg_block *hb)
{
	if(!hb)
		return;

	hg_arena_reg_del(hb);
	hg_arena_destroy(hb);
	lock_destroy(&hb->lock);
	/* munmap last: hb itself lives inside hbase. The whole cap, not just
	 * the committed part - the reservation is one mapping */
	munmap(hb->hbase, hb->hcap);
}

/* pthread_atfork() child handler: drop the copy of the parent's slots in
 * every shared arena, or both would hand out the same cells */
void hg_malloc_child_init_all(void)
{
	int i;

	for(i = 0; i < HG_ARENA_REG_MAX; i++)
		if(hg_arena_reg[i].hb && hg_arena_reg[i].hb->shared)
			hg_arena_child_init(hg_arena_reg[i].hb);
}

/* fills a malloc info structure with info about the block */
void hg_info(struct hg_block *hb, struct mem_info *info)
{
	unsigned long recycled;

	memset(info, 0, sizeof *info);
	info->total_size = hb->size;
	info->min_frag = 64; /* smallest cell class, see hg_arena.c cell_sizes */
	info->used_size = hg_used(hb);

	/* idle carved capacity counts as free, so real_used follows live demand */
	recycled = hg_slab_recycled(hb);
	info->real_used = hb->real_used > recycled ? hb->real_used - recycled : 0;
	/* room left to carve: cached cells serve only their own class */
	info->free_size = hb->size > hb->real_used ? hb->size - hb->real_used : 0;

	/* peak of real_used above, not of the carve */
	if(info->real_used > hb->max_live_used)
		hb->max_live_used = info->real_used;
	info->max_used = hb->max_live_used;
	info->total_frags = hg_fragments(hb);
}

void hg_status(struct hg_block *hb)
{
	unsigned int nchunks;
	unsigned long bytes;

	HG_MEMLOG("hg_status (%p):\n", hb);
	if(!hb)
		return;

	hg_arena_stats(hb, &nchunks, &bytes);
	HG_MEMLOG(" heap size= %lu, tier=%s, pinned=%lu MB\n", hb->size,
			hg_mem_tier_str(hb->tier), hb->locked_mb);
	HG_MEMLOG(" chunks= %u, chunk bytes= %lu\n", nchunks, bytes);
	/* the live figures this process can see - its own slot(s) plus,
	 * for shm, every other process's */
	HG_MEMLOG(" live: used= %lu, fragments= %lu, cell bytes= %lu\n",
			hg_used(hb), hg_fragments(hb), hg_cell_live(hb));
#ifdef DBG_HG_MALLOC
	hg_sums(hb);
#endif
}

#ifdef DBG_HG_MALLOC
/* per-site figures by (file, func, line) into a mem_counter list built
 * with system malloc while the walker holds the lock */
struct hg_site_ctx
{
	struct hg_block *hb; /* the arena walked */
	mem_counter **root;
	unsigned long skipped_notlive; /* see hg_site_cb() */
	unsigned long no_memory;
};

static void hg_site_cb(void *payload, void *ctx)
{
	struct hg_site_ctx *c = ctx;
	char *tag = HG_HDR(payload);
	const char *file, *func;
	mem_counter *x;

	/* a corrupt chunk can yield an unmapped address: skip it */
	if(!hg_owns_any(tag)) {
		LM_CRIT("%s: the per-site walk produced %p, outside every arena - "
				"skipping it\n",
				HG_MALLOC_NAME, payload);
		hg_corrupt(c->hb, HG_C_FOREIGN_PTR);
		return;
	}
	file = HG_CELL_AT(tag)->file;
	if(!file)
		return; /* carved, never handed out by a debug-built call */
	func = HG_CELL_AT(tag)->func;

	/* file/func pointing into the arena means a free cell, not __FILE__ */
	if(hg_owns_any((void *)file) || hg_owns_any((void *)func)) {
		c->skipped_notlive++;
		return;
	}

	for(x = *c->root; x; x = x->next)
		if(x->file == file && x->func == func
				&& x->line == HG_CELL_AT(tag)->line)
			break;
	if(!x) {
		x = malloc(sizeof(mem_counter));
		if(!x) {
			c->no_memory++;
			return;
		}
		x->file = file;
		x->func = func;
		x->line = HG_CELL_AT(tag)->line;
		x->mname = HG_CELL_AT(tag)->mname;
		x->count = 0;
		x->size = 0;
		x->next = *c->root;
		*c->root = x;
	}
	x->count++;
	x->size += hg_frag_size(payload);
}

static void hg_sites_collect(
		struct hg_block *hb, mem_counter **root, struct hg_site_ctx *ctx)
{
	ctx->hb = hb;
	ctx->root = root;
	ctx->skipped_notlive = 0;
	ctx->no_memory = 0;
	hg_arena_walk_live(hb, hg_site_cb, ctx);
	hg_large_walk_live(hb, hg_site_cb, ctx);
}

void hg_sums(struct hg_block *hb)
{
	struct hg_site_ctx ctx;
	mem_counter *root = NULL, *x;

	if(!hb)
		return;
	HG_MEMLOG("summarizing all alloc'ed. fragments:\n");
	hg_sites_collect(hb, &root, &ctx);
	while(root) {
		x = root;
		HG_MEMLOG(" count=%6d size=%10lu bytes from %s: %s(%ld)\n", x->count,
				x->size, x->file, x->func, x->line);
		root = x->next;
		free(x);
	}
	if(ctx.skipped_notlive)
		HG_MEMLOG(" %lu cell(s) skipped: header held free-list linkage, so "
				  "the cell was free despite not being in the free set\n",
				ctx.skipped_notlive);
	if(ctx.no_memory)
		HG_MEMLOG(" %lu cell(s) not counted: no system memory for the "
				  "summary\n",
				ctx.no_memory);
	HG_MEMLOG("-----------------------------\n");
}

void hg_mod_get_stats(struct hg_block *hb, void **rootp)
{
	struct hg_site_ctx ctx;

	if(!hb || !rootp)
		return;
	hg_sites_collect(hb, (mem_counter **)rootp, &ctx);
}

void hg_mod_free_stats(void *rootp)
{
	mem_counter *x, *next;

	for(x = (mem_counter *)rootp; x; x = next) {
		next = x->next;
		free(x);
	}
}
#endif /* DBG_HG_MALLOC */

#include "hg_malloc_dyn.h"

#endif /* HG_MALLOC */

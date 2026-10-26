/*
 * slab allocator with elastic arenas - host memory checks
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

#include "hg_malloc.h"
#include "hg_memlimit.h"
#include "hg_backing.h"
#include "../dprint.h"
#include "../globals.h"
#include "shm.h"

/* RAM floor in MB for a host of @total_kb: hg_ram_floor_mb if set, else
 * MemTotal/20 but at least min(256 MB, MemTotal/8); 256 MB if unreadable.
 * MemTotal moves with memory hotplug and balloons, so it is never cached */
static long hg_ram_floor_for(long total_kb)
{
	if(hg_ram_floor_mb > 0)
		return hg_ram_floor_mb;
	if(total_kb <= 0)
		return 256;
	return hg_floor_mb_for(total_kb >> 10);
}

static long hg_ram_floor(void)
{
	return hg_ram_floor_for(
			hg_ram_floor_mb > 0 ? 0 : hg_meminfo_kb("MemTotal"));
}

/* hugetlb pool bytes free and not reserved */
static unsigned long long hg_pool_free_bytes(void)
{
	long free_pg = hg_meminfo_kb("HugePages_Free");
	long rsvd_pg = hg_meminfo_kb("HugePages_Rsvd");
	long page_kb = hg_meminfo_kb("Hugepagesize");

	if(free_pg < 0 || rsvd_pg < 0 || page_kb <= 0 || free_pg <= rsvd_pg)
		return 0;
	return (unsigned long long)(free_pg - rsvd_pg)
		   * ((unsigned long long)page_kb << 10);
}

/* RAM that can still be committed: above the host floor, or the cgroup's
 * when tighter; HG_ROOM_ANY when unknown. @ci gets the binding level */
static unsigned long long hg_ram_room_why(struct hg_cg_info *ci)
{
	long total_kb, avail_kb;
	unsigned long long floor_b, avail_b, room, cg;
	struct hg_cg_info c;

	/* one pass of /proc/meminfo for both */
	hg_meminfo_kb2("MemTotal", &total_kb, "MemAvailable", &avail_kb);
	floor_b = (unsigned long long)hg_ram_floor_for(total_kb) << 20;
	avail_b = (unsigned long long)avail_kb << 10;

	if(avail_kb < 0)
		room = HG_ROOM_ANY;
	else if(avail_b <= floor_b)
		room = 0;
	else
		room = avail_b - floor_b;
	cg = hg_cgroup_room(&c);
	if(cg < room)
		room = cg;
	else
		c.version = 0;
	if(ci)
		*ci = c;
	return room;
}

static unsigned long long hg_ram_room(void)
{
	return hg_ram_room_why(NULL);
}

/* can a new arena commit @init now (RAM above the floor, or the hugetlb
 * pool for a shared one)? Logs why not for @what; 1 if it fits */
int hg_init_fits(
		const char *what, unsigned long init, unsigned long cap, int shared)
{
	unsigned long apage = hg_arena_page(init, cap);
	int huge_grid = (apage == HG_HPS);
	/* an INIT under one huge page may be raised to one on huge pages */
	unsigned long need = hg_page_round(init, huge_grid ? HG_HPS : apage);
	struct hg_cg_info ci;
	unsigned long long room = hg_ram_room_why(&ci), pool;

	if(need <= room)
		return 1;
	pool = (huge_grid && shared) ? hg_pool_free_bytes() : 0;
	if(need <= pool)
		return 1;
	if(ci.version)
		LM_ERR("%s: the memory cgroup (v%d) is the tighter limit: %lu MB "
			   "limit, %lu MB in use, %lu MB floor\n",
				what, ci.version, ci.limit_mb, ci.used_mb, ci.floor_mb);
	LM_ERR("%s: committing %lu %s now does not fit - %lu MB of RAM is "
		   "available above the %ld MB floor (hg_ram_floor_mb) and the "
		   "hugetlb pool has %lu MB free. Lower INIT (-m/-M), the number of "
		   "processes, or the floor, or add memory or huge pages.\n",
			what, HG_SZ_VAL(need), HG_SZ_UNIT(need),
			room == HG_ROOM_ANY ? 0 : (unsigned long)(room >> 20),
			hg_ram_floor(), (unsigned long)(pool >> 20));
	return 0;
}

/* each process publishes its pkg arena's growth state here, when created
 * and at its sweeps, for the statistics to sum */
struct hg_pkg_pub
{
	unsigned long grow_refused;
	unsigned int grow_blocked;
	unsigned int live;		 /* the slot belongs to a running process */
	unsigned long committed; /* bytes backed */
	unsigned long grows;
	unsigned long shrinks;
	unsigned long carved_peak; /* max_real_used: what had to fit */
};
static struct hg_pkg_pub *hg_pkg_pub;
static unsigned int hg_pkg_pub_n;

int hg_pkg_pub_init(unsigned int nproc)
{
#ifdef PKG_MALLOC
	if(!HG_SERVES_PKG())
		return 0;
	hg_pkg_pub = shm_malloc(nproc * sizeof *hg_pkg_pub);
	if(!hg_pkg_pub) {
		LM_ERR("no shm for the pkg growth statistics\n");
		return -1;
	}
	memset(hg_pkg_pub, 0, nproc * sizeof *hg_pkg_pub);
	hg_pkg_pub_n = nproc;
#endif
	return 0;
}

void hg_pkg_publish(struct hg_block *hb)
{
	if(!hg_pkg_pub || process_no < 0
			|| (unsigned int)process_no >= hg_pkg_pub_n)
		return;
	hg_pkg_pub[process_no].grow_refused = hb->grow_refused;
	hg_pkg_pub[process_no].grow_blocked = hb->grow_blocked;
	hg_pkg_pub[process_no].committed = hb->committed_bytes;
	hg_pkg_pub[process_no].grows = hb->grows;
	hg_pkg_pub[process_no].shrinks = hb->shrinks;
	hg_pkg_pub[process_no].carved_peak = hb->max_real_used;
	hg_pkg_pub[process_no].live = 1;
}

/* the largest carve peak any process published, for hg.advise: -M has to
 * cover the worst process. Returns how many have published */
unsigned int hg_pkg_pub_peak(unsigned long *peak)
{
	unsigned int i, n = 0;

	*peak = 0;
	if(!hg_pkg_pub)
		return 0;
	for(i = 0; i < hg_pkg_pub_n; i++) {
		if(!hg_pkg_pub[i].live)
			continue;
		n++;
		if(hg_pkg_pub[i].carved_peak > *peak)
			*peak = hg_pkg_pub[i].carved_peak;
	}
	return n;
}

/* the published pkg arenas, summed: @n processes that have published,
 * their committed bytes (sum and largest), grows and shrinks */
void hg_pkg_pub_sum(unsigned int *n, unsigned long *committed,
		unsigned long *committed_max, unsigned long *grows,
		unsigned long *shrinks)
{
	unsigned int i;

	*n = 0;
	*committed = *committed_max = *grows = *shrinks = 0;
	if(!hg_pkg_pub)
		return;
	for(i = 0; i < hg_pkg_pub_n; i++) {
		if(!hg_pkg_pub[i].live)
			continue;
		(*n)++;
		*committed += hg_pkg_pub[i].committed;
		if(hg_pkg_pub[i].committed > *committed_max)
			*committed_max = hg_pkg_pub[i].committed;
		*grows += hg_pkg_pub[i].grows;
		*shrinks += hg_pkg_pub[i].shrinks;
	}
}

/* @ctx: 0 = refused grows summed over all processes, 1 = processes whose
 * pkg arena is GROW-BLOCKED now */
unsigned long hg_pkg_pub_stat(void *ctx)
{
	unsigned long sum = 0;
	unsigned int i;

	if(!hg_pkg_pub)
		return 0;
	for(i = 0; i < hg_pkg_pub_n; i++)
		sum += ctx ? (hg_pkg_pub[i].grow_blocked ? 1 : 0)
				   : hg_pkg_pub[i].grow_refused;
	return sum;
}

/* before a fork: can the child's pkg arena commit its INIT from RAM?
 * Refuse the fork cleanly if not; logged once per episode */
int hg_fork_fits(void)
{
#ifdef PKG_MALLOC
	static int said;

	if(!HG_SERVES_PKG())
		return 1;
	if(hg_ram_room() >= hg_page_round(pkg_mem_size,
			   hg_arena_page(pkg_mem_size, hg_pkg_cap_bytes) == HG_HPS
					   ? HG_HPS
					   : HG_SMALL_PAGE)) {
		said = 0;
		return 1;
	}
	if(!said) {
		said = 1;
		hg_init_fits(
				"new process's pkg arena", pkg_mem_size, hg_pkg_cap_bytes, 0);
		LM_ERR("not forking more processes until memory frees up\n");
	}
	return 0;
#else
	return 1; /* pkg memory is the system malloc */
#endif
}

/* before the first fork: does every process's pkg arena (-M INIT) fit in
 * the RAM above the floor? Private arenas never use the hugetlb pool.
 * -1 if not */
int hg_capacity_check(unsigned int nprocs)
{
#ifndef PKG_MALLOC
	return 0; /* pkg memory is the system malloc */
#else
	unsigned long init = pkg_mem_size, cap, initr;
	/* sums over every process: 64-bit, as the room they are held to */
	unsigned long long room, ram, caps;
	long total_kb;
	unsigned int children;

	if(!HG_SERVES_PKG())
		return 0;

	cap = hg_pkg_cap_bytes > init ? hg_pkg_cap_bytes : init;
	initr = hg_page_round(
			init, hg_arena_page(init, cap) == HG_HPS ? HG_HPS : HG_SMALL_PAGE);
	room = hg_ram_room();

	/* the main process keeps its pre-fork arena, every child creates one */
	children = nprocs > 1 ? nprocs - 1 : 0;
	ram = (unsigned long long)children * initr;
	LM_INFO("pkg: %u processes, -M INIT %lu %s: %lu MB from RAM; %ld MB of "
			"RAM available above the %ld MB floor\n",
			nprocs, HG_SZ_VAL(init), HG_SZ_UNIT(init),
			(unsigned long)(ram >> 20),
			room == HG_ROOM_ANY ? -1L : (long)(room >> 20), hg_ram_floor());

	if(room != HG_ROOM_ANY && ram > room) {
		LM_ERR("pkg: the %u processes need %lu MB of RAM for "
			   "their -M INIT of %lu %s, but %lu MB is available above the "
			   "%ld MB floor. Lower -M INIT or the worker counts, or add "
			   "memory.\n",
				nprocs, (unsigned long)(ram >> 20), HG_SZ_VAL(init),
				HG_SZ_UNIT(init), (unsigned long)(room >> 20), hg_ram_floor());
		return -1;
	}

	total_kb = hg_meminfo_kb("MemTotal");
	caps = (unsigned long long)children * cap;
	if(HG_SERVES_SHM() && hg_shm_block)
		caps += ((struct hg_block *)hg_shm_block)->hcap;
	if(total_kb > 0 && caps > ((unsigned long long)total_kb << 10))
		LM_WARN("the -m cap plus -M cap x %u processes is %lu MB, more "
				"than the host's %ld MB of memory: those caps can never all "
				"be met, and growth will be refused at the %ld MB floor\n",
				children, (unsigned long)(caps >> 20), total_kb >> 10,
				hg_ram_floor());
	return 0;
#endif
}

/* host RAM check of the growth ceiling: MemAvailable (or the cgroup) must
 * stay above the floor; passes when MemAvailable is missing. Reads /proc
 * and writes only @rc, so it runs with hb->lock released */
int hg_grow_room_short(
		struct hg_block *hb, unsigned long delta, struct hg_room_check *rc)
{
	unsigned long long cg_room;
	struct hg_cg_info ci;
	long total_kb;

	memset(rc, 0, sizeof *rc);
	/* hugetlb: the cap's pages were taken from the pool at map time */
	if(hb->tier == HG_MEM_HUGETLB)
		return 0;

	/* one pass of /proc/meminfo for the total (the floor) and the room */
	hg_meminfo_kb2("MemTotal", &total_kb, "MemAvailable", &rc->avail_kb);
	rc->floor_mb = hg_ram_floor_for(total_kb);
	cg_room = hg_cgroup_room(&ci);

	rc->nproc = 1;
	rc->effective = delta;
	if(!hb->shared) {
		/* pkg: every process grows its own arena under the same load */
		rc->nproc = hg_nprocs() ? hg_nprocs() : 1;
		rc->effective = (unsigned long long)delta * rc->nproc;
	}

	/* the host (unknown MemAvailable: let mlock decide), then the memory
	 * cgroup, which a container or a unit's MemoryMax hits first */
	rc->host_short =
			rc->avail_kb >= 0
			&& ((unsigned long long)rc->avail_kb << 10)
					   < rc->effective
								 + ((unsigned long long)rc->floor_mb << 20);
	rc->cg_short = cg_room != HG_ROOM_ANY && cg_room < rc->effective;
	rc->cg_version = ci.version;
	rc->cg_limit_mb = ci.limit_mb;
	rc->cg_used_mb = ci.used_mb;
	rc->cg_floor_mb = ci.floor_mb;
	return rc->host_short || rc->cg_short;
}

/* the refusal hg_grow_room_short() found: its reason, and the warning
 * once per episode; hb->lock held */
void hg_grow_room_refused(struct hg_block *hb, unsigned long delta,
		const struct hg_room_check *rc)
{
	hb->grow_refuse_why = rc->host_short ? HG_GR_HOST_FLOOR : HG_GR_CGROUP;
	if(hb->grow_refuse_said)
		return;
	hb->grow_refuse_said = 1;
	if(rc->host_short)
		LM_WARN("%s: refusing to grow by %lu MB: %lu MB effective"
				" (x%lu processes) would leave the host under the "
				"%ld MB floor (MemAvailable %ld MB). Freeing host "
				"memory or lowering the floor lifts this.\n",
				hb->name, delta >> 20, (unsigned long)(rc->effective >> 20),
				rc->nproc, rc->floor_mb, rc->avail_kb / 1024);
	else
		LM_WARN("%s: refusing to grow by %lu MB: %lu MB effective"
				" (x%lu processes) would leave the memory cgroup (v%d) "
				"under its %lu MB floor (limit %lu MB, %lu MB in use). "
				"Raising the cgroup's memory limit or freeing memory in "
				"it lifts this.\n",
				hb->name, delta >> 20, (unsigned long)(rc->effective >> 20),
				rc->nproc, rc->cg_version, rc->cg_floor_mb, rc->cg_limit_mb,
				rc->cg_used_mb);
}

#endif /* HG_MALLOC */

/*
 * slab allocator with elastic arenas - page backing tiers
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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/resource.h> /* getrlimit(RLIMIT_MEMLOCK) */

#include "hg_malloc.h"
#include "hg_memlimit.h"
#include "hg_backing.h"
#include "../dprint.h"

#define HG_HPS_FALLBACK (2UL * 1024 * 1024)

static unsigned long hg_hps_cached;

unsigned long hg_hps(void)
{
	FILE *f;
	char line[256];
	unsigned long kb = 0;

	if(hg_hps_cached)
		return hg_hps_cached;

	f = fopen("/proc/meminfo", "r");
	if(f) {
		while(fgets(line, sizeof line, f)) {
			if(!strncmp(line, "Hugepagesize:", 13)) {
				kb = strtoul(line + 13, NULL, 10);
				break;
			}
		}
		fclose(f);
	}

	/* must be a power of two for the alignment masks below to work */
	if(kb == 0 || (kb * 1024UL) & ((kb * 1024UL) - 1))
		hg_hps_cached = HG_HPS_FALLBACK;
	else
		hg_hps_cached = kb * 1024UL;

	return hg_hps_cached;
}

/* tier ladder: every tier is tried and verified through /proc, never
 * inferred from the kernel version or sysfs */

/* one numeric field ("Key:   value") from a /proc file, parsed in @base;
 * -1 if the file or the field is missing */
static long hg_proc_field(const char *path, const char *key, int base)
{
	FILE *f;
	char line[256];
	size_t klen = strlen(key);
	long v = -1;

	f = fopen(path, "r");
	if(!f)
		return -1;
	while(fgets(line, sizeof line, f)) {
		if(!strncmp(line, key, klen) && line[klen] == ':') {
			v = strtol(line + klen + 1, NULL, base);
			break;
		}
	}
	fclose(f);
	return v;
}

long hg_meminfo_kb(const char *key)
{
	return hg_proc_field("/proc/meminfo", key, 10);
}

/* two fields of /proc/meminfo in one pass (-1 each when missing) */
void hg_meminfo_kb2(const char *k1, long *v1, const char *k2, long *v2)
{
	FILE *f;
	char line[256];
	size_t l1 = strlen(k1), l2 = strlen(k2);

	*v1 = *v2 = -1;
	f = fopen("/proc/meminfo", "r");
	if(!f)
		return;
	while(fgets(line, sizeof line, f) && (*v1 < 0 || *v2 < 0)) {
		if(!strncmp(line, k1, l1) && line[l1] == ':')
			*v1 = strtol(line + l1 + 1, NULL, 10);
		else if(!strncmp(line, k2, l2) && line[l2] == ':')
			*v2 = strtol(line + l2 + 1, NULL, 10);
	}
	fclose(f);
}

/* mlock() checks RLIMIT_MEMLOCK for every mapping, hugetlb included, so a
 * low limit refuses grows with a full pool; these tell the two ENOMEMs apart */
#define HG_CAP_IPC_LOCK 14 /* CAP_IPC_LOCK, linux/capability.h */

static int hg_has_cap_ipc_lock(void)
{
	FILE *f;
	char line[256];
	unsigned long long eff = 0;
	int found = 0;

	f = fopen("/proc/self/status", "r");
	if(!f)
		return 0;
	while(fgets(line, sizeof line, f)) {
		if(!strncmp(line, "CapEff:", 7)) {
			eff = strtoull(line + 7, NULL, 16);
			found = 1;
			break;
		}
	}
	fclose(f);
	return found && (eff & (1ULL << HG_CAP_IPC_LOCK));
}

/* the RLIMIT_MEMLOCK soft limit in bytes, or 0 when it does not bind:
 * unlimited, unreadable, or lifted by CAP_IPC_LOCK */
unsigned long hg_memlock_limit(void)
{
	struct rlimit rl;

	if(getrlimit(RLIMIT_MEMLOCK, &rl) != 0 || rl.rlim_cur == RLIM_INFINITY)
		return 0;
	if(hg_has_cap_ipc_lock())
		return 0;
	return (unsigned long)rl.rlim_cur;
}

enum hg_grow_refusal hg_mlock_refusal_why(unsigned long lim, long locked_kb,
		unsigned long delta, int hugetlb, long free_pg, long rsvd_pg,
		long page_kb)
{
	if(locked_kb < 0)
		locked_kb = 0;
	if(lim && ((unsigned long long)locked_kb << 10) + delta > lim)
		return HG_GR_MEMLOCK;
	if(hugetlb && free_pg >= 0 && rsvd_pg >= 0 && page_kb > 0
			&& (unsigned long long)(free_pg - rsvd_pg)
							   * ((unsigned long long)page_kb << 10)
					   < delta)
		return HG_GR_HUGETLB_POOL;
	return HG_GR_MLOCK;
}

/* why mlock() refused @delta bytes of @hb, with the errno of the failed
 * mlock(); when @say, also the warning that names it */
static enum hg_grow_refusal hg_mlock_refusal(
		struct hg_block *hb, unsigned long delta, int err, int say)
{
	unsigned long lim = hg_memlock_limit();
	long locked_kb = hg_proc_field("/proc/self/status", "VmLck", 10);
	long free_pg = -1, rsvd_pg = -1, page_kb = -1;
	enum hg_grow_refusal why;

	if(hb->tier == HG_MEM_HUGETLB) {
		free_pg = hg_meminfo_kb("HugePages_Free");
		rsvd_pg = hg_meminfo_kb("HugePages_Rsvd");
		page_kb = hg_meminfo_kb("Hugepagesize");
	}
	why = hg_mlock_refusal_why(lim, locked_kb, delta,
			hb->tier == HG_MEM_HUGETLB, free_pg, rsvd_pg, page_kb);
	if(!say)
		return why;
	if(locked_kb < 0)
		locked_kb = 0;

	if(why == HG_GR_MEMLOCK)
		LM_WARN("%s: cannot grow by %lu MB: RLIMIT_MEMLOCK is %lu KB and "
				"this process already has %ld KB locked - every grow is "
				"mlock()ed, on hugetlb as well. Set LimitMEMLOCK=infinity "
				"in the service unit (or ulimit -l unlimited).\n",
				hb->name, delta >> 20, lim >> 10, locked_kb);
	else if(why == HG_GR_HUGETLB_POOL)
		LM_WARN("%s: cannot grow by %lu MB: the hugetlb pool has %ld "
				"free pages of %ld KB, %ld of them already reserved, "
				"and the grow needs %lu. Raise vm.nr_hugepages or "
				"vm.nr_overcommit_hugepages.\n",
				hb->name, delta >> 20, free_pg, page_kb, rsvd_pg,
				delta / ((unsigned long)page_kb << 10));
	else
		LM_WARN("%s: cannot grow by %lu MB: mlock failed (%s), and "
				"neither RLIMIT_MEMLOCK (%s) nor %s accounts for it.\n",
				hb->name, delta >> 20, strerror(err),
				lim ? "set" : "not binding",
				hb->tier == HG_MEM_HUGETLB ? "the hugetlb pool"
										   : "the locked-memory total");
	return why;
}

/* a grow's mlock() was refused: classified, logged once per episode and
 * recorded every time */
static void hg_mlock_refused(struct hg_block *hb, unsigned long delta, int err)
{
	if(!hb->grow_refuse_said || !hb->grow_refuse_mlock) {
		hb->grow_refuse_mlock =
				hg_mlock_refusal(hb, delta, err, !hb->grow_refuse_said);
		hb->grow_refuse_said = 1;
	}
	hb->grow_refuse_why = hb->grow_refuse_mlock;
}

/* shared anon memory shows as ShmemHugePages, private as AnonHugePages */

/* MADV_COLLAPSE ignores THP "never"; honour "never" (and shmem "deny")
 * as the admin's choice. Cached per mapping kind */
static int hg_thp_collapse_denied(int shared)
{
	static int cached[2] = {-1, -1};
	char buf[128];
	FILE *f;

	if(cached[!!shared] >= 0)
		return cached[!!shared];
	f = fopen(shared ? "/sys/kernel/mm/transparent_hugepage/shmem_enabled"
					 : "/sys/kernel/mm/transparent_hugepage/enabled",
			"r");
	if(!f || !fgets(buf, sizeof buf, f)) {
		if(f)
			fclose(f);
		cached[!!shared] = 0; /* cannot tell: not denied */
		return 0;
	}
	fclose(f);
	cached[!!shared] =
			strstr(buf, "[never]") != NULL || strstr(buf, "[deny]") != NULL;
	return cached[!!shared];
}

/* why a 4K arena is 4K: name the host setting (or the missing
 * MADV_COLLAPSE before Linux 6.1) in the log */
const char *hg_4k_reason(int shared)
{
	if(!hg_thp_collapse_denied(shared))
		return "";
	return shared ? " - host shmem_enabled is never/deny and this build "
					"honours "
					"it; set it to 'advise' to allow the MADV_COLLAPSE retrofit"
				  : " - host transparent_hugepage/enabled is never";
}

static long hg_read_huge_kb(int shared)
{
	return hg_meminfo_kb(shared ? "ShmemHugePages" : "AnonHugePages");
}

/* did a populate of @len land on huge pages? From the host-wide counter
 * delta (90% of the range), not smaps, which is slow on big arenas */
static int hg_delta_is_huge(long before_kb, long after_kb, unsigned long len)
{
	return before_kb >= 0 && after_kb >= 0
		   && after_kb - before_kb >= (long)(len / 1024) * 9 / 10;
}

/* keep the pre-faulted, pinned arena out of core dumps unless
 * HG_DUMP_ARENA=1 is set (read at init, before the config) */
static void hg_exclude_from_core(void *base, unsigned long size)
{
#ifdef MADV_DONTDUMP
	const char *want = getenv("HG_DUMP_ARENA");

	if(want && *want && *want != '0') {
		/* explicit: the range may have inherited VM_DONTDUMP */
		madvise(base, size, MADV_DODUMP);
		return;
	}

	madvise(base, size, MADV_DONTDUMP);
#endif
}

/* reserve at least @size bytes, huge-page backed where possible, pinned;
 * shm is MAP_SHARED, pkg MAP_PRIVATE so each child has its own COW arena */
void *hg_mem_reserve(unsigned long size, unsigned long *cap,
		enum hg_mem_tier *tier, unsigned long *locked_b, int shared,
		unsigned long apage)
{
	unsigned long asize = hg_page_round(size, apage);
	unsigned long csize = hg_page_round(*cap < size ? size : *cap, apage);
	int vis = shared ? MAP_SHARED : MAP_PRIVATE;
	int small = apage < HG_HPS;
	char *resv, *base;
	long shmem_kb;
	void *p;

	*locked_b = 0;
	*tier = HG_MEM_4K;
	*cap = csize; /* rewritten below if a fallback shrinks it */

#ifndef __OS_linux
	/* no verified huge-page route off Linux: plain mapping, 4K tier */
	p = mmap(NULL, csize, PROT_READ | PROT_WRITE, vis | MAP_ANONYMOUS, -1, 0);
	if(p == MAP_FAILED)
		return NULL;
	hg_exclude_from_core(p, csize);
	if(mlock(p, asize) == 0)
		*locked_b = asize;
	else
		memset(p, 0, asize);
	return p;
#else

	/* tier 1, MAP_HUGETLB: the whole cap first, else INIT alone (a fixed
	 * arena keeps hugetlb rather than dropping to THP) */
	/* shared arenas only: a private one may be inherited copy-on-write by
	 * a fork() the core did not make, and a COW fault on an empty hugetlb
	 * pool is SIGBUS, where THP splits to 4K instead */
	p = (!shared || small) ? MAP_FAILED
						   : mmap(NULL, csize, PROT_READ | PROT_WRITE,
									 vis | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
	if(p == MAP_FAILED && shared && !small && csize > asize) {
		p = mmap(NULL, asize, PROT_READ | PROT_WRITE,
				vis | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if(p != MAP_FAILED) {
			LM_NOTICE("hugetlb pool cannot back a %lu MB cap; "
					  "reserving the %lu MB in use instead - the arena "
					  "keeps huge pages but cannot grow. Raise "
					  "vm.nr_hugepages to allow growth.\n",
					csize >> 20, asize >> 20);
			*cap = asize; /* the arena is fixed after all */
		}
		/* on total failure csize stays at the full cap for tiers 2-4: THP
		 * reservations are plain VA, which can hold the cap */
	}
	if(p != MAP_FAILED) {
		hg_exclude_from_core(p, *cap);
		memset(p, 0, asize);
		*tier = HG_MEM_HUGETLB;
		*locked_b = asize;
		return p;
	}

	/* tiers 2-4: PROT_NONE reserve then a MAP_FIXED R/W mapping of the whole
	 * cap at a huge-page boundary, made before fork so growth is seen by all */
	resv = mmap(
			NULL, csize + apage, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(resv == MAP_FAILED)
		return NULL;
	base = (char *)(((unsigned long)resv + apage - 1) & ~(apage - 1));
	p = mmap(base, csize, PROT_READ | PROT_WRITE,
			vis | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if(p == MAP_FAILED) {
		munmap(resv, csize + apage);
		return NULL;
	}

	hg_exclude_from_core(base, csize);

	/* small mode: no THP possible, report the 4K tier */
	if(small) {
		if(mlock(base, asize) == 0) {
			*locked_b = asize;
		} else {
			LM_WARN("mlock of the %lu KB HG_MALLOC arena failed "
					"(%s): continuing unpinned (swappable)\n",
					asize >> 10, strerror(errno));
			memset(base, 0, asize); /* still pre-fault */
		}
		return base;
	}

	/* advise huge before first touch, then mlock to pin and pre-fault; each
	 * grow verifies its own backing */
	madvise(base, csize, MADV_HUGEPAGE);
	shmem_kb = hg_read_huge_kb(shared);
	if(mlock(base, asize) == 0) {
		*locked_b = asize;
	} else {
		LM_WARN("mlock of the %lu MB HG_MALLOC arena failed (%s): "
				"continuing unpinned (swappable). If running under "
				"systemd, add LimitMEMLOCK=infinity to the unit.\n",
				asize >> 20, strerror(errno));
		memset(base, 0, asize); /* still pre-fault */
	}

	if(hg_delta_is_huge(shmem_kb, hg_read_huge_kb(shared), asize)) {
		*tier = HG_MEM_THP_ADVISE;
	} else if(shmem_kb >= 0 && !hg_thp_collapse_denied(shared)
			  && madvise(base, asize, MADV_COLLAPSE) == 0
			  && hg_read_huge_kb(shared) - shmem_kb >= (long)(asize / 1024)) {
		*tier = HG_MEM_THP_COLLAPSE;
	} else {
		*tier = HG_MEM_4K; /* reserved+pinned but 4K */
	}
	return base;
#endif /* __OS_linux */
}

/* MADV_COLLAPSE a committed range: 1 on huge pages, else -errno (0 when
 * the THP policy forbids); EAGAIN is often transient */
int hg_mem_collapse(struct hg_block *hb, char *base, unsigned long len)
{
#ifdef __OS_linux
	if(hg_thp_collapse_denied(hb->shared))
		return 0;
	return madvise(base, len, MADV_COLLAPSE) == 0 ? 1 : -errno;
#else
	return 0;
#endif
}

/* commit [hbase+off, +delta): mlock populates (and pins) with a clean
 * errno, never a later SIGBUS. Returns the achieved tier, or -1 undone */
int hg_mem_commit(struct hg_block *hb, unsigned long off, unsigned long delta,
		int *collapse_later)
{
	char *base = hb->hbase + off;
	int mlock_rc, is_huge, collapsed;

	if(off + delta > hb->hcap) {
		LM_BUG("%s: commit of %lu@%lu overruns the %lu byte cap\n", hb->name,
				delta, off, hb->hcap);
		hb->grow_refuse_why = HG_GR_CAP;
		return -1;
	}

	if(hb->tier == HG_MEM_HUGETLB) {
		mlock_rc = mlock(base, delta);
		if(mlock_rc != 0) {
			hg_mlock_refused(hb, delta, errno);
			munlock(base, delta);
			return -1;
		}
		__sync_fetch_and_add(&hb->locked_mb, delta >> 20);
		return HG_MEM_HUGETLB;
	}

#ifdef __OS_linux
	{
		long shmem_kb;

		shmem_kb = hg_read_huge_kb(hb->shared);

		/* re-advise the delta: a later madvise may have split the VMA */
		madvise(base, delta, MADV_HUGEPAGE);

		if(hb->unpinned) {
			/* unpinned arena (init mlock failed): grow by write faults */
			if(madvise(base, delta, MADV_POPULATE_WRITE) != 0)
				memset(base, 0, delta); /* pre-5.14 kernels */
			mlock_rc = 0;
		} else {
			mlock_rc = mlock(base, delta);
			if(mlock_rc != 0 && hg_memlock_limit()) {
				/* THP/4K memory is swappable: a grow past the memlock limit
				 * goes on unpinned; hugetlb grows still need the lock */
				munlock(base, delta);
				LM_WARN("%s: RLIMIT_MEMLOCK (%lu KB) cannot pin a %lu MB "
						"grow - the arena continues unpinned (swappable). "
						"Set LimitMEMLOCK=infinity in the service unit (or "
						"ulimit -l unlimited) to keep it pinned.\n",
						hb->name, hg_memlock_limit() >> 10, delta >> 20);
				hb->unpinned = 1;
				if(madvise(base, delta, MADV_POPULATE_WRITE) != 0)
					memset(base, 0, delta); /* pre-5.14 kernels */
				mlock_rc = 0;
			}
		}
		if(mlock_rc != 0) {
			hg_mlock_refused(hb, delta, errno);
			munlock(base, delta);
			return -1;
		}
		if(!hb->unpinned)
			__sync_fetch_and_add(&hb->locked_mb, delta >> 20);

		/* the delta's backing is negotiated afresh: verify it */
		/* O(1) verification - the counter delta across the populate,
		 * not a walk of the whole VMA's page tables */
		is_huge =
				hg_delta_is_huge(shmem_kb, hg_read_huge_kb(hb->shared), delta);
		if(is_huge)
			return HG_MEM_THP_ADVISE;
		/* a worker is waiting: publish on 4 KB now, the maintenance process
		 * collapses later (hg_collapse_deferred) */
		if(collapse_later && shmem_kb >= 0
				&& !hg_thp_collapse_denied(hb->shared)) {
			*collapse_later = 1;
			return HG_MEM_4K;
		}
		collapsed = shmem_kb >= 0 && !hg_thp_collapse_denied(hb->shared)
					&& madvise(base, delta, MADV_COLLAPSE) == 0
					&& hg_read_huge_kb(hb->shared) - shmem_kb
							   >= (long)(delta / 1024);
		if(collapsed)
			return HG_MEM_THP_COLLAPSE;
		return HG_MEM_4K;
	}
#else
	if(mlock(base, delta) != 0) {
		memset(base, 0, delta); /* still pre-fault */
	} else {
		__sync_fetch_and_add(&hb->locked_mb, delta >> 20);
	}
	return HG_MEM_4K;
#endif
}

/* release [off, off + len) of the span to the host */
int hg_mem_release(struct hg_block *hb, unsigned long off, unsigned long len)
{
	char *base = hb->hbase + off;
#ifdef __OS_linux
	int advice = hb->shared ? MADV_REMOVE : MADV_DONTNEED;
#endif

	if(off + len > hb->span_end) {
		LM_BUG("%s: release of %lu@%lu overruns the %lu byte span\n", hb->name,
				len, off, hb->span_end);
		return -1;
	}

#ifndef __OS_linux
	/* no portable way to give shared pages back off Linux: refuse */
	(void)base;
	if(!hb->shrink_unsupported) {
		hb->shrink_unsupported = 1;
		LM_NOTICE("%s: returning memory to the OS is Linux-only - this "
				  "arena grows but does not shrink here\n",
				hb->name);
	}
	return -1;
#else
	munlock(base, len);

	if(madvise(base, len, advice) != 0) {
		/* structural: say once and stop shrinking this arena */
		hb->shrink_unsupported = 1;
		LM_WARN("%s: cannot release memory (%s of %lu MB failed: %s) "
				"- shrink disabled for this arena\n",
				hb->name, hb->shared ? "MADV_REMOVE" : "MADV_DONTNEED",
				len >> 20, strerror(errno));
		return -1;
	}

	if(hb->locked_mb >= len >> 20)
		hb->locked_mb -= len >> 20;
	else
		hb->locked_mb = 0;
	return 0;
#endif /* __OS_linux */
}

/* re-commit a punched range in the calling process: mlock is per VMA and
 * not inherited. Tiers 2-4 go on unpinned on failure, hugetlb refuses */
int hg_mem_repin(
		struct hg_block *hb, unsigned long off, unsigned long len, int tier)
{
	char *base = hb->hbase + off;

	if(off + len > hb->span_end) {
		LM_BUG("%s: repin of %lu@%lu is outside the %lu byte span\n", hb->name,
				len, off, hb->span_end);
		return -1;
	}

	if(hb->unpinned) {
		/* init could not pin and the arena went on unpinned; a
		 * populating write fault is the commit without the pin */
#ifdef __OS_linux
		if(madvise(base, len, MADV_POPULATE_WRITE) != 0)
#endif
			memset(base, 0, len);
		return 0;
	}

	if(mlock(base, len) != 0) {
		hb->repin_failed++;
		if(tier == HG_MEM_HUGETLB) {
			if(!hb->repin_said) {
				hb->repin_said = 1;
				LM_WARN("%s: the hugetlb pool cannot back a "
						"%lu KB page released earlier (%s) - "
						"it "
						"was returned to the pool and is gone; "
						"the request is refused. Raise "
						"vm.nr_hugepages.\n",
						hb->name, len >> 10, strerror(errno));
			}
			return -1; /* not touched: nothing is there */
		}
		if(!hb->repin_said) {
			hb->repin_said = 1;
			LM_WARN("%s: could not re-pin a %lu KB page released "
					"earlier (%s) - it is in use again but NOT "
					"pinned from swapping. Watch "
					"hg_shm_repin_failed; raise RLIMIT_MEMLOCK "
					"(LimitMEMLOCK=infinity under systemd).\n",
					hb->name, len >> 10, strerror(errno));
		}
		/* still populate, so the caller gets usable memory */
		memset(base, 0, len);
		return -1;
	}
	__sync_fetch_and_add(&hb->locked_mb, len >> 20);
	return 0;
}

#endif /* HG_MALLOC */

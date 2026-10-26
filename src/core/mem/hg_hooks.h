/*
 * slab allocator with elastic arenas - hooks for the rest of the core
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

/* calls the core makes into hg; no-ops unless hg serves shm or pkg, and
 * nothing without HG_MALLOC. No includes: cfg/cfg_struct.h includes it */

#ifndef hg_hooks_h
#define hg_hooks_h

#ifdef HG_MALLOC

/* set when -x / -X select hg (hg_kam.c) */
extern int hg_shm_mode;
extern int hg_pkg_mode;

#define HG_ACTIVE() ((hg_shm_mode | hg_pkg_mode) != 0)

/* the caps of -m INIT:CAP and -M INIT:CAP in bytes; 0 = a fixed arena */
extern unsigned long hg_shm_cap_bytes;
extern unsigned long hg_pkg_cap_bytes;

/* after the config is parsed and shm exists: config-dependent settings,
 * counters and RPC commands */
int hg_kam_post_cfg(void);
/* once the process table is sized, before the first fork */
int hg_kam_prefork(void);
/* before each fork: can the new process's pkg arena be committed */
int hg_kam_fork_fits(void);
/* first thing in a forked child */
void hg_kam_child_init(const char *desc);
/* the maintenance process: how many to count, and the fork itself */
int hg_maint_count_processes(void);
int hg_maint_start(void);
/* a place where the process may run its idle-cache sweep */
void hg_sweep_point(void);
/* a thread sharing its process's arenas (TCP reactor pool): once at its
 * start, and at each job boundary */
void hg_thread_stats_init(void);
void hg_cache_flush_if_due(void);

#define KSR_MEM_SWEEP_POINT() \
	do {                      \
		if(HG_ACTIVE())       \
			hg_sweep_point(); \
	} while(0)

#else /* HG_MALLOC */

#define HG_ACTIVE() 0
#define hg_kam_post_cfg() 0
#define hg_kam_prefork() 0
#define hg_kam_fork_fits() 1
#define hg_kam_child_init(desc) \
	do {                        \
	} while(0)
#define hg_maint_count_processes() 0
#define hg_maint_start() 0
#define hg_thread_stats_init() \
	do {                       \
	} while(0)
#define hg_cache_flush_if_due() \
	do {                        \
	} while(0)
#define KSR_MEM_SWEEP_POINT() \
	do {                      \
	} while(0)

#endif /* HG_MALLOC */

#endif /* hg_hooks_h */

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

/* what the allocator needs from the core: what it serves, the caps, the
 * tunables and the log helpers (defined in hg_kam.c) */

#ifndef hg_kam_h
#define hg_kam_h

/* the core's memory debugging (MEMDBG=1, the default build) passes every
 * call's file, function, line and module; the cell header keeps them */
#ifdef DBG_SR_MEMORY
#ifndef DBG_HG_MALLOC
#define DBG_HG_MALLOC
#endif
#endif

#include "../lock_ops.h"
#include "../dprint.h"
#include "../pt.h"
#include "../cfg/cfg.h"
#include "../cfg_core.h"
#include "hg_hooks.h"

/* how the core uses the allocator, per memory type (hg_shm_mode and
 * hg_pkg_mode, declared in hg_hooks.h) */
#define HG_MODE_NONE 0 /* another manager serves this memory */
#define HG_MODE_ON 1   /* "hg" */
#define HG_MODE_DBG 2  /* "hgdbg": red zones, poisoned frees, quarantine */

#define HG_SERVES_SHM() (hg_shm_mode != HG_MODE_NONE)
#define HG_SERVES_PKG() (hg_pkg_mode != HG_MODE_NONE)

/* the arenas behind shm_malloc() and pkg_malloc() when the allocator
 * serves them (struct hg_block *), NULL otherwise */
extern void *hg_shm_block;
extern void *hg_pkg_block;

/* the caps of -m INIT:CAP and -M INIT:CAP in bytes; 0 = a fixed arena */
extern unsigned long hg_shm_cap_bytes;
extern unsigned long hg_pkg_cap_bytes;

/* tunables, see hg_kam.c */
extern int hg_ram_floor_mb;
extern unsigned long hg_shm_grow_granule;
extern unsigned long hg_pkg_grow_granule;
extern int hg_lock_stall_us;
extern int hg_grow_ahead;
extern int hg_scaling_cycle;
extern unsigned long hg_shrink_step;
extern int hg_interior_release;

/* the managers' init, from shm_init_manager() / pkg_init_manager();
 * @dbg selects the debug checks ("hgdbg") */
int hg_malloc_init_shm_manager(int dbg);
int hg_malloc_init_pkg_manager(int dbg);

/* processes this instance runs: the process table size once it exists,
 * an estimate before */
unsigned int hg_nprocs(void);

/* threads that share one process's private caches: the TCP reactor pool
 * when it runs, else 0 (the caller falls back to the CPU count) */
int hg_io_threads(void);

/* status and summary dumps at the core's memlog level, like the others */
#define HG_MEMLOG(fmt, args...)                                            \
	LOG_FP(DEFAULT_FACILITY, cfg_get(core, core_cfg, memlog), "hg: ", fmt, \
			##args)

#endif /* hg_kam_h */

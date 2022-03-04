/*
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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

/**
 * \file
 * \brief  Shared memory functions
 * \ingroup mem
 */

#ifndef _sr_shm_h_
#define _sr_shm_h_

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>

#ifndef SHM_MMAP

#include <sys/shm.h>

#endif

#include <sys/sem.h>

#include "memapi.h"

#include "../dprint.h"
#include "../lock_ops.h" /* we don't include locking.h on purpose */

extern sr_shm_api_t _shm_root;

#ifdef DBG_SR_MEMORY

#	define shm_malloc(s)         _shm_root.xmalloc(_shm_root.mem_block, (s), _SRC_LOC_, \
									_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_mallocxz(s)       _shm_root.xmallocxz(_shm_root.mem_block, (s), _SRC_LOC_, \
									_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_malloc_unsafe(s)  _shm_root.xmalloc_unsafe(_shm_root.mem_block, (s), _SRC_LOC_, \
									_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_realloc(p, s)     _shm_root.xrealloc(_shm_root.mem_block, (p), (s), \
									_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_reallocxf(p, s)   _shm_root.xreallocxf(_shm_root.mem_block, (p), (s), \
									_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_resize(p, s)      _shm_root.xresize(_shm_root.mem_block, (p), (s), \
									_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_free(p)           _shm_root.xfree(_shm_root.mem_block, (p), _SRC_LOC_, \
									_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define shm_free_unsafe(p)    _shm_root.xfree_unsafe(_shm_root.mem_block, (p), _SRC_LOC_, \
									_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#else
#	define shm_malloc(s)         _shm_root.xmalloc(_shm_root.mem_block, (s))
#	define shm_mallocxz(s)       _shm_root.xmallocxz(_shm_root.mem_block, (s))
#	define shm_malloc_unsafe(s)  _shm_root.xmalloc_unsafe(_shm_root.mem_block, (s))
#	define shm_realloc(p, s)     _shm_root.xrealloc(_shm_root.mem_block, (p), (s))
#	define shm_reallocxf(p, s)   _shm_root.xreallocxf(_shm_root.mem_block, (p), (s))
#	define shm_resize(p, s)      _shm_root.xresize(_shm_root.mem_block, (p), (s))
#	define shm_free(p)           _shm_root.xfree(_shm_root.mem_block, (p))
#	define shm_free_unsafe(p)    _shm_root.xfree_unsafe(_shm_root.mem_block, (p))
#endif

#	define shm_status()    _shm_root.xstatus(_shm_root.mem_block)
#	define shm_info(mi)    _shm_root.xinfo(_shm_root.mem_block, mi)
#	define shm_report(mr)  _shm_root.xreport(_shm_root.mem_block, mr)
#	define shm_available() _shm_root.xavailable(_shm_root.mem_block)
#	define shm_sums()      _shm_root.xsums(_shm_root.mem_block)
#	define shm_mod_get_stats(x)     _shm_root.xmodstats(_shm_root.mem_block, x)
#	define shm_mod_free_stats(x)    _shm_root.xfmodstats(x)

#	define shm_global_lock() _shm_root.xglock(_shm_root.mem_block)
#	define shm_global_unlock() _shm_root.xgunlock(_shm_root.mem_block)


void* shm_core_get_pool(void);
int shm_init_api(sr_shm_api_t *ap);
int shm_init_manager(char *name);
void shm_destroy_manager(void);
void shm_print_manager(void);

int shm_address_in(void *p);

#define shm_available_safe() shm_available()
#define shm_malloc_on_fork() do{}while(0)

/* generic logging helper for allocation errors in shared memory pool */
#define SHM_MEM_ERROR LM_ERR("could not allocate shared memory from shm pool\n")
#define SHM_MEM_CRITICAL LM_CRIT("could not allocate shared memory from shm pool\n")

#ifdef __SUNPRO_C
#define SHM_MEM_ERROR_FMT(...) LM_ERR("could not allocate shared memory from shm pool" __VA_ARGS__)
#define SHM_MEM_CRITICAL_FMT(...) LM_CRIT("could not allocate shared memory from shm pool" __VA_ARGS__)
#else
#define SHM_MEM_ERROR_FMT(fmt, args...) LM_ERR("could not allocate shared memory from shm pool - " fmt , ## args)
#define SHM_MEM_CRITICAL_FMT(fmt, args...) LM_CRIT("could not allocate shared memory from shm pool - " fmt , ## args)
#endif

#endif /* _sr_shm_h_ */

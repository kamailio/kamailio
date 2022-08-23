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


#ifndef _sr_mem_api_
#define _sr_mem_api_

#include <string.h>

#include "src_loc.h"
#include "meminfo.h"
#include "memdbg.h"

#ifdef DBG_SR_MEMORY

typedef void* (*sr_malloc_f)(void* mbp, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname);
typedef void  (*sr_free_f)(void* mbp, void* p, const char* file, const char* func,
					unsigned int line, const char* mname);
typedef void* (*sr_realloc_f)(void* mbp, void* p, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname);
typedef void* (*sr_resize_f)(void* mbp, void* p, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname);

#else /*DBG_SR_MEMORY*/

typedef void* (*sr_malloc_f)(void* mbp, size_t size);
typedef void  (*sr_free_f)(void* mbp, void* p);
typedef void* (*sr_realloc_f)(void* mbp, void* p, size_t size);
typedef void* (*sr_resize_f)(void* mbp, void* p, size_t size);

#endif /*DBG_SR_MEMORY*/

typedef void  (*sr_shm_glock_f)(void* mbp);
typedef void  (*sr_shm_gunlock_f)(void* mbp);

typedef void  (*sr_mem_status_f)(void* mbp);
typedef void  (*sr_mem_info_f)(void* mbp, struct mem_info* info);
typedef void  (*sr_mem_report_f)(void* mbp, mem_report_t* mrep);
typedef unsigned long (*sr_mem_available_f)(void* mbp);
typedef void  (*sr_mem_sums_f)(void* mbp);

typedef void  (*sr_mem_destroy_f)(void);

typedef void (*sr_mem_mod_get_stats_f)(void* mbp, void **p);
typedef void (*sr_mem_mod_free_stats_f)(void* mbp);

/*private memory api*/
typedef struct sr_pkg_api {
	/*memory manager name - soft copy*/
	char *mname;
	/*entire private memory zone - soft copy*/
	char *mem_pool;
	/*memory manager block - soft copy*/
	void *mem_block;
	/*memory chunk allocation*/
	sr_malloc_f        xmalloc;
	/*memory chunk allocation with 0 filling */
	sr_malloc_f        xmallocxz;
	/*memory chunk reallocation*/
	sr_realloc_f       xrealloc;
	/*memory chunk reallocation with always free of old buffer*/
	sr_realloc_f       xreallocxf;
	/*memory chunk free*/
	sr_free_f          xfree;
	/*memory status*/
	sr_mem_status_f    xstatus;
	/*memory info - internal metrics*/
	sr_mem_info_f      xinfo;
	/*memory report - internal report*/
	sr_mem_report_f    xreport;
	/*memory available size*/
	sr_mem_available_f xavailable;
	/*memory summary*/
	sr_mem_sums_f      xsums;
	/*memory destroy manager*/
	sr_mem_destroy_f   xdestroy;
	/*memory stats per module*/
	sr_mem_mod_get_stats_f  xmodstats;
	/*memory stats free per module*/
	sr_mem_mod_free_stats_f xfmodstats;
} sr_pkg_api_t;

/*shared memory api*/
typedef struct sr_shm_api {
	/*memory manager name - soft copy*/
	char *mname;
	/*entire private memory zone - soft copy*/
	void *mem_pool;
	/*memory manager block - soft copy*/
	void *mem_block;
	/*memory chunk allocation*/
	sr_malloc_f        xmalloc;
	/*memory chunk allocation with 0 filling */
	sr_malloc_f        xmallocxz;
	/*memory chunk allocation without locking shm*/
	sr_malloc_f        xmalloc_unsafe;
	/*memory chunk reallocation*/
	sr_realloc_f       xrealloc;
	/*memory chunk reallocation with always free of old buffer*/
	sr_realloc_f       xreallocxf;
	/*memory chunk resizing - free+malloc in same locking*/
	sr_resize_f        xresize;
	/*memory chunk free*/
	sr_free_f          xfree;
	/*memory chunk free without locking shm*/
	sr_free_f          xfree_unsafe;
	/*memory status*/
	sr_mem_status_f    xstatus;
	/*memory info - internal metrics*/
	sr_mem_info_f      xinfo;
	/*memory report - internal report*/
	sr_mem_report_f    xreport;
	/*memory available size*/
	sr_mem_available_f xavailable;
	/*memory summary*/
	sr_mem_sums_f      xsums;
	/*memory destroy manager*/
	sr_mem_destroy_f   xdestroy;
	/*memory stats per module*/
	sr_mem_mod_get_stats_f  xmodstats;
	/*memory stats free per module*/
	sr_mem_mod_free_stats_f xfmodstats;
	/*memory managing global lock*/
	sr_shm_glock_f          xglock;
	/*memory managing global unlock*/
	sr_shm_gunlock_f        xgunlock;
} sr_shm_api_t;

#endif

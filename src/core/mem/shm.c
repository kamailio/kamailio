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

#include <stdlib.h>

#include "../config.h"
#include "../globals.h"
#include "memdbg.h"
#include "shm.h"

#ifdef SHM_MMAP

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h> /*open*/
#include <sys/stat.h>
#include <fcntl.h>

#endif

#include "memcore.h"

#define _ROUND2TYPE(s, type) \
	(((s) + (sizeof(type) - 1)) & (~(sizeof(type) - 1)))
#define _ROUND_LONG(s) _ROUND2TYPE(s, long)


void shm_core_destroy(void);

#ifndef SHM_MMAP
static int *_shm_core_shmid = NULL; /* shared memory ids */
#endif

static void **_shm_core_pools_mem = NULL;
static unsigned long *_shm_core_pools_size = NULL;
static unsigned int _shm_core_pools_num = 0;
static int _shm_core_pools_touched = 0;

sr_shm_api_t _shm_root = {0};

typedef struct sr_shm_block_api
{
	sr_shm_api_t api;
	gen_lock_t *lock;
} sr_shm_block_api_t;

static sr_shm_block_api_t *_shm_blocks = NULL;
static unsigned int _shm_blocks_count = 0;

/**
 *
 */
int shm_core_pools_init(void)
{
	unsigned int i;
	unsigned int pinit;
	unsigned long base_size;
	unsigned long remainder;

#ifdef SHM_MMAP
#ifndef USE_ANON_MMAP
	int fd;
#endif
#else
	struct shmid_ds shm_info;
#endif

	if(_shm_core_pools_mem == NULL) {
		if(shm_mem_blocks == 0 || shm_mem_size < shm_mem_blocks) {
			LM_CRIT("invalid shm layout: %lu bytes in %u blocks\n",
					shm_mem_size, shm_mem_blocks);
			return -1;
		}
		_shm_core_pools_num = shm_mem_blocks;
		_shm_core_pools_mem =
				calloc(_shm_core_pools_num, sizeof(*_shm_core_pools_mem));
		_shm_core_pools_size =
				calloc(_shm_core_pools_num, sizeof(*_shm_core_pools_size));
#ifndef SHM_MMAP
		_shm_core_shmid =
				malloc(_shm_core_pools_num * sizeof(*_shm_core_shmid));
#endif
		if(_shm_core_pools_mem == NULL || _shm_core_pools_size == NULL
#ifndef SHM_MMAP
				|| _shm_core_shmid == NULL
#endif
		) {
			LM_CRIT("could not allocate shm pool metadata\n");
			free(_shm_core_pools_mem);
			free(_shm_core_pools_size);
#ifndef SHM_MMAP
			free(_shm_core_shmid);
			_shm_core_shmid = NULL;
#endif
			_shm_core_pools_mem = NULL;
			_shm_core_pools_size = NULL;
			_shm_core_pools_num = 0;
			return -1;
		}
		base_size = shm_mem_size / _shm_core_pools_num;
		remainder = shm_mem_size % _shm_core_pools_num;
		LM_INFO("shared memory: %lu bytes in %u blocks (%lu bytes per "
				"block%s)\n",
				shm_mem_size, _shm_core_pools_num, base_size,
				remainder ? ", remainder assigned to first blocks" : "");
		for(i = 0; i < _shm_core_pools_num; i++) {
			_shm_core_pools_mem[i] = (void *)-1;
			_shm_core_pools_size[i] = base_size + (i < remainder ? 1 : 0);
#ifndef SHM_MMAP
			_shm_core_shmid[i] = -1;
#endif
		}
	}

	pinit = 0;
	for(i = 0; i < _shm_core_pools_num; i++) {
#ifdef SHM_MMAP
		if(_shm_core_pools_mem[i] != (void *)-1) {
#else
		if((_shm_core_shmid[i] != -1)
				|| (_shm_core_pools_mem[i] != (void *)-1)) {
#endif
			LM_DBG("shm pool[%u] already initialized\n", i);
			pinit++;
		}
	}
	if(pinit != 0) {
		if(pinit == _shm_core_pools_num) {
			LM_DBG("all shm pools initialized\n");
			return 0;
		} else {
			LM_CRIT("partial initialization of shm pools (%u / %u)\n", pinit,
					_shm_core_pools_num);
			return -1;
		}
	} else {
		LM_DBG("preparing to initialize shm core pools\n");
	}

	for(i = 0; i < _shm_core_pools_num; i++) {
#ifdef SHM_MMAP
#ifdef USE_ANON_MMAP
		_shm_core_pools_mem[i] = mmap(0, _shm_core_pools_size[i],
				PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
#else
		fd = open("/dev/zero", O_RDWR);
		if(fd == -1) {
			LOG(L_CRIT, "could not open /dev/zero [%u]: %s\n", i,
					strerror(errno));
			shm_core_destroy();
			return -1;
		}
		_shm_core_pools_mem[i] = mmap(0, _shm_core_pools_size[i],
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		/* close /dev/zero */
		close(fd);
#endif /* USE_ANON_MMAP */
#else

		_shm_core_shmid[i] = shmget(IPC_PRIVATE, _shm_core_pools_size[i], 0700);
		if(_shm_core_shmid[i] == -1) {
			LOG(L_CRIT, "could not allocate shared memory segment[%u]: %s\n", i,
					strerror(errno));
			shm_core_destroy();
			return -1;
		}
		_shm_core_pools_mem[i] = shmat(_shm_core_shmid[i], 0, 0);
#endif
		if(_shm_core_pools_mem[i] == (void *)-1) {
			LOG(L_CRIT, "could not attach shared memory segment[%u]: %s\n", i,
					strerror(errno));
			/* destroy segment*/
			shm_core_destroy();
			return -1;
		}
	}
	return 0;
}

/**
 *
 */
void *shm_core_get_pool(unsigned int idx)
{
	int ret;
	long sz;
	long *p;
	long *end;
	unsigned int i;

	ret = shm_core_pools_init();
	if(ret < 0) {
		return NULL;
	}

	for(i = 0; !_shm_core_pools_touched && i < _shm_core_pools_num; i++) {
		if(shm_force_alloc) {
			sz = sysconf(_SC_PAGESIZE);
			DBG("%ld bytes/page\n", sz);
			if((sz < sizeof(*p)) || (_ROUND_LONG(sz) != sz)) {
				LOG(L_WARN, "invalid page size %ld, using 4096\n", sz);
				sz = 4096; /* invalid page size, use 4096 */
			}
			end = (long *)((char *)_shm_core_pools_mem[i]
						   + _shm_core_pools_size[i] - sizeof(*p));
			/* touch one word in every page */
			for(p = (long *)_ROUND_LONG((long)_shm_core_pools_mem[i]); p <= end;
					p = (long *)((char *)p + sz)) {
				*p = 0;
			}
		}
	}
	_shm_core_pools_touched = 1;
	if(idx >= _shm_core_pools_num) {
		return NULL;
	}
	return _shm_core_pools_mem[idx];
}

unsigned long shm_core_get_pool_size(unsigned int idx)
{
	if(shm_core_pools_init() < 0 || idx >= _shm_core_pools_num) {
		return 0;
	}
	return _shm_core_pools_size[idx];
}

unsigned int shm_core_get_pool_count(void)
{
	return shm_mem_blocks;
}

int shm_core_get_pool_index(void *p)
{
	unsigned int i;
	if(p == NULL || _shm_core_pools_mem == NULL) {
		return -1;
	}
	for(i = 0; i < _shm_core_pools_num; i++) {
		if(_shm_core_pools_mem[i] != (void *)-1
				&& (char *)p >= (char *)_shm_core_pools_mem[i]
				&& (char *)p < (char *)_shm_core_pools_mem[i]
									   + _shm_core_pools_size[i]) {
			return (int)i;
		}
	}
	return -1;
}

/**
 *
 */
int shm_address_in(void *p)
{
	return shm_core_get_pool_index(p) >= 0;
}

/**
 *
 */
void shm_core_destroy(void)
{
	unsigned int i;

#ifndef SHM_MMAP
	struct shmid_ds shm_info;
#endif

	for(i = 0; i < _shm_core_pools_num; i++) {
		if(_shm_core_pools_mem[i] != (void *)-1) {
#ifdef SHM_MMAP
			munmap(_shm_core_pools_mem[i], _shm_core_pools_size[i]);
#else
			shmdt(_shm_core_pools_mem[i]);
#endif
			_shm_core_pools_mem[i] = (void *)-1;
		}
#ifndef SHM_MMAP
		if(_shm_core_shmid[i] != -1) {
			shmctl(_shm_core_shmid[i], IPC_RMID, &shm_info);
			_shm_core_shmid[i] = -1;
		}
#endif
	}
	free(_shm_core_pools_mem);
	free(_shm_core_pools_size);
#ifndef SHM_MMAP
	free(_shm_core_shmid);
	_shm_core_shmid = NULL;
#endif
	_shm_core_pools_mem = NULL;
	_shm_core_pools_size = NULL;
	_shm_core_pools_num = 0;
	_shm_core_pools_touched = 0;
}

/**
 *
 */
static unsigned int shm_best_block(void)
{
	unsigned int i;
	unsigned int best = 0;
	unsigned long available;
	unsigned long best_available = 0;

	for(i = 0; i < _shm_blocks_count; i++) {
		lock_get(_shm_blocks[i].lock);
		available = _shm_blocks[i].api.xavailable(_shm_blocks[i].api.mem_block);
		lock_release(_shm_blocks[i].lock);
		if(i == 0 || available > best_available) {
			best = i;
			best_available = available;
		}
	}
	return best;
}

static int shm_pointer_block(void *p)
{
	int idx = shm_core_get_pool_index(p);
	if(idx < 0 || (unsigned int)idx >= _shm_blocks_count) {
		LM_CRIT("pointer %p is outside the shared memory blocks\n", p);
		return -1;
	}
	return idx;
}

#ifdef DBG_SR_MEMORY
static void *shm_api_malloc(void *mbp, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	unsigned int i = shm_best_block();
	void *r;
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xmalloc(
			_shm_blocks[i].api.mem_block, size, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_mallocxz(void *mbp, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	unsigned int i = shm_best_block();
	void *r;
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xmallocxz(
			_shm_blocks[i].api.mem_block, size, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_mallocxn(void *mbp, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	unsigned int i = shm_best_block();
	void *r;
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xmallocxn(
			_shm_blocks[i].api.mem_block, size, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_malloc_unsafe(void *mbp, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	unsigned int i = (_shm_blocks_count == 1) ? 0 : shm_best_block();
	void *r;
	if(_shm_blocks_count > 1) {
		lock_get(_shm_blocks[i].lock);
	}
	r = _shm_blocks[i].api.xmalloc_unsafe(
			_shm_blocks[i].api.mem_block, size, file, func, line, mname);
	if(_shm_blocks_count > 1) {
		lock_release(_shm_blocks[i].lock);
	}
	return r;
}

static void *shm_api_realloc(void *mbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	int i = p ? shm_pointer_block(p) : (int)shm_best_block();
	void *r;
	if(i < 0) {
		return NULL;
	}
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xrealloc(
			_shm_blocks[i].api.mem_block, p, size, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_reallocxf(void *mbp, void *p, size_t size,
		const char *file, const char *func, unsigned int line,
		const char *mname)
{
	int i = p ? shm_pointer_block(p) : (int)shm_best_block();
	void *r;
	if(i < 0) {
		return NULL;
	}
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xreallocxf(
			_shm_blocks[i].api.mem_block, p, size, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_resize(void *mbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	int i = p ? shm_pointer_block(p) : (int)shm_best_block();
	void *r;
	if(i < 0) {
		return NULL;
	}
	lock_get(_shm_blocks[i].lock);
	if(p) {
		_shm_blocks[i].api.xfree_unsafe(
				_shm_blocks[i].api.mem_block, p, file, func, line, mname);
	}
	r = _shm_blocks[i].api.xmalloc_unsafe(
			_shm_blocks[i].api.mem_block, size, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void shm_api_free(void *mbp, void *p, const char *file, const char *func,
		unsigned int line, const char *mname)
{
	int i;
	if(p == NULL) {
		return;
	}
	i = shm_pointer_block(p);
	if(i < 0) {
		return;
	}
	lock_get(_shm_blocks[i].lock);
	_shm_blocks[i].api.xfree(
			_shm_blocks[i].api.mem_block, p, file, func, line, mname);
	lock_release(_shm_blocks[i].lock);
}

static void shm_api_free_unsafe(void *mbp, void *p, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	int i;
	if(p == NULL) {
		return;
	}
	i = shm_pointer_block(p);
	if(i < 0) {
		return;
	}
	if(_shm_blocks_count > 1) {
		lock_get(_shm_blocks[i].lock);
	}
	_shm_blocks[i].api.xfree_unsafe(
			_shm_blocks[i].api.mem_block, p, file, func, line, mname);
	if(_shm_blocks_count > 1) {
		lock_release(_shm_blocks[i].lock);
	}
}
#else
static void *shm_api_malloc(void *mbp, size_t size)
{
	unsigned int i = shm_best_block();
	void *r;
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xmalloc(_shm_blocks[i].api.mem_block, size);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_mallocxz(void *mbp, size_t size)
{
	unsigned int i = shm_best_block();
	void *r;
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xmallocxz(_shm_blocks[i].api.mem_block, size);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_mallocxn(void *mbp, size_t size)
{
	unsigned int i = shm_best_block();
	void *r;
	lock_get(_shm_blocks[i].lock);
	r = _shm_blocks[i].api.xmallocxn(_shm_blocks[i].api.mem_block, size);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void *shm_api_malloc_unsafe(void *mbp, size_t size)
{
	unsigned int i = (_shm_blocks_count == 1) ? 0 : shm_best_block();
	void *r;
	if(_shm_blocks_count > 1) {
		lock_get(_shm_blocks[i].lock);
	}
	r = _shm_blocks[i].api.xmalloc_unsafe(_shm_blocks[i].api.mem_block, size);
	if(_shm_blocks_count > 1) {
		lock_release(_shm_blocks[i].lock);
	}
	return r;
}

#define SHM_REALLOC_WRAPPER(name, field)                                     \
	static void *name(void *mbp, void *p, size_t size)                       \
	{                                                                        \
		int i = p ? shm_pointer_block(p) : (int)shm_best_block();            \
		void *r;                                                             \
		if(i < 0) {                                                          \
			return NULL;                                                     \
		}                                                                    \
		lock_get(_shm_blocks[i].lock);                                       \
		r = _shm_blocks[i].api.field(_shm_blocks[i].api.mem_block, p, size); \
		lock_release(_shm_blocks[i].lock);                                   \
		return r;                                                            \
	}
SHM_REALLOC_WRAPPER(shm_api_realloc, xrealloc)
SHM_REALLOC_WRAPPER(shm_api_reallocxf, xreallocxf)

static void *shm_api_resize(void *mbp, void *p, size_t size)
{
	int i = p ? shm_pointer_block(p) : (int)shm_best_block();
	void *r;
	if(i < 0) {
		return NULL;
	}
	lock_get(_shm_blocks[i].lock);
	if(p) {
		_shm_blocks[i].api.xfree_unsafe(_shm_blocks[i].api.mem_block, p);
	}
	r = _shm_blocks[i].api.xmalloc_unsafe(_shm_blocks[i].api.mem_block, size);
	lock_release(_shm_blocks[i].lock);
	return r;
}

static void shm_api_free(void *mbp, void *p)
{
	int i;
	if(p == NULL) {
		return;
	}
	i = shm_pointer_block(p);
	if(i < 0) {
		return;
	}
	lock_get(_shm_blocks[i].lock);
	_shm_blocks[i].api.xfree(_shm_blocks[i].api.mem_block, p);
	lock_release(_shm_blocks[i].lock);
}

static void shm_api_free_unsafe(void *mbp, void *p)
{
	int i;
	if(p == NULL) {
		return;
	}
	i = shm_pointer_block(p);
	if(i < 0) {
		return;
	}
	if(_shm_blocks_count > 1) {
		lock_get(_shm_blocks[i].lock);
	}
	_shm_blocks[i].api.xfree_unsafe(_shm_blocks[i].api.mem_block, p);
	if(_shm_blocks_count > 1) {
		lock_release(_shm_blocks[i].lock);
	}
}
#endif

static void shm_api_status(void *mbp)
{
	unsigned int i;
	for(i = 0; i < _shm_blocks_count; i++) {
		LM_DBG("shared memory block %u status\n", i);
		lock_get(_shm_blocks[i].lock);
		_shm_blocks[i].api.xstatus(_shm_blocks[i].api.mem_block);
		lock_release(_shm_blocks[i].lock);
	}
}

static void shm_api_status_filter(void *mbp, str *fmatch, FILE *fp)
{
	unsigned int i;
	for(i = 0; i < _shm_blocks_count; i++) {
		if(_shm_blocks[i].api.xstatus_filter == NULL) {
			continue;
		}
		lock_get(_shm_blocks[i].lock);
		_shm_blocks[i].api.xstatus_filter(
				_shm_blocks[i].api.mem_block, fmatch, fp);
		lock_release(_shm_blocks[i].lock);
	}
}

static void shm_api_info(void *mbp, struct mem_info *info)
{
	unsigned int i;
	struct mem_info one;
	memset(info, 0, sizeof(*info));
	for(i = 0; i < _shm_blocks_count; i++) {
		lock_get(_shm_blocks[i].lock);
		_shm_blocks[i].api.xinfo(_shm_blocks[i].api.mem_block, &one);
		lock_release(_shm_blocks[i].lock);
		info->total_size += one.total_size;
		info->free_size += one.free_size;
		info->used_size += one.used_size;
		info->real_used += one.real_used;
		info->max_used += one.max_used;
		info->total_frags += one.total_frags;
		if(info->min_frag == 0
				|| (one.min_frag && one.min_frag < info->min_frag)) {
			info->min_frag = one.min_frag;
		}
	}
}

#define SHM_REPORT_SUM(field) mrep->field += one.field
#define SHM_REPORT_MAX(field, prefix)              \
	if(one.field > mrep->field) {                  \
		mrep->field = one.field;                   \
		mrep->prefix##_file = one.prefix##_file;   \
		mrep->prefix##_func = one.prefix##_func;   \
		mrep->prefix##_mname = one.prefix##_mname; \
		mrep->prefix##_line = one.prefix##_line;   \
	}
#define SHM_REPORT_MIN(field, prefix)                                \
	if(one.field && (mrep->field == 0 || one.field < mrep->field)) { \
		mrep->field = one.field;                                     \
		mrep->prefix##_file = one.prefix##_file;                     \
		mrep->prefix##_func = one.prefix##_func;                     \
		mrep->prefix##_mname = one.prefix##_mname;                   \
		mrep->prefix##_line = one.prefix##_line;                     \
	}

static void shm_api_report(void *mbp, mem_report_t *mrep)
{
	unsigned int i;
	mem_report_t one;
	memset(mrep, 0, sizeof(*mrep));
	for(i = 0; i < _shm_blocks_count; i++) {
		if(_shm_blocks[i].api.xreport == NULL) {
			continue;
		}
		lock_get(_shm_blocks[i].lock);
		_shm_blocks[i].api.xreport(_shm_blocks[i].api.mem_block, &one);
		lock_release(_shm_blocks[i].lock);
		SHM_REPORT_SUM(total_size);
		SHM_REPORT_SUM(free_size_s);
		SHM_REPORT_SUM(free_size_m);
		SHM_REPORT_SUM(used_size_s);
		SHM_REPORT_SUM(used_size_m);
		SHM_REPORT_SUM(real_used_s);
		SHM_REPORT_SUM(max_used_s);
		SHM_REPORT_SUM(free_frags);
		SHM_REPORT_SUM(used_frags);
		SHM_REPORT_SUM(total_frags);
		SHM_REPORT_MAX(max_free_frag_size, max_free_frag);
		SHM_REPORT_MIN(min_free_frag_size, min_free_frag);
		SHM_REPORT_MAX(max_used_frag_size, max_used_frag);
		SHM_REPORT_MIN(min_used_frag_size, min_used_frag);
	}
}

static unsigned long shm_api_available(void *mbp)
{
	unsigned int i;
	unsigned long one;
	unsigned long available = 0;
	for(i = 0; i < _shm_blocks_count; i++) {
		lock_get(_shm_blocks[i].lock);
		one = _shm_blocks[i].api.xavailable(_shm_blocks[i].api.mem_block);
		lock_release(_shm_blocks[i].lock);
		if(one > available) {
			available = one;
		}
	}
	return available;
}

static void shm_api_sums(void *mbp)
{
	unsigned int i;
	for(i = 0; i < _shm_blocks_count; i++) {
		lock_get(_shm_blocks[i].lock);
		_shm_blocks[i].api.xsums(_shm_blocks[i].api.mem_block);
		lock_release(_shm_blocks[i].lock);
	}
}

static void shm_api_modstats(void *mbp, void **rootp)
{
	unsigned int i;
	for(i = 0; i < _shm_blocks_count; i++) {
		lock_get(_shm_blocks[i].lock);
		_shm_blocks[i].api.xmodstats(_shm_blocks[i].api.mem_block, rootp);
		lock_release(_shm_blocks[i].lock);
	}
}

static void shm_api_fmodstats(void *rootp)
{
	if(_shm_blocks_count && _shm_blocks[0].api.xfmodstats) {
		_shm_blocks[0].api.xfmodstats(rootp);
	}
}

static void shm_api_glock(void *mbp)
{
	if(_shm_blocks_count == 1) {
		lock_get(_shm_blocks[0].lock);
	}
}

static void shm_api_gunlock(void *mbp)
{
	if(_shm_blocks_count == 1) {
		lock_release(_shm_blocks[0].lock);
	}
}

static void shm_api_setfunc(void *mbp, void *p, char *func)
{
	int i = shm_pointer_block(p);
	if(i < 0 || _shm_blocks[i].api.xsetfunc == NULL) {
		return;
	}
	lock_get(_shm_blocks[i].lock);
	_shm_blocks[i].api.xsetfunc(_shm_blocks[i].api.mem_block, p, func);
	lock_release(_shm_blocks[i].lock);
}

static void shm_api_destroy(void)
{
	unsigned int i;
	for(i = 0; i < _shm_blocks_count; i++) {
		if(_shm_blocks[i].lock) {
			lock_destroy(_shm_blocks[i].lock);
		}
	}
	free(_shm_blocks);
	_shm_blocks = NULL;
	_shm_blocks_count = 0;
}

int shm_init_api(sr_shm_api_t *ap, unsigned int count)
{
	unsigned int i;
	if(ap == NULL || count == 0) {
		return -1;
	}
	_shm_blocks = calloc(count, sizeof(*_shm_blocks));
	if(_shm_blocks == NULL) {
		return -1;
	}
	_shm_blocks_count = 0;
	for(i = 0; i < count; i++) {
		_shm_blocks[i].api = ap[i];
#ifdef DBG_SR_MEMORY
		_shm_blocks[i].lock =
				ap[i].xmalloc_unsafe(ap[i].mem_block, sizeof(gen_lock_t),
						_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_);
#else
		_shm_blocks[i].lock =
				ap[i].xmalloc_unsafe(ap[i].mem_block, sizeof(gen_lock_t));
#endif
		if(_shm_blocks[i].lock == NULL || lock_init(_shm_blocks[i].lock) == 0) {
			LM_CRIT("could not initialize shared memory block %u lock\n", i);
			shm_api_destroy();
			return -1;
		}
		_shm_blocks_count = i + 1;
	}

	memset(&_shm_root, 0, sizeof(_shm_root));
	_shm_root.mname = ap[0].mname;
	_shm_root.mem_pool = ap[0].mem_pool;
	_shm_root.mem_block = _shm_blocks;
	_shm_root.xmalloc = shm_api_malloc;
	_shm_root.xmallocxz = shm_api_mallocxz;
	_shm_root.xmallocxn = shm_api_mallocxn;
	_shm_root.xmalloc_unsafe = shm_api_malloc_unsafe;
	_shm_root.xfree = shm_api_free;
	_shm_root.xfree_unsafe = shm_api_free_unsafe;
	_shm_root.xrealloc = shm_api_realloc;
	_shm_root.xreallocxf = shm_api_reallocxf;
	_shm_root.xresize = shm_api_resize;
	_shm_root.xstatus = shm_api_status;
	_shm_root.xstatus_filter =
			ap[0].xstatus_filter ? shm_api_status_filter : NULL;
	_shm_root.xinfo = shm_api_info;
	_shm_root.xreport = ap[0].xreport ? shm_api_report : NULL;
	_shm_root.xavailable = shm_api_available;
	_shm_root.xsums = shm_api_sums;
	_shm_root.xdestroy = shm_api_destroy;
	_shm_root.xmodstats = shm_api_modstats;
	_shm_root.xfmodstats = shm_api_fmodstats;
	_shm_root.xglock = shm_api_glock;
	_shm_root.xgunlock = shm_api_gunlock;
	_shm_root.xsetfunc = ap[0].xsetfunc ? shm_api_setfunc : NULL;
	return 0;
}

/**
 *
 */
int shm_init_manager(char *name)
{
	if(strcmp(name, "fm") == 0 || strcmp(name, "f_malloc") == 0
			|| strcmp(name, "fmalloc") == 0) {
		/*fast malloc*/
		return fm_malloc_init_shm_manager();
	} else if(strcmp(name, "qm") == 0 || strcmp(name, "q_malloc") == 0
			  || strcmp(name, "qmalloc") == 0) {
		/*quick malloc*/
		return qm_malloc_init_shm_manager();
	} else if(strcmp(name, "tlsf") == 0 || strcmp(name, "tlsf_malloc") == 0) {
		/*tlsf malloc*/
		return tlsf_malloc_init_shm_manager();
	} else if(strcmp(name, "sm") == 0) {
		/*system malloc - process private, cannot back shared memory*/
		LM_ERR("sm (system malloc) is only for pkg, not for shm\n");
	} else {
		/*custom malloc - module*/
	}
	return -1;
}

/**
 *
 */
void shm_destroy_manager(void)
{
	if(_shm_root.xdestroy) {
		LM_DBG("destroying memory manager: %s\n",
				(_shm_root.mname) ? _shm_root.mname : "unknown");
		_shm_root.xdestroy();
	}
	shm_core_destroy();
}

/**
 *
 */
void shm_print_manager(void)
{
	LM_DBG("shm - using memory manager: %s\n",
			(_shm_root.mname) ? _shm_root.mname : "unknown");
}

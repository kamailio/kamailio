/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Common Data Structures - functions for memory allocation and deallocation
 * and may be other memory management. */

#include <stdio.h>
#include <stdlib.h>
#include <cds/memory.h>

#ifdef SER

#include <mem/mem.h>
#include <mem/shm_mem.h>

static void* shm_malloc_x(unsigned int size);
static void shm_free_x(void *ptr);

cds_malloc_func cds_malloc = shm_malloc_x;
cds_free_func cds_free = shm_free_x;

#else

cds_malloc_func cds_malloc = malloc;
cds_free_func cds_free = free;

#endif

void cds_set_memory_functions(cds_malloc_func _malloc, cds_free_func _free)
{
	cds_malloc = _malloc;
	cds_free = _free;
}

#ifdef SER

static void* shm_malloc_x(unsigned int size)
{
	return shm_malloc(size);
}

static void shm_free_x(void *ptr)
{
	shm_free(ptr);
}

#endif

/*
#ifdef SER

static gen_lock_t *mem_mutex = NULL;
int *allocated_cnt = NULL;

#else

static int allocated_cnt = 0;

#endif

void debug_mem_init()
{
#ifdef SER
	mem_mutex = lock_alloc();
	allocated_cnt = shm_malloc(sizeof(int));
	*allocated_cnt = 0;
	debug_print_allocated_mem();
#else
	allocated_cnt = 0;
#endif
}

void *debug_malloc(int size)
{
#ifdef SER
	void *m = NULL;
	lock_get(mem_mutex);
	if (allocated_cnt) (*allocated_cnt)++;
	lock_release(mem_mutex);
	m = shm_malloc(size);
	LOG(L_INFO, "debug_malloc(): %p\n", m);
	return m;
#else
	allocated_cnt++;
	return malloc(size);
#endif
}

void debug_free(void *block)
{
#ifdef SER
	LOG(L_INFO, "debug_free(): %p\n", block);
	shm_free(block);
	lock_get(mem_mutex);
	if (allocated_cnt) (*allocated_cnt)--;
	lock_release(mem_mutex);
#else
	free(block);
	allocated_cnt--;
#endif
}

void *debug_malloc_ex(int size, const char *file, int line)
{
#ifdef SER
	void *m = NULL;
	lock_get(mem_mutex);
	if (allocated_cnt) (*allocated_cnt)++;
	lock_release(mem_mutex);
	m = shm_malloc(size);
	LOG(L_INFO, "ALLOC: %s:%d -> %p\n", file, line, m);
	return m;
#else
	allocated_cnt++;
	return malloc(size);
#endif
}

void debug_free_ex(void *block, const char *file, int line)
{
#ifdef SER
	LOG(L_INFO, "FREE: %s:%d -> %p\n", file, line, block);
	shm_free(block);
	lock_get(mem_mutex);
	if (allocated_cnt) (*allocated_cnt)--;
	lock_release(mem_mutex);
#else
	free(block);
	allocated_cnt--;
#endif
}

void debug_print_allocated_mem()
{
#ifdef SER
	lock_get(mem_mutex);
	LOG(L_INFO, "There are allocated: %d memory blocks\n", *allocated_cnt);
	lock_release(mem_mutex);
#else
	printf("There are allocated: %d memory blocks\n", allocated_cnt);
#endif
}
*/

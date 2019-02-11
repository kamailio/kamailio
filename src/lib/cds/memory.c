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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* Common Data Structures - functions for memory allocation and deallocation
 * and may be other memory management. */

#include <stdio.h>
#include <stdlib.h>
#include <cds/memory.h>
#include <cds/sync.h>
#include <cds/logger.h>

#ifdef TRACE_CDS_MEMORY

cds_mutex_t *mem_mutex = NULL;
int *allocated_cnt = NULL;
char *debug_file = "/tmp/mem.log";

#define write_debug(s,args...)	if (1) { \
	FILE *f = fopen(debug_file, "a"); \
	if (f) { \
		fprintf(f,s,##args); \
		fclose(f); \
	} \
	TRACE_LOG(s,##args); \
}

void *debug_malloc(int size, const char *file, int line)
{
	void *m = NULL;
	if (allocated_cnt && mem_mutex) {
		cds_mutex_lock(mem_mutex);
		(*allocated_cnt)++;
		cds_mutex_unlock(mem_mutex);
	}
#ifdef SER
	m = shm_malloc(size);
#else
	m = malloc(size);
#endif
	write_debug("ALLOC %p size %u from %s(%d)\n", m, size, file, line);
	/* LOG(L_INFO, "%p\n", m); */
	return m;
}

void debug_free(void *block, const char *file, int line)
{
	if (allocated_cnt && mem_mutex) {
		cds_mutex_lock(mem_mutex);
		(*allocated_cnt)--;
		cds_mutex_unlock(mem_mutex);
	}
#ifdef SER
	shm_free(block);
#else
	free(block);
#endif
	write_debug("FREE %p from %s(%d)\n", block, file, line);
}

void *debug_malloc_ex(unsigned int size)
{
	return debug_malloc(size, "<none>", 0);
}

void debug_free_ex(void *block)
{
	debug_free(block, "<none>", 0);
}

void cds_memory_trace_init()
{
	cds_mutex_init(mem_mutex);
	allocated_cnt = cds_malloc(sizeof(int));
	*allocated_cnt = 0;
}

void cds_memory_trace(char *dst, int dst_len)
{
	if (allocated_cnt && mem_mutex) {
		cds_mutex_lock(mem_mutex);
		snprintf(dst, dst_len, "There are allocated: %d memory blocks\n", *allocated_cnt);
		cds_mutex_unlock(mem_mutex);
	}
}

#else /* ! CDS_TRACE_MEMORY */

#ifdef SER

void* shm_malloc_x(unsigned int size)
{
	return shm_malloc(size);
}

void shm_free_x(void *ptr)
{
	shm_free(ptr);
}

#endif

#endif

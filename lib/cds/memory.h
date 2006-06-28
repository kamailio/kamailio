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

#ifndef __CDS_MEMORY_H
#define __CDS_MEMORY_H

/* #define TRACE_CDS_MEMORY */

#ifdef __cplusplus
extern "C" {
#endif

/* typedef void*(*cds_malloc_func)(unsigned int size);
typedef void(*cds_free_func)(void *ptr);

extern cds_malloc_func cds_malloc;
extern cds_free_func cds_free;

void cds_set_memory_functions(cds_malloc_func _malloc, cds_free_func _free); */

#ifdef TRACE_CDS_MEMORY

void *debug_malloc(int size, const char *file, int line);
void debug_free(void *block, const char *file, int line);
void *debug_malloc_ex(unsigned int size);
void debug_free_ex(void *block);

/* trace function */
void cds_memory_trace(char *dst, int dst_len);
/* initializes internal variables for memory tracing */
void cds_memory_trace_init();

#define cds_malloc(s)	debug_malloc(s,__FILE__, __LINE__)
#define cds_free(p)		debug_free(p,__FILE__, __LINE__)
#define cds_free_ptr	debug_free_ex
#define cds_malloc_ptr	debug_malloc_ex
#define cds_malloc_pkg(s)	debug_malloc(s,__FILE__, __LINE__)
#define cds_free_pkg(p)		debug_free(p,__FILE__, __LINE__)

#else /* !TRACE */

#ifdef SER

#include <mem/mem.h>
#include <mem/shm_mem.h>

void* shm_malloc_x(unsigned int size);
void shm_free_x(void *ptr);

#define cds_malloc(s)	shm_malloc(s)
#define cds_free(p)		shm_free(p)
#define cds_malloc_ptr	shm_malloc_x
#define cds_free_ptr	shm_free_x
#define cds_malloc_pkg(s)	pkg_malloc(s)
#define cds_free_pkg(p)		pkg_free(p)

#else /* !SER */

#include <stdlib.h>

#define cds_malloc(s)	malloc(s)
#define cds_free(p)		free(p)
#define cds_malloc_ptr	malloc
#define cds_free_ptr	free
#define cds_malloc_pkg(s)	malloc(s)
#define cds_free_pkg(p)		free(p)

#endif /* !SER */

#endif /* !TRACE_CDS_MEMORY */

#ifdef __cplusplus
}
#endif


#endif


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

#ifndef __CDS_MEMORY_H
#define __CDS_MEMORY_H

/* #define TRACE_CDS_MEMORY */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup cds
 * @{ 
 *
 * \defgroup cds_memory Memory management
 *
 * Memory operations are common for whole CDS library. Because it must work
 together with SER's memory management and must work without it too, there are
 wrapper macros for memory allocation/deallocation.
 * 
 * @{ */

/* typedef void*(*cds_malloc_func)(unsigned int size);
typedef void(*cds_free_func)(void *ptr);

extern cds_malloc_func cds_malloc;
extern cds_free_func cds_free;

void cds_set_memory_functions(cds_malloc_func _malloc, cds_free_func _free); */

/** \def cds_malloc(s)
 * Function/macro for memory allocation. Which function is choosen depends on
 * SER and TRACE_CDS_MEMORY defines. 
 *
 * When SER is defined shm_malloc is choosen, standard malloc otherwise. */

/** \def cds_free(p)
 * Function/macro for memory deallocation. Which function is choosen depends
 * on SER and TRACE_CDS_MEMORY defines. 
 *
 * If SER is defined shm_free is choosen, standard free otherwise. */

/** \def cds_malloc_ptr
 * Function/macro for memory allocation when pointer to function needed. Which
 * function is choosen depends on SER and TRACE_CDS_MEMORY defines. 
 *
 * If SER is defined shm_malloc is choosen, standard malloc otherwise.  */

/** \def cds_free_ptr
 * Function/macro for memory deallocation when pointer to function needed.
 * Which function is choosen depends on SER and TRACE_CDS_MEMORY defines. 
 *
 * If SER is defined shm_free is choosen, standard free otherwise.  */

/** \def cds_malloc_pkg(s)
 * Function/macro for 'local' memory allocation. Which function is choosen
 * depends on SER and TRACE_CDS_MEMORY defines. 
 *
 * When SER is defined pkg_malloc is choosen, standard malloc otherwise. */

/** \def cds_free_pkg(p)
 * Function/macro for 'local' memory deallocation. Which function is choosen
 * depends on SER and TRACE_CDS_MEMORY defines. 
 *
 * When SER is defined pkg_free is choosen, standard free otherwise. */

#ifdef TRACE_CDS_MEMORY

/** \internal Debugging variant of alloc function */
void *debug_malloc(int size, const char *file, int line);

/** \internal Debugging variant of free function */
void debug_free(void *block, const char *file, int line);

/** \internal Another debugging variant of alloc function - used when pointer
 * to function needed. */
void *debug_malloc_ex(unsigned int size);

/** \internal Another debugging variant of free function - used when pointer to
 * function needed. */
void debug_free_ex(void *block);

/* \internal Helper function for debugging - shows some debugging information about
 * memory allocations (currently only the number of allocated blocks). */
void cds_memory_trace(char *dst, int dst_len);

/** \internal Helper function which is useful for memory debugging only - initializes
 * internal variables for memory tracing */
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

/** @} 
 * @} */

#endif


/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \defgroup mem SIP-router memory manager
 * \brief  SIP-router internal memory manager
 * 
 * SIP-router internal memory manager for private (per process) and shared
 * memory pools. It provides several different strategies for the memory
 * management, like really fast, with extended debugging and also plain system
 * memory management.
 */

/**
 * \file
 * \brief Main definitions for memory manager
 * 
 * \brief Main definitions for memory manager, like malloc, free and realloc
 * \ingroup mem
 */


#ifndef mem_h
#define mem_h
#include "../config.h"
#include "../dprint.h"

/* fix debug defines, DBG_F_MALLOC <=> DBG_QM_MALLOC */
#ifdef F_MALLOC
	#ifdef DBG_F_MALLOC
		#ifndef DBG_QM_MALLOC
			#define DBG_QM_MALLOC
		#endif
	#elif defined(DBG_QM_MALLOC)
		#define DBG_F_MALLOC
	#endif
#elif defined TLSF_MALLOC
	#ifdef DBG_TLSF_MALLOC
		#ifndef DBG_QM_MALLOC
			#define DBG_QM_MALLOC
		#endif
	#elif defined(DBG_QM_MALLOC)
		#define DBG_TLSF_MALLOC
	#endif
#endif

#ifdef PKG_MALLOC
#	ifdef F_MALLOC
#		include "f_malloc.h"
		extern struct fm_block* mem_block;
#	elif defined DL_MALLOC
#		include "dl_malloc.h"
#	elif defined TLSF_MALLOC
#		include "tlsf.h"
		extern tlsf_t mem_block;
#   else
#		include "q_malloc.h"
		extern struct qm_block* mem_block;
#	endif

	extern char* mem_pool;


#	ifdef DBG_QM_MALLOC

#	include "src_loc.h" /* src location macros: _SRC_* */
#		ifdef F_MALLOC
#			define pkg_malloc(s) fm_malloc(mem_block, (s), _SRC_LOC_, \
				_SRC_FUNCTION_, _SRC_LINE_)
#			define pkg_free(p)   fm_free(mem_block, (p), _SRC_LOC_,  \
				_SRC_FUNCTION_, _SRC_LINE_)
#			define pkg_realloc(p, s) fm_realloc(mem_block, (p), (s), \
					_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_)
#		elif defined TLSF_MALLOC
#			define pkg_malloc(s) tlsf_malloc(mem_block, (s), _SRC_LOC_, \
				_SRC_FUNCTION_, _SRC_LINE_)
#			define pkg_free(p)   tlsf_free(mem_block, (p), _SRC_LOC_,  \
				_SRC_FUNCTION_, _SRC_LINE_)
#			define pkg_realloc(p, s) tlsf_realloc(mem_block, (p), (s), \
					_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_)
#		else
#			define pkg_malloc(s) qm_malloc(mem_block, (s),_SRC_LOC_, \
				_SRC_FUNCTION_, _SRC_LINE_)
#			define pkg_realloc(p, s) qm_realloc(mem_block, (p), (s), \
				_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_)
#			define pkg_free(p)   qm_free(mem_block, (p), _SRC_LOC_,  \
				_SRC_FUNCTION_, _SRC_LINE_)
#		endif
#	else
#		ifdef F_MALLOC
#			define pkg_malloc(s) fm_malloc(mem_block, (s))
#			define pkg_realloc(p, s) fm_realloc(mem_block, (p), (s))
#			define pkg_free(p)   fm_free(mem_block, (p))
#		elif defined DL_MALLOC
#			define pkg_malloc(s) dlmalloc((s))
#			define pkg_realloc(p, s) dlrealloc((p), (s))
#			define pkg_free(p)   dlfree((p))
#		elif defined TLSF_MALLOC
#			define pkg_malloc(s) tlsf_malloc(mem_block, (s))
#			define pkg_realloc(p, s) tlsf_realloc(mem_block, (p), (s))
#			define pkg_free(p)   tlsf_free(mem_block, (p))
#		else
#			define pkg_malloc(s) qm_malloc(mem_block, (s))
#			define pkg_realloc(p, s) qm_realloc(mem_block, (p), (s))
#			define pkg_free(p)   qm_free(mem_block, (p))
#		endif
#	endif
#	ifdef F_MALLOC
#		define pkg_status()    fm_status(mem_block)
#		define pkg_info(mi)    fm_info(mem_block, mi)
#		define pkg_available() fm_available(mem_block)
#		define pkg_sums()      fm_sums(mem_block)
#	elif defined DL_MALLOC
#		define pkg_status()  0
#		define pkg_info(mi)  0
#		define pkg_available()  0
#		define pkg_sums()  0
#	elif defined TLSF_MALLOC
#		define pkg_status()  tlsf_status(mem_block)
#		define pkg_info(mi)  tlsf_meminfo(mem_block, (mi))
#		define pkg_available()  tlsf_available(mem_block)
#		define pkg_sums()  tlsf_sums(mem_block)
#	else
#		define pkg_status()    qm_status(mem_block)
#		define pkg_info(mi)    qm_info(mem_block, mi)
#		define pkg_available() qm_available(mem_block)
#		define pkg_sums()      qm_sums(mem_block)
#	endif
#elif defined(SHM_MEM) && defined(USE_SHM_MEM)
#	include "shm_mem.h"
#	define pkg_malloc(s)	shm_malloc((s))
#	define pkg_relloc(p, s)	shm_malloc((p), (s))
#	define pkg_free(p)		shm_free((p))
#	define pkg_status()		shm_status()
#	define pkg_sums()		shm_sums()
#else
#	include <stdlib.h>
#	include "memdbg.h"
#	ifdef DBG_SYS_MALLOC
#	define pkg_malloc(s) \
	(  { void *____v123; ____v123=malloc((s)); \
	   MDBG("malloc %p size %lu end %p (%s:%d)\n", ____v123, (unsigned long)(s), (char*)____v123+(s), __FILE__, __LINE__);\
	   ____v123; } )
#	define pkg_realloc(p, s) \
	(  { void *____v123; ____v123=realloc(p, s); \
	   MDBG("realloc %p size %lu end %p (%s:%d)\n", ____v123, (unsigned long)(s), (char*)____v123+(s), __FILE__, __LINE__);\
	    ____v123; } )
#	define pkg_free(p)  do{ MDBG("free %p (%s:%d)\n", (p), __FILE__, __LINE__); free((p)); }while(0)
#	else
#	define pkg_malloc(s)		malloc((s))
#	define pkg_realloc(p, s)	realloc((p), (s))
#	define pkg_free(p)			free((p))
#	endif
#	define pkg_status()
#	define pkg_sums()
#endif

/**
 * \brief Initialize private memory pool
 * \return 0 if the memory allocation was successful, -1 otherwise
 */
int init_pkg_mallocs(void);

/**
 * \brief Destroy private memory pool
 */
void destroy_pkg_mallocs(void);

/**
 * \brief Initialize shared memory pool
 * \param force_alloc Force allocation of memory, e.g. initialize complete block with zero
 * \return 0 if the memory allocation was successful, -1 otherwise
 */
int init_shm_mallocs(int force_alloc);

/** generic logging helper for allocation errors in private memory pool/ system */
#ifdef SYSTEM_MALLOC
#define PKG_MEM_ERROR LM_ERR("could not allocate private memory from system")
#else
#define PKG_MEM_ERROR LM_ERR("could not allocate private memory from available pool")
#endif
/** generic logging helper for allocation errors in shared memory pool */
#define SHM_MEM_ERROR LM_ERR("could not allocate shared memory from available pool")

#endif

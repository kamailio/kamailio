/* $Id$
 *
 * memory related stuff (malloc & friends)
 * 
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
/*
 * History:
 * --------
 *  2003-03-10  __FUNCTION__ is a gcc-ism, defined it to "" for sun cc
 *               (andrei)
 *  2003-03-07  split init_malloc into init_pkg_mallocs & init_shm_mallocs 
 *               (andrei)
 */



#ifndef mem_h
#define mem_h
#include "../config.h"
#include "../dprint.h"

#ifdef PKG_MALLOC
#	ifdef VQ_MALLOC
#		include "vq_malloc.h"
		extern struct vqm_block* mem_block;
#	elif defined F_MALLOC
#		include "f_malloc.h"
		extern struct fm_block* mem_block;
#   else
#		include "q_malloc.h"
		extern struct qm_block* mem_block;
#	endif

	extern char mem_pool[PKG_MEM_POOL_SIZE];


#	ifdef DBG_QM_MALLOC
#ifdef __SUNPRO_C
		#define __FUNCTION__ ""  /* gcc specific */
#endif
#		ifdef VQ_MALLOC
#			define pkg_malloc(s) vqm_malloc(mem_block, (s),__FILE__, \
				__FUNCTION__, __LINE__)
#			define pkg_free(p)   vqm_free(mem_block, (p), __FILE__,  \
				__FUNCTION__, __LINE__)
#		elif defined F_MALLOC
#			define pkg_malloc(s) fm_malloc(mem_block, (s),__FILE__, \
				__FUNCTION__, __LINE__)
#			define pkg_free(p)   fm_free(mem_block, (p), __FILE__,  \
				__FUNCTION__, __LINE__)
#		else
#			define pkg_malloc(s) qm_malloc(mem_block, (s),__FILE__, \
				__FUNCTION__, __LINE__)
#			define pkg_free(p)   qm_free(mem_block, (p), __FILE__,  \
				__FUNCTION__, __LINE__)
#		endif
#	else
#		ifdef VQ_MALLOC
#			define pkg_malloc(s) vqm_malloc(mem_block, (s))
#			define pkg_free(p)   vqm_free(mem_block, (p))
#		elif defined F_MALLOC
#			define pkg_malloc(s) fm_malloc(mem_block, (s))
#			define pkg_free(p)   fm_free(mem_block, (p))
#		else
#			define pkg_malloc(s) qm_malloc(mem_block, (s))
#			define pkg_free(p)   qm_free(mem_block, (p))
#		endif
#	endif
#	ifdef VQ_MALLOC
#		define pkg_status()  vqm_status(mem_block)
#	elif defined F_MALLOC
#		define pkg_status()  fm_status(mem_block)
#	else
#		define pkg_status()  qm_status(mem_block)
#	endif
#elif defined(SHM_MEM) && defined(USE_SHM_MEM)
#	include "shm_mem.h"
#	define pkg_malloc(s) shm_malloc((s))
#	define pkg_free(p)   shm_free((p))
#	define pkg_status()  shm_status()
#else
#	include <stdlib.h>
#	define pkg_malloc(s) \
	(  { void *v; v=malloc((s)); \
	   DBG("malloc %x size %d end %x\n", v, s, (unsigned int)v+(s));\
	   v; } )
#	define pkg_free(p)  do{ DBG("free %x\n", (p)); free((p)); }while(0);
#	define pkg_status()
#endif

int init_pkg_mallocs();
int init_shm_mallocs();

#endif

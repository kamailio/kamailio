/* $Id$
 *
 * memory related stuff (malloc & friends)
 * 
 */


#ifndef mem_h
#define mem_h
#include "../config.h"
#include "../dprint.h"

#ifdef PKG_MALLOC
#	ifdef VQ_MALLOC
#		include "vq_malloc.h"
		extern struct vqm_block* mem_block;
#	else
#		include "q_malloc.h"
		extern struct qm_block* mem_block;
#	endif

	extern char mem_pool[PKG_MEM_POOL_SIZE];


#	ifdef DBG_QM_MALLOC
#		ifdef VQ_MALLOC
#			define pkg_malloc(s) vqm_malloc(mem_block, (s),__FILE__, \
				__FUNCTION__, __LINE__);
#			define pkg_free(p)   vqm_free(mem_block, (p), __FILE__,  \
				__FUNCTION__, __LINE__);
#		else
#			define pkg_malloc(s) qm_malloc(mem_block, (s),__FILE__, \
				__FUNCTION__, __LINE__);
#			define pkg_free(p)   qm_free(mem_block, (p), __FILE__,  \
				__FUNCTION__, __LINE__);
#		endif
#	else
#		ifdef VQ_MALLOC
#			define pkg_malloc(s) vqm_malloc(mem_block, (s))
#			define pkg_free(p)   vqm_free(mem_block, (p))
#		else
#			define pkg_malloc(s) qm_malloc(mem_block, (s))
#			define pkg_free(p)   qm_free(mem_block, (p))
#		endif
#	endif
#	ifdef VQ_MALLOC
#		define pkg_status()  vqm_status(mem_block)
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

int init_mallocs();

#endif

/* $Id$
 *
 * memory related stuff (malloc & friends)
 * 
 */


#ifndef mem_h
#define mem_h
#include "dprint.h"

#ifdef PKG_MALLOC
#include "q_malloc.h"

extern struct qm_block* mem_block;


#define pkg_malloc(s) qm_malloc(mem_block, s)
#define pkg_free(p)   qm_free(mem_block, p)
#define pkg_status()  qm_status(mem_block)

#elif defined(SHM_MEM) && defined(USE_SHM_MEM)

#include "shm_mem.h"

#define pkg_malloc(s) shm_malloc(s)
#define pkg_free(p)   shm_free(p)
#define pkg_status()  shm_status()

#else

#include <stdlib.h>

#define pkg_malloc(s) \
	(  { void *v; v=malloc(s); \
	   DBG("malloc %x size %d end %x\n", v, s, (unsigned int)v+s);\
	   v; } )
#define pkg_free(p)  do{ DBG("free %x\n", p); free(p); }while(0);
#define pkg_status()

#endif


#endif

/* $Id$
 *
 * memory related stuff (malloc & friends)
 * 
 */


#ifndef mem_h
#define mem_h

#ifdef PKG_MALLOC
#include "q_malloc.h"

extern struct qm_block* mem_block;


#define pkg_malloc(s) qm_malloc(mem_block, s)
#define pkg_free(p)   qm_free(mem_block, p)
#define pkg_status()  qm_status(mem_block)

#else
#include <stdlib.h>

#define pkg_malloc(s) malloc(s)
#define pkg_free(p)  free(p)
#define pkg_status()

#endif


#endif

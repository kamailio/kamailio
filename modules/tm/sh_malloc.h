/*
 * $Id$
 */


#ifndef _SH_MALLOC_H
#define _SH_MALLOC_H

#include "../../mem/shm_mem.h"

#if defined SHM_MEM

#include "../../mem/shm_mem.h"

#define sh_malloc(size)		shm_malloc((size))
#define sh_free(ptr)		shm_free((ptr))
#define sh_status()			shm_status()

#else

#include <stdlib.h>

#warn "you should define SHM_MEM"
#define sh_malloc(size)		malloc((size))
#define sh_free(ptr)		free((ptr))
#define sh_status()

#endif

#endif

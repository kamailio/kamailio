/*
 * $Id$
 */


#ifndef _SH_MALLOC_H
#define _SH_MALLOC_H

#include "../../shm_mem.h"

#if defined SHM_MEM

#include "../../shm_mem.h"

#define sh_malloc(size)		shm_malloc((size))
#define sh_free(ptr)		shm_free((ptr))

#else

#include <stdlib.h>

#define sh_malloc(size)		shm_malloc((size))
#define sh_free(ptr)		shm_free((ptr))

#endif

#endif

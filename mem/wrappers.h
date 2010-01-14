#ifndef __PKG_WRAPPERS_H__
#define __PKG_WRAPPERS_H__

#include "mem.h"

#if defined(PKG_MALLOC) || (defined(SHM_MEM) && defined(USE_SHM_MEM))
static void *w_pkg_calloc(size_t nmemb, size_t size)
{
	void *slub;
	size_t sz = nmemb * size;
	if ((slub = pkg_malloc(sz)))
		memset(slub, 0, sz);
	return slub;
}

static void *w_pkg_malloc(size_t size)
{
	return pkg_malloc(size);
}

static void w_pkg_free(void *ptr)
{
	pkg_free(ptr);
}

static void *w_pkg_realloc(void *ptr, size_t size)
{
	return pkg_realloc(ptr, size);
}

/* shm, if ever needed */

#else

#	include <stdlib.h>

#	define w_pkg_calloc		calloc
#	define w_pkg_malloc		malloc
#	define w_pkg_free		free
#	define w_pkg_realloc	realloc

#endif /* PKG_MALLOC || (SHM_MEM && USE_SHM_MEM) */

#endif /* __PKG_WRAPPERS_H__ */

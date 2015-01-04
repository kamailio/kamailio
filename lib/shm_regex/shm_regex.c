/*
 * Copyright (C) 2009 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <malloc.h>	/* hook prototypes */

#include "../../mem/shm_mem.h"
#include "shm_regex.h"

typedef void *(malloc_hook_t) (size_t, const void *);
typedef void *(realloc_hook_t) (void *, size_t, const void *);
typedef void (free_hook_t) (void *, const void *);

/* The memory hooks are overwritten before calling regcomp(), regfree(),
 * and regexec(), and shared memory function are called
 * from the hooks instead of libc malloc/realloc/free.
 */

static void *shm_malloc_hook(size_t size, const void *caller)
{
	return shm_malloc (size);
}

static void *shm_realloc_hook(void *p, size_t size, const void *caller)
{
	return shm_realloc (p, size);
}

static void shm_free_hook(void *ptr, const void *caller)
{
	if (ptr) shm_free (ptr);
}

#define replace_malloc_hooks() \
	do { \
		orig_malloc_hook = __malloc_hook; \
		orig_realloc_hook = __realloc_hook; \
		orig_free_hook = __free_hook; \
		__malloc_hook = shm_malloc_hook; \
		__realloc_hook = shm_realloc_hook; \
		__free_hook = shm_free_hook; \
	} while (0)

#define restore_malloc_hooks() \
	do { \
		__malloc_hook = orig_malloc_hook; \
		__realloc_hook = orig_realloc_hook; \
		__free_hook = orig_free_hook; \
	} while (0)

int shm_regcomp(shm_regex_t *preg, const char *regex, int cflags)
{
	malloc_hook_t	*orig_malloc_hook;
	realloc_hook_t	*orig_realloc_hook;
	free_hook_t	*orig_free_hook;
	int		ret;

	if(!lock_init(&preg->lock)) {
		return REG_EEND;
	}
	replace_malloc_hooks();
	ret = regcomp(&preg->regexp, regex, cflags);
	restore_malloc_hooks();

	if(ret) lock_destroy(&preg->lock);

	return ret;
}

void shm_regfree(shm_regex_t *preg)
{
	malloc_hook_t	*orig_malloc_hook;
	realloc_hook_t	*orig_realloc_hook;
	free_hook_t	*orig_free_hook;
	lock_destroy(&preg->lock);
	replace_malloc_hooks();
	regfree(&preg->regexp);
	restore_malloc_hooks();
}

int shm_regexec(shm_regex_t *preg, const char *string, size_t nmatch,
                   regmatch_t pmatch[], int eflags)
{
	malloc_hook_t	*orig_malloc_hook;
	realloc_hook_t	*orig_realloc_hook;
	free_hook_t	*orig_free_hook;
	int		ret;

	/* regexec() allocates some memory for the pattern buffer
	 * when it is successfully called for the first time, therefore
	 * shared memory is required also here.
	 * The drawback is that shared memory allocation is also used
	 * needlessly for allocating the temporary space for
	 * the elements of pmatch. -- Does not happen if pmatch and
	 * nmatch are 0.
	 * It is safe to call regexec() concurrently without locking,
	 * because regexec() has its own locks.
	 * (Miklos)
	 *
	 * Those locks, however, don't work with shm and multi-core hardware
	 * causing a dead-lock. Tested with glibc 2.3.6. (skeller)
	 */

	lock_get(&preg->lock);
	replace_malloc_hooks();
	ret = regexec(&preg->regexp, string, nmatch,
			pmatch, eflags);
	restore_malloc_hooks();
	lock_release(&preg->lock);

	return ret;
}

size_t shm_regerror(int errcode, const shm_regex_t *preg, char *errbuf,
                      size_t errbuf_size)
{
	return regerror(errcode, &preg->regexp, errbuf, errbuf_size);
}


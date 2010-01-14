/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#ifndef __ASI_STRUTIL_H__
#define __ASI_STRUTIL_H__

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "str.h"

//strclone_shm
enum STR_CLONE_FL {
	STR_CLONE_PKG	= 1 << 0, /* alloc in private mem */
	STR_CLONE_SHM	= 1 << 1, /* alloc in private mem */
	STR_CLONE_TERM	= 1 << 2, /* add 0-terminator to clone */
	STR_CLONE_TRIM	= 1 << 3, /* strip 0-terminator from clone */
};

#define _chr_clone(FUNC, _src_, _dst_, _alloc, _copy) \
	do { \
		_dst_ = (char *)FUNC(_alloc * sizeof(char)); \
		if (! (_dst_)) { \
			errno = ENOMEM; \
			_dst_ = NULL; \
		} else { \
			memcpy(_dst_, _src_, _copy); \
		} \
	} while (0)

static inline char *_chr_clone_pkg(char *orig, size_t alloc, size_t copy)
{
	char *dup;
	_chr_clone(pkg_malloc, orig, dup, alloc, copy);
	return dup;
}

static inline char *_chr_clone_shm(char *orig, size_t alloc, size_t copy)
{
	char *dup;
	_chr_clone(shm_malloc, orig, dup, alloc, copy);
	return dup;
}

static inline int strclo(char *orig, size_t olen, str *dup, unsigned flags)
{
	size_t alloc, copy;

#define init_alloc_copy(_val)	copy = alloc = _val
#define init_alloc(_val)		alloc = _val
#define clone_str(FUNC) \
	do { \
		init_alloc(olen); \
		if (! (dup->s = FUNC(orig, alloc, alloc))) {  \
			errno = ENOMEM; \
			return -1; \
		} \
	} while (0)
#define clone_term(FUNC) \
	do { \
		init_alloc_copy(olen); \
		alloc ++; \
		if (! (dup->s = FUNC(orig, alloc, copy))) { \
			errno = ENOMEM; \
			return -1; \
		} \
		dup->s[copy] = 0; \
	} while (0)
#define clone_trim(FUNC) \
	do { \
		init_alloc(olen - 1); \
		if (! (dup->s = FUNC(orig, alloc, alloc))) { \
			errno = ENOMEM; \
			return -1; \
		} \
	} while (0)

	switch (flags) {
		case STR_CLONE_PKG: clone_str(_chr_clone_pkg); break;
		case STR_CLONE_PKG | STR_CLONE_TERM: clone_term(_chr_clone_pkg); break;
		case STR_CLONE_PKG | STR_CLONE_TRIM: clone_trim(_chr_clone_pkg); break;

#if defined(SHM_MEM) && defined(USE_SHM_MEM)
		case STR_CLONE_SHM: clone_str(_chr_clone_shm); break;
		case STR_CLONE_SHM | STR_CLONE_TERM: clone_term(_chr_clone_shm); break;
		case STR_CLONE_SHM | STR_CLONE_TRIM: clone_trim(_chr_clone_shm); break;
#endif

		default:
			errno = EINVAL;
			return -1;
	}
	dup->len = alloc;
	return 0;
#undef init_alloc_copy
#undef init_alloc
#undef clone_str
#undef clone_term
#undef clone_trim
}

static inline int strdclo(char *orig, size_t olen, str **dup, unsigned flags)
{
	switch (flags) {
		case STR_CLONE_PKG:
		case STR_CLONE_PKG | STR_CLONE_TERM:
		case STR_CLONE_PKG | STR_CLONE_TRIM:
			if (! (dup = (str **)pkg_malloc(sizeof(str *)))) {
				errno = ENOMEM;
				return -1;
			}
			if (strclo(orig, olen, *dup, flags) < 0) {
				pkg_free(dup);
				return -1;
			}
			break;

#if defined(SHM_MEM) && defined(USE_SHM_MEM)
		case STR_CLONE_SHM:
		case STR_CLONE_SHM | STR_CLONE_TERM:
		case STR_CLONE_SHM | STR_CLONE_TRIM:
			if (! (dup = (str **)shm_malloc(sizeof(str *)))) {
				errno = ENOMEM;
				return -1;
			}
			if (strclo(orig, olen, *dup, flags) < 0) {
				shm_free(dup);
				return -1;
			}
			break;
#endif
		default:
			errno = EINVAL;
			return -1;
	}
	return 0;
}

#endif /*__ASI_STRUTIL_H__ */

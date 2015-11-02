/*
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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


#ifndef _sr_pkg_h_
#define _sr_pkg_h_

#ifdef PKG_MALLOC

#include "memapi.h"

extern sr_pkg_api_t _pkg_root;

#ifdef DBG_SR_MEMORY
#	define pkg_malloc(s)      _pkg_root.xmalloc(_pkg_root.mem_block, (s), _SRC_LOC_, \
				_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define pkg_free(p)        _pkg_root.xfree(_pkg_root.mem_block, (p), _SRC_LOC_, \
				_SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#	define pkg_realloc(p, s)  _pkg_root.xrealloc(_pkg_root.mem_block, (p), (s), \
				_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_)
#else
#	define pkg_malloc(s)      _pkg_root.xmalloc(_pkg_root.mem_block, (s))
#	define pkg_realloc(p, s)  _pkg_root.xrealloc(_pkg_root.mem_block, (p), (s))
#	define pkg_free(p)        _pkg_root.xfree(_pkg_root.mem_block, (p))
#endif

#	define pkg_status()    _pkg_root.xstatus(_pkg_root.mem_block)
#	define pkg_info(mi)    _pkg_root.xinfo(_pkg_root.mem_block, mi)
#	define pkg_available() _pkg_root.xavailable(_pkg_root.mem_block)
#	define pkg_sums()      _pkg_root.xsums(_pkg_root.mem_block)
#	define pkg_mod_get_stats(x)     _pkg_root.xstats(_pkg_root.mem_block, x)
#	define pkg_mod_free_stats(x)    _pkg_root.xfstats(x)

int pkg_init_api(sr_pkg_api_t *ap);
int pkg_init_manager(char *name);
void pkg_destroy_manager(void);
void pkg_print_manager(void);

#else /*PKG_MALLOC*/
/* use system allocator */
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
#	define pkg_status() do{}while(0)
#	define pkg_info(mi) do{ memset((mi),0, sizeof(*(mi))); } while(0)
#	define pkg_available() 0
#	define pkg_sums() do{}while(0)
#endif /*PKG_MALLOC*/

#endif

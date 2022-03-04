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

/**
 * \file
 * \brief Main definitions for memory manager
 * 
 * Main definitions for PKG memory manager, like malloc, free and realloc
 * \ingroup mem
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../dprint.h"

#include "pkg.h"

#include "memcore.h"

sr_pkg_api_t _pkg_root = {0};

/**
 *
 */
int pkg_init_api(sr_pkg_api_t *ap)
{
	memset(&_pkg_root, 0, sizeof(sr_pkg_api_t));
	_pkg_root.mname      = ap->mname;
	_pkg_root.mem_pool   = ap->mem_pool;
	_pkg_root.mem_block  = ap->mem_block;
	_pkg_root.xmalloc    = ap->xmalloc;
	_pkg_root.xmallocxz  = ap->xmallocxz;
	_pkg_root.xfree      = ap->xfree;
	_pkg_root.xrealloc   = ap->xrealloc;
	_pkg_root.xreallocxf = ap->xreallocxf;
	_pkg_root.xstatus    = ap->xstatus;
	_pkg_root.xinfo      = ap->xinfo;
	_pkg_root.xreport    = ap->xreport;
	_pkg_root.xavailable = ap->xavailable;
	_pkg_root.xsums      = ap->xsums;
	_pkg_root.xdestroy   = ap->xdestroy;
	_pkg_root.xmodstats  = ap->xmodstats;
	_pkg_root.xfmodstats = ap->xfmodstats;
	return 0;
}

/**
 *
 */
int pkg_init_manager(char *name)
{
	if(strcmp(name, "fm")==0
			|| strcmp(name, "f_malloc")==0
			|| strcmp(name, "fmalloc")==0) {
		/*fast malloc*/
		return fm_malloc_init_pkg_manager();
	} else if(strcmp(name, "qm")==0
			|| strcmp(name, "q_malloc")==0
			|| strcmp(name, "qmalloc")==0) {
		/*quick malloc*/
		return qm_malloc_init_pkg_manager();
	} else if(strcmp(name, "tlsf")==0
			|| strcmp(name, "tlsf_malloc")==0) {
		/*tlsf malloc*/
		return tlsf_malloc_init_pkg_manager();
	} else if(strcmp(name, "sm")==0) {
		/*system malloc*/
	} else {
		/*custom malloc - module*/
	}
	return -1;
}


/**
 *
 */
void pkg_destroy_manager(void)
{
	if(_pkg_root.xdestroy) {
		LM_DBG("destroying memory manager: %s\n",
				(_pkg_root.mname)?_pkg_root.mname:"unknown");
		_pkg_root.xdestroy();
	}
}

/**
 *
 */
void pkg_print_manager(void)
{
	LM_DBG("pkg - using memory manager: %s\n",
			(_pkg_root.mname)?_pkg_root.mname:"unknown");
}

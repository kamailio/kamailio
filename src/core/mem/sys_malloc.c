/*
 * Copyright (C) 2026 S-P Chan
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
 * \brief System malloc ("sm") pkg memory manager
 *
 * pkg api backed by the libc allocator, selected at runtime with -X sm.
 * Same semantics as a build without PKG_MALLOC: no pool, no stats.
 * \ingroup mem
 */

#include <stdlib.h>
#include <string.h>

#include "memapi.h"
#include "memcore.h"
#include "pkg.h"

static char _sm_mem_name[] = "sm";

#ifdef DBG_SR_MEMORY
#define SM_DBG_ARGS \
	, const char *file, const char *func, unsigned int line, const char *mname
#else
#define SM_DBG_ARGS
#endif

static void *sm_malloc(void *mbp, size_t size SM_DBG_ARGS)
{
	return malloc(size);
}

static void *sm_mallocxz(void *mbp, size_t size SM_DBG_ARGS)
{
	void *p = malloc(size);
	if(p)
		memset(p, 0, size);
	return p;
}

static void *sm_mallocxn(void *mbp, size_t size SM_DBG_ARGS)
{
	char *p = malloc(size);
	if(p && size > 0)
		p[size - 1] = 0;
	return p;
}

static void *sm_realloc(void *mbp, void *p, size_t size SM_DBG_ARGS)
{
	return realloc(p, size);
}

static void *sm_reallocxf(void *mbp, void *p, size_t size SM_DBG_ARGS)
{
	void *r = realloc(p, size);
	if(!r && p)
		free(p);
	return r;
}

static void sm_free(void *mbp, void *p SM_DBG_ARGS)
{
	free(p);
}

static void sm_status(void *mbp)
{
}

static void sm_status_filter(void *mbp, str *fmatch, FILE *fp)
{
}

static void sm_info(void *mbp, struct mem_info *info)
{
	memset(info, 0, sizeof(*info));
}

static void sm_report(void *mbp, mem_report_t *mrep)
{
	memset(mrep, 0, sizeof(*mrep));
}

static unsigned long sm_available(void *mbp)
{
	return 0;
}

static void sm_sums(void *mbp)
{
}

static void sm_mod_get_stats(void *mbp, void **p)
{
	*p = NULL;
}

static void sm_mod_free_stats(void *mbp)
{
}

int sm_malloc_init_pkg_manager(void)
{
	sr_pkg_api_t ma;

	memset(&ma, 0, sizeof(sr_pkg_api_t));
	ma.mname = _sm_mem_name;
	ma.xmalloc = sm_malloc;
	ma.xmallocxz = sm_mallocxz;
	ma.xmallocxn = sm_mallocxn;
	ma.xrealloc = sm_realloc;
	ma.xreallocxf = sm_reallocxf;
	ma.xfree = sm_free;
	ma.xstatus = sm_status;
	ma.xstatus_filter = sm_status_filter;
	ma.xinfo = sm_info;
	ma.xreport = sm_report;
	ma.xavailable = sm_available;
	ma.xsums = sm_sums;
	ma.xmodstats = sm_mod_get_stats;
	ma.xfmodstats = sm_mod_free_stats;

	return pkg_init_api(&ma);
}

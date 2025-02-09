/*
 * Copyright (C) 2025 Daniel-Constantin Mierla (asipto.com)
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

#ifndef __KSRXRAND_H__
#define __KSRXRAND_H__

#include <limits.h>
#include <stdlib.h>

#include "../coreparam.h"

#if RAND_MAX < INT_MAX
#define KSR_XRAND_MAX ((int)(0x7FFFFFFF)) /* (1<<31) - 1 */
#else
#define KSR_XRAND_MAX RAND_MAX
#endif

typedef int (*ksr_xrand_f)(void);
typedef void (*ksr_xsrand_f)(unsigned int seed);

typedef struct ksr_xrand
{
	ksr_xrand_f xrand;
	ksr_xsrand_f xsrand;
} ksr_xrand_t;

extern ksr_xrand_t _ksr_xrand_api;

#define ksr_xrand() _ksr_xrand_api.xrand()
#define ksr_xsrand(s) _ksr_xrand_api.xsrand(s)

int ksr_xrand_set(char *name);
int ksr_xrand_use(ksr_xrand_t *xrand);

int ksr_xrand_cp(str *pname, ksr_cpval_t *pval, void *eparam);

#endif

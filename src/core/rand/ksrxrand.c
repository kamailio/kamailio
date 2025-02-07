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

#include <string.h>

#include "../dprint.h"

#include "fastrand.h"
#include "ksrxrand.h"

/**
 * wrapper function for rand()/random()
 */
int ksr_wrand(void)
{
#if RAND_MAX < INT_MAX
	return (int)random();
#else
	return rand();
#endif
}

/**
 * wrapper function for srand()/srandom()
 */
void ksr_wsrand(unsigned int seed)
{
#if RAND_MAX < INT_MAX
	return srandom(seed);
#else
	return srand(seed);
#endif
}

/**
 * wrapper function for fastrand()
 */
int ksr_wfastrand(void)
{
	return (int)(fastrand() % ((unsigned)KSR_XRAND_MAX + 1));
}

/**
 * global with internal RAND API
 */
ksr_xrand_t _ksr_xrand_api = {.xrand = ksr_wrand, .xsrand = ksr_wsrand};

/**
 *
 */
int ksr_xrand_set(char *name)
{
	if(strcmp(name, "fast") == 0) {
		_ksr_xrand_api.xrand = ksr_wfastrand;
		_ksr_xrand_api.xsrand = fastrand_seed;
		return 0;
	}
	if((strcmp(name, "rand") == 0) || (strcmp(name, "rand") == 0)) {
		/* default - nothing to do */
		return 0;
	}
	LM_WARN("unknown rand engine: %s\n", name);
	return -1;
}

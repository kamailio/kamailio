/*
 * Copyright (C) 2016 Spencer Thomason
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

#ifndef __KAM_RAND_H__
#define __KAM_RAND_H__

#include <limits.h>
#include <stdlib.h>

#if RAND_MAX < INT_MAX
#define KAM_RAND_MAX ((int) (0x7FFFFFFF)) /* (1<<31) - 1 */
#define kam_rand(x) ((int)random(x))
#define kam_srand(x) srandom(x)
#else
#define KAM_RAND_MAX RAND_MAX
#define kam_rand(x) rand(x)
#define kam_srand(x) srand(x)
#endif

#endif

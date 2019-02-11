/* 
 * Copyright (C) 2007 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core ::  bit scan operations, see bit_scan.h.
 * \ingroup core
 * Module: \ref core
 */


#include "bit_scan.h"

unsigned char _debruijn_hash32[32]={0, 1, 2, 6, 3, 11, 7, 16, 4, 14, 12, 21, 8,
	23, 17, 26, 31, 5, 10, 15, 13, 20, 22, 25, 30, 9, 19, 24, 29, 18, 28, 27 };

unsigned char _debruijn_hash64[64]={0, 1, 2, 7, 3, 13, 8, 19, 4, 25, 14, 28, 9,
	34, 20, 40, 5, 17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
	63, 6, 12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56, 62, 11, 23,
	32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58 };


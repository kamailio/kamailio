/*
 * Copyright (C) 2010 iptelorg GmbH
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
 *
 */

/** Kamailio core ::  Implements the bit counting function:
 * Copyright (C) 2010 iptelorg GmbH
 * @ingroup core
 * Module: core
 *
 *   int bit_count(unsigned int u)
 *   Returns the number of bits in u.
 */


#ifndef _BIT_COUNT_H
#define _BIT_COUNT_H

/* fix __CPU_i386 -> __CPU_x86 */
#if defined __CPU_i386 && ! defined __CPU_x86
#define __CPU_x86
#endif
 
#ifdef CC_GCC_LIKE_ASM
#if defined __CPU_x86 || defined __CPU_x86_64
#ifdef __SSE4_2__
/* popcnt requires SSE4.2 support,
 * see http://en.wikipedia.org/wiki/SSE4 */
#define BIT_COUNT_ASM
#endif
#endif
#endif

#ifdef BIT_COUNT_ASM

/* Returns the number of 1 bits in u. */
static inline int bit_count(unsigned int u)
{
	int	v;

	asm volatile(" popcnt %1, %0 " : "=r" (v) : "rm" (u));
	return v;
}

#else /* BIT_COUNT_ASM */

/* Returns the number of 1 bits in u.
 * source: http://en.wikipedia.org/wiki/Hamming_weight
 */
#if 0
static inline int bit_count(unsigned int u)
{
	int	count;

	/* It is likely to have only few
	 * bits set to 1, so there will be only
	 * few iterations */
	for (count=0; u; count++)
		u &= u-1;
	return count;
}
#endif

static inline int bit_count(unsigned int i)
{
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

#if 0
/* number of bits in a byte.
 * (Only slightly faster then the above version,
 * It is not worth the extra memory usage.)
 */
extern int	bits_in_char[256];

static inline int bit_count(unsigned int u)
{
	return bits_in_char [u & 0xffu]
		+  bits_in_char [(u >>  8 ) & 0xffu]
		+  bits_in_char [(u >> 16) & 0xffu]
		+  bits_in_char [(u >> 24) & 0xffu];
}
#endif

#endif /* BIT_COUNT_ASM */

#endif /* _BIT_COUNT_H */

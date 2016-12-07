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

/** Kamailio core :: Bit test functions
 * @ingroup core
 * Module: core
 *
 * Bit test functions:
 *  - int bit_test(int offset, unsigned int *addr)
 *      Returns the bit found at offset position 
 *      in a bitstring pointed by addr.
 *
 *  - int bit_test_and_set(int offset, unsigned int *addr)
 *      Returns the bit found at offset position 
 *      in a bitstring pointed by addr, and sets
 *      the bit at the given offset.
 *
 *  - int bit_test_and_reset(int offset, unsigned int *addr)
 *      Returns the bit found at offset position 
 *      in a bitstring pointed by addr, and resets
 *      the bit at the given offset.
 *
 * Note that 0 <= offset <= 128, Make sure that addr points to
 * a large enough memory area.
 */

#ifndef _BIT_TEST_H
#define _BIT_TEST_H

/* fix __CPU_i386 -> __CPU_x86 */
#if defined __CPU_i386 && ! defined __CPU_x86
#define __CPU_x86
#endif
 
#ifdef CC_GCC_LIKE_ASM
#if defined __CPU_x86 || defined __CPU_x86_64
#define BIT_TEST_ASM
#endif
#endif

#ifdef BIT_TEST_ASM

/* Returns the bit found at offset position in the bitstring
 * pointed by addr.
 * Note that the CPU can access 4 bytes starting from addr,
 * hence 0 <= offset < 128 holds. Make sure that addr points
 * to a memory area that is large enough.
 */
static inline int bit_test(int offset, unsigned int *addr)
{
	unsigned char	v;

	asm volatile(
		" bt %2, %1 \n\t"
		" setc %0 \n\t"
		: "=qm" (v) : "m" (*addr), "r" (offset)
	);
	return (int)v;
}

/* Returns the bit found at offset position in the bitstring
 * pointed by addr and sets it to 1.
 * Note that the CPU can access 4 bytes starting from addr,
 * hence 0 <= offset < 128 holds. Make sure that addr points
 * to a memory area that is large enough.
 */
static inline int bit_test_and_set(int offset, unsigned int *addr)
{
	unsigned char	v;

	asm volatile(
		" bts %2, %1 \n\t"
		" setc %0 \n\t"
		: "=qm" (v) : "m" (*addr), "r" (offset)
	);
	return (int)v;
}

/* Returns the bit found at offset position in the bitstring
 * pointed by addr and resets it to 0.
 * Note that the CPU can access 4 bytes starting from addr,
 * hence 0 <= offset < 128 holds. Make sure that addr points
 * to a memory area that is large enough.
 */
static inline int bit_test_and_reset(int offset, unsigned int *addr)
{
	unsigned char	v;

	asm volatile(
		" btr %2, %1 \n\t"
		" setc %0 \n\t"
		: "=qm" (v) : "m" (*addr), "r" (offset)
	);
	return (int)v;
}

#else /* BIT_TEST_ASM */

/* Returns the bit found at offset position in the bitstring
 * pointed by addr.
 * Note that offset can be grater than 32, make sure that addr points
 * to a memory area that is large enough.
 */
static inline int bit_test(int offset, unsigned int *addr)
{
	return ((*(addr + offset/32)) & (1U << (offset % 32))) ? 1 : 0;
}

/* Returns the bit found at offset position in the bitstring
 * pointed by addr and sets it to 1.
 * Note that offset can be grater than 32, make sure that addr points
 * to a memory area that is large enough.
 */
static inline int bit_test_and_set(int offset, unsigned int *addr)
{
	unsigned int	*i;
	int	mask, res;

	i = addr + offset/32;
	mask = 1U << (offset % 32);
	res = ((*i) & mask) ? 1 : 0;
	(*i) |= mask;

	return res;
}

/* Returns the bit found at offset position in the bitstring
 * pointed by addr and resets it to 0.
 * Note that offset can be grater than 32, make sure that addr points
 * to a memory area that is large enough.
 */
static inline int bit_test_and_reset(int offset, unsigned int *addr)
{
	unsigned int	*i;
	int	mask, res;

	i = addr + offset/32;
	mask = 1U << (offset % 32);
	res = ((*i) & mask) ? 1 : 0;
	(*i) &= ~mask;

	return res;
}

#endif /* BIT_TEST_ASM */

#endif /* #ifndef _BIT_TEST_H */

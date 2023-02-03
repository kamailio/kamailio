/* 
 * Copyright (C) 2008 iptelorg GmbH
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

/** @file
 *  Kamailio core :: endianness compile and runtime  tests
 *  @author andrei
 *  @ingroup core
 * 
 *
 * Defines:
 *  -  __IS_LITTLE_ENDIAN if the system is little endian and
 *  -  __IS_BIG_ENDIAN    if it's big endian
 * Function/macros:
 *  -   is_big_endian()  - runtime test for big endian
 *  -   is_little_endian() - runtime test for little endian
 *  -   endianness_sanity_check() - returns 0 if the compile time
 *                                  detected endianness corresponds to
 *                                  the runtime detected one and -1 on 
 *                                  error (recommended action: bail out)
 *  -   bswap16() - 16 bit byte swap
 *  -   bswap32() - 32 bit byte swap
 *
 * Implementation notes:
 * Endian macro names/tests for different OSes:
 * linux:  __BYTE_ORDER == __LITTLE_ENDIAN | __BIG_ENDIAN
 *           BYTE_ORDER == LITTLE_ENDIAN | BIG_ENDIAN
 * bsd:     _BYTE_ORDER == _LITTLE_ENDIAN | _BIG_ENDIAN
 *           BYTE_ORDER == LITTLE_ENDIAN | BIG_ENDIAN
 * solaris: _LITTLE_ENDIAN | _BIG_ENDIAN
 *
 * Include file for the endian macros:
 * linux: <endian.h> (glibc), <sys/param.h>
 * bsd:   <sys/param.h>, <sys/endian.h>
 * solaris: <sys/param.h>
 *
 * Note: BIG_ENDIAN, LITTLE_ENDIAN, _BIG_ENDIAN, _LITTLE_ENDIAN cannot be 
 *       used always,  some OSes define both of them for BYTE_ORDER use
 *       (e.g. linux defines both BIG_ENDIAN & LITTLE_ENDIAN, bsds define
 *          _BIG_ENDIAN, _LITTLE_ENDIAN, BIG_ENDIAN, LITTLE_ENDIAN)
 *
 */


#ifndef _endianness_h
#define _endianness_h

/* use bsd includes: they work everywhere */
#include <sys/types.h>
#include <sys/param.h>



extern int _endian_test_int;

/* returns 1 for little endian, 0 for big endian */
#define endian_test()		(*(char*)&_endian_test_int==1)
#define is_big_endian()		(!endian_test())
#define is_little_endian()	endian_test()


extern int endianness_sanity_check(void);

/* detect compile time endianness */
#if defined __BYTE_ORDER && defined __LITTLE_ENDIAN && defined __BIG_ENDIAN
/* linux */
#if __BYTE_ORDER == __LITTLE_ENDIAN && ! defined __IS_LITTLE_ENDIAN
	#define __IS_LITTLE_ENDIAN 0x01020304
#endif
#if __BYTE_ORDER == __BIG_ENDIAN && ! defined __IS_BIG_ENDIAN
	#define __IS_BIG_ENDIAN 0x01020304
#endif
#elif defined _BYTE_ORDER && defined _LITTLE_ENDIAN && defined _BIG_ENDIAN
/* bsd */
#if _BYTE_ORDER == _LITTLE_ENDIAN && ! defined __IS_LITTLE_ENDIAN
	#define __IS_LITTLE_ENDIAN 0x01020304
#endif
#if _BYTE_ORDER == _BIG_ENDIAN && ! defined __IS_BIG_ENDIAN
	#define __IS_BIG_ENDIAN 0x01020304
#endif
#elif defined BYTE_ORDER && defined LITTLE_ENDIAN && defined BIG_ENDIAN
/* bsd old/deprecated */
#if BYTE_ORDER == LITTLE_ENDIAN && ! defined __IS_LITTLE_ENDIAN
	#define __IS_LITTLE_ENDIAN 0x01020304
#endif
#if BYTE_ORDER == BIG_ENDIAN && ! defined __IS_BIG_ENDIAN
	#define __IS_BIG_ENDIAN 0x01020304
#endif
#elif !(defined _LITTLE_ENDIAN && defined _BIG_ENDIAN) && \
		(defined _LITTLE_ENDIAN || defined _BIG_ENDIAN)
/* OSes that don't define BYTE_ORDER (sanity check above makes sure
 *   little & big endian are not defined in the same time )*/
/* solaris */
#if defined _LITTLE_ENDIAN && !defined __IS_LITTLE_ENDIAN
	#define __IS_LITTLE_ENDIAN 0x01020304
#endif
#if defined _BIG_ENDIAN && !defined __IS_BIG_ENDIAN
	#define __IS_BIG_ENDIAN 0x04030201
#endif
#elif !(defined LITTLE_ENDIAN && defined BIG_ENDIAN) && \
		(defined LITTLE_ENDIAN || defined BIG_ENDIAN)
/* OSes that don't define BYTE_ORDER (sanity check above makes sure
 *   little & big endian are not defined in the same time )*/
#if defined LITTLE_ENDIAN && !defined __IS_LITTLE_ENDIAN
	#define __IS_LITTLE_ENDIAN 0x01020304
#endif
#if defined BIG_ENDIAN && !defined __IS_BIG_ENDIAN
	#define __IS_BIG_ENDIAN 0x04030201
#endif

#else
#error could not detect endianness
#endif

#if !defined __IS_LITTLE_ENDIAN && !defined __IS_BIG_ENDIAN
#error BUG: could not detect endianness
#endif

#if defined __IS_LITTLE_ENDIAN && defined __IS_BIG_ENDIAN
#error BUG: both little & big endian detected in the same time
#endif

#if defined __IS_LITTLE_ENDIAN
#include <arpa/inet.h>
#define bswap16(x) ntohs(x)
#define bswap32(x) ntohl(x)
#else /* !__IS_LITTLE_ENDIAN */
#include <stdint.h>
#if defined __GNUC__ && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
/* need at least GCC 4.8 for __builtin_bswap16 on all archs */
#define bswap16(x)	((uint16_t)__builtin_bswap16(x))
#else
#define bswap16(x)	(((uint16_t)(x) >> 8) | \
			((uint16_t)(x) << 8))
#endif
#if defined __GNUC__ && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
/* GCC >= 4.3 provides __builtin_bswap32 */
#define bswap32(x)	((uint32_t)__builtin_bswap32(x))
#else
#define bswap32(x)	(((uint32_t)(x) << 24) | \
			(((uint32_t)(x) << 8) & 0xff0000) | \
			(((uint32_t)(x) >> 8) & 0xff00) | \
			((uint32_t)(x)  >> 24))
#endif
#endif /* !__IS_LITTLE_ENDIAN */

#endif /* _endianness_h */


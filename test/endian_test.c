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
/*
 *  endianness.h tests
 *  compile/run with:
 *  gcc  -Wall endian_test.c ../endianness.c  -o endian_test; ./endian_test
 */
/* 
 * History:
 * --------
 *  2008-06-13  created by andrei
 */

/*
 *
 * Macro names:
 * linux:  __BYTE_ORDER == __LITTLE_ENDIAN | __BIG_ENDIAN
 *           BYTE_ORDER == LITTLE_ENDIAN | BIG_ENDIAN
 * bsd:     _BYTE_ORDER == _LITTLE_ENDIAN | _BIG_ENDIAN
 *           BYTE_ORDER == LITTLE_ENDIAN | BIG_ENDIAN
 * solaris: _LITTLE_ENDIAN | _BIG_ENDIAN
 *
 * Note: BIG_ENDIAN, LITTLE_ENDIAN, _BIG_ENDIAN, _LITTLE_ENDIAN cannot be 
 *       used always,  some OSes define both of them for BYTE_ORDER use
 *       (e.g. linux defines both BIG_ENDIAN & LITTLE_ENDIAN, bsds define
 *          _BIG_ENDIAN, _LITTLE_ENDIAN, BIG_ENDIAN, LITTLE_ENDIAN)
 *
 * is sys/param.h universal ?
 */

#include <stdio.h>
#include "../endianness.h"
/* 
 * Tested:
 * linux:   y
 * freebsd: y
 * openbsd:
 * netbsd:
 * solaris: y
 * darwin: 
 * cygwin:
 *
 * Header files:
 * linux:  <endian.h> , <sys/param.h>
 * bsd:    <sys/param.h> or <sys/endian.h>
 * solaris: <sys/param.h>
 * openbsd
 * netbsd
 * solaris
 * cywin
 */

/*
 *
 * Macro names:
 * linux:  __BYTE_ORDER == __LITTLE_ENDIAN | __BIG_ENDIAN
 *           BYTE_ORDER == LITTLE_ENDIAN | BIG_ENDIAN
 * bsd:     _BYTE_ORDER == _LITTLE_ENDIAN | _BIG_ENDIAN
 *           BYTE_ORDER == LITTLE_ENDIAN | BIG_ENDIAN
 * solaris: _LITTLE_ENDIAN | _BIG_ENDIAN
 *
 * Note: BIG_ENDIAN, LITTLE_ENDIAN, _BIG_ENDIAN, _LITTLE_ENDIAN cannot be 
 *       used always,  some OSes define both of them for BYTE_ORDER use
 *       (e.g. linux defines both BIG_ENDIAN & LITTLE_ENDIAN, bsds define
 *          _BIG_ENDIAN, _LITTLE_ENDIAN, BIG_ENDIAN, LITTLE_ENDIAN)
 *
 * is sys/param.h universal ?
 */

/* test only */
#if defined __BYTE_ORDER && defined __LITTLE_ENDIAN 
#if	__BYTE_ORDER == __LITTLE_ENDIAN
#warning little endian (via __BYTE_ORDER)
#define __BYTE_ORDER_FOUND
#endif
#endif
#if defined __BYTE_ORDER && defined __BIG_ENDIAN
#if	__BYTE_ORDER == __BIG_ENDIAN
#warning big endian (via __BYTE_ORDER)
#define __BYTE_ORDER_FOUND
#endif
#endif
#if defined __BYTE_ORDER && !defined __BYTE_ORDER_FOUND
#error __BYTE_ORDER defined, but w/ a strange value
#endif

#if defined _BYTE_ORDER && defined _LITTLE_ENDIAN
#if _BYTE_ORDER == _LITTLE_ENDIAN
#warning little endian (via _BYTE_ORDER)
#define _BYTE_ORDER_FOUND
#endif
#endif
#if defined _BYTE_ORDER && defined _BIG_ENDIAN 
#if _BYTE_ORDER == _BIG_ENDIAN
#warning big endian (via _BYTE_ORDER)
#define _BYTE_ORDER_FOUND
#endif
#endif
#if defined _BYTE_ORDER && !defined _BYTE_ORDER_FOUND
#error _BYTE_ORDER defined, but w/ a strange value
#endif

#if defined BYTE_ORDER && defined LITTLE_ENDIAN 
#if BYTE_ORDER == LITTLE_ENDIAN
#warning little endian (via BYTE_ORDER)
#define BYTE_ORDER_FOUND
#endif
#endif
#if defined BYTE_ORDER && defined BIG_ENDIAN 
#if BYTE_ORDER == BIG_ENDIAN
#warning big endian (via BYTE_ORDER)
#define BYTE_ORDER_FOUND
#endif
#endif
#if defined BYTE_ORDER && !defined BYTE_ORDER_FOUND
#error BYTE_ORDER defined, but w/ a strange value
#endif

#if defined _LITTLE_ENDIAN
#warning _LITTLE_ENDIAN defined
#endif
#if defined _BIG_ENDIAN
#warning _BIG_ENDIAN defined
#endif
#if defined LITTLE_ENDIAN
#warning LITTLE_ENDIAN defined
#endif
#if defined BIG_ENDIAN
#warning BIG_ENDIAN defined
#endif


int main(int argc, char** argv)
{
	int ret;
	
	ret=0;
	if (endianness_sanity_check()!=0){
		printf("ERROR: sanity checks failed\n");
		ret=-1;
	}
	if (is_little_endian()){
#ifdef __IS_LITTLE_ENDIAN
		printf("OK: little endian confirmed\n");
#else 
		printf("ERROR: macro claims BIG ENDIAN, but it's little\n");
		return -1;
#endif
	}else{
#ifdef __IS_BIG_ENDIAN
		printf("OK: big endian confirmed\n");
#else 
		printf("ERROR: macro claims LITTLE ENDIAN, but it's big\n");
		return -1;
#endif
	}
	return ret;
}

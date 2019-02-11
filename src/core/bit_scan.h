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
 * \brief Kamailio core :: bit scan operations
 *
 * Copyright (C) 2007 iptelorg GmbH
 * \ingroup core
 * Module: \ref core
 *
 *  bit scan operations
 *
 *  - int bit_scan_forward(unsigned long v)   - returns the index of the first
 *                                          set bit (undefined value if v==0)
 *  - int bit_scan_forward32(unsigned int v)   - returns the index of the first
 *                                          set bit (undefined value if v==0)
 *  - int bit_scan_forward64(long long v)      - returns the index of the first
 *                                          set bit (undefined value if v==0)
 *  - int bit_scan_reverse(unsigned long v)   - returns the index of the last
 *                                          set bit (undefined value if v==0)
 *  - int bit_scan_reverse32(unsigned int v)  - returns the index of the last
 *                                          set bit (undefined value if v==0)
 *  - int bit_scan_reverse64(long long v)     - returns the index of the last
 *                                          set bit (undefined value if v==0)
 *
 * Config defines:   CC_GCC_LIKE_ASM  - the compiler support gcc style
 *                     inline asm,
 *                  __CPU_x86, __CPU_x86_64,
 *                  ULONG_MAX (limits.h)
 */
/* 
 * History:
 * --------
 *  2007-06-23  created by andrei
 */

#ifndef _bit_scan_h
#define _bit_scan_h

#include <limits.h>

/*! \brief fix __CPU_i386 -> __CPU_x86 */
#if defined __CPU_i386 && ! defined __CPU_x86
#define __CPU_x86
#endif


#ifdef CC_GCC_LIKE_ASM
#if defined __CPU_x86 || defined __CPU_x86_64
#define BIT_SCAN_ASM
#endif
#endif


/*! \brief set default bitscan versions, depending on the architecture
 * In general the order is  asm, debruijn, br, slow for bit_scan_forward
 *  and asm, br, slow, debruijn for bit_scan_reverse. */
#ifdef BIT_SCAN_ASM
/* have asm => use it */

#define bit_scan_forward32(i)	bit_scan_forward_asm32(i)
#define bit_scan_forward64(i)	bit_scan_forward_asm64(i)
#define bit_scan_reverse32(i)	bit_scan_reverse_asm32(i)
#define bit_scan_reverse64(i)	bit_scan_reverse_asm64(i)

#elif defined __CPU_x86 || defined __CPU_x86_64
/* no asm (e.g. no CC_GCC_LIKE_ASM) => debruijn for bit_scan_forward and
 *  br for bit_scan_reverse */
/* make sure debruijn an branch version are enabled */
#ifndef BIT_SCAN_DEBRUIJN
#define BIT_SCAN_DEBRUIJN
#endif
#ifndef BIT_SCAN_BRANCH
#define BIT_SCAN_BRANCH
#endif

#define bit_scan_forward32(i)	bit_scan_forward_debruijn32(i)
#define bit_scan_forward64(i)	bit_scan_forward_debruijn64(i)
#define bit_scan_reverse32(i)	bit_scan_reverse_br32(i)
#define bit_scan_reverse64(i)	bit_scan_reverse_br64(i)

#elif defined __CPU_sparc64
/* no asm yet => use branch for everything in 64 bit mode
 *               and debruijn + branch in 32 bit mode
 *  (in 64bit mode the branch method is slightly faster then debruijn,
 *   however note that in 32 bit mode the roles are reversed for _forward)*/
#ifndef BIT_SCAN_BRANCH
#define BIT_SCAN_BRANCH
#endif

#define bit_scan_reverse32(i)	bit_scan_reverse_br32(i)
#define bit_scan_reverse64(i)	bit_scan_reverse_br64(i)
#ifdef LP64
#define bit_scan_forward32(i)	bit_scan_forward_br32(i)
#define bit_scan_forward64(i)	bit_scan_forward_br64(i)
#else /* LP64 */

#ifndef BIT_SCAN_DEBRUIJN
#define BIT_SCAN_DEBRUIJN
#endif
#define bit_scan_forward32(i)	bit_scan_forward_debruijn32(i)
#define bit_scan_forward64(i)	bit_scan_forward_debruijn64(i)
#endif /* LP64 */

#else /* __CPU_XXX */
/* default - like x86 no asm */
/* make sure debruijn an branch version are enabled */
#ifndef BIT_SCAN_DEBRUIJN
#define BIT_SCAN_DEBRUIJN
#endif
#ifndef BIT_SCAN_BRANCH
#define BIT_SCAN_BRANCH
#endif

#define bit_scan_forward32(i)	bit_scan_forward_debruijn32(i)
#define bit_scan_forward64(i)	bit_scan_forward_debruijn64(i)
#define bit_scan_reverse32(i)	bit_scan_reverse_br32(i)
#define bit_scan_reverse64(i)	bit_scan_reverse_br64(i)

#endif /* __CPU_XXX */


/*! \brief try to use the right version for bit_scan_forward(unisgned long l)
 */
#if (defined (ULONG_MAX) && ULONG_MAX > 4294967295) || defined LP64
/*! \brief long is 64 bits */
#define bit_scan_forward(l)	bit_scan_forward64((unsigned long long)(l))
#define bit_scan_reverse(l)	bit_scan_reverse64((unsigned long long)(l))

#else
/*! \brief long is 32 bits */
#define bit_scan_forward(l)	bit_scan_forward32((l))
#define bit_scan_reverse(l)	bit_scan_reverse32((l))
#endif




#ifdef BIT_SCAN_DEBRUIJN

/*! \brief use a de Bruijn sequence to get the index of the set bit for a number
 *  of the form 2^k (DEBRUIJN_HASH32() and DEBRUIJN_HASH64()).
 *  bit_scan_forward & bit_scan_reverse would need first to convert
 *  the argument to 2^k (where k is the first set bit or last set bit index)-
 *  For bit_scan_forward this can be done very fast using x & (-x).
 *  For more info about this method see:
 *  http://citeseer.ist.psu.edu/leiserson98using.html
 *  ("Using de Bruijn Sequences to Index a 1 in a Computer Word")
 */

extern unsigned char _debruijn_hash32[32]; /* see bit_scan.c */
extern unsigned char _debruijn_hash64[64]; /* see bit_scan.c */

#define DEBRUIJN_CT32  0x04653ADFU
#define DEBRUIJN_CT64  0x0218A392CD3D5DBFULL 

#define DEBRUIJN_HASH32(x)\
	(((x)*DEBRUIJN_CT32)>>(sizeof(x)*8-5))

#define DEBRUIJN_HASH64(x)\
	(((x)*DEBRUIJN_CT64)>>(sizeof(x)*8-6))

#define bit_scan_forward_debruijn32(x) \
	( _debruijn_hash32[DEBRUIJN_HASH32((x) & (-(x)))])

#define bit_scan_forward_debruijn64(x) \
	( _debruijn_hash64[DEBRUIJN_HASH64((x) & (-(x)))])


static inline int bit_scan_reverse_debruijn32(unsigned int v)
{
	unsigned int last;
	
	do{
		last=v;
		v=v&(v-1);
	}while(v); /* => last is 2^k */
	return _debruijn_hash32[DEBRUIJN_HASH32(last)];
}


static inline int bit_scan_reverse_debruijn64(unsigned long long v)
{
	unsigned long long last;
	
	do{
		last=v;
		v=v&(v-1);
	}while(v); /* => last is 2^k */
	return _debruijn_hash64[DEBRUIJN_HASH64(last)];
}


#endif /* BIT_SCAN_DEBRUIJN */

#ifdef BIT_SCAN_SLOW
/* only for reference purposes (testing the other versions against it) */

static inline int bit_scan_forward_slow32(unsigned int v)
{
	int r;
	for(r=0; r<(sizeof(v)*8); r++, v>>=1)
		if (v&1) return r;
	return 0;
}


static inline int bit_scan_reverse_slow32(unsigned int v)
{
	int r;
	for(r=sizeof(v)*8-1; r>0; r--, v<<=1)
		if (v& (1UL<<(sizeof(v)*8-1))) return r;
	return 0;
}


static inline int bit_scan_forward_slow64(unsigned long long v)
{
	int r;
	for(r=0; r<(sizeof(v)*8); r++, v>>=1)
		if (v&1ULL) return r;
	return 0;
}


static inline int bit_scan_reverse_slow64(unsigned long long v)
{
	int r;
	for(r=sizeof(v)*8-1; r>0; r--, v<<=1)
		if (v& (1ULL<<(sizeof(v)*8-1))) return r;
	return 0;
}


#endif /* BIT_SCAN_SLOW */


#ifdef BIT_SCAN_BRANCH

static inline int bit_scan_forward_br32(unsigned int v)
{
	int b;
	
	b=0;
	if (v&0x01)
		return 0;
	if (!(v & 0xffff)){
		b+=16;
		v>>=16;
	}
	if (!(v&0xff)){
		b+=8;
		v>>=8;
	}
	if (!(v&0x0f)){
		b+=4;
		v>>=4;
	}
	if (!(v&0x03)){
		b+=2;
		v>>=2;
	}
	b+= !(v&0x01);
	return b;
}


static inline int bit_scan_reverse_br32(unsigned int v)
{
	int b;
	
	b=0;
	if (v & 0xffff0000){
		b+=16;
		v>>=16;
	}
	if (v&0xff00){
		b+=8;
		v>>=8;
	}
	if (v&0xf0){
		b+=4;
		v>>=4;
	}
	if (v&0x0c){
		b+=2;
		v>>=2;
	}
	b+= !!(v&0x02);
	return b;
}


static inline int bit_scan_forward_br64(unsigned long long v)
{
	int b;
	
	b=0;
	if (v&0x01ULL)
		return 0;
	if (!(v & 0xffffffffULL)){
		b+=32;
		v>>=32;
	}
	if (!(v & 0xffffULL)){
		b+=16;
		v>>=16;
	}
	if (!(v&0xffULL)){
		b+=8;
		v>>=8;
	}
	if (!(v&0x0fULL)){
		b+=4;
		v>>=4;
	}
	if (!(v&0x03ULL)){
		b+=2;
		v>>=2;
	}
	b+= !(v&0x01ULL);
	return b;
}


static inline int bit_scan_reverse_br64(unsigned long long v)
{
	int b;
	
	b=0;
	if (v & 0xffffffff00000000ULL){
		b+=32;
		v>>=32;
	}
	if (v & 0xffff0000ULL){
		b+=16;
		v>>=16;
	}
	if (v&0xff00ULL){
		b+=8;
		v>>=8;
	}
	if (v&0xf0ULL){
		b+=4;
		v>>=4;
	}
	if (v&0x0cULL){
		b+=2;
		v>>=2;
	}
	b+= !!(v&0x02ULL);
	return b;
}
#endif  /* BIT_SCAN_BRANCH */



#ifdef BIT_SCAN_ASM
#if defined __CPU_x86 || defined __CPU_x86_64
#define HAS_BIT_SCAN_ASM

static inline int bit_scan_forward_asm32(unsigned int v)
{
	int r;
	asm volatile(" bsfl %1, %0": "=r"(r): "rm"(v) );
	return r;
}

static inline int bit_scan_reverse_asm32(unsigned int v)
{
	int r;
	asm volatile(" bsrl %1, %0": "=r"(r): "rm"(v) );
	return r;
}

#ifdef __CPU_x86_64
static inline int bit_scan_forward_asm64(unsigned long long v)
{
	long r;
	asm volatile(" bsfq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

static inline int bit_scan_reverse_asm64(unsigned long long v)
{
	long r;
	asm volatile(" bsrq %1, %0": "=r"(r): "rm"(v) );
	return r;
}
#else
static inline int bit_scan_forward_asm64(unsigned long long v)
{
	if ((unsigned int)v)
		return bit_scan_forward_asm32((unsigned int)v);
	return 32+bit_scan_forward_asm32(*(((unsigned int*)(void*)&v)+1));
}

static inline int bit_scan_reverse_asm64(unsigned long long v)
{
	if (v & 0xffffffff00000000ULL)
		return 32+bit_scan_reverse_asm32(*(((unsigned int*)(void*)&v)+1));
	return bit_scan_reverse_asm32((unsigned int)v);
}
#endif /* __CPU_x86_64 */

#endif /* __CPU_x86 || __CPU_x86_64 */
#endif /* BIT_SCAN_ASM */

#endif

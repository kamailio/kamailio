/*
 * test bit_scan operations from bit_scan.h
 *  (both for correctness  and speed)
 * 
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
/* 
 * Example gcc command line:
 *  gcc -O9 -Wall -DCC_GCC_LIKE_ASM  -D__CPU_x86 bit_scan_test.c ../bit_scan.c
 *      -o bit_scan_test
 *
 * History:
 * --------
 *  2007-06-23  created by andrei
 */


#include <stdlib.h>
#include <stdio.h>


#define BIT_SCAN_DEBRUIJN
#define BIT_SCAN_BRANCH
#define BIT_SCAN_SLOW

#include "../bit_scan.h"
#ifdef NO_PROFILE
#define profile_init(x,y)  do{}while(0)
#define profile_start(x)  do{}while(0)
#define profile_end(x)  do{}while(0)
#define PROFILE_PRINT(x) do{}while(0)
#else
#include "profile.h"
#endif

#define CHECK(txt, v1, val, f, pd) \
	do{ \
		unsigned long long ret; \
		profile_start(pd); \
		ret=(unsigned long long)f(val); \
		profile_end(pd); \
		if ((unsigned long long)v1!=ret){ \
			fprintf(stderr, "ERROR:" #f ": %s, expected %llx (%llx), got"\
					" %llx\n", \
					(txt), (unsigned long long)v1, \
					(unsigned long long)val, ret); \
			exit(-1); \
		} \
	}while(0)

#ifndef PROFILE_PRINT
#define PROFILE_PRINT(pd) \
	do{ \
		printf("profile: %s (%ld/%ld) total %llu max %llu average %llu\n", \
				(pd)->name,  (pd)->entries, (pd)->exits, \
				(pd)->total_cycles,  (pd)->max_cycles, \
				(pd)->entries? \
				(pd)->total_cycles/(unsigned long long)(pd)->entries:0ULL ); \
	}while(0)
#endif

int main(int argc, char** argv)
{
	int r;
	unsigned int v;
	unsigned long long ll;
	int i;
#ifndef NO_PROFILE
	struct profile_data pdf1, pdf2, pdf4, pdf5, pdf6, pdf8;
	struct profile_data pdl1, pdl2, pdl4, pdl5, pdl6, pdl8;
#ifdef HAS_BIT_SCAN_ASM
	struct profile_data pdf3, pdf7, pdl3, pdl7;
#endif
	struct profile_data pdf_32, pdf_64, pdl_32, pdl_64;
	struct profile_data pdf_long, pdl_long;
#endif /* NO_PROFILE */
	
	profile_init(&pdf1, "first_debruijn32");
	profile_init(&pdf2, "first_slow32");
#ifdef HAS_BIT_SCAN_ASM
	profile_init(&pdf3, "first_asm32");
#endif
	profile_init(&pdf4, "first_br32");
	profile_init(&pdf5, "first_debruijn64");
	profile_init(&pdf6, "first_slow64");
#ifdef HAS_BIT_SCAN_ASM
	profile_init(&pdf7, "first_asm64");
#endif
	profile_init(&pdf8, "first_br64");
	profile_init(&pdl1, "last_debruijn32");
	profile_init(&pdl2, "last_slow32");
#ifdef HAS_BIT_SCAN_ASM
	profile_init(&pdl3, "last_asm32");
#endif
	profile_init(&pdl4, "last_br32");
	profile_init(&pdl5, "last_debruijn64");
	profile_init(&pdl6, "last_slow64");
#ifdef HAS_BIT_SCAN_ASM
	profile_init(&pdl7, "last_asm64");
#endif
	profile_init(&pdl8, "last_br64");
	
	profile_init(&pdf_32, "scan_forward32");
	profile_init(&pdf_64, "scan_forward64");
	profile_init(&pdl_32, "scan_reverse32");
	profile_init(&pdl_64, "scan_reverse64");
	profile_init(&pdf_long, "scan_forward_l");
	profile_init(&pdl_long, "scan_reverse_l");


	for (i=0; i<100; i++){
	for (r=0; r<32; r++){
		v=(1U<<r);
		CHECK("first debruijn 32bit", r, v, bit_scan_forward_debruijn32, &pdf1);
		CHECK("first slow 32bit", r, v, bit_scan_forward_slow32, &pdf2);
#ifdef HAS_BIT_SCAN_ASM
		CHECK("first asm 32bit", r, v, bit_scan_forward_asm32, &pdf3);
#endif
		CHECK("first br 32bit", r, v, bit_scan_forward_br32, &pdf4);
		CHECK("scan_forward32", r, v, bit_scan_forward32, &pdf_32);
		if (sizeof(long)<=4){
			CHECK("scan_forward_l", r, v, bit_scan_forward, &pdf_long);
		}
		v+=(v-1);
		CHECK("last debruijn 32bit", r, v, bit_scan_reverse_debruijn32, &pdl1);
		CHECK("last slow 32bit", r, v, bit_scan_reverse_slow32, &pdl2);
#ifdef HAS_BIT_SCAN_ASM
		CHECK("last asm 32bit", r, v, bit_scan_reverse_asm32, &pdl3);
#endif
		CHECK("last br 32bit", r, v, bit_scan_reverse_br32, &pdl4);
		CHECK("scan_reverse32", r, v, bit_scan_reverse32, &pdl_32);
		if (sizeof(long)<=4){
			CHECK("scan_reverse_l", r, v, bit_scan_reverse, &pdl_long);
		}
	}
	for (r=0; r<64; r++){
		ll=(1ULL<<r);
		CHECK("first debruijn 64bit", r, ll, bit_scan_forward_debruijn64, &pdf5);
		CHECK("first slow 64bit", r, ll, bit_scan_forward_slow64, &pdf6);
#ifdef HAS_BIT_SCAN_ASM
		CHECK("first asm 64bit", r, ll, bit_scan_forward_asm64, &pdf7);
#endif
		CHECK("first br 64bit", r, ll, bit_scan_forward_br64, &pdf8);
		CHECK("scan_forward64", r, ll, bit_scan_forward64, &pdf_64);
		if (sizeof(long)>4){
			CHECK("scan_forward_l", r, ll, bit_scan_forward, &pdf_long);
		}
		ll+=ll-1;
		CHECK("last debruijn 64bit", r, ll, bit_scan_reverse_debruijn64, &pdl5);
		CHECK("last slow 64bit", r, ll, bit_scan_reverse_slow64, &pdl6);
#ifdef HAS_BIT_SCAN_ASM
		CHECK("last asm 64bit", r, ll, bit_scan_reverse_asm64, &pdl7);
#endif
		CHECK("last br 64bit", r, ll, bit_scan_reverse_br64, &pdl8);
		CHECK("scan_reverse64", r, ll, bit_scan_reverse64, &pdl_64);
		if (sizeof(long)>4){
			CHECK("scan_reverse_l", r, ll, bit_scan_reverse, &pdl_long);
		}
	}
	}

	PROFILE_PRINT(&pdf1);
	PROFILE_PRINT(&pdf2);
#ifdef HAS_BIT_SCAN_ASM
	PROFILE_PRINT(&pdf3);
#endif
	PROFILE_PRINT(&pdf4);
	PROFILE_PRINT(&pdl1);
	PROFILE_PRINT(&pdl2);
#ifdef HAS_BIT_SCAN_ASM
	PROFILE_PRINT(&pdl3);
#endif
	PROFILE_PRINT(&pdl4);
	PROFILE_PRINT(&pdf5);
	PROFILE_PRINT(&pdf6);
#ifdef HAS_BIT_SCAN_ASM
	PROFILE_PRINT(&pdf7);
#endif
	PROFILE_PRINT(&pdf8);
	PROFILE_PRINT(&pdl5);
	PROFILE_PRINT(&pdl6);
#ifdef HAS_BIT_SCAN_ASM
	PROFILE_PRINT(&pdl7);
#endif
	PROFILE_PRINT(&pdl8);
	
	PROFILE_PRINT(&pdf_32);
	PROFILE_PRINT(&pdf_64);
	PROFILE_PRINT(&pdf_long);
	PROFILE_PRINT(&pdl_32);
	PROFILE_PRINT(&pdl_64);
	PROFILE_PRINT(&pdl_long);
	return 0;
}

/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifdef DBG_QM_MALLOC

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "../globals.h"
#include "../config.h"

#if 0
#ifdef PKG_MALLOC
#       ifdef VQ_MALLOC
#               include "vq_malloc.h"
#		define MY_MALLOC vqm_malloc
#		define MY_FREE vqm_free
#		define MY_INIT vqm_malloc_init
#		define MY_BLOCK vqm_block
#		define MY_STATUS vqm_status
#       else
#               include "q_malloc.h"
#		define MY_MALLOC qm_malloc
#		define MY_FREE qm_free
#		define MY_INIT qm_malloc_init
#		define MY_BLOCK qm_block
#		define MY_STATUS qm_status
#       endif
#endif

void memtest()
{
#define	TEST_SIZE 1024*1024
#define	TEST_RUN 1024
#define LONG_RUN 100000
#define ma(s) MY_MALLOC(mem_block, (s),__FILE__, __FUNCTION__, \
                                                                __LINE__);
#define mf(p)   MY_FREE(mem_block, (p), __FILE__,  __FUNCTION__, \
                                                                __LINE__);
	char tst_mem[TEST_SIZE];
	struct MY_BLOCK* mem_block;
	char *p0,*p1,*p2,*p3,*p4,*p5,*p6/*,*p7,*p8,*p9*/;
	int i, j, f;
	char *p[TEST_RUN];
	int t;

	debug=7;
	log_stderr=1;

	printf("entering test\n");

	mem_block=MY_INIT( tst_mem, TEST_SIZE );

	/* coalescing test w/big fragments */
	p0=ma(8194);
	p1=ma(8194);
	p2=ma(8194);
	MY_STATUS(mem_block);
	mf(p1);
	mf(p0);
	MY_STATUS(mem_block);
	mf(p2);
	MY_STATUS(mem_block);

	/* reuse test w/big fragments */
	p0=ma(8194);
	p1=ma(4196);
	mf(p0);
	p0=ma(8190);
	MY_STATUS(mem_block);
	mf(p1);
	mf(p0);
	MY_STATUS(mem_block);


	exit(0);

	p0=ma(8);
	p1=ma(24);
	p2=ma(32);
	p3=ma(32);
	p4=ma(32);
	p5=ma(1024);
	p6=ma(2048);

//	MY_STATUS(mem_block);

//	*(p0+9)=0;
	mf(p0);
	mf(p2);
	mf(p5);
	mf(p6);
	
//	MY_STATUS(mem_block);

	mf(p1);
	mf(p4);
	mf(p3);
//	mf(p3);

//	MY_STATUS(mem_block);

	for (i=0;i<TEST_RUN;i++)
		p[i]=ma( random() & 1023 );
//	MY_STATUS(mem_block);
	for (i=0;i<TEST_RUN;i++)
		mf( p[i] );
//	MY_STATUS(mem_block);

	f = 0;
#define GRANULARITY 100
	for (j=0; j<LONG_RUN; j++) {
		for (i=0;i<TEST_RUN;i++) {
			t=random() & 1023;
			if (! (t%24) ) t=(t+4096)*2;
			p[i]=ma( random() & 1023 );
		}
		for (i=TEST_RUN/3;i<2*TEST_RUN/3;i++)
			mf( p[i] );
		for (i=TEST_RUN/3;i<2*TEST_RUN/3;i++) {
			t=random() & 1023;
			if (! (t%24) ) t=(t+4096)*2;
			p[i]=ma( random() & 1023 );
		}
		for (i=0;i<TEST_RUN;i++)
			mf( p[i] );
		if ( GRANULARITY*j/LONG_RUN > f ) {
			f=GRANULARITY*j/LONG_RUN ;
			printf("%d%% done\n", f);
		}
	}
	printf("now I'm really done\n");
	MY_STATUS(mem_block);
	printf("And I'm done with dumping final report too\n");
	
	exit(0);
}
#endif


#endif

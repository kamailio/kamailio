/*
 * timer related functions (internal)
 *
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 * @brief Kamailio core :: Timer related functions (internal)
 * @ingroup core
 * Module: @ref core
 */


#ifndef timer_funcs_h
#define timer_funcs_h

#include "timer.h"


struct timer_head{
	struct timer_ln* volatile next;
	struct timer_ln* volatile prev;
};



/** @name hierarchical timing wheel with 3 levels
 *
 * Most timeouts should go in the first "wheel" (h0)
 * h0 will contain timers expiring from crt. time up to
 * crt. time + (1<<H0_BITS)/TICKS_HZ s and will use
 * (1<<H0_BITS)*sizeof(struct timer_head) bytes of memory, so arrange it
 * accordingly
 *
 * Uses ~280K on a 64 bits system and ~140K on a 32 bit system; for TICKS_HZ=10
 * holds ~ 30 min in the first hash/wheel and ~233h in the first two.
 * More perfomant arrangement: 16, 8, 8 (but eats 1 MB on a 64 bit system, and
 *  512K on a 32 bit one). For TICKS_HZ=10 it holds almost 2h in the
 *  first hash/wheel and ~460h in the first two.
 */
/*@{ */

#define H0_BITS 14
#define H1_BITS  9 
#define H2_BITS  (32-H1_BITS-H0_BITS)


#define H0_ENTRIES (1<<H0_BITS)
#define H1_ENTRIES (1<<H1_BITS)
#define H2_ENTRIES (1<<H2_BITS)

#define H0_MASK (H0_ENTRIES-1)
#define H1_MASK (H1_ENTRIES-1)
#define H1_H0_MASK ((1<<(H0_BITS+H1_BITS))-1)

/*@} */

struct timer_lists{
	struct timer_head  h0[H0_ENTRIES];
	struct timer_head  h1[H1_ENTRIES];
	struct timer_head  h2[H2_ENTRIES];
	struct timer_head  expired; /* list of expired entries */
};

extern struct timer_lists* timer_lst;


#define _timer_init_list(head)	clist_init((head), next, prev)


#define _timer_add_list(head, tl) \
	clist_append((head), (tl), next, prev)

#define _timer_rm_list(tl) \
	clist_rm((tl), next, prev)

#define timer_foreach(tl, head)	clist_foreach((head), (tl), next)
#define timer_foreach_safe(tl, tmp, head)	\
	clist_foreach_safe((head), (tl), (tmp), next)




/** @brief generic add timer entry to the timer lists function (see _timer_add)
 *
 * tl->expire must be set previously, delta is the difference in ticks
 * from current time to the timer desired expire (should be tl->expire-*tick)
 * If you don't know delta, you probably want to call _timer_add instead.
 */
static inline int _timer_dist_tl(struct timer_ln* tl, ticks_t delta)
{
	if (delta<H0_ENTRIES){
		if (delta==0){
			LM_WARN("0 expire timer added\n");
			_timer_add_list(&timer_lst->expired, tl);
		}else{
			_timer_add_list( &timer_lst->h0[tl->expire & H0_MASK], tl);
		}
	}else if (delta<(H0_ENTRIES*H1_ENTRIES)){
		_timer_add_list(&timer_lst->h1[(tl->expire & H1_H0_MASK)>>H0_BITS],tl);
	}else{
		_timer_add_list(&timer_lst->h2[tl->expire>>(H1_BITS+H0_BITS)], tl);
	}
	return 0;
}



#define _timer_mv_expire(h) \
	do{ \
		if ((h)->next!=(struct timer_ln*)(h)){ \
			clist_append_sublist(&timer_lst->expired, (h)->next, \
									(h)->prev, next, prev); \
			_timer_init_list(h); \
		} \
	}while(0)


#if 1

static inline void timer_redist(ticks_t t, struct timer_head *h)
{
	struct timer_ln* tl;
	struct timer_ln* tmp;
	
	timer_foreach_safe(tl, tmp, h){
		_timer_dist_tl(tl, tl->expire-t);
	}
	/* clear the current list */
	_timer_init_list(h);
}

static inline void timer_run(ticks_t t)
{
	struct timer_head *thp;

	/* trust the compiler for optimizing */
	if ((t & H0_MASK)==0){              /*r1*/
		if ((t & H1_H0_MASK)==0){        /*r2*/
			timer_redist(t, &timer_lst->h2[t>>(H0_BITS+H1_BITS)]);
		}
		
		timer_redist(t, &timer_lst->h1[(t & H1_H0_MASK)>>H0_BITS]);/*r2 >> H0*/
	}
	/*
	DBG("timer_run: ticks %u, expire h0[%u]\n",
						(unsigned ) t, (unsigned)(t & H0_MASK));*/
	thp = &timer_lst->h0[t & H0_MASK];
	_timer_mv_expire(thp);  /*r1*/
}
#else

static inline void timer_lst_mv0(ticks_t t, struct timer_head* h)
{
	struct timer_ln* tl;
	struct timer_ln* tmp;
	
	timer_foreach_safe(tl, tmp, h){
			_timer_dist_tl(tl, &timer_lst->h0[tl->expire & H0_MASK]);
	}
	/* clear the current list */
	_timer_init_list(h);
}

static inline void timer_lst_mv1(ticks_t t, struct timer_head* h)
{
	struct timer_ln* tl;
	struct timer_ln* tmp;
	
	timer_foreach_safe(tl, tmp, h){
		if ((tl->expire & H0_MASK)==0) /* directly to h0 */
			_timer_add_list(tl, &timer_lst->h0[tl->expire & H0_MASK]);
		else  /* to h1 */
			_timer_add_list(tl, 
						&timer_lst->h1[(tl->expire & H1_H0_MASK)>>H0_BITS]);
	}
	/* clear the current list */
	_timer_init_list(h);
}


/** @brief possible faster version */
static inline void timer_run(ticks_t t)
{
	/* trust the compiler for optimizing */
	if ((t & H0_MASK)==0){              /*r1*/
		if ((t & H1_H0_MASK)==0)        /*r2*/
			/* just move the list "down" to hash1 */
			timer_lst_mv1(&timer_lst->h2[t>>(H0_BITS+H1_BITS)]); 
		/* move "down" to hash0 */
		timer_lst_mv0(&timer_lst->h1[(t & H1_H0_MASK)>>H0_BITS]);
	}
	_timer_mv_expire(t, &timer_lst->h0[t & H0_MASK]);  /*r1*/
}
#endif



#endif

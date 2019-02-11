/*
 * Copyright (C) 2007 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core :: local, per process timer routines
 * WARNING: this should be used only within the same process, the timers
 *  are not multi-process safe or multi-thread safe
 *  (there are no locks)
 * \ingroup core
 * Module: \ref core
 */


#include "timer.h"
#include "timer_funcs.h"
#include "dprint.h"
#include "tcp_conn.h"
#include "mem/mem.h"
#include "compiler_opt.h"

#include "local_timer.h"



/** init a local_timer handle
 * returns 0 on success, -1 on error */
int init_local_timer(struct local_timer *t, ticks_t crt_ticks)
{
	int r;
	
	/* initial values */
	memset(t, 0, sizeof(*t));
	t->prev_ticks=crt_ticks;
	/* init timer structures */
	for (r=0; r<H0_ENTRIES; r++)
		_timer_init_list(&t->timer_lst.h0[r]);
	for (r=0; r<H1_ENTRIES; r++)
		_timer_init_list(&t->timer_lst.h1[r]);
	for (r=0; r<H2_ENTRIES; r++)
		_timer_init_list(&t->timer_lst.h2[r]);
	_timer_init_list(&t->timer_lst.expired);
	LM_DBG("timer_list between %p and %p\n",
			&t->timer_lst.h0[0], &t->timer_lst.h2[H2_ENTRIES]);
	return 0;
}



void destroy_local_timer(struct local_timer* lt)
{
}



/** generic add timer entry to the timer lists function (see _timer_add)
 * tl->expire must be set previously, delta is the difference in ticks
 * from current time to the timer desired expire (should be tl->expire-*tick)
 * If you don't know delta, you probably want to call _timer_add instead.
 */
static inline int _local_timer_dist_tl(struct local_timer* h, 
										struct timer_ln* tl, ticks_t delta)
{
	if (likely(delta<H0_ENTRIES)){
		if (unlikely(delta==0)){
			LM_WARN("0 expire timer added\n");
			_timer_add_list(&h->timer_lst.expired, tl);
		}else{
			_timer_add_list( &h->timer_lst.h0[tl->expire & H0_MASK], tl);
		}
	}else if (likely(delta<(H0_ENTRIES*H1_ENTRIES))){
		_timer_add_list(&h->timer_lst.h1[(tl->expire & H1_H0_MASK)>>H0_BITS],
							tl);
	}else{
		_timer_add_list(&h->timer_lst.h2[tl->expire>>(H1_BITS+H0_BITS)], tl);
	}
	return 0;
}



static inline void local_timer_redist(struct local_timer* l,
										ticks_t t, struct timer_head *h)
{
	struct timer_ln* tl;
	struct timer_ln* tmp;
	
	timer_foreach_safe(tl, tmp, h){
		_local_timer_dist_tl(l, tl, tl->expire-t);
	}
	/* clear the current list */
	_timer_init_list(h);
}



/** local timer add function (no lock, not multithread or multiprocess safe,
 * designed for local process use only)
 * t = current ticks
 * tl must be filled (the intial_timeout and flags must be set)
 * returns -1 on error, 0 on success */
static inline int _local_timer_add(struct local_timer *h, ticks_t t,
									struct timer_ln* tl)
{
	ticks_t delta;
	
	delta=tl->initial_timeout;
	tl->expire=t+delta;
	return _local_timer_dist_tl(h, tl, delta);
}



/** "public", safe timer add functions (local process use only)
 * adds a timer at delta ticks from the current time
 * returns -1 on error, 0 on success
 * WARNING: to re-add a deleted or expired timer you must call
 *          timer_reinit(tl) prior to timer_add
 *          The default behaviour allows timer_add to add a timer only if it
 *          has never been added before.*/
int local_timer_add(struct local_timer* h, struct timer_ln* tl, ticks_t delta,
						ticks_t crt_ticks)
{
	int ret;
	
	if (unlikely(tl->flags & F_TIMER_ACTIVE)){
		LM_DBG("called on an active timer %p (%p, %p), flags %x\n",
				tl, tl->next, tl->prev, tl->flags);
		ret=-1; /* refusing to add active or non-reinit. timer */
		goto error;
	}
	tl->initial_timeout=delta;
	if (unlikely((tl->next!=0) || (tl->prev!=0))){
		LM_CRIT("called with linked timer: %p (%p, %p)\n", tl, tl->next, tl->prev);
		ret=-1;
		goto error;
	}
	tl->flags|=F_TIMER_ACTIVE;
	ret=_local_timer_add(h, crt_ticks, tl);
error:
	return ret;
}



/** safe timer delete
 * deletes tl and inits the list pointer to 0
 * WARNING: to be able to reuse a deleted timer you must call
 *          timer_reinit(tl) on it
 * 
 */
void local_timer_del(struct local_timer* h, struct timer_ln* tl)
{
	/* quick exit if timer inactive */
	if (unlikely(!(tl->flags & F_TIMER_ACTIVE))){
		LM_DBG("called on an inactive timer %p (%p, %p), flags %x\n",
				tl, tl->next, tl->prev, tl->flags);
		return;
	}
	if (likely((tl->next!=0)&&(tl->prev!=0))){
		_timer_rm_list(tl); /* detach */
		tl->next=tl->prev=0;
	}else{
		LM_DBG("(f) timer %p (%p, %p) flags %x already detached\n",
			tl, tl->next, tl->prev, tl->flags);
	}
}



/** called from timer_handle*/
inline static void local_timer_list_expire(struct local_timer* l, 
											ticks_t t, struct timer_head* h)
{
	struct timer_ln * tl;
	ticks_t ret;
	
	/*LM_DBG("@ ticks = %lu, list =%p\n", (unsigned long) *ticks, h); */
	while(h->next!=(struct timer_ln*)h){
		tl=h->next;
		_timer_rm_list(tl); /* detach */
			tl->next=tl->prev=0; /* debugging */
				/*FIXME: process tcpconn */
				ret=tl->f(t, tl, tl->data);
				if (ret!=0){
					/* not one-shot, re-add it */
					if (ret!=(ticks_t)-1) /* ! periodic */
						tl->initial_timeout=ret;
					_local_timer_add(l, t, tl);
				}
	}
}



/** run all the handler that expire at t ticks */
static inline void local_timer_expire(struct local_timer* h, ticks_t t)
{
	/* trust the compiler for optimizing */
	if (unlikely((t & H0_MASK)==0)){              /*r1*/
		if (unlikely((t & H1_H0_MASK)==0)){        /*r2*/
			local_timer_redist(h, t, &h->timer_lst.h2[t>>(H0_BITS+H1_BITS)]);
		}
		
		local_timer_redist(h, t, &h->timer_lst.h1[(t & H1_H0_MASK)>>H0_BITS]);
															/*r2 >> H0*/
	}
	/* run handler immediately, no need to move it to the expired list
	 * (since no locks are used) */
	local_timer_list_expire(h, t, &h->timer_lst.h0[t & H0_MASK]);
}



/** "main" local timer routine, should be called with a proper ticks value
 * WARNING: it should never be called twice for the same ticks value
 * (it could cause too fast expires for long timers), ticks must be also
 *  always increasing */
void local_timer_run(struct local_timer* lt, ticks_t saved_ticks)
{
	
		/* protect against time running backwards */
		if (unlikely(lt->prev_ticks>=saved_ticks)){
			LM_CRIT("backwards or still time\n");
			/* try to continue */
			lt->prev_ticks=saved_ticks-1;
			return;
		}
		/* go through all the "missed" ticks, taking a possible overflow
		 * into account */
		for (lt->prev_ticks=lt->prev_ticks+1; lt->prev_ticks!=saved_ticks; 
															lt->prev_ticks++)
			local_timer_expire(lt, lt->prev_ticks);
		local_timer_expire(lt, lt->prev_ticks); /* do it for saved_ticks too */
	local_timer_list_expire(lt, saved_ticks, &lt->timer_lst.expired);
	/* WARNING: add_timer(...,0) must go directly to expired list, since
	 * otherwise there is a race between timer running and adding it
	 * (it could expire it H0_ENTRIES ticks later instead of 'now')*/
}


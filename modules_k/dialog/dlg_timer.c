/*
 * $Id$
 *
 * Copyright (C) 2006 Voice System SRL
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2007-03-06  to avoid races, tests on timer links are done under locks
 *             (bogdan)
 */


#include "../../mem/shm_mem.h"
#include "../../timer.h"
#include "dlg_timer.h"

struct dlg_timer *d_timer = 0;
dlg_timer_handler timer_hdl = 0;


int init_dlg_timer( dlg_timer_handler hdl )
{
	d_timer = (struct dlg_timer*)shm_malloc(sizeof(struct dlg_timer));
	if (d_timer==0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}
	memset( d_timer, 0, sizeof(struct dlg_timer) );

	d_timer->first.next = d_timer->first.prev = &(d_timer->first);

	d_timer->lock = lock_alloc();
	if (d_timer->lock==0) {
		LM_ERR("failed to alloc lock\n");
		goto error0;
	}

	if (lock_init(d_timer->lock)==0) {
		LM_ERR("failed to init lock\n");
		goto error1;
	}

	timer_hdl = hdl;
	return 0;
error1:
	lock_dealloc(d_timer->lock);
error0:
	shm_free(d_timer);
	d_timer = 0;
	return -1;
}



void destroy_dlg_timer(void)
{
	if (d_timer==0)
		return;

	lock_destroy(d_timer->lock);
	lock_dealloc(d_timer->lock);

	shm_free(d_timer);
	d_timer = 0;
}



static inline void insert_dlg_timer_unsafe(struct dlg_tl *tl)
{
	struct dlg_tl* ptr;

	for(ptr = d_timer->first.prev; ptr != &d_timer->first ; ptr = ptr->prev) {
		if ( ptr->timeout <= tl->timeout )
			break;
	}

	LM_DBG("inserting %p for %d\n", tl,tl->timeout);
	tl->prev = ptr;
	tl->next = ptr->next;
	tl->prev->next = tl;
	tl->next->prev = tl;
}



int insert_dlg_timer(struct dlg_tl *tl, int interval)
{
	lock_get( d_timer->lock);

	if (tl->next!=0 || tl->prev!=0) {
		lock_release( d_timer->lock);
		LM_CRIT("Trying to insert a bogus dlg tl=%p tl->next=%p tl->prev=%p\n",
			tl, tl->next, tl->prev);
		return -1;
	}
	tl->timeout = get_ticks()+interval;
	insert_dlg_timer_unsafe( tl );

	lock_release( d_timer->lock);

	return 0;
}



static inline void remove_dlg_timer_unsafe(struct dlg_tl *tl)
{
	tl->prev->next = tl->next;
	tl->next->prev = tl->prev;
}



int remove_dlg_timer(struct dlg_tl *tl)
{
	lock_get( d_timer->lock);

	if (tl->prev==0) {
		lock_release( d_timer->lock);
		return -1;
	}

	remove_dlg_timer_unsafe(tl);
	tl->next = 0;
	tl->prev = 0;
	tl->timeout = 0;

	lock_release( d_timer->lock);
	return 0;
}



int update_dlg_timer( struct dlg_tl *tl, int timeout )
{
	lock_get( d_timer->lock);

	if ( tl->next ) {
		if (tl->prev==0) {
			lock_release( d_timer->lock);
			return -1;
		}
		remove_dlg_timer_unsafe(tl);
	}

	tl->timeout = get_ticks()+timeout;
	insert_dlg_timer_unsafe( tl );

	lock_release( d_timer->lock);
	return 0;
}



static inline struct dlg_tl* get_expired_dlgs(unsigned int time)
{
	struct dlg_tl *tl , *end, *ret;

	lock_get( d_timer->lock);

	if (d_timer->first.next==&(d_timer->first)
	|| d_timer->first.next->timeout > time ) {
		lock_release( d_timer->lock);
		return 0;
	}

	end = &d_timer->first;
	tl = d_timer->first.next;
	LM_WARN("start with tl=%p tl->prev=%p tl->next=%p (%d) at %d "
		"and end with end=%p end->prev=%p end->next=%p\n",
		tl,tl->prev,tl->next,tl->timeout,time,
		end,end->prev,end->next);
	while( tl!=end && tl->timeout <= time) {
		LM_WARN("getting tl=%p tl->prev=%p tl->next=%p with %d\n",
			tl,tl->prev,tl->next,tl->timeout);
		tl->prev = 0;
		tl=tl->next;
	}
	LM_WARN("end with tl=%p tl->prev=%p tl->next=%p and d_timer->first.next->prev=%p\n",
		tl,tl->prev,tl->next,d_timer->first.next->prev);

	if (tl==end && d_timer->first.next->prev) {
		ret = 0;
	} else {
		ret = d_timer->first.next;
		tl->prev->next = 0;
		d_timer->first.next = tl;
		tl->prev = &d_timer->first;
	}

	lock_release( d_timer->lock);

	return ret;
}



void dlg_timer_routine(unsigned int ticks , void * attr)
{
	struct dlg_tl *tl, *ctl;

	tl = get_expired_dlgs( ticks );

	while (tl) {
		ctl = tl;
		tl = tl->next;
		ctl->next = (struct dlg_tl *)NULL;
		LM_DBG("tl=%p next=%p\n", ctl, tl);
		timer_hdl( ctl );
	}
}


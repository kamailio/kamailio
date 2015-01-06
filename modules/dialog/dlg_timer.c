/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Timer related functions for the dialog module
 * \ingroup dialog
 * Module: \ref dialog
 */

#include "../../mem/shm_mem.h"
#include "../../timer.h"
#include "dlg_timer.h"

/*! global dialog timer */
struct dlg_timer *d_timer = 0;
/*! global dialog timer handler */
dlg_timer_handler timer_hdl = 0;


/*!
 * \brief Initialize the dialog timer handler
 * Initialize the dialog timer handler, allocate the lock and a global
 * timer in shared memory. The global timer handler will be set on success.
 * \param hdl dialog timer handler
 * \return 0 on success, -1 on failure
 */
int init_dlg_timer(dlg_timer_handler hdl)
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


/*!
 * \brief Destroy global dialog timer
 */
void destroy_dlg_timer(void)
{
	if (d_timer==0)
		return;

	lock_destroy(d_timer->lock);
	lock_dealloc(d_timer->lock);

	shm_free(d_timer);
	d_timer = 0;
}


/*!
 * \brief Helper function for insert_dialog_timer
 * \see insert_dialog_timer
 * \param tl dialog timer list
 */
static inline void insert_dialog_timer_unsafe(struct dlg_tl *tl)
{
	struct dlg_tl* ptr;

	/* insert in sorted order */
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


/*!
 * \brief Insert a dialog timer to the list
 * \param tl dialog timer list
 * \param interval timeout value in seconds
 * \return 0 on success, -1 when the input timer list is invalid
 */
int insert_dlg_timer(struct dlg_tl *tl, int interval)
{
	lock_get( d_timer->lock);

	if (tl->next!=0 || tl->prev!=0) {
		LM_CRIT("Trying to insert a bogus dlg tl=%p tl->next=%p tl->prev=%p\n",
			tl, tl->next, tl->prev);
		lock_release( d_timer->lock);
		return -1;
	}
	tl->timeout = get_ticks()+interval;
	insert_dialog_timer_unsafe( tl );

	lock_release( d_timer->lock);

	return 0;
}


/*!
 * \brief Helper function for remove_dialog_timer
 * \param tl dialog timer list
 * \see remove_dialog_timer
 */
static inline void remove_dialog_timer_unsafe(struct dlg_tl *tl)
{
	tl->prev->next = tl->next;
	tl->next->prev = tl->prev;
}


/*!
 * \brief Remove a dialog timer from the list
 * \param tl dialog timer that should be removed
 * \return 1 when the input timer is empty, 0 when the timer was removed,
 * -1 when the input timer list is invalid
 */
int remove_dialog_timer(struct dlg_tl *tl)
{
	lock_get( d_timer->lock);

	if (tl->prev==NULL && tl->timeout==0) {
		lock_release( d_timer->lock);
		return 1;
	}

	if (tl->prev==NULL || tl->next==NULL) {
		LM_CRIT("bogus tl=%p tl->prev=%p tl->next=%p\n",
			tl, tl->prev, tl->next);
		lock_release( d_timer->lock);
		return -1;
	}

	remove_dialog_timer_unsafe(tl);
	tl->next = NULL;
	tl->prev = NULL;
	tl->timeout = 0;

	lock_release( d_timer->lock);
	return 0;
}


/*!
 * \brief Update a dialog timer on the list
 * \param tl dialog timer
 * \param timeout new timeout value in seconds
 * \return 0 on success, -1 when the input list is invalid
 * \note the update is implemented as a remove, insert
 */
int update_dlg_timer(struct dlg_tl *tl, int timeout)
{
	lock_get( d_timer->lock);

	if (tl->next==0 || tl->prev==0) {
		LM_CRIT("Trying to update a bogus dlg tl=%p tl->next=%p tl->prev=%p\n",
			tl, tl->next, tl->prev);
		lock_release( d_timer->lock);
		return -1;
	}
	remove_dialog_timer_unsafe( tl );
	tl->timeout = get_ticks()+timeout;
	insert_dialog_timer_unsafe( tl );

	lock_release( d_timer->lock);
	return 0;
}


/*!
 * \brief Helper function for dlg_timer_routine
 * \param time time for expiration check
 * \return list of expired dialogs on success, 0 on failure
 */
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
	LM_DBG("start with tl=%p tl->prev=%p tl->next=%p (%d) at %d "
		"and end with end=%p end->prev=%p end->next=%p\n",
		tl,tl->prev,tl->next,tl->timeout,time,
		end,end->prev,end->next);
	while( tl!=end && tl->timeout <= time) {
		LM_DBG("getting tl=%p tl->prev=%p tl->next=%p with %d\n",
			tl,tl->prev,tl->next,tl->timeout);
		tl->prev = 0;
		tl->timeout = 0;
		tl=tl->next;
	}
	LM_DBG("end with tl=%p tl->prev=%p tl->next=%p and d_timer->first.next->prev=%p\n",
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


/*!
 * \brief Timer routine for expiration of dialogs
 * Timer handler for expiration of dialogs, runs the global timer handler on them.
 * \param ticks time for expiration checks
 * \param attr unused
 */
void dlg_timer_routine(unsigned int ticks , void * attr)
{
	struct dlg_tl *tl, *ctl;

	tl = get_expired_dlgs( ticks );

	while (tl) {
		ctl = tl;
		tl = tl->next;
		ctl->next = NULL;
		LM_DBG("tl=%p next=%p\n", ctl, tl);
		timer_hdl( ctl );
	}
}

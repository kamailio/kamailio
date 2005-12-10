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
/*
 * History:
 * --------
 *  2003-09-12  timer_link.tg exists only if EXTRA_DEBUG (andrei)
 *  2004-02-13  timer_link.payload removed (bogdan)
 *  2005-11-03  rewritten to use the new timers (andrei)
 */


#ifndef _TM_TIMER_H
#define _TM_TIMER_H

#include "defs.h"

#include "lock.h"

#include "../../timer.h"
#include "h_table.h"

extern struct msgid_var user_fr_timeout;
extern struct msgid_var user_fr_inv_timeout;

extern ticks_t fr_timeout;
extern ticks_t fr_inv_timeout;
extern ticks_t wait_timeout;
extern ticks_t delete_timeout;
extern ticks_t rt_t1_timeout;
extern ticks_t rt_t2_timeout;

extern int tm_init_timers();

ticks_t wait_handler(ticks_t t, struct timer_ln *tl, void* data);
ticks_t retr_buf_handler(ticks_t t, struct timer_ln *tl, void* data);

#define init_cell_timers(c) \
	timer_init(&(c)->wait_timer, wait_handler, (c), 0) /* slow? */

#define init_rb_timers(rb) \
	timer_init(&(rb)->timer, retr_buf_handler, \
				(void*)(unsigned long)rt_t1_timeout, 0)

/* set fr & retr timer
 * rb  -  pointer to struct retr_buf
 * retr - initial retr. in ticks (use (ticks_t)(-1) to disable)
 * returns: -1 on error, 0 on success
 */
#ifdef TIMER_DEBUG
inline static int _set_fr_retr(struct retr_buf* rb, ticks_t retr,
								const char* file, const char* func,
								unsigned line)
#else
inline static int _set_fr_retr(struct retr_buf* rb, ticks_t retr)
#endif
{
	ticks_t timeout;
	ticks_t ticks;
	int ret;
	
	ticks=get_ticks_raw();
	timeout=rb->my_T->fr_timeout;
	rb->timer.data=(void*)(unsigned long)retr; /* hack */
	rb->retr_expire=ticks+retr;
	if (rb->t_active){
		/* we could have set_fr_retr called in the same time (acceptable 
		 * race), we rely on timer_add adding it only once */
#ifdef TIMER_DEBUG
		LOG(L_WARN, "WARNING: _set_fr_timer called from: %s(%s):%d\n", 
						file, func, line);
#endif
		LOG(L_CRIT, "WARNING: -_set_fr_timer- already added: %p , tl=%p!!!\n",
					rb, &rb->timer);
	}
	/* set active & if retr==-1 set disabled */
	rb->flags|= (F_RB_RETR_DISABLED & -(retr==-1)); 
	rb->fr_expire=ticks+timeout;
#ifdef TIMER_DEBUG
	ret=timer_add_safe(&(rb)->timer, (timeout<retr)?timeout:retr,
							file, func, line);
#else
	ret=timer_add(&(rb)->timer, (timeout<retr)?timeout:retr);
#endif
	if (ret==0) rb->t_active=1;
	return ret;
}



/* stop the timers assoc. with a retr. buf. */
#define stop_rb_timers(rb) \
do{ \
	if ((rb)->t_active){ \
		(rb)->t_active=0; \
		timer_del(&(rb)->timer); \
	}\
}while(0)

/* one shot, once disabled it cannot be re-enabled */
#define stop_rb_retr(rb) \
	((rb)->flags|=F_RB_RETR_DISABLED)

/* reset retr. interval to t2 and restart retr. timer */
#define switch_rb_retr_to_t2(rb) \
	do{ \
		(rb)->retr_expire=get_ticks_raw()+rt_t2_timeout; \
		(rb)->flags|=F_RB_T2; \
	}while(0)


/* restart fr */
#define restart_rb_fr(rb, new_val) \
	((rb)->fr_expire=get_ticks_raw()+(new_val))



/* change default & uac fr timers on-the-fly (if they are still running)
 *  if timer value==0 => leave it unchanged
 */
inline static void change_fr(struct cell* t, ticks_t fr_inv, ticks_t fr)
{
	int i;
	ticks_t fr_inv_expire, fr_expire;
	
	fr_expire=get_ticks_raw();
	fr_inv_expire=fr_expire+fr_inv;
	fr_expire+=fr;
	if (fr_inv) t->fr_inv_timeout=fr_inv;
	if (fr) t->fr_timeout=fr;
	for (i=0; i<t->nr_of_outgoings; i++){
		if (t->uac[i].request.t_active){ 
				if ((t->uac[i].request.flags & F_RB_FR_INV) && fr_inv)
					t->uac[i].request.fr_expire=fr_inv_expire;
				else if (fr)
					t->uac[i].request.fr_expire=fr_expire;
		}
	}
}


#endif

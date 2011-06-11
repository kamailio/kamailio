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
 *  2007-06-01  support for different retr. intervals per transaction;
 *              added maximum inv. and non-inv. transaction life time (andrei)
 *  2007-06-09  wait timers and retr. timers (if TM_FAST_RETR_TIMER is defined)
 *               are run in a fast timer context switching to SLOW timer
 *               automatically for FR (andrei)
 */


#ifndef _TM_TIMER_H
#define _TM_TIMER_H

#include "defs.h"

#include "../../compiler_opt.h"
#include "lock.h"

#include "../../timer.h"
#include "h_table.h"
#include "config.h"

/* try to do fast retransmissions (but fall back to slow timer for FR */
#define TM_FAST_RETR_TIMER


#ifdef  TM_DIFF_RT_TIMEOUT
#define RT_T1_TIMEOUT(rb)	((rb)->my_T->rt_t1_timeout)
#define RT_T2_TIMEOUT(rb)	((rb)->my_T->rt_t2_timeout)
#else
#define RT_T1_TIMEOUT(rb)	(cfg_get(tm, tm_cfg, rt_t1_timeout))
#define RT_T2_TIMEOUT(rb)	(cfg_get(tm, tm_cfg, rt_t2_timeout))
#endif

#define TM_REQ_TIMEOUT(t) \
	(is_invite(t)? \
		cfg_get(tm, tm_cfg, tm_max_inv_lifetime): \
		cfg_get(tm, tm_cfg, tm_max_noninv_lifetime))


extern struct msgid_var user_fr_timeout;
extern struct msgid_var user_fr_inv_timeout;
#ifdef TM_DIFF_RT_TIMEOUT
extern struct msgid_var user_rt_t1_timeout;
extern struct msgid_var user_rt_t2_timeout;
#endif
extern struct msgid_var user_inv_max_lifetime;
extern struct msgid_var user_noninv_max_lifetime;


extern int tm_init_timers();
int timer_fixup(void *handle, str *gname, str *name, void **val);

ticks_t wait_handler(ticks_t t, struct timer_ln *tl, void* data);
ticks_t retr_buf_handler(ticks_t t, struct timer_ln *tl, void* data);


#define init_cell_timers(c) \
	timer_init(&(c)->wait_timer, wait_handler, (c), F_TIMER_FAST) /* slow? */

#define init_rb_timers(rb) \
	timer_init(&(rb)->timer, retr_buf_handler, \
				(void*)(unsigned long)RT_T1_TIMEOUT(rb), 0)

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
	ticks_t eol;
	int ret;
	
	ticks=get_ticks_raw();
	timeout=rb->my_T->fr_timeout;
	eol=rb->my_T->end_of_life;
	rb->timer.data=(void*)(unsigned long)(2*retr); /* hack , next retr. int. */
	rb->retr_expire=ticks+retr;
	if (unlikely(rb->t_active)){
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
#ifdef TM_FAST_RETR_TIMER
	/* set timer to fast if retr enabled (retr!=-1) */
	rb->timer.flags|=(F_TIMER_FAST & -(retr!=-1));
#endif
	/* adjust timeout to MIN(fr, maximum lifetime) if rb is a request
	 *  (for neg. replies we are force to wait for the ACK so use fr) */
	if (unlikely ((rb->activ_type==TYPE_REQUEST) && 
		((s_ticks_t)(eol-(ticks+timeout))<0)) ){ /* fr after end of life */
		timeout=(((s_ticks_t)(eol-ticks))>0)?(eol-ticks):1; /* expire now */ 
	}
	atomic_cmpxchg_int((void*)&rb->fr_expire, 0, (int)(ticks+timeout));
	if (unlikely(rb->flags & F_RB_DEL_TIMER)){
		/* timer marked for deletion before we got a chance to add it
		 * (e..g we got immediately a final reply before in another process)
		 * => do nothing */
		DBG("_set_fr_timer: too late, timer already marked for deletion\n");
		return 0;
	}
#ifdef TIMER_DEBUG
	ret=timer_add_safe(&(rb)->timer, (timeout<retr)?timeout:retr,
							file, func, line);
#else
	ret=timer_add(&(rb)->timer, (timeout<retr)?timeout:retr);
#endif
	if (ret==0) rb->t_active=1;
	membar_write_atomic_op(); /* make sure t_active will be commited to mem.
								 before the transaction would be deref. by the
								 current process */
	return ret;
}



/* stop the timers assoc. with a retr. buf. */
#define stop_rb_timers(rb) \
do{ \
	membar_depends(); \
	(rb)->flags|=F_RB_DEL_TIMER; /* timer should be deleted */ \
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
		(rb)->flags|=F_RB_T2; \
		(rb)->retr_expire=get_ticks_raw()+RT_T2_TIMEOUT(rb); \
	}while(0)



inline static void restart_rb_fr(struct retr_buf* rb, ticks_t new_val)
{
	ticks_t now;
	struct cell* t;
	
	now=get_ticks_raw();
	t=rb->my_T;
	if (unlikely ((rb->activ_type==TYPE_REQUEST) &&
					(((s_ticks_t)(t->end_of_life-(now+new_val)))<0)) )
		rb->fr_expire=t->end_of_life;
	else
		rb->fr_expire=now+new_val;
}



/* change default & uac fr timers on-the-fly (if they are still running)
 *  if timer value==0 => leave it unchanged
 */
inline static void change_fr(struct cell* t, ticks_t fr_inv, ticks_t fr)
{
	int i;
	ticks_t fr_inv_expire, fr_expire, req_fr_expire;
	
	fr_expire=get_ticks_raw();
	fr_inv_expire=fr_expire+fr_inv;
	fr_expire+=fr;
	req_fr_expire=((s_ticks_t)(t->end_of_life-fr_expire)<0)?
						t->end_of_life:fr_expire;
	if (fr_inv) t->fr_inv_timeout=fr_inv;
	if (fr) t->fr_timeout=fr;
	for (i=0; i<t->nr_of_outgoings; i++){
		if (t->uac[i].request.t_active){ 
				if ((t->uac[i].request.flags & F_RB_FR_INV) && fr_inv)
					t->uac[i].request.fr_expire=fr_inv_expire;
				else if (fr){
					if (t->uac[i].request.activ_type==TYPE_REQUEST)
						t->uac[i].request.fr_expire=req_fr_expire;
					else
						t->uac[i].request.fr_expire=fr_expire;
				}
		}
	}
}


#ifdef TM_DIFF_RT_TIMEOUT
/* change t1 & t2 retransmissions timers
 * if now==1 try to change them almost on the fly 
 *  (next retransmission either at rt_t1 or rt_t2)
 * else only rt_t2 for running branches and both of them for new branches
 *  if timer value==0 => leave it unchanged
 */
inline static void change_retr(struct cell* t, int now,
								ticks_t rt_t1, ticks_t rt_t2)
{
	int i;

	if (rt_t1) t->rt_t1_timeout=rt_t1;
	if (rt_t2) t->rt_t2_timeout=rt_t2;
	if (now){
		for (i=0; i<t->nr_of_outgoings; i++){
			if (t->uac[i].request.t_active){ 
					if ((t->uac[i].request.flags & F_RB_T2) && rt_t2)
						/* not really needed (?) - if F_RB_T2 is set
						 * t->rt_t2_timeout will be used anyway */
						t->uac[i].request.timer.data=
									(void*)(unsigned long)rt_t2;
					else if (rt_t1)
						t->uac[i].request.timer.data=
									(void*)(unsigned long)rt_t1;
			}
		}
	}
}
#endif /* TM_DIFF_RT_TIMEOUT */



/* set the maximum transaction lifetime (from the present moment)
 * if adj is 1, adjust final response timeouts for all the req. branches such
 * that they are all <= eol (note however that this will work only for
 *  branches that still retransmit) */
inline static void change_end_of_life(struct cell* t, int adj, ticks_t eol)
{
	int i;
	
	t->end_of_life=get_ticks_raw()+eol;
	if (adj){
		for (i=0; i<t->nr_of_outgoings; i++){
			if (t->uac[i].request.t_active){ 
					if ((t->uac[i].request.activ_type==TYPE_REQUEST) &&
							((s_ticks_t)(t->end_of_life - 
										t->uac[i].request.fr_expire)<0))
						t->uac[i].request.fr_expire=t->end_of_life;
			}
		}
	}
}

inline static void cleanup_localcancel_timers( struct cell *t )
{
	int i;
	for (i=0; i<t->nr_of_outgoings; i++ )
		stop_rb_timers(&t->uac[i].local_cancel);
}



inline static void unlink_timers( struct cell *t )
{
	int i;

	stop_rb_timers(&t->uas.response);
	for (i=0; i<t->nr_of_outgoings; i++)
		stop_rb_timers(&t->uac[i].request);
	cleanup_localcancel_timers(t);
}



#endif

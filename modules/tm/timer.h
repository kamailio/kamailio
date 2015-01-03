/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \file
 * \brief TM :: timer support
 * 
 * TM timer support. It has been designed for high performance using
 * some techniques of which timer users need to be aware.
 * - One technique is "fixed-timer-length". We maintain separate 
 * timer lists, all of them include elements of the same time
 * to fire. That allows *appending* new events to the list as
 * opposed to inserting them by time, which is costly due to
 * searching time spent in a mutex. The performance benefit is
 * noticeable. The limitation is you need a new timer list for
 * each new timer length.
 * - Another technique is the timer process slices off expired elements
 * from the list in a mutex, but executes the timer after the mutex
 * is left. That saves time greatly as whichever process wants to
 * add/remove a timer, it does not have to wait until the current
 * list is processed. However, be aware the timers may hit in a delayed
 * manner; you have no guarantee in your process that after resetting a timer, 
 * it will no more hit. It might have been removed by timer process,
 * and is waiting to be executed.
 * 
 * The following example shows it:
 * 
 *		PROCESS1				TIMER PROCESS
 * 
 * -	0.						timer hits, it is removed from queue and
 * 							about to be executed
 * -	1.	process1 decides to
 * 		reset the timer 
 * -	2.						timer is executed now
 * -	3.	if the process1 naively
 * 		thinks the timer could not 
 * 		have been executed after 
 * 		resetting the timer, it is
 * 		WRONG -- it was (step 2.)
 * 
 * So be careful when writing the timer handlers. Currently defined timers 
 * don't hurt if they hit delayed, I hope at least. Retransmission timer 
 * may results in a useless retransmission -- not too bad. FR timer not too
 * bad either as timer processing uses a REPLY mutex making it safe to other
 * processing affecting transaction state. Wait timer not bad either -- processes
 * putting a transaction on wait don't do anything with it anymore.
 * 
 * 	Example when it does not hurt:
 * 
 * 		PROCESS1				TIMER PROCESS
 * 
 * -	0.						RETR timer removed from list and
 * 							scheduled for execution
 * -	1. 200/BYE received->
 * 	   reset RETR, put_on_wait
 * -	2.						RETR timer executed -- too late but it does
 * 							not hurt
 * -	3.						WAIT handler executed
 *
 * The rule of thumb is don't touch data you put under a timer. Create data,
 * put them under a timer, and let them live until they are safely destroyed from
 * wait/delete timer.  The only safe place to manipulate the data is 
 * from timer process in which delayed timers cannot hit (all timers are
 * processed sequentially).
 * 
 * A "bad example" -- rewriting content of retransmission buffer
 * in an unprotected way is bad because a delayed retransmission timer might 
 * hit. Thats why our reply retransmission procedure is enclosed in 
 * a REPLY_LOCK.
 * \ingroup tm
 */



#ifndef _TM_TIMER_H
#define _TM_TIMER_H

#include "defs.h"

#include "../../compiler_opt.h"
#include "lock.h"

#include "../../timer.h"
#include "h_table.h"
#include "config.h"

/**
 * \brief try to do fast retransmissions (but fall back to slow timer for FR
 */
#define TM_FAST_RETR_TIMER


#ifdef  TM_DIFF_RT_TIMEOUT
#define RT_T1_TIMEOUT_MS(rb)	((rb)->my_T->rt_t1_timeout_ms)
#define RT_T2_TIMEOUT_MS(rb)	((rb)->my_T->rt_t2_timeout_ms)
#else
#define RT_T1_TIMEOUT_MS(rb)	(cfg_get(tm, tm_cfg, rt_t1_timeout_ms))
#define RT_T2_TIMEOUT_MS(rb)	(cfg_get(tm, tm_cfg, rt_t2_timeout_ms))
#endif

#define TM_REQ_TIMEOUT(t) \
	(is_invite(t)? \
		cfg_get(tm, tm_cfg, tm_max_inv_lifetime): \
		cfg_get(tm, tm_cfg, tm_max_noninv_lifetime))


extern struct msgid_var user_fr_timeout;
extern struct msgid_var user_fr_inv_timeout;
#ifdef TM_DIFF_RT_TIMEOUT
extern struct msgid_var user_rt_t1_timeout_ms;
extern struct msgid_var user_rt_t2_timeout_ms;
#endif
extern struct msgid_var user_inv_max_lifetime;
extern struct msgid_var user_noninv_max_lifetime;


/**
 * \brief fix timer values to ticks
 */
extern int tm_init_timers(void);

/**
 * \brief Fixup function for the timer values
 * 
 * Fixup function for the timer values, (called by the
 * configuration framework)
 * \param handle not used
 * \param gname not used
 * \param name not used
 * \param val fixed timer value
 * \return 0 on success, -1 on error
 */
int timer_fixup(void *handle, str *gname, str *name, void **val);
int timer_fixup_ms(void *handle, str *gname, str *name, void **val);

ticks_t wait_handler(ticks_t t, struct timer_ln *tl, void* data);
ticks_t retr_buf_handler(ticks_t t, struct timer_ln *tl, void* data);


#define init_cell_timers(c) \
	timer_init(&(c)->wait_timer, wait_handler, (c), F_TIMER_FAST) /* slow? */

#define init_rb_timers(rb) \
	timer_init(&(rb)->timer, retr_buf_handler, \
				(void*)(unsigned long)(RT_T1_TIMEOUT_MS(rb)), 0)

/* set fr & retr timer
 * rb  -  pointer to struct retr_buf
 * retr - initial retr. in ticks (use (ticks_t)(-1) to disable)
 * returns: -1 on error, 0 on success
 */
#ifdef TIMER_DEBUG
inline static int _set_fr_retr(struct retr_buf* rb, unsigned retr_ms,
								const char* file, const char* func,
								unsigned line)
#else
inline static int _set_fr_retr(struct retr_buf* rb, unsigned retr_ms)
#endif
{
	ticks_t timeout;
	ticks_t ticks;
	ticks_t eol;
	ticks_t retr_ticks;
	int ret;
	
	ticks=get_ticks_raw();
	timeout=rb->my_T->fr_timeout;
	eol=rb->my_T->end_of_life;
	retr_ticks = (retr_ms != (unsigned)(-1))?MS_TO_TICKS(retr_ms):retr_ms;
	/* hack , next retr. int. */
	rb->timer.data=(void*)(unsigned long)(2*retr_ms);
	rb->retr_expire=ticks + retr_ticks;
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
	/* set active & if retr_ms==-1 set disabled */
	rb->flags|= (F_RB_RETR_DISABLED & -(retr_ms==(unsigned)-1));
#ifdef TM_FAST_RETR_TIMER
	/* set timer to fast if retr enabled (retr_ms!=-1) */
	rb->timer.flags|=(F_TIMER_FAST & -(retr_ms!=(unsigned)-1));
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
	ret=timer_add_safe(&(rb)->timer, (timeout<retr_ticks)?timeout:retr_ticks,
							file, func, line);
#else
	ret=timer_add(&(rb)->timer, (timeout<retr_ticks)?timeout:retr_ticks);
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
		(rb)->retr_expire=get_ticks_raw()+MS_TO_TICKS(RT_T2_TIMEOUT_MS(rb)); \
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
								unsigned rt_t1_ms, unsigned rt_t2_ms)
{
	int i;

	if (rt_t1_ms) t->rt_t1_timeout_ms=rt_t1_ms;
	if (rt_t2_ms) t->rt_t2_timeout_ms=rt_t2_ms;
	if (now){
		for (i=0; i<t->nr_of_outgoings; i++){
			if (t->uac[i].request.t_active){
					if ((t->uac[i].request.flags & F_RB_T2) && rt_t2_ms)
						/* not really needed (?) - if F_RB_T2 is set
						 * t->rt_t2_timeout will be used anyway */
						t->uac[i].request.timer.data =
							(void*)(unsigned long)rt_t2_ms;
					else if (rt_t1_ms)
						t->uac[i].request.timer.data =
							(void*)(unsigned long)rt_t1_ms;
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

/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
  timer.c is where we implement TM timers. It has been designed
  for high performance using some techniques of which timer users
  need to be aware.

	One technique is "fixed-timer-length". We maintain separate 
	timer lists, all of them include elements of the same time
	to fire. That allows *appending* new events to the list as
	opposed to inserting them by time, which is costly due to
	searching time spent in a mutex. The performance benefit is
	noticeable. The limitation is you need a new timer list for
	each new timer length.

	Another technique is the timer process slices off expired elements
	from the list in a mutex, but executes the timer after the mutex
	is left. That saves time greatly as whichever process wants to
	add/remove a timer, it does not have to wait until the current
	list is processed. However, be aware the timers may hit in a delayed
	manner; you have no guarantee in your process that after resetting a timer, 
	it will no more hit. It might have been removed by timer process,
    and is waiting to be executed.  The following example shows it:

			PROCESS1				TIMER PROCESS

	0.								timer hits, it is removed from queue and
									about to be executed
	1.	process1 decides to
		reset the timer 
	2.								timer is executed now
	3.	if the process1 naively
		thinks the timer could not 
		have been executed after 
		resetting the timer, it is
		WRONG -- it was (step 2.)

	So be careful when writing the timer handlers. Currently defined timers 
	don't hurt if they hit delayed, I hope at least. Retransmission timer 
	may results in a useless retransmission -- not too bad. FR timer not too
	bad either as timer processing uses a REPLY mutex making it safe to other
	processing affecting transaction state. Wait timer not bad either -- processes
	putting a transaction on wait don't do anything with it anymore.

		Example when it does not hurt:

			P1						TIMER
	0.								RETR timer removed from list and
									scheduled for execution
	1. 200/BYE received->
	   reset RETR, put_on_wait
	2.								RETR timer executed -- too late but it does
									not hurt
	3.								WAIT handler executed

	The rule of thumb is don't touch data you put under a timer. Create data,
    put them under a timer, and let them live until they are safely destroyed from
    wait/delete timer.  The only safe place to manipulate the data is 
    from timer process in which delayed timers cannot hit (all timers are
    processed sequentially).

	A "bad example" -- rewriting content of retransmission buffer
	in an unprotected way is bad because a delayed retransmission timer might 
	hit. Thats why our reply retransmission procedure is enclosed in 
	a REPLY_LOCK.

*/


#include "config.h"
#include "h_table.h"
#include "timer.h"
#include "../../dprint.h"
#include "lock.h"
#include "t_stats.h"

#include "../../hash_func.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "t_funcs.h"
#include "t_reply.h"
#include "t_cancel.h"


static struct timer_table *timertable;

int noisy_ctimer=0;


int timer_group[NR_OF_TIMER_LISTS] = 
{
	TG_FR, TG_FR,
	TG_WT,
	TG_DEL,
	TG_RT, TG_RT, TG_RT, TG_RT
};

/* default values of timeouts for all the timer list
   (see timer.h for enumeration of timer lists)
*/
unsigned int timer_id2timeout[NR_OF_TIMER_LISTS] = {
	FR_TIME_OUT, 		/* FR_TIMER_LIST */
	INV_FR_TIME_OUT, 	/* FR_INV_TIMER_LIST */
	WT_TIME_OUT, 		/* WT_TIMER_LIST */
	DEL_TIME_OUT,		/* DELETE_LIST */
	RETR_T1, 			/* RT_T1_TO_1 */
	RETR_T1 << 1, 		/* RT_T1_TO_2 */
	RETR_T1 << 2, 		/* RT_T1_TO_3 */
	RETR_T2 			/* RT_T2 */
						/* NR_OF_TIMER_LISTS */
};

/******************** handlers ***************************/



static void delete_cell( struct cell *p_cell, int unlock )
{

#ifdef EXTRA_DEBUG
	int i;
#endif

	/* there may still be FR/RETR timers, which have been reset
	   (i.e., time_out==TIMER_DELETED) but are stilled linked to
	   timer lists and must be removed from there before the
	   structures are released
	*/
	unlink_timers( p_cell );

#ifdef EXTRA_DEBUG

	if (is_in_timer_list2(& p_cell->wait_tl )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on WAIT, timeout=%d\n", p_cell, p_cell->wait_tl.time_out);
		abort();
	}
	if (is_in_timer_list2(& p_cell->uas.response.retr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on RETR (rep), timeout=%d\n",
			p_cell, p_cell->uas.response.retr_timer.time_out);
		abort();
	}
	if (is_in_timer_list2(& p_cell->uas.response.fr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on FR (rep), timeout=%d\n", p_cell,
			p_cell->uas.response.fr_timer.time_out);
		abort();
	}
	for (i=0; i<p_cell->nr_of_outgoings; i++) {
		if (is_in_timer_list2(& p_cell->uac[i].request.retr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on RETR (req %d), timeout %d\n", p_cell, i,
				p_cell->uac[i].request.retr_timer.time_out);
			abort();
		}
		if (is_in_timer_list2(& p_cell->uac[i].request.fr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on FR (req %d), timeout %d\n", p_cell, i,
				p_cell->uac[i].request.fr_timer.time_out);
			abort();
		}
		if (is_in_timer_list2(& p_cell->uac[i].local_cancel.retr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on RETR/cancel (req %d), timeout %d\n", p_cell, i,
				p_cell->uac[i].request.retr_timer.time_out);
			abort();
		}
		if (is_in_timer_list2(& p_cell->uac[i].local_cancel.fr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on FR/cancel (req %d), timeout %d\n", p_cell, i,
				p_cell->uac[i].request.fr_timer.time_out);
			abort();
		}
	}
	/* reset_retr_timers( hash__XX_table, p_cell ); */
#endif
	/* still in use ... don't delete */
	if ( IS_REFFED_UNSAFE(p_cell) ) {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
		DBG("DEBUG: delete_cell %p: can't delete -- still reffed\n",
			p_cell);
		/* it's added to del list for future del */
		set_timer( &(p_cell->dele_tl), DELETE_LIST );
	} else {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
		DBG("DEBUG: delete transaction %p\n", p_cell );
		free_cell( p_cell );
	}
}

static void fake_reply(struct cell *t, int branch, int code )
{
	branch_bm_t cancel_bitmap;
	short do_cancel_branch;
	enum rps reply_status;

	do_cancel_branch=t->is_invite && should_cancel_branch(t, branch);

	cancel_bitmap=do_cancel_branch ? 1<<branch : 0;
	if (t->local) {
		reply_status=local_reply( t, FAKED_REPLY, branch, 
			code, &cancel_bitmap );
	} else {
		reply_status=relay_reply( t, FAKED_REPLY, branch, code,
			&cancel_bitmap );
	}
	/* now when out-of-lock do the cancel I/O */
	if (do_cancel_branch) cancel_branch(t, branch );
	/* it's cleaned up on error; if no error occured and transaction
	   completed regularly, I have to clean-up myself
	*/
	if (reply_status==RPS_COMPLETED) {
		/* don't need to cleanup uac_timers -- they were cleaned
		   branch by branch and this last branch's timers are
		   reset now too
		*/
		/* don't need to issue cancels -- local cancels have been
		   issued branch by branch and this last branch was
		   cancelled now too
		*/
		/* then the only thing to do now is to put the transaction
		   on FR/wait state 
		*/
		set_final_timer(  t );
	}
}


inline static void retransmission_handler( void *attr)
{
	struct retr_buf* r_buf ;
	enum lists id;

	r_buf = (struct retr_buf*)attr;
#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from RETR timer\n",r_buf->my_T);
		abort();
	}	
#endif

	/*the transaction is already removed from RETRANSMISSION_LIST by timer*/
	/* retransmision */
	if ( r_buf->activ_type==TYPE_LOCAL_CANCEL 
		|| r_buf->activ_type==0 ) {
			DBG("DEBUG: retransmission_handler : "
				"request resending (t=%p, %.9s ... )\n", 
				r_buf->my_T, r_buf->buffer);
			if (SEND_BUFFER( r_buf )<=0) {
				reset_timer( &r_buf->fr_timer );
				fake_reply(r_buf->my_T, r_buf->branch, 503 );
				return;
			}
	} else {
			DBG("DEBUG: retransmission_handler : "
				"reply resending (t=%p, %.9s ... )\n", 
				r_buf->my_T, r_buf->buffer);
			t_retransmit_reply(r_buf->my_T);
	}

	id = r_buf->retr_list;
	r_buf->retr_list = id < RT_T2 ? id + 1 : RT_T2;

	set_timer(&(r_buf->retr_timer),id < RT_T2 ? id + 1 : RT_T2 );

	DBG("DEBUG: retransmission_handler : done\n");
}




inline static void final_response_handler( void *attr)
{
	int silent;
	struct retr_buf* r_buf;
	struct cell *t;

	r_buf = (struct retr_buf*)attr;
	t=r_buf->my_T;

#	ifdef EXTRA_DEBUG
	if (t->damocles) 
	{
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from FR timer\n",r_buf->my_T);
		abort();
	}
#	endif

	reset_timer(  &(r_buf->retr_timer) );

	/* the transaction is already removed from FR_LIST by the timer */

	/* FR for local cancels.... */
	if (r_buf->activ_type==TYPE_LOCAL_CANCEL)
	{
		DBG("DEBUG: FR_handler: stop retr for Local Cancel\n");
		return;
	}

	/* FR for replies (negative INVITE replies) */
	if (r_buf->activ_type>0) {
#		ifdef EXTRA_DEBUG
		if (t->uas.request->REQ_METHOD!=METHOD_INVITE
			|| t->uas.status < 300 ) {
			LOG(L_ERR, "ERROR: FR timer: uknown type reply buffer\n");
			abort();
		}
#		endif
		put_on_wait( t );
		return;
	};

	/* lock reply processing to determine how to proceed reliably */
	LOCK_REPLIES( t );
	/* now it can be only a request retransmission buffer;
	   try if you can simply discard the local transaction 
	   state without compellingly removing it from the
	   world */
	silent=
		/* not for UACs */
		!t->local
		/* invites only */
		&& t->is_invite
		/* parallel forking does not allow silent state discarding */
		&& t->nr_of_outgoings==1
		/* on_no_reply handler not installed -- serial forking could occur 
		   otherwise */
		&& t->on_negative==0
		/* something received -- we will not be silent on error */
		&& t->uac[r_buf->branch].last_received>0
		/* don't go silent if disallowed globally ... */
		&& noisy_ctimer==0
		/* ... or for this particular transaction */
		&& t->noisy_ctimer==0;
	if (silent) {
		UNLOCK_REPLIES(t);
		DBG("DEBUG: FR_handler: transaction silently dropped (%p)\n",t);
		put_on_wait( t );
		return;
	}

	DBG("DEBUG: FR_handler:stop retr. and send CANCEL (%p)\n", t);
	fake_reply(t, r_buf->branch, 408 );

	DBG("DEBUG: final_response_handler : done\n");
}

void cleanup_localcancel_timers( struct cell *t )
{
	int i;
	for (i=0; i<t->nr_of_outgoings; i++ )  {
		reset_timer(  &t->uac[i].local_cancel.retr_timer );
		reset_timer(  &t->uac[i].local_cancel.fr_timer );
	}
}


inline static void wait_handler( void *attr)
{
	struct cell *p_cell = (struct cell*)attr;

#ifdef EXTRA_DEBUG
	if (p_cell->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from WAIT timer\n",p_cell);
		abort();
	}	
	DBG("DEBUG: WAIT timer hit\n");
#endif

	/* stop cancel timers if any running */
	if (p_cell->is_invite) cleanup_localcancel_timers( p_cell );

	/* the transaction is already removed from WT_LIST by the timer */
	/* remove the cell from the hash table */
	DBG("DEBUG: wait_handler : removing %p from table \n", p_cell );
	LOCK_HASH( p_cell->hash_index );
	remove_from_hash_table_unsafe(  p_cell );
	/* jku: no more here -- we do it when we put a transaction on wait */
#ifdef EXTRA_DEBUG
	p_cell->damocles = 1;
#endif
	/* delete (returns with UNLOCK-ed_HASH) */
	delete_cell( p_cell, 1 /* unlock on return */ );
	DBG("DEBUG: wait_handler : done\n");
}




inline static void delete_handler( void *attr)
{
	struct cell *p_cell = (struct cell*)attr;

	DBG("DEBUG: delete_handler : removing %p \n", p_cell );
#ifdef EXTRA_DEBUG
	if (p_cell->damocles==0) {
		LOG( L_ERR, "ERROR: transaction %p not scheduled for deletion"
			" and called from DELETE timer\n",p_cell);
		abort();
	}	
#endif

	/* we call delete now without any locking on hash/ref_count;
	   we can do that because delete_handler is only entered after
	   the delete timer was installed from wait_handler, which
	   removed transaction from hash table and did not destroy it
	   because some processes were using it; that means that the
	   processes currently using the transaction can unref and no
	   new processes can ref -- we can wait until ref_count is
	   zero safely without locking
	*/
	delete_cell( p_cell, 0 /* don't unlock on return */ );
    DBG("DEBUG: delete_handler : done\n");
}


/***********************************************************/

struct timer_table *get_timertable()
{
	return timertable;
}


void unlink_timer_lists()
{
	struct timer_link  *tl, *end, *tmp;
	enum lists i;

	/* remember the DELETE LIST */
	tl = timertable->timers[DELETE_LIST].first_tl.next_tl;
	end = & timertable->timers[DELETE_LIST].last_tl;
	/* unlink the timer lists */
	for( i=0; i<NR_OF_TIMER_LISTS ; i++ )
		reset_timer_list( i );
	DBG("DEBUG: tm_shutdown : empting DELETE list\n");
	/* deletes all cells from DELETE_LIST list 
	   (they are no more accessible from enrys) */
	while (tl!=end) {
		tmp=tl->next_tl;
		free_cell((struct cell*)tl->payload);
		tl=tmp;
	}
	
}

struct timer_table *tm_init_timers()
{
	enum lists i;

	timertable=(struct timer_table *) shm_malloc(sizeof(struct timer_table));
	if (!timertable) {
		LOG(L_ERR, "ERROR: tm_init_timers: no shmem for timer_Table\n");
		goto error0;
	}
	memset(timertable, 0, sizeof (struct timer_table));
		

	/* inits the timers*/
	for(  i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
        init_timer_list( i );
    
    /* init. timer lists */
	timertable->timers[RT_T1_TO_1].id = RT_T1_TO_1;
	timertable->timers[RT_T1_TO_2].id = RT_T1_TO_2;
	timertable->timers[RT_T1_TO_3].id = RT_T1_TO_3;
	timertable->timers[RT_T2].id      = RT_T2;
	timertable->timers[FR_TIMER_LIST].id     = FR_TIMER_LIST; 
	timertable->timers[FR_INV_TIMER_LIST].id = FR_INV_TIMER_LIST;
	timertable->timers[WT_TIMER_LIST].id     = WT_TIMER_LIST;
	timertable->timers[DELETE_LIST].id       = DELETE_LIST;

	return timertable;

error0:
	return 0;
}

void free_timer_table()
{
	enum lists i;

	if (timertable) {
		/* the mutexs for sync the lists are released*/
		for ( i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
			release_timerlist_lock( &timertable->timers[i] );
		shm_free(timertable);
	}
		
}

void reset_timer_list( enum lists list_id)
{
	timertable->timers[list_id].first_tl.next_tl =
		&(timertable->timers[list_id].last_tl );
	timertable->timers[list_id].last_tl.prev_tl =
		&(timertable->timers[list_id].first_tl );
	timertable->timers[list_id].first_tl.prev_tl =
		timertable->timers[list_id].last_tl.next_tl = NULL;
	timertable->timers[list_id].last_tl.time_out = -1;
}




void init_timer_list( /* struct s_table* ht, */ enum lists list_id)
{
	reset_timer_list( /* ht, */ list_id );
	init_timerlist_lock( /* ht, */ list_id );
}




void print_timer_list( enum lists list_id)
{
	struct timer* timer_list=&(timertable->timers[ list_id ]);
	struct timer_link *tl ;

	tl = timer_list->first_tl.next_tl;
	while (tl!=& timer_list->last_tl)
	{
		DBG("DEBUG: print_timer_list[%d]: %p, next=%p \n",
			list_id, tl, tl->next_tl);
		tl = tl->next_tl;
	}
}




void remove_timer_unsafe(  struct timer_link* tl )
{
#ifdef EXTRA_DEBUG
	if (tl && tl->timer_list &&
		tl->timer_list->last_tl.prev_tl==0) {
		LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif
	if (is_in_timer_list2( tl )) {
#ifdef EXTRA_DEBUG
		DBG("DEBUG: unlinking timer: tl=%p, timeout=%d, group=%d\n", 
			tl, tl->time_out, tl->tg);
#endif
		tl->prev_tl->next_tl = tl->next_tl;
		tl->next_tl->prev_tl = tl->prev_tl;
		tl->next_tl = 0;
		tl->prev_tl = 0;
		tl->timer_list = NULL;
	}
}




/* put a new cell into a list nr. list_id */
void add_timer_unsafe( struct timer *timer_list, struct timer_link *tl,
	unsigned int time_out )
{
#ifdef EXTRA_DEBUG
	if (timer_list->last_tl.prev_tl==0) {
	LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif

	tl->time_out = time_out;
	tl->prev_tl = timer_list->last_tl.prev_tl;
	tl->next_tl = & timer_list->last_tl;
	timer_list->last_tl.prev_tl = tl;
	tl->prev_tl->next_tl = tl;
	tl->timer_list = timer_list;
#ifdef EXTRA_DEBUG
	if ( tl->tg != timer_group[ timer_list->id ] ) {
		LOG( L_CRIT, "CRITICAL error: changing timer group\n");
		abort();
	}
#endif
	DBG("DEBUG: add_to_tail_of_timer[%d]: %p\n",timer_list->id,tl);
}




/* detach items passed by the time from timer list */
struct timer_link  *check_and_split_time_list( struct timer *timer_list,
	int time )
{
	struct timer_link *tl , *end, *ret;


	/* quick check whether it is worth entering the lock */
	if (timer_list->first_tl.next_tl==&timer_list->last_tl 
			|| ( /* timer_list->first_tl.next_tl
				&& */ timer_list->first_tl.next_tl->time_out > time) )
		return NULL;

	/* the entire timer list is locked now -- noone else can manipulate it */
	lock(timer_list->mutex);

	end = &timer_list->last_tl;
	tl = timer_list->first_tl.next_tl;
	while( tl!=end && tl->time_out <= time) {
		tl->timer_list = NULL;
		tl=tl->next_tl;
	}

	/* nothing to delete found */
	if (tl->prev_tl==&(timer_list->first_tl)) {
		ret = NULL;
	} else { /* we did find timers to be fired! */
		/* the detached list begins with current beginning */
		ret = timer_list->first_tl.next_tl;
		/* and we mark the end of the split list */
		tl->prev_tl->next_tl = NULL;
		/* the shortened list starts from where we suspended */
		timer_list->first_tl.next_tl = tl;	
		tl->prev_tl = & timer_list->first_tl;
	}
#ifdef EXTRA_DEBUG
	if (timer_list->last_tl.prev_tl==0) {
		LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif
	/* give the list lock away */
	unlock(timer_list->mutex);

	return ret;
}



/* stop timer */
void reset_timer( struct timer_link* tl )
{
	/* disqualify this timer from execution by setting its time_out
	   to zero; it will stay in timer-list until the timer process
	   starts removing outdated elements; then it will remove it
	   but not execute; there is a race condition, though -- see
	   timer.c for more details
	*/
	tl->time_out = TIMER_DELETED;
#ifdef EXTRA_DEBUG
	DBG("DEBUG: reset_timer (group %d, tl=%p)\n", tl->tg, tl );
#endif
#ifdef _OBSOLETED
	/* lock(timer_group_lock[ tl->tg ]); */
	/* hack to work arround this timer group thing*/
	lock(hash__XX_table->timers[timer_group[tl->tg]].mutex);
	remove_timer_unsafe( tl );
	unlock(hash_XX_table->timers[timer_group[tl->tg]].mutex);
	/*unlock(timer_group_lock[ tl->tg ]);*/
#endif
}




/* determine timer length and put on a correct timer list */
void set_timer( struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	struct timer* list;


	if (list_id<FR_TIMER_LIST || list_id>=NR_OF_TIMER_LISTS) {
		LOG(L_CRIT, "ERROR: set_timer: unkown list: %d\n", list_id);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	}
	timeout = timer_id2timeout[ list_id ];
	list= &(timertable->timers[ list_id ]);

	lock(list->mutex);
	/* make sure I'm not already on a list */
	remove_timer_unsafe( new_tl );
	add_timer_unsafe( list, new_tl, get_ticks()+timeout);
	unlock(list->mutex);
}

/* similar to set_timer, except it allows only one-time
   timer setting and all later attempts are ignored */
void set_1timer( struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	struct timer* list;


	if (list_id<FR_TIMER_LIST || list_id>=NR_OF_TIMER_LISTS) {
		LOG(L_CRIT, "ERROR: set_timer: unkown list: %d\n", list_id);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	}
	timeout = timer_id2timeout[ list_id ];
	list= &(timertable->timers[ list_id ]);

	lock(list->mutex);
	if (!(new_tl->time_out>TIMER_DELETED)) {
		/* make sure I'm not already on a list */
		/* remove_timer_unsafe( new_tl ); */
		add_timer_unsafe( list, new_tl, get_ticks()+timeout);

		/* set_1timer is used only by WAIT -- that's why we can
		   afford updating wait statistics; I admit its not nice
		   but it greatly utilizes existing lock 
		*/
	}
	unlock(list->mutex);
	t_stats_wait();
}


void unlink_timers( struct cell *t )
{
	int i;
	int remove_fr, remove_retr;

	remove_fr=0; remove_retr=0;

	/* first look if we need to remove timers and play with
	   costly locks at all

	    note that is_in_timer_list2 is unsafe but it does not
	    hurt -- transaction is already dead (wait state) so that
	    noone else will install a FR/RETR timer and it can only
	    be removed from timer process itself -> it is safe to
	    use it without any protection
	*/
	if (is_in_timer_list2(&t->uas.response.fr_timer)) remove_fr=1; 
	else for (i=0; i<t->nr_of_outgoings; i++)
		if (is_in_timer_list2(&t->uac[i].request.fr_timer)
			|| is_in_timer_list2(&t->uac[i].local_cancel.fr_timer)) {
				remove_fr=1;
				break;
		}
	if (is_in_timer_list2(&t->uas.response.retr_timer)) remove_retr=1; 
	else for (i=0; i<t->nr_of_outgoings; i++)
		if (is_in_timer_list2(&t->uac[i].request.retr_timer)
			|| is_in_timer_list2(&t->uac[i].local_cancel.retr_timer)) {
				remove_retr=1;
				break;
		}

	/* do what we have to do....*/
	if (remove_retr) {
		/* RT_T1 lock is shared by all other RT timer
		   lists -- we can safely lock just one
		*/
		lock(timertable->timers[RT_T1_TO_1].mutex);
		remove_timer_unsafe(&t->uas.response.retr_timer);
		for (i=0; i<t->nr_of_outgoings; i++) {
			remove_timer_unsafe(&t->uac[i].request.retr_timer);
			remove_timer_unsafe(&t->uac[i].local_cancel.retr_timer);
		}
		unlock(timertable->timers[RT_T1_TO_1].mutex);
	}
	if (remove_fr) {
		/* FR lock is shared by all other FR timer
		   lists -- we can safely lock just one
		*/
		lock(timertable->timers[FR_TIMER_LIST].mutex);
		remove_timer_unsafe(&t->uas.response.fr_timer);
		for (i=0; i<t->nr_of_outgoings; i++) {
			remove_timer_unsafe(&t->uac[i].request.fr_timer);
			remove_timer_unsafe(&t->uac[i].local_cancel.fr_timer);
		}
		unlock(timertable->timers[FR_TIMER_LIST].mutex);
	}
}




#define run_handler_for_each( _tl , _handler ) \
	while ((_tl))\
	{\
		/* reset the timer list linkage */\
		tmp_tl = (_tl)->next_tl;\
		(_tl)->next_tl = (_tl)->prev_tl = 0;\
		DBG("DEBUG: timer routine:%d,tl=%p next=%p\n",\
			id,(_tl),tmp_tl);\
		if ((_tl)->time_out>TIMER_DELETED) \
			(_handler)( (_tl)->payload );\
		(_tl) = tmp_tl;\
	}




void timer_routine(unsigned int ticks , void * attr)
{
	/* struct timer_table *tt= (struct timer_table*)attr; */
	struct timer_link *tl, *tmp_tl;
	int                id;

#ifdef BOGDAN_TRIFLE
	DBG(" %d \n",ticks);
#endif

	for( id=0 ; id<NR_OF_TIMER_LISTS ; id++ )
	{
		/* to waste as little time in lock as possible, detach list
		   with expired items and process them after leaving the lock */
		tl=check_and_split_time_list( &timertable->timers[ id ], ticks);
		/* process items now */
		switch (id)
		{
			case FR_TIMER_LIST:
			case FR_INV_TIMER_LIST:
				run_handler_for_each(tl,final_response_handler);
				break;
			case RT_T1_TO_1:
			case RT_T1_TO_2:
			case RT_T1_TO_3:
			case RT_T2:
				run_handler_for_each(tl,retransmission_handler);
				break;
			case WT_TIMER_LIST:
				run_handler_for_each(tl,wait_handler);
				break;
			case DELETE_LIST:
				run_handler_for_each(tl,delete_handler);
				break;
		}
	}
}


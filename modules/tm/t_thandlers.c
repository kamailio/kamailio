/*
 * $Id$
 *
 * Timer handlers
 */

#include "../../hash_func.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "t_funcs.h"
#include "t_reply.h"
#include "t_cancel.h"

int noisy_ctimer=0;

static void unlink_timers( struct cell *t )
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
		lock(hash_table->timers[RT_T1_TO_1].mutex);
		remove_timer_unsafe(&t->uas.response.retr_timer);
		for (i=0; i<t->nr_of_outgoings; i++) {
			remove_timer_unsafe(&t->uac[i].request.retr_timer);
			remove_timer_unsafe(&t->uac[i].local_cancel.retr_timer);
		}
		unlock(hash_table->timers[RT_T1_TO_1].mutex);
	}
	if (remove_fr) {
		/* FR lock is shared by all other FR timer
		   lists -- we can safely lock just one
		*/
		lock(hash_table->timers[FR_TIMER_LIST].mutex);
		remove_timer_unsafe(&t->uas.response.fr_timer);
		for (i=0; i<t->nr_of_outgoings; i++) {
			remove_timer_unsafe(&t->uac[i].request.fr_timer);
			remove_timer_unsafe(&t->uac[i].local_cancel.fr_timer);
		}
		unlock(hash_table->timers[FR_TIMER_LIST].mutex);
	}
}

/* delete_cell attempt to delete a transaction of not refered
   by any process; if so, it is put on a delete timer which will
   try the same later; it assumes it is safe to read ref_count --
   either the hash entry is locked or the transaction has been
   removed from hash table (i.e., other processes can only
   decrease ref_count)

   it is static as it is safe to be called only from WAIT/DELETE
   timers, the only valid place from which a transaction can be
   removed
*/

static void delete_cell( struct cell *p_cell, int unlock )
{

	int i;

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
	/* reset_retr_timers( hash_table, p_cell ); */
#endif
	/* still in use ... don't delete */
	if ( IS_REFFED_UNSAFE(p_cell) ) {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
		DBG("DEBUG: delete_cell %p: can't delete -- still reffed\n",
			p_cell);
		/* it's added to del list for future del */
		set_timer( hash_table, &(p_cell->dele_tl), DELETE_LIST );
	} else {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
		DBG("DEBUG: delete transaction %p\n", p_cell );
		free_cell( p_cell );
	}
}



inline void retransmission_handler( void *attr)
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
			SEND_BUFFER( r_buf );
			DBG("DEBUG: retransmission_handler : "
				"request resending (t=%p, %.9s ... )\n", 
				r_buf->my_T, r_buf->buffer);
	} else {
			DBG("DEBUG: retransmission_handler : "
				"reply resending (t=%p, %.9s ... )\n", 
				r_buf->my_T, r_buf->buffer);
			t_retransmit_reply(r_buf->my_T);
	}

	id = r_buf->retr_list;
	r_buf->retr_list = id < RT_T2 ? id + 1 : RT_T2;

	set_timer(hash_table,&(r_buf->retr_timer),id < RT_T2 ? id + 1 : RT_T2 );

	DBG("DEBUG: retransmission_handler : done\n");
}




inline void final_response_handler( void *attr)
{
	int silent;
	struct retr_buf* r_buf;
	enum rps reply_status;
	struct cell *t;
	branch_bm_t cancel_bitmap;
	short do_cancel_branch;

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

	reset_timer( hash_table , &(r_buf->retr_timer) );

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
	do_cancel_branch=t->is_invite && 
		should_cancel_branch(t, r_buf->branch);

#ifdef _OBSOLETED
	/* set global environment for currently processed transaction */
	T=t;
	global_msg_id=T->uas.request->id;
#endif 

	cancel_bitmap=do_cancel_branch ? 1<<r_buf->branch : 0;
	if (t->local) {
		reply_status=local_reply( t, FAKED_REPLY, r_buf->branch, 
			408, &cancel_bitmap );
	} else {
		reply_status=relay_reply( t, FAKED_REPLY, r_buf->branch, 408, 
			&cancel_bitmap );
	}
	/* now when out-of-lock do the cancel I/O */
	if (do_cancel_branch) cancel_branch(t, r_buf->branch );
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
		set_final_timer( /* hash_table, */ t );
	}
	DBG("DEBUG: final_response_handler : done\n");
}

void cleanup_localcancel_timers( struct cell *t )
{
	int i;
	for (i=0; i<t->nr_of_outgoings; i++ )  {
		reset_timer( hash_table, &t->uac[i].local_cancel.retr_timer );
		reset_timer( hash_table, &t->uac[i].local_cancel.fr_timer );
	}
}


inline void wait_handler( void *attr)
{
	struct cell *p_cell = (struct cell*)attr;

#ifdef EXTRA_DEBUG
	if (p_cell->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from WAIT timer\n",p_cell);
		abort();
	}	
	DBG("DEBUG: ---------- WAIT timer hit ------- \n");
#endif

	/* stop cancel timers if any running */
	if (p_cell->is_invite) cleanup_localcancel_timers( p_cell );

	/* the transaction is already removed from WT_LIST by the timer */
	/* remove the cell from the hash table */
	DBG("DEBUG: wait_handler : removing %p from table \n", p_cell );
	LOCK_HASH( p_cell->hash_index );
	remove_from_hash_table_unsafe( hash_table, p_cell );
	/* jku: no more here -- we do it when we put a transaction on wait */
#ifdef EXTRA_DEBUG
	p_cell->damocles = 1;
#endif
	/* delete (returns with UNLOCK-ed_HASH) */
	delete_cell( p_cell, 1 /* unlock on return */ );
	DBG("DEBUG: wait_handler : done\n");
}




inline void delete_handler( void *attr)
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
	struct s_table    *hash_table = (struct s_table *)attr;
	struct timer_link *tl, *tmp_tl;
	int                id;

#ifdef BOGDAN_TRIFLE
	DBG(" %d \n",ticks);
#endif

	for( id=0 ; id<NR_OF_TIMER_LISTS ; id++ )
	{
		/* to waste as little time in lock as possible, detach list
		   with expired items and process them after leaving the lock */
		tl=check_and_split_time_list( &(hash_table->timers[ id ]), ticks);
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


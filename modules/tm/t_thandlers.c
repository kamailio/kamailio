/*
 * $Id$
 *
 * Timer handlers
 */

#include "hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
//#include "../../timer.h"





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
	DBG("DEBUG: retransmission_handler : resending (t=%p)\n", r_buf->my_T);
	switch ( r_buf->activ_type )
	{
		case (TYPE_REQUEST):
			SEND_BUFFER( r_buf );
			break;
		case (TYPE_LOCAL_CANCEL):
			SEND_CANCEL_BUFFER( r_buf );
			break;
		default:
			T=r_buf->my_T;
			t_retransmit_reply();
	}

	id = r_buf->retr_list;
	r_buf->retr_list = id < RT_T2 ? id + 1 : RT_T2;

	set_timer(hash_table,&(r_buf->retr_timer),id < RT_T2 ? id + 1 : RT_T2 );

	DBG("DEBUG: retransmission_handler : done\n");
}




inline void final_response_handler( void *attr)
{
	struct retr_buf* r_buf = (struct retr_buf*)attr;

#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) 
	{
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from FR timer\n",r_buf->my_T);
		abort();
	}
#endif

	/* the transaction is already removed from FR_LIST by the timer */
	if (r_buf->activ_type==TYPE_LOCAL_CANCEL)
	{
		DBG("DEBUG: FR_handler: stop retransmission for Local Cancel\n");
		reset_timer( hash_table , &(r_buf->retr_timer) );
		return;
	}
	/* send a 408 */
	if ( r_buf->my_T->uac[r_buf->branch].status<200
#ifdef SILENT_FR
	&& (r_buf->my_T->nr_of_outgoings>1     /*if we have forked*/
		|| r_buf->my_T->uas.request->first_line.u.request.method_value!=
			METHOD_INVITE                  /*if is not an INVITE */
		|| r_buf->my_T->uac[r_buf->my_T->nr_of_outgoings].uri.s
		                                   /*if "no on no response" was set*/
		|| r_buf->my_T->uac[r_buf->branch].rpl_received==0
											/*if no reply was received*/
	)
#endif
	)
	{
		DBG("DEBUG: FR_handler:stop retr. and send CANCEL (%p)\n",r_buf->my_T);
		reset_timer( hash_table, &(r_buf->retr_timer) );
		t_build_and_send_CANCEL( r_buf->my_T ,r_buf->branch);
		/* dirty hack:t_send_reply would increase ref_count which would indeed
		result in refcount++ which would not -- until timer processe's
		T changes again; currently only on next call to t_send_reply from
		FR timer; thus I fake the values now to avoid recalculating T
		and refcount++ JKU */
		T=r_buf->my_T;
		global_msg_id=T->uas.request->id;
		DBG("DEBUG: FR_handler: send 408 (%p)\n", r_buf->my_T);
		t_send_reply( r_buf->my_T->uas.request, 408, "Request Timeout",
			r_buf->branch);
	}else{
		/* put it on WT_LIST - transaction is over */
		DBG("DEBUG: final_response_handler:-> put on wait"
			" (t=%p)\n", r_buf->my_T);
		t_put_on_wait(  r_buf->my_T );
	}
	DBG("DEBUG: final_response_handler : done\n");
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
#endif

	/* the transaction is already removed from WT_LIST by the timer */
	/* the cell is removed from the hash table */
	DBG("DEBUG: wait_handler : removing %p from table \n", p_cell );
	remove_from_hash_table( hash_table, p_cell );
	/* jku: no more here -- we do it when we put a transaction on wait */
	DBG("DEBUG: wait_handler : stopping all timers\n");
	reset_retr_timers(hash_table,p_cell) ; 
	/* put it on DEL_LIST - sch for del */
#ifdef EXTRA_DEBUG
	p_cell->damocles = 1;
#endif
	delete_cell( p_cell );
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
	delete_cell( p_cell );
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


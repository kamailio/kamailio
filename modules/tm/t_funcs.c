/*
 * $Id$
 *
 * transaction maintenance functions
 */

#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "hash_func.h"
#include "t_funcs.h"
#include "t_fork.h"


struct cell      *T;
unsigned int     global_msg_id;
struct s_table*  hash_table;


int tm_startup()
{
	/* building the hash table*/
	hash_table = init_hash_table();
	if (!hash_table)
		return -1;

	/* init. timer lists */
	hash_table->timers[RT_T1_TO_1].id = RT_T1_TO_1;
	hash_table->timers[RT_T1_TO_2].id = RT_T1_TO_2;
	hash_table->timers[RT_T1_TO_3].id = RT_T1_TO_3;
	hash_table->timers[RT_T2].id      = RT_T2;
	hash_table->timers[FR_TIMER_LIST].id     = FR_TIMER_LIST;
	hash_table->timers[FR_INV_TIMER_LIST].id = FR_INV_TIMER_LIST;
	hash_table->timers[WT_TIMER_LIST].id     = WT_TIMER_LIST;
	hash_table->timers[DELETE_LIST].id       = DELETE_LIST;

	/* register the timer function */
	register_timer( timer_routine , hash_table , 1 );

	/* fork table */
	nr_forks = 0;

	/*first msg id*/
	global_msg_id = 0;
	T = T_UNDEFINED;

	return 0;
}




void tm_shutdown()
{
	struct timer_link  *tl, *end, *tmp;
	int i;

	DBG("DEBUG: tm_shutdown : start\n");
	/* remember the DELETE LIST */
	tl = hash_table->timers[DELETE_LIST].first_tl.next_tl;
	end = & hash_table->timers[DELETE_LIST].last_tl;
	/* unlink the timer lists */
	for( i=0; i<NR_OF_TIMER_LISTS ; i++ )
		reset_timer_list( hash_table, i );

	DBG("DEBUG: tm_shutdown : empting DELETE list\n");
	/* deletes all cells from DELETE_LIST list
	(they are no more accessible from enrys) */
	while (tl!=end) {
		tmp=tl->next_tl;
		free_cell((struct cell*)tl->payload);
		tl=tmp;
	}

	/* destroy the hash table */
	DBG("DEBUG: tm_shutdown : empting hash table\n");
	free_hash_table( hash_table );
	DBG("DEBUG: tm_shutdown : removing semaphores\n");
	lock_cleanup();
	DBG("DEBUG: tm_shutdown : done\n");
}




/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int t_add_transaction( struct sip_msg* p_msg )
{
	struct cell*    new_cell;

	DBG("DEBUG: t_add_transaction: adding......\n");
	/* sanity check: ACKs can never establish a transaction */
	if ( p_msg->REQ_METHOD==METHOD_ACK )
	{
		LOG(L_ERR, "ERROR: add_transaction: ACK can't be used to add"
			" transaction\n");
		return -1;
	}

	/* creates a new transaction */
	new_cell = build_cell( p_msg ) ;
	DBG("DEBUG: t_add_transaction: new transaction created %p\n", new_cell);
	if  ( !new_cell ){
		LOG(L_ERR, "ERROR: add_transaction: out of mem:\n");
		sh_status();
		return -1;
	}
	/*insert the transaction into hash table*/
	insert_into_hash_table( hash_table , new_cell );
	DBG("DEBUG: t_add_transaction: new transaction inserted, hash: %d\n",
		new_cell->hash_index );

	T = new_cell;
	T_REF(T);
	return 1;
}




/*   returns 1 if everything was OK or -1 for error
*/
int t_release_transaction( struct sip_msg* p_msg)
{
      return t_put_on_wait( T );
}



int t_unref( /* struct sip_msg* p_msg */ )
{
	if (T==T_UNDEFINED || T==T_NULL)
		return -1;
	T_UNREF( T );
	T=T_UNDEFINED;
	return 1;
}





/* ----------------------------HELPER FUNCTIONS-------------------------------- */


int t_update_timers_after_sending_reply( struct retr_buf *rb )
{
	struct cell *Trans = rb->my_T;

	/* make sure that if we send something final upstream, everything else
	   will be cancelled */
	if (Trans->uas.status>=300&&Trans->uas.request->REQ_METHOD==METHOD_INVITE)
	{
		rb->retr_list = RT_T1_TO_1;
		set_timer( hash_table, &(rb->retr_timer), RT_T1_TO_1 );
		set_timer( hash_table, &(rb->fr_timer), FR_TIMER_LIST );
	} else if ( Trans->uas.request->REQ_METHOD==METHOD_CANCEL ) {
		if ( Trans->T_canceled==T_UNDEFINED )
			Trans->T_canceled = t_lookupOriginalT( hash_table ,
				Trans->uas.request );
		if ( Trans->T_canceled==T_NULL )
			return 1;
		/* put CANCEL transaction on wait only if canceled transaction already
		    is in final status and there is nothing to cancel; */
		if ( Trans->T_canceled->uas.status>=200)
			t_put_on_wait( Trans );
	} else if (Trans->uas.status>=200)
		t_put_on_wait( Trans );
   return 1;
}




/*
  */
int t_put_on_wait(  struct cell  *Trans  )
{
	unsigned int i;
	//struct retrans_buff* rb;

#ifndef WAIT
	if (is_in_timer_list2( &(Trans->wait_tl)))
  	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		return 1;
	}
#else
	/* have some race conditons occured and we already
	  entered/passed the wait status previously?
	  if so, exit now
	*/

	LOCK_WAIT(Trans);
	if (Trans->on_wait)
	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		UNLOCK_WAIT(Trans);
		return 1;
	} else {
		Trans->on_wait=1;
		UNLOCK_WAIT(Trans);
	}
#endif

	/* remove from  retranssmision  and  final response   list */
	DBG("DEBUG: t_put_on_wait: stopping timers (FR and RETR)\n");
	reset_retr_timers(hash_table,Trans) ;

#ifdef SILENT_FR
	if (Trans->nr_of_outgoings>1)
#endif
	{
	/* cancel pending client transactions, if any */
	for( i=0 ; i<Trans->nr_of_outgoings ; i++ )
		if ( Trans->uac[i].rpl_received && Trans->uac[i].status<200 )
			t_build_and_send_CANCEL(Trans , i);
	}

	/* adds to Wait list*/
	set_timer( hash_table, &(Trans->wait_tl), WT_TIMER_LIST );
	return 1;
}





void delete_cell( struct cell *p_cell )
{
#ifdef EXTRA_DEBUG
	int i;

	if (is_in_timer_list2(& p_cell->wait_tl )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on WAIT\n", p_cell);
		abort();
	}
	/*
	if (is_in_timer_list2(& p_cell->outbound_response.retr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on RETR (rep)\n",
			p_cell);
		abort();
	}
	if (is_in_timer_list2(& p_cell->outbound_response.fr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on FR (rep)\n", p_cell);
		abort();
	}
	for (i=0; i<p_cell->nr_of_outgoings; i++) {
		if (is_in_timer_list2(& p_cell->outbound_request[i]->retr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on RETR (req %d)\n", p_cell, i);
			abort();
		}
		if (is_in_timer_list2(& p_cell->outbound_request[i]->fr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on FR (req %d)\n", p_cell, i);
			abort();
		}
	}
	*/
	reset_retr_timers( hash_table, p_cell );
#endif
	/* still in use ... don't delete */
	if ( T_IS_REFED(p_cell) ) {
#ifdef	EXTRA_DEBUG
		if (T_REFCOUNTER(p_cell)>1) {
			DBG("DEBUG: while debugging with a single process, ref_count>1\n");
			DBG("DEBUG: transaction =%p\n", p_cell );
			abort();
		}
#endif
		DBG("DEBUG: delete_cell: t=%p post for delete (refbitmap %x,"
			" refcount %d)\n",p_cell,p_cell->ref_bitmap,T_REFCOUNTER(p_cell));
		/* it's added to del list for future del */
		set_timer( hash_table, &(p_cell->dele_tl), DELETE_LIST );
	} else {
		DBG("DEBUG: delete transaction %p\n", p_cell );
		free_cell( p_cell );
	}
}



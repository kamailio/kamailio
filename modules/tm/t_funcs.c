/*
 * $Id$
 *
 * transaction maintenance functions
 */

#include <limits.h>
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "../../dset.h"
#include "t_funcs.h"
#include "t_fwd.h"
#include "t_lookup.h"

/* pointer to the big table where all the transaction data
   lives
*/
struct s_table*  hash_table;

/* ----------------------------------------------------- */

int send_pr_buffer( struct retr_buf *rb,
	void *buf, int len, char *function, int line )
{
	if (buf && len && rb )
		return udp_send( rb->send_sock, buf,
			len, &rb->to,  sizeof(union sockaddr_union) ) ;
	else {
		LOG(L_CRIT, "ERROR: sending an empty buffer from %s (%d)\n",
			function, line );
		return -1;
	}
}

void start_retr( struct retr_buf *rb )
{
	rb->retr_list=RT_T1_TO_1;
	set_timer( hash_table, &rb->retr_timer, RT_T1_TO_1 );
	set_timer( hash_table, &rb->fr_timer, FR_TIMER_LIST );
}

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


	/* fork table */
	/* nr_forks = 0; */	

	/* init static hidden values */
	init_t();

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


/*   returns 1 if everything was OK or -1 for error
*/
int t_release_transaction( struct cell *trans )
{
	trans->kr|=REQ_RLSD;

	reset_timer( hash_table, & trans->uas.response.fr_timer );
	reset_timer( hash_table, & trans->uas.response.retr_timer );

	cleanup_uac_timers( trans );
	
	put_on_wait( trans );
	return 1;
}


/* ----------------------------HELPER FUNCTIONS-------------------------------- */


/*
  */
void put_on_wait(  struct cell  *Trans  )
{

#ifdef _XWAIT
	LOCK_WAIT(Trans);
	if (Trans->on_wait)
	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		UNLOCK_WAIT(Trans);
	} else {
		Trans->on_wait=1;
		UNLOCK_WAIT(Trans);
	}
#endif
#ifdef EXTRA_DEBUG
	DBG("DEBUG: --- out on WAIT --- \n");
#endif


	/* we put the transaction on wait timer; we do it only once
	   in transaction's timelife because putting it multiple-times
	   might result in a second instance of a wait timer to be
	   set after the first one fired; on expiration of the second
	   instance, the transaction would be re-deleted

			PROCESS1		PROCESS2		TIMER PROCESS
		0. 200/INVITE rx;
		   put_on_wait
		1.					200/INVITE rx;
		2.									WAIT fires; transaction
											about to be deleted
		3.					avoid putting
							on WAIT again
		4.									WAIT timer executed,
											transaction deleted
	*/
	set_1timer( hash_table, &(Trans->wait_tl), WT_TIMER_LIST );
}



static int kill_transaction( struct cell *trans )
{
	char err_buffer[128];
	int sip_err;
	int reply_ret;
	int ret;

	/*  we reply statefuly and enter WAIT state since error might
		have occured in middle of forking and we do not
		want to put the forking burden on upstream client;
		howver, it may fail too due to lack of memory */

	ret=err2reason_phrase( ser_error, &sip_err,
		err_buffer, sizeof(err_buffer), "TM" );
	if (ret>0) {
		reply_ret=t_reply( trans, trans->uas.request, 
			sip_err, err_buffer);
		/* t_release_transaction( T ); */
		return reply_ret;
	} else {
		LOG(L_ERR, "ERROR: kill_transaction: err2reason failed\n");
		return -1;
	}
}



int t_relay_to( struct sip_msg  *p_msg , struct proxy_l *proxy,
	int replicate)
{
	int ret;
	int new_tran;
	str *uri;
	int reply_ret;
	/* struct hdr_field *hdr; */
	struct cell *t;

	ret=0;

	new_tran = t_newtran( p_msg );
	

	/* parsing error, memory alloc, whatever ... if via is bad
	   and we are forced to reply there, return with 0 (->break),
	   pass error status otherwise
	*/
	if (new_tran<0) {
		ret = (ser_error==E_BAD_VIA && reply_to_via) ? 0 : new_tran;
		goto done;
	}
	/* if that was a retransmission, return we are happily done */
	if (new_tran==0) {
		ret = 1;
		goto done;
	}

	/* new transaction */

	/* ACKs do not establish a transaction and are fwd-ed statelessly */
	if ( p_msg->REQ_METHOD==METHOD_ACK) {
		DBG( "SER: forwarding ACK  statelessly \n");
		if (proxy==0) {
			uri=(p_msg->new_uri.s==0 || p_msg->new_uri.len==0) ?
				&p_msg->first_line.u.request.uri :
				&p_msg->new_uri;
			proxy=uri2proxy( uri );
			if (proxy==0) {
					ret=E_BAD_ADDRESS;
					goto done;
			}
			ret=forward_request( p_msg , proxy ) ;
			free_proxy( proxy );	
			free( proxy );
		} else {
			ret=forward_request( p_msg , proxy ) ;
		}
		goto done;
	}

	/* if replication flag is set, mark the transaction as local
	   so that replies will not be relaied
	*/
	t=get_t();
	t->local=replicate;

	/* INVITE processing might take long, partcularly because of DNS
	   look-ups -- let upstream know we're working on it */
	if (p_msg->REQ_METHOD==METHOD_INVITE )
	{
		DBG( "SER: new INVITE\n");
		if (!t_reply( t, p_msg , 100 ,
			"trying -- your call is important to us"))
				DBG("SER: ERROR: t_reply (100)\n");
	} 

	/* now go ahead and forward ... */
	ret=t_forward_nonack(t, p_msg, proxy);
	if (ret<=0) {
		DBG( "SER:ERROR: t_forward \n");
		reply_ret=kill_transaction( t );
		if (reply_ret>0) {
			/* we have taken care of all -- do nothing in
		  	script */
			DBG("ERROR: generation of a stateful reply "
				"on error succeeded\n");
			ret=0;
		}  else {
			DBG("ERROR: generation of a stateful reply "
				"on error failed\n");
		}
	} else {
		DBG( "SER: new transaction fwd'ed\n");
	}

done:
	return ret;
}

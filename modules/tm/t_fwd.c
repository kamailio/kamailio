/*
 * $Id$
 *
 */

#include "hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser_f.h"
#include "../../ut.h"
#include "../../timer.h"

/* function returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward_nonack( struct sip_msg* p_msg , unsigned int dest_ip_param ,
	unsigned int dest_port_param )
{
	unsigned int        dest_ip = dest_ip_param;
	unsigned int        dest_port = dest_port_param;
	int                  branch;
	unsigned int         len;
	char                *buf, *shbuf;
	struct retrans_buff *rb = 0;
	struct cell         *T_source = T;


	buf=NULL;
	shbuf = NULL;
	branch = 0;	/* we don't do any forking right now */

	if ( T->outbound_request[branch]==NULL )
	{
		DBG("DEBUG: t_forward_nonack: first time forwarding\n");
		/* special case : CANCEL */
		if ( p_msg->REQ_METHOD==METHOD_CANCEL  )
		{
			DBG("DEBUG: t_forward_nonack: it's CANCEL\n");
			/* find original cancelled transaction; if found, use its
			   next-hops; otherwise use those passed by script */
			if ( T->T_canceled==T_UNDEFINED )
				T->T_canceled = t_lookupOriginalT( hash_table , p_msg );
			/* if found */
			if ( T->T_canceled!=T_NULL )
			{
				/* if in 1xx status, send to the same destination */
				if ( (T->T_canceled->status/100)==1 )
				{
					DBG("DEBUG: t_forward_nonack: it's CANCEL and I will send "
						"to the same place where INVITE went\n");
					dest_ip=T->T_canceled->outbound_request[branch]->
						to.sin_addr.s_addr;
					dest_port = T->T_canceled->outbound_request[branch]->
						to.sin_port;
#ifdef USE_SYNONIM
					T_source = T->T_canceled;
					T->label  = T->T_canceled->label;
#endif
				} else { /* transaction exists, but nothing to cancel */
					DBG("DEBUG: t_forward_nonack: it's CANCEL but "
						"I have nothing to cancel here\n");
					/* forward CANCEL as a stand-alone transaction */
				}
			} else { /* transaction doesnot exists  */
				DBG("DEBUG: t_forward_nonack: canceled request not found! "
				"nothing to CANCEL\n");
			}
		}/* end special case CANCEL*/

		if ( add_branch_label( T_source, T->inbound_request , branch )==-1)
			goto error;
		if ( add_branch_label( T_source, p_msg , branch )==-1)
			goto error;
		if ( !(buf = build_req_buf_from_sip_req  ( p_msg, &len)))
			goto error;

		/* allocates a new retrans_buff for the outbound request */
		DBG("DEBUG: t_forward_nonack: building outbound request\n");
		shm_lock();
		rb = (struct retrans_buff*) shm_malloc_unsafe( sizeof(struct retrans_buff)  );
		if (!rb)
		{
			LOG(L_ERR, "ERROR: t_forward_nonack: out of shmem\n");
			shm_unlock();
			goto error;
		}
		shbuf = (char *) shm_malloc_unsafe( len );
		if (!shbuf)
		{
			LOG(L_ERR, "ERROR: t_forward_nonack: out of shmem buffer\n");
			shm_unlock();
			goto error;
		}
		shm_unlock();
		memset( rb , 0 , sizeof (struct retrans_buff) );
		rb->retr_timer.tg=TG_RT;
		rb->fr_timer.tg=TG_FR;
		rb->retr_buffer = shbuf;
		rb->retr_timer.payload =  rb;
		rb->fr_timer.payload =  rb;
		rb->my_T =  T;
		T->nr_of_outgoings = 1;
		rb->bufflen = len ;
		memcpy( rb->retr_buffer , buf , len );
		/* send the request */
		/* known to be in network order */
		rb->to.sin_port     =  dest_port;
		rb->to.sin_addr.s_addr =  dest_ip;
		rb->to.sin_family = AF_INET;
		T->outbound_request[branch] = rb;
		SEND_BUFFER( rb );
		/* link the retransmission buffer to our structures when the job is done */
		pkg_free( buf ) ; buf=NULL;

		DBG("DEBUG: t_forward_nonack: starting timers (retrans and FR) %d\n",get_ticks() );
		/*sets and starts the FINAL RESPONSE timer */
#ifdef FR
		set_timer( hash_table, &(rb->fr_timer), FR_TIMER_LIST );
#endif

		/* sets and starts the RETRANS timer */
		rb->retr_list = RT_T1_TO_1;
		set_timer( hash_table, &(rb->retr_timer), RT_T1_TO_1 );
	}/* end for the first time */ else {
		/* rewriting a request should really not happen -- retransmission
	       does not rewrite, whereas a new request should be written
		   somewhere else
		*/
		LOG( L_CRIT, "ERROR: t_forward_nonack: attempt to rewrite request structures\n");
		return 0;
	}

	if (  p_msg->REQ_METHOD==METHOD_CANCEL )
	{
		DBG("DEBUG: t_forward_nonack: forwarding CANCEL\n");
		/* if no transaction to CANCEL */
		/* or if the canceled transaction has a final status -> drop the CANCEL*/
		if ( T->T_canceled!=T_NULL && T->T_canceled->status>=200)
		{
#ifdef FR
			reset_timer( hash_table, &(rb->fr_timer ));
#endif
			reset_timer( hash_table, &(rb->retr_timer ));
			return 1;
		}
	}
	return 1;

error:
	if (shbuf) shm_free(shbuf);
	if (rb) {
		shm_free(rb);
		T->outbound_request[branch]=NULL;
	}
	if (buf) pkg_free( buf );

	return -1;

}

int t_forward_ack( struct sip_msg* p_msg , unsigned int dest_ip_param ,
										unsigned int dest_port_param )
{
	int branch;
	int len;
	char *buf;
	struct retrans_buff *srb;
#ifdef _DONT_USE
	struct sockaddr_in to_sock;
#endif



	/* drop local ACKs */
	if (T->status/100!=2 ) {
		DBG("DEBUG: local ACK dropped\n");
		return 1;
	}

	branch=T->relaied_reply_branch;
	/* double-check for odd relaying */
	if ( branch <0 || branch>=T->nr_of_outgoings ) {
		DBG("DEBUG: t_forward_ack: strange relaied_reply_branch: %d out of %d\n",
			branch, T->nr_of_outgoings );
		return -1;
	}

	DBG("DEBUG: t_forward_ack: forwarding ACK [%d]\n",branch);
	/* not able to build branch -- then better give up */
	if ( add_branch_label( T, p_msg , branch )==-1) {
		LOG( L_ERR, "ERROR: t_forward_ack failed to add branch label\n" );
		return 0;
	}
	/* not able to build outbound request -- then better give up */
	if ( !(buf = build_req_buf_from_sip_req  ( p_msg, &len)))  {
		LOG( L_ERR, "ERROR: t_forward_ack failed to generate outbound message\n" );
		return 0;
	};

#ifdef _DONT_USE
	/* strange conditions -- no INVITE before me ?!?! */
	if ( (rb=T->outbound_request[branch])==NULL ) {
		/* better stateless than nothing */
		goto fwd_sl;
	}
#endif

	shm_lock();
	/* check for bizzar race condition if two processes receive
	   two ACKs concurrently; use shmem semaphore for protection
	   -- we have to enter it here anyway (the trick with inACKed
	   inside the protection region)
    */
	if  (T->inbound_request_isACKed ) {
		shm_unlock();
		LOG(L_WARN, "Warning: ACK received when there's one; check upstream\n");
		return 1;
	}
	srb = (struct retrans_buff *) shm_malloc_unsafe( sizeof( struct retrans_buff ) + len );
	T->inbound_request_isACKed = 1;
	shm_unlock();

	memcpy( (char *) srb + sizeof ( struct retrans_buff ), buf, len );
	pkg_free( buf );

	relay_ack( T, branch, srb, len );
	return 1;

#ifdef _DON_USE
fwd_sl: /* some strange conditions occured; try statelessly */
	LOG(L_ERR, "ERROR: fwd-ing a 2xx ACK with T-state failed; "
		"trying statelessly\n");
	memset( &to_sock, sizeof to_sock, 0 );
	to_sock.sin_family = AF_INET;
	to_sock.sin_port =  dest_port_param;
	to_sock.sin_addr.s_addr = dest_ip_param;
	udp_send( buf, len, (struct sockaddr*)(&to_sock), 
		sizeof(struct sockaddr_in) );
	free( buf );
	return 1;
#endif
}

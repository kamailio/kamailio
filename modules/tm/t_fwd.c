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
	int          branch,i;
	unsigned int len;
	char         *buf, *shbuf;
	struct cell  *T_source = T;
	struct lump  *a,*b,*b1,*c;


	buf    = 0;
	shbuf  = 0;
	nr_forks++;
	t_forks[0].ip = dest_ip_param;
	t_forks[0].port = dest_port_param;
	t_forks[0].uri.s = p_msg->new_uri.s;
	t_forks[0].uri.len = p_msg->new_uri.len;

	/* are we forwarding for the first time? */
	if ( T->uac[0].request.buffer )
	{	/* rewriting a request should really not happen -- retransmission
		   does not rewrite, whereas a new request should be written
		   somewhere else */
		LOG( L_CRIT, "ERROR: t_forward_nonack: attempt to rewrite"
			" request structures\n");
		return 0;
	}

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
			for(nr_forks=0,i=0;i<T->T_canceled->nr_of_outgoings;i++)
			{
				/* if in 1xx status, send to the same destination */
				if ( (T->T_canceled->uac[i].status/100)==1 )
				{
					DBG("DEBUG: t_forward_nonack: branch %d not finalize"
						": sending CANCEL for it\n",i);
					t_forks[nr_forks].ip =
						T->T_canceled->uac[i].request.to.sin_addr.s_addr;
					t_forks[nr_forks].port =
						T->T_canceled->uac[i].request.to.sin_port;
					t_forks[nr_forks].uri.s = T->T_canceled->uac[i].uri.s;
					t_forks[nr_forks].uri.len = T->T_canceled->uac[i].uri.len;
					nr_forks++;
				}else{
					/* transaction exists, but nothing to cancel */
					DBG("DEBUG: t_forward_nonack: branch %d finalized"
						": no CANCEL sent here\n",i);
				}
			}
#ifdef USE_SYNONIM
			T_source = T->T_canceled;
			T->label  = T->T_canceled->label;
#endif
		} else { /* transaction doesnot exists  */
			DBG("DEBUG: t_forward_nonack: canceled request not found! "
			"nothing to CANCEL\n");
		}
	}/* end special case CANCEL*/

#ifndef USE_SYNONIM
	if ( nr_forks && add_branch_label( T_source, T->uas.request , branch )==-1)
		goto error;
#endif

	DBG("DEBUG: t_forward_nonack: nr_forks=%d\n",nr_forks);
	for(branch=0;branch<nr_forks;branch++)
	{
		DBG("DEBUG: t_forward_nonack: branch = %d\n",branch);
		/*generates branch param*/
		if ( add_branch_label( T_source, p_msg , branch )==-1)
			goto error;
		/* remove all the HDR_VIA type lumps */
		if (branch)
			for(b=p_msg->add_rm,b1=0;b;b1=b,b=b->next)
				if (b->type==HDR_VIA)
				{
					for(a=b->before;a;)
						{c=a->before;free_lump(a);a=c;}
					for(a=b->after;a;)
						{c=a->after;free_lump(a);a=c;}
					if (b1) b1->next = b->next;
						else p_msg->add_rm = b->next;
					free_lump(b);
				}
		/* updates the new uri*/
		p_msg->new_uri.s = t_forks[branch].uri.s;
		p_msg->new_uri.len = t_forks[branch].uri.len;
		if ( !(buf = build_req_buf_from_sip_req  ( p_msg, &len)))
			goto error;
		/* allocates a new retrans_buff for the outbound request */
		DBG("DEBUG: t_forward_nonack: building outbound request"
			" for branch %d.\n",branch);
		shbuf = (char *) shm_malloc( len );
		if (!shbuf)
		{
			LOG(L_ERR, "ERROR: t_forward_nonack: out of shmem buffer\n");
			goto error;
		}
		T->uac[branch].request.buffer = shbuf;
		T->uac[branch].request.buffer_len = len ;
		memcpy( T->uac[branch].request.buffer , buf , len );
		/* keeps a hooker to uri inside buffer*/
		T->uac[branch].uri.s = T->uac[branch].request.buffer +
			(p_msg->first_line.u.request.uri.s - p_msg->buf);
		T->uac[branch].uri.len=t_forks[branch].uri.s?(t_forks[branch].uri.len)
			:(p_msg->first_line.u.request.uri.len);
		DBG("DEBUG: uri= |%.*s| \n",T->uac[branch].uri.len,
			T->uac[branch].uri.s);
		T->nr_of_outgoings++ ;
		/* send the request */
		T->uac[branch].request.to.sin_addr.s_addr = t_forks[branch].ip;
		T->uac[branch].request.to.sin_port = t_forks[branch].port;
		T->uac[branch].request.to.sin_family = AF_INET;
		SEND_BUFFER( &(T->uac[branch].request) );

		pkg_free( buf ) ;
		buf=NULL;

		DBG("DEBUG: t_forward_nonack: starting timers (retrans and FR) %d\n",
			get_ticks() );
		/*sets and starts the FINAL RESPONSE timer */
		set_timer( hash_table, &(T->uac[branch].request.fr_timer),
			FR_TIMER_LIST );
		/* sets and starts the RETRANS timer */
		T->uac[branch].request.retr_list = RT_T1_TO_1;
		set_timer( hash_table, &(T->uac[branch].request.retr_timer),
			RT_T1_TO_1 );
	}

	p_msg->new_uri.s = t_forks[0].uri.s;
	p_msg->new_uri.len = t_forks[0].uri.len;
	nr_forks = 0;
	return 1;

error:
	if (shbuf) shm_free(shbuf);
	T->uac[branch].request.buffer=NULL;
	if (buf) pkg_free( buf );
	nr_forks = 0;
	return -1;
}




int t_forward_ack( struct sip_msg* p_msg , unsigned int dest_ip_param ,
										unsigned int dest_port_param )
{
	int branch;
	unsigned int len;
	char *buf, *ack;
#ifdef _DONT_USE
	struct sockaddr_in to_sock;
#endif


	/* drop local ACKs */
	if (T->uas.status/100!=2 ) {
		DBG("DEBUG: t_forward_ACK:  local ACK dropped\n");
		return 1;
	}

	branch=T->relaied_reply_branch;
	/* double-check for odd relaying */
	if ( branch <0 || branch>=T->nr_of_outgoings ) {
		DBG("DEBUG: t_forward_ack: strange relaied_reply_branch:"
			" %d out of %d\n",branch, T->nr_of_outgoings );
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
		LOG(L_ERR,"ERROR: t_forward_ack failed to generate outbound ACK\n");
		return 0;
	};

#ifdef _DONT_USE
	/* strange conditions -- no INVITE before me ?!?! */
	if ( (rb=T->outbound_request[branch])==NULL ) {
		/* better stateless than nothing */
		goto fwd_sl;
	}
#endif

	/* check for bizzar race condition if two processes receive
	   two ACKs concurrently; use shmem semaphore for protection
	   -- we have to enter it here anyway (the trick with inACKed
	   inside the protection region) */
	if  (T->uas.isACKed ) {
		LOG(L_WARN,"Warning: ACK received when there's one; check upstream\n");
		return 1;
	}
	ack = shm_malloc( len );
	memcpy(ack , buf , len);
	pkg_free( buf );

	T->uas.isACKed = 1;
	SEND_PR_BUFFER( &(T->uac[branch].request), ack, len );
	return attach_ack( T, branch, ack , len );

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

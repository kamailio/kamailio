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
//#include "../../timer.h"




#define  append_mem_block(_d,_s,_len) \
		do{\
			memcpy((_d),(_s),(_len));\
			(_d) += (_len);\
		}while(0);
#define  req_line(_msg) \
		((_msg)->first_line.u.request)



struct cell         *T;
unsigned int     global_msg_id;
struct s_table*  hash_table;

void timer_routine(unsigned int, void*);



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


int t_update_timers_after_sending_reply( struct retrans_buff *rb )
{
	struct cell *Trans = rb->my_T;

	/* make sure that if we send something final upstream, everything else
	   will be cancelled */
	if (Trans->status>=300&&Trans->inbound_request->REQ_METHOD==METHOD_INVITE)
	{
		rb->retr_list = RT_T1_TO_1;
		set_timer( hash_table, &(rb->retr_timer), RT_T1_TO_1 );
		set_timer( hash_table, &(rb->fr_timer), FR_TIMER_LIST );
	} else if ( Trans->inbound_request->REQ_METHOD==METHOD_CANCEL ) {
		if ( Trans->T_canceled==T_UNDEFINED )
			Trans->T_canceled = t_lookupOriginalT( hash_table ,
				Trans->inbound_request );
		if ( Trans->T_canceled==T_NULL )
			return 1;
		/* put CANCEL transaction on wait only if canceled transaction already
		    is in final status and there is nothing to cancel; */
		if ( Trans->T_canceled->status>=200)
			t_put_on_wait( Trans );
	} else if (Trans->status>=200)
		t_put_on_wait( Trans );
   return 1;
}




/* Checks if the new reply (with new_code status) should be sent or not
 *  based on the current
 * transactin status.
 * Returns 	- branch number (0,1,...) which should be relayed
 *         -1 if nothing to be relayed
 */
int t_should_relay_response( struct cell *Trans , int new_code, 
	int branch , int *should_store )
{
	int T_code;
	int b, lowest_b, lowest_s;

	T_code = Trans->status;

	/* note: this code never lets replies to CANCEL go through;
	   we generate always a local 200 for CANCEL; 200s are
	   not relayed because it's not an INVITE transaction;
	   >= 300 are not relayed because 200 was already sent
	   out
	*/

	/* if final response sent out, allow only INVITE 2xx  */
	if ( T_code >= 200 ) { 
		if (new_code>=200 && new_code < 300  && 
			Trans->inbound_request->REQ_METHOD==METHOD_INVITE) {
			DBG("DBG: t_should_relay: 200 INV after final sent\n");
			*should_store=1;
			return branch;
		} else {
			*should_store=0;
			return -1;
		}
	} else { /* no final response sent yet */
		/* negative replies subject to fork picking */
		if (new_code >=300 ) {
			*should_store=1;
			/* if all_final return lowest */
			lowest_b=-1; lowest_s=999;
			for ( b=0; b<Trans->nr_of_outgoings ; b++ ) {
				/* "fake" for the currently processed branch */
				if (b==branch) {
					if (new_code<lowest_s) {
						lowest_b=b;
						lowest_s=new_code;
					}
					continue;
				}
				/* there is still an unfinished UAC transaction; wait now! */
				if ( !Trans->inbound_response[b] ||
					Trans->inbound_response[b]->REPLY_STATUS<200 )
					return -1;
				if ( Trans->inbound_response[b]->REPLY_STATUS<lowest_s )
				{
					lowest_b =b;
					lowest_s = T->inbound_response[b]->REPLY_STATUS;
				}
			}
			return lowest_b;
		/* 1xx except 100 and 2xx will be relayed */
		} else if (new_code>100) {
			*should_store=1;
			return branch;
		}
		/* 100 won't be relayed */
		else {
			if (!T->inbound_response[branch]) *should_store=1;
			else *should_store=0;
			return -1;
		}
	}

	LOG(L_CRIT, "ERROR: Oh my gooosh! We don't know whether to relay\n");
	abort();
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

	LOCK_WAIT(T);
	if (Trans->on_wait)
	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		UNLOCK_WAIT(T);
		return 1;
	} else {
		Trans->on_wait=1;
		UNLOCK_WAIT(T);
	};
#endif


	/* remove from  retranssmision  and  final response   list */
	DBG("DEBUG: t_put_on_wait: stopping timers (FR and RETR)\n");
	reset_retr_timers(hash_table,Trans) ;

	/* cancel pending client transactions, if any */
	for( i=0 ; i<Trans->nr_of_outgoings ; i++ )
		if ( Trans->inbound_response[i] && 
		REPLY_CLASS(Trans->inbound_response[i])==1)
			t_cancel_branch(i);


	/* we don't need outbound requests anymore -- let's save
	   memory and junk them right now!
	*/
/*
	shm_lock();
	for ( i =0 ; i<Trans->nr_of_outgoings;  i++ )
	{
		if ( rb=Trans->outbound_request[i] )
		{
			if (rb->retr_buffer) shm_free_unsafe( rb->retr_buffer );
			Trans->outbound_request[i] = NULL;
			shm_free_unsafe( rb );
		}
	}
	shm_unlock();
*/

	/* adds to Wait list*/
	set_timer( hash_table, &(Trans->wait_tl), WT_TIMER_LIST );
	return 1;
}




/*
  */
int t_cancel_branch(unsigned int branch)
{
	LOG(L_ERR, "ERROR: t_cancel_branch: NOT IMPLEMENTED YET\n");
	return 1;
}



#ifdef _REALLY_TOO_OLD
/* Builds an ACK request based on an INVITE request. ACK is send
  * to same address */
int t_build_and_send_ACK(struct cell *Trans,unsigned int branch,
														struct sip_msg* rpl)
{
	struct sip_msg      *p_msg , *r_msg;
	struct hdr_field    *hdr;
	char                *ack_buf, *p, *via;
	unsigned int         len, via_len;
	int                  n;
	struct retrans_buff *srb;

	ack_buf = 0;
	via =0;
	p_msg = Trans->inbound_request;
	r_msg = rpl;

	if ( parse_headers(rpl,HDR_TO)==-1 || !rpl->to )
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: "
			"cannot generate a HBH ACK if key HFs in reply missing\n");
		goto error;
	}

	len = 0;
	/*first line's len */
	len += 4/*reply code and one space*/+
		p_msg->first_line.u.request.version.len+CRLF_LEN;
	/*uri's len*/
	if (p_msg->new_uri.s)
		len += p_msg->new_uri.len +1;
	else
		len += p_msg->first_line.u.request.uri.len +1;
	/*via*/
	via = via_builder( p_msg , &via_len );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_CSEQ)
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
		else if ( hdr->type==HDR_TO )
			len += ((r_msg->to->body.s+r_msg->to->body.len ) -
				r_msg->to->name.s ) + CRLF_LEN ;
	/* CSEQ method : from INVITE-> ACK */
	len -= 3  ;
	/* end of message */
	len += CRLF_LEN; /*new line*/

	srb=(struct retrans_buff*)sh_malloc(sizeof(struct retrans_buff)+len+1);
	if (!srb)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: cannot allocate memory\n");
		goto error1;
	}
	ack_buf = (char *) srb + sizeof(struct retrans_buff);
	p = ack_buf;

	/* first line */
	memcpy( p , "ACK " , 4);
	p += 4;
	/* uri */
	if ( p_msg->new_uri.s )
	{
		memcpy(p,p_msg->orig+(p_msg->new_uri.s-p_msg->buf),p_msg->new_uri.len);
		p +=p_msg->new_uri.len;
	}else{
		memcpy(p,p_msg->orig+(p_msg->first_line.u.request.uri.s-p_msg->buf),
			p_msg->first_line.u.request.uri.len );
		p += p_msg->first_line.u.request.uri.len;
	}
	/* SIP version */
	*(p++) = ' ';
	memcpy(p,p_msg->orig+(p_msg->first_line.u.request.version.s-p_msg->buf),
		p_msg->first_line.u.request.version.len );
	p += p_msg->first_line.u.request.version.len;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;

	/* insert our via */
	memcpy( p , via , via_len );
	p += via_len;

	/*other headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
	{
		if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID  )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((hdr->body.s+hdr->body.len ) - hdr->name.s ) );
			p += ((hdr->body.s+hdr->body.len ) - hdr->name.s );
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_TO )
		{
			memcpy( p , r_msg->orig+(r_msg->to->name.s-r_msg->buf) ,
				((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s));
			p+=((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s);
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_CSEQ )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			p+=((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s);
			memcpy( p , "ACK" CRLF, 3+CRLF_LEN );
			p += 3+CRLF_LEN;
		}
	}

	/* end of message */
	memcpy( p , CRLF , CRLF_LEN );
	p += CRLF_LEN;

	send_ack( T, branch, srb, p-ack_buf );
	pkg_free( via );
	DBG("DEBUG: t_build_and_send_ACK: ACK sent\n");
	return 0;

error1:
	pkg_free(via );
error:
	return -1;
}

#endif /* _REALLY_TOO_OLD */





/* Builds a CANCEL request based on an INVITE request. CANCEL is send
 * to same address as the INVITE */
int t_build_and_send_CANCEL(struct cell *Trans,unsigned int branch)
{
	struct sip_msg      *p_msg;
	struct hdr_field    *hdr;
	char                *cancel_buf, *p, *via;
	unsigned int         len, via_len;
	struct retrans_buff *srb;


	if ( !Trans->inbound_response[branch] )
	{
		DBG("DEBUG: t_build_and_send_CANCEL: no response ever received"
			" : dropping local cancel! \n");
		return 1;
	}

	if (Trans->outbound_cancel[branch]!=NO_CANCEL)
	{
		DBG("DEBUG: t_build_and_send_CANCEL: this branch was already canceled"
			" : dropping local cancel! \n");
		return 1;
	}

	cancel_buf = 0;
	via = 0;
	p_msg = Trans->inbound_request;

	len = 0;
	/*first line's len - CANCEL and INVITE has the same lenght */
	len += ( req_line(p_msg).version.s+req_line(p_msg).version.len)-
		req_line(p_msg).method.s+CRLF_LEN;
	/*check if the REQ URI was override */
	if (p_msg->new_uri.s)
		len += p_msg->new_uri.len - req_line(p_msg).uri.len;
	/*via*/
	via = via_builder( p_msg , &via_len );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM || hdr->type==HDR_CALLID || 
			hdr->type==HDR_CSEQ || hdr->type==HDR_TO )
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
	/* User Agent header*/
	len += USER_AGENT_LEN + CRLF_LEN;
	/* Content Lenght heder*/
	len += CONTENT_LEN_LEN + CRLF_LEN;
	/* end of message */
	len += CRLF_LEN;

	srb=(struct retrans_buff*)sh_malloc(sizeof(struct retrans_buff)+len+1);
	if (!srb)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: cannot allocate memory\n");
		goto error;
	}
	cancel_buf = (char*) srb + sizeof(struct retrans_buff);
	p = cancel_buf;

	/* first line -> do we have a new URI? */
	if (p_msg->new_uri.s)
	{
		append_mem_block(p,req_line(p_msg).method.s,
			req_line(p_msg).uri.s-req_line(p_msg).method.s);
		append_mem_block(p,p_msg->new_uri.s,p_msg->new_uri.len);
		append_mem_block(p,req_line(p_msg).uri.s+req_line(p_msg).uri.len,
			req_line(p_msg).version.s+req_line(p_msg).version.len-
			(req_line(p_msg).uri.s+req_line(p_msg).uri.len))
	}else{
	append_mem_block(p,req_line(p_msg).method.s,
		req_line(p_msg).version.s+req_line(p_msg).version.len-
		req_line(p_msg).method.s);
	}
	/* changhing method name*/
	memcpy(cancel_buf,"CANCEL",6);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* insert our via */
	append_mem_block(p,via,via_len);

	/*other headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
	{
		if(hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_TO)
		{
			append_mem_block(p,hdr->name.s,
				((hdr->body.s+hdr->body.len)-hdr->name.s) );
			append_mem_block(p, CRLF, CRLF_LEN );
		}else if ( hdr->type==HDR_CSEQ )
		{
			append_mem_block(p,hdr->name.s,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			append_mem_block(p,"CANCEL" CRLF, 6+CRLF_LEN );
		}
}

	/* User Agent header */
	append_mem_block(p,USER_AGENT,USER_AGENT_LEN);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* Content Lenght header*/
	append_mem_block(p,CONTENT_LEN,CONTENT_LEN_LEN);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* end of message */
	append_mem_block(p,CRLF,CRLF_LEN);
	*p=0;

	memset( srb, 0, sizeof( struct retrans_buff ) );
	memcpy( & srb->to, & Trans->outbound_request[branch]->to,
		sizeof (struct sockaddr_in));
	srb->tolen = sizeof (struct sockaddr_in);
	srb->my_T = Trans;
	srb->fr_timer.payload = srb->retr_timer.payload = srb;
	srb->branch = branch;
	srb->status = STATUS_LOCAL_CANCEL;
	srb->retr_buffer = (char *) srb + sizeof( struct retrans_buff );
	srb->bufflen = len;
	if (Trans->outbound_cancel[branch]) {
		shm_free( srb );	
		LOG(L_WARN, "send_cancel: Warning: CANCEL already sent out\n");
		goto error;
	}
	Trans->outbound_cancel[branch] = srb;
	/*sets and starts the FINAL RESPONSE timer */
	set_timer( hash_table, &(srb->fr_timer), FR_TIMER_LIST );

	/* sets and starts the RETRANS timer */
	srb->retr_list = RT_T1_TO_1;
	set_timer( hash_table, &(srb->retr_timer), RT_T1_TO_1 );
	DBG("DEBUG: T_build_and_send_CANCEL : sending cancel...\n");
	SEND_BUFFER( srb );

	pkg_free(via);
	return 1;
error:
	if (via) pkg_free(via);
	return -1;
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




/* Returns  -1 = error
                    0 = OK
*/
int get_ip_and_port_from_uri( struct sip_msg* p_msg , unsigned int *param_ip, unsigned int *param_port)
{
	struct hostent  *nhost;
	unsigned int      ip, port;
	struct sip_uri    parsed_uri;
	str                      uri;
	int                      err;
#ifdef DNS_IP_HACK
	int                      len;
#endif

	/* the original uri has been changed? */
	if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
		uri = p_msg->first_line.u.request.uri;
	else
		uri = p_msg->new_uri;

	/* parsing the request uri in order to get host and port */
	if (parse_uri( uri.s , uri.len , &parsed_uri )<0)
	{
		LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: "
		   "unable to parse destination uri: %.*s\n", uri.len, uri.s );
		goto error;
	}

	/* getting the port */
	if ( parsed_uri.port.s==0 || parsed_uri.port.len==0 )
		port = SIP_PORT;
	else{
		port = str2s( (unsigned char*) parsed_uri.port.s, parsed_uri.port.len,
						&err );
		if ( err<0 ){
			LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: converting port "
				"from str to int failed; using default SIP port\n\turi:%.*s\n",
				uri.len, uri.s );
			port = SIP_PORT;
		}
	}
	port = htons( port );

	/* getting host address*/
#ifdef DNS_IP_HACK
	len=strlen( parsed_uri.host.s );
	ip=str2ip( (unsigned char*)parsed_uri.host.s, len, &err);
	if (err==0)
		goto success;
#endif
	/* fail over to normal lookup */
	nhost = gethostbyname( parsed_uri.host.s );
	if ( !nhost )
	{
		LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: "
		  "cannot resolve host in uri: %.*s\n", uri.len, uri.s );
		free_uri(&parsed_uri);
		goto error;
	}
	memcpy(&ip, nhost->h_addr_list[0], sizeof(unsigned int));


success:
	free_uri(&parsed_uri);
	*param_ip = ip;
	*param_port = port;
	return 0;

error:
	*param_ip = 0;
	*param_port = 0;
	return -1;
}






/*---------------------------TIMERS FUNCTIONS-------------------------------*/





inline void retransmission_handler( void *attr)
{
	struct retrans_buff* r_buf ;
	enum lists id;
	DBG("DEBUG: entering retransmisson with attr = %p\n",attr);
	r_buf = (struct retrans_buff*)attr;
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
	if (r_buf->status!=STATUS_LOCAL_CANCEL && r_buf->status!=STATUS_REQUEST){
		T=r_buf->my_T;
		t_retransmit_reply();
	}else{
		SEND_BUFFER( r_buf );
	}

	id = r_buf->retr_list;
	r_buf->retr_list = id < RT_T2 ? id + 1 : RT_T2;

	set_timer(hash_table,&(r_buf->retr_timer),id < RT_T2 ? id + 1 : RT_T2 );

	DBG("DEBUG: retransmission_handler : done\n");
}




inline void final_response_handler( void *attr)
{
	struct retrans_buff* r_buf = (struct retrans_buff*)attr;

#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) 
	{
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from FR timer\n",r_buf->my_T);
		abort();
	}
#endif

	/* the transaction is already removed from FR_LIST by the timer */
	if (r_buf->status==STATUS_LOCAL_CANCEL)
	{
		DBG("DEBUG: FR_handler: stop retransmission for Local Cancel\n");
		reset_timer( hash_table , &(r_buf->retr_timer) );
		return;
	}
	/* send a 408 */
	if ( r_buf->my_T->status<200)
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
		global_msg_id=T->inbound_request->id;
		DBG("DEBUG: FR_handler: send 408 (%p)\n", r_buf->my_T);
		t_send_reply( r_buf->my_T->inbound_request,408,"Request Timeout" );
	}else{
		/* put it on WT_LIST - transaction is over */
		DBG("DEBUG: final_response_handler:cancel transaction->put on wait"
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







#ifndef _REALLY_TOO_OLD

/* Builds an ACK request based on an INVITE request. ACK is send
  * to same address */
struct retrans_buff *build_ack( struct sip_msg* rpl, struct cell *trans, int branch )
{
	struct sip_msg      *p_msg , *r_msg;
	struct hdr_field    *hdr;
	char                *ack_buf, *p, *via;
	unsigned int         len, via_len;
	struct retrans_buff *srb;

	ack_buf = 0;
	via =0;
	p_msg = trans->inbound_request;
	r_msg = rpl;

	if ( parse_headers(rpl,HDR_TO)==-1 || !rpl->to )
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: "
			"cannot generate a HBH ACK if key HFs in reply missing\n");
		goto error;
	}

	len = 0;
	/*first line's len */
	len += 4/*reply code and one space*/+
		p_msg->first_line.u.request.version.len+CRLF_LEN;
	/*uri's len*/
	if (p_msg->new_uri.s)
		len += p_msg->new_uri.len +1;
	else
		len += p_msg->first_line.u.request.uri.len +1;
	/*via*/
	via = via_builder( p_msg , &via_len );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_CSEQ)
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
		else if ( hdr->type==HDR_TO )
			len += ((r_msg->to->body.s+r_msg->to->body.len ) -
				r_msg->to->name.s ) + CRLF_LEN ;
	/* CSEQ method : from INVITE-> ACK */
	len -= 3  ;
	/* end of message */
	len += CRLF_LEN; /*new line*/

	srb=(struct retrans_buff*)sh_malloc(sizeof(struct retrans_buff)+len+1);
	if (!srb)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: cannot allocate memory\n");
		goto error1;
	}
	ack_buf = (char *) srb + sizeof(struct retrans_buff);
	p = ack_buf;

	/* first line */
	memcpy( p , "ACK " , 4);
	p += 4;
	/* uri */
	if ( p_msg->new_uri.s )
	{
		memcpy(p,p_msg->orig+(p_msg->new_uri.s-p_msg->buf),p_msg->new_uri.len);
		p +=p_msg->new_uri.len;
	}else{
		memcpy(p,p_msg->orig+(p_msg->first_line.u.request.uri.s-p_msg->buf),
			p_msg->first_line.u.request.uri.len );
		p += p_msg->first_line.u.request.uri.len;
	}
	/* SIP version */
	*(p++) = ' ';
	memcpy(p,p_msg->orig+(p_msg->first_line.u.request.version.s-p_msg->buf),
		p_msg->first_line.u.request.version.len );
	p += p_msg->first_line.u.request.version.len;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;

	/* insert our via */
	memcpy( p , via , via_len );
	p += via_len;

	/*other headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
	{
		if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID  )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((hdr->body.s+hdr->body.len ) - hdr->name.s ) );
			p += ((hdr->body.s+hdr->body.len ) - hdr->name.s );
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_TO )
		{
			memcpy( p , r_msg->orig+(r_msg->to->name.s-r_msg->buf) ,
				((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s));
			p+=((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s);
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_CSEQ )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			p+=((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s);
			memcpy( p , "ACK" CRLF, 3+CRLF_LEN );
			p += 3+CRLF_LEN;
		}
	}

	/* end of message */
	memcpy( p , CRLF , CRLF_LEN );
	p += CRLF_LEN;

	/* fill in the structure */
	srb->bufflen = p-ack_buf;
	srb->tolen = sizeof( struct sockaddr_in );
	srb->my_T = trans;
	srb->retr_buffer = (char *) srb + sizeof( struct retrans_buff );
	memcpy( &srb->to, & trans->outbound_request[ branch ]->to, sizeof (struct sockaddr_in));

	pkg_free( via );
	DBG("DEBUG: t_build_and_send_ACK: ACK sent\n");
	return srb;

error1:
	pkg_free(via );
error:
	return 0;
}

#endif



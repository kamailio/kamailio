/*
 * $Id$
 *
 * transaction maintenance functions
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
 * History:
 * -------
 *  2003-03-31  200 for INVITE/UAS resent even for UDP (jiri)
 *               info only if compiling w/ -DEXTRA_DEBUG (andrei)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-13  send_pr_buffer is called w/ file/function/line debugging
 *  2003-03-01  start_retr changed to retransmit only for UDP
 *  2003-02-13  modified send_pr_buffer to use msg_send & rb->dst (andrei)
 *  2003-04-14  use protocol from uri (jiri)
 *  2003-04-25  do it (^) really everywhere (jiri)
 *  2003-04-26  do it (^) really really really everywhere (jiri)
 *  2003-07-07  added get_proto calls when proxy!=0 (andrei)
 *  2004-02-13  t->is_invite and t->local replaced with flags (bogdan)
 *  2004-02-18  t_write_req imported from vm module (bogdan)
 */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/uio.h>

#include "../../dprint.h"
#include "../../config.h"
#include "../../usr_avp.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_nameaddr.h"
#include "../../parser/contact/parse_contact.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "../../dset.h"
#include "../../mem/mem.h"
#include "defs.h"
#include "t_funcs.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "config.h"
#include "t_stats.h"
#include "t_reply.h"





/* ----------------------------------------------------- */
int send_pr_buffer(	struct retr_buf *rb, void *buf, int len
#ifdef EXTRA_DEBUG
						, char* file, char *function, int line
#endif
					)
{
	if (buf && len && rb )
		return msg_send( rb->dst.send_sock, rb->dst.proto, &rb->dst.to,
				         rb->dst.proto_reserved1, buf, len);
	else {
#ifdef EXTRA_DEBUG
		LOG(L_CRIT, "ERROR: send_pr_buffer: sending an empty buffer"
				"from %s: %s (%d)\n", file, function, line );
#else
		LOG(L_CRIT, "ERROR: send_pr_buffer: attempt to send an "
				"empty buffer\n");
#endif
		return -1;
	}
}

#ifdef _OBSOLETED
void start_retr( struct retr_buf *rb )
{
	if (rb->dst.proto==PROTO_UDP) {
		rb->retr_list=RT_T1_TO_1;
		set_timer( &rb->retr_timer, RT_T1_TO_1 );
	}
	set_timer( &rb->fr_timer, FR_TIMER_LIST );
}
#endif





void tm_shutdown()
{

	DBG("DEBUG: tm_shutdown : start\n");
	unlink_timer_lists();

	/* destroy the hash table */
	DBG("DEBUG: tm_shutdown : empting hash table\n");
	free_hash_table( );
	DBG("DEBUG: tm_shutdown : releasing timers\n");
	free_timer_table();
	DBG("DEBUG: tm_shutdown : removing semaphores\n");
	lock_cleanup();
	DBG("DEBUG: tm_shutdown : destroing tmcb lists\n");
	destroy_tmcb_lists();
	free_tm_stats();
	DBG("DEBUG: tm_shutdown : done\n");
}


/*   returns 1 if everything was OK or -1 for error
*/
int t_release_transaction( struct cell *trans )
{
	set_kr(REQ_RLSD);

	reset_timer( & trans->uas.response.fr_timer );
	reset_timer( & trans->uas.response.retr_timer );

	cleanup_uac_timers( trans );
	
	put_on_wait( trans );
	return 1;
}


/* ----------------------------HELPER FUNCTIONS-------------------------------- */


/*
  */
void put_on_wait(  struct cell  *Trans  )
{

#ifdef EXTRA_DEBUG
	DBG("DEBUG: put on WAIT \n");
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
	set_1timer( &Trans->wait_tl, WT_TIMER_LIST );
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



int t_relay_to( struct sip_msg  *p_msg , struct proxy_l *proxy, int proto,
				int replicate)
{
	int ret;
	int new_tran;
	str *uri;
	int reply_ret;
	/* struct hdr_field *hdr; */
	struct cell *t;
#ifdef ACK_FORKING_HACK
	str ack_uri;
	str backup_uri;
#endif

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
			uri = GET_RURI(p_msg);
			proxy=uri2proxy(GET_NEXT_HOP(p_msg), proto);
			if (proxy==0) {
					ret=E_BAD_ADDRESS;
					goto done;
			}
			proto=proxy->proto; /* uri2proxy set it correctly */
			ret=forward_request( p_msg , proxy, proto) ;
			free_proxy( proxy );	
			pkg_free( proxy );
#ifdef ACK_FORKING_HACK
			backup_uri=p_msg->new_uri;
			init_branch_iterator();
			while((ack_uri.s=next_branch(&ack_uri.len))) {
				p_msg->new_uri=ack_uri;
				proxy=uri2proxy(GET_NEXT_HOP(p_msg), proto);
				if (proxy==0) continue;
				forward_request(p_msg, proxy, proxy->proto); /* ok, uri2proxy*/
				free_proxy( proxy );	
				pkg_free( proxy );
			}
			p_msg->new_uri=backup_uri;
#endif
		} else {
			proto=get_proto(proto, proxy->proto);
			ret=forward_request( p_msg , proxy, proto ) ;
#ifdef ACK_FORKING_HACK
			backup_uri=p_msg->new_uri;
			init_branch_iterator();
			while((ack_uri.s=next_branch(&ack_uri.len))) {
				p_msg->new_uri=ack_uri;
				forward_request(p_msg, proxy, proto);
			}
			p_msg->new_uri=backup_uri;
#endif
		}
		goto done;
	}

	/* if replication flag is set, mark the transaction as local
	   so that replies will not be relaied */
	t=get_t();
	if (replicate) t->flags|=T_IS_LOCAL_FLAG;

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
	ret=t_forward_nonack(t, p_msg, proxy, proto);
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

/********************   T_WRITE_REQ function   **************************/

#define TWRITE_FROM_PARSED     (1<<1)
#define TWRITE_CONTACT_PARSED  (1<<2)
#define TWRITE_RR_PARSED       (1<<3)
#define TWRITE_RRTMP_PARSED    (1<<4)

#define TWRITE_PARAMS          21
#define TWRITE_VERSION_S       "0.2"
#define TWRITE_VERSION_LEN     (sizeof(TWRITE_VERSION_S)-1)
#define eol_line(_i_)          ( lines_eol[2*(_i_)] )

#define IDBUF_LEN              128
#define ROUTE_BUFFER_MAX       512
#define HDRS_BUFFER_MAX        512
#define CMD_BUFFER_MAX         128

#define append_str(_dest,_src,_len) \
	do{ \
		memcpy( (_dest) , (_src) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

#define append_chr(_dest,_c) \
	*((_dest)++) = _c;

#define copy_route(s,len,rs,rlen) \
	do {\
		if(rlen+len+3 >= ROUTE_BUFFER_MAX){\
			LOG(L_ERR,"vm: buffer overflow while copying new route\n");\
			goto error3;\
		}\
		if(len){\
			append_chr(s,','); len++;\
		}\
		append_chr(s,'<');len++;\
		append_str(s,rs,rlen);\
		len += rlen; \
		append_chr(s,'>');len++;\
	} while(0)

static str   lines_eol[2*TWRITE_PARAMS];
static str   eol={"\n",1};



int init_twrite_lines()
{
	int i;

	/* init the line table */
	for(i=0;i<TWRITE_PARAMS;i++) {
		lines_eol[2*i].s = 0;
		lines_eol[2*i].len = 0;
		lines_eol[2*i+1] = eol;
	}

	/* first line is the version - fill it now */
	eol_line(0).s   = TWRITE_VERSION_S;
	eol_line(0).len = TWRITE_VERSION_LEN;

	return 0;
}


static int inline write_to_fifo(char *fifo, int cnt )
{
	int   fd_fifo;

	/* open FIFO file stream */
	if((fd_fifo = open(fifo,O_WRONLY | O_NONBLOCK)) == -1){
		switch(errno){
			case ENXIO:
				LOG(L_ERR,"ERROR:tm:t_write_req: nobody listening on "
					" [%s] fifo for reading!\n",fifo);
			default:
				LOG(L_ERR,"ERROR:tm:t_write_req: failed to open [%s] "
					"fifo : %s\n", fifo, strerror(errno));
		}
		goto error;
	}

	/* write now (unbuffered straight-down write) */
repeat:
	if (writev(fd_fifo, (struct iovec*)lines_eol, 2*cnt)<0) {
		if (errno!=EINTR) {
			LOG(L_ERR, "ERROR:tm:t_write_req: writev failed: %s\n",
				strerror(errno));
			close(fd_fifo);
			goto error;
		} else {
			goto repeat;
		}
	}
	close(fd_fifo);

	DBG("DEBUG:tm:t_write_req: write completed\n");
	return 1; /* OK */

error:
	return -1;
}


int t_write_req(struct sip_msg* msg, char* vm_fifo, char* action)
{
	static char     id_buf[IDBUF_LEN];
	static char     route_buffer[ROUTE_BUFFER_MAX];
	static char     hdrs_buf[HDRS_BUFFER_MAX];
	static char     cmd_buf[CMD_BUFFER_MAX];
	static str      empty_param = {".",1};
	static str      email_attr = {"email",5};
	unsigned int      hash_index;
	unsigned int      label;
	contact_body_t*   cb=0;
	contact_t*        c=0;
	name_addr_t       na;
	rr_t*             record_route;
	struct hdr_field* p_hdr;
	param_hooks_t     hooks;
	struct usr_avp    *email_avp;
	str               body;
	str               str_uri;
	int               l;
	char*             s;
	char              fproxy_lr;
	str               route;
	str               next_hop;
	str               hdrs;
	int               parse_flags;
	int               ret;
	str               tmp_s;

	ret = -1;

	if(msg->first_line.type != SIP_REQUEST){
		LOG(L_ERR,"ERROR:tm:t_write_req: called for something else then"
			"a SIP request\n");
		goto error;
	}

	parse_flags = 0;
	email_avp = 0;
	body = empty_param;

	/* parse all -- we will need every header field for a UAS
	 * avoid parsing if in FAILURE_ROUTE - all hdr have already been parsed */
	if ( rmode==MODE_REQUEST && parse_headers(msg, HDR_EOH, 0)==-1) {
		LOG(L_ERR,"ERROR:tm:t_write_req: parse_headers failed\n");
		goto error;
	}

	/* find index and hash; (the transaction can be safely used due 
	 * to refcounting till script completes) */
	if( t_get_trans_ident(msg,&hash_index,&label) == -1 ) {
		LOG(L_ERR,"ERROR:tm:t_write_req: t_get_trans_ident failed\n");
		goto error;
	}

	/* if in FAILURE_MODE, we have to remember if the from will be parsed by us
	 * or was already parsed - if it's parsed by us, free it at the end */
	if (msg->from->parsed==0) {
		if (rmode==MODE_ONFAILURE )
			parse_flags |= TWRITE_FROM_PARSED;
		if(parse_from_header(msg) == -1){
			LOG(L_ERR,"ERROR:tm:t_write_req:while parsing <From:> header\n");
			goto error;
		}
	}

	/* parse the RURI (doesn't make any malloc) */
	msg->parsed_uri_ok = 0; /* force parsing */
	if (parse_sip_msg_uri(msg)<0) {
		LOG(L_ERR,"ERROR:tm:t_write_req: vm: uri has not been parsed\n");
		goto error1;
	}

	/* parse contact header */
	str_uri.s = 0;
	str_uri.len = 0;
	if(msg->contact) {
		if (msg->contact->parsed==0) {
			if (rmode==MODE_ONFAILURE)
				parse_flags |= TWRITE_CONTACT_PARSED;
			if( parse_contact(msg->contact) == -1) {
				LOG(L_ERR,"ERROR:tm:t_write_req: error while parsing "
						"<Contact:> header\n");
				goto error1;
			}
		}
#ifdef EXTRA_DEBUG
		DBG("DEBUG: vm:msg->contact->parsed ******* contacts: *******\n");
#endif
		cb = (contact_body_t*)msg->contact->parsed;
		if(cb && (c=cb->contacts)) {
			str_uri = c->uri;
			if (find_not_quoted(&str_uri,'<')) {
				parse_nameaddr(&str_uri,&na);
				str_uri = na.uri;
			}
#ifdef EXTRA_DEBUG
			/*print_contacts(c);*/
			for(; c; c=c->next)
				DBG("DEBUG:           %.*s\n",c->uri.len,c->uri.s);
#endif
		}
#ifdef EXTRA_DEBUG
		DBG("DEBUG:tm:t_write_req: **** end of contacts ****\n");
#endif
	}

	/* str_uri is taken from caller's contact or from header
	 * for backwards compatibility with pre-3261 (from is already parsed)*/
	if(!str_uri.len || !str_uri.s)
		str_uri = get_from(msg)->uri;

	/* parse Record-Route headers */
	route.s = s = route_buffer; route.len = 0;
	fproxy_lr = 0;
	next_hop = empty_param;

	p_hdr = msg->record_route;
	if(p_hdr) {
		if (p_hdr->parsed==0) {
			if (rmode==MODE_ONFAILURE)
				parse_flags |= TWRITE_RR_PARSED;
			if ( parse_rr(p_hdr) ) {
				LOG(L_ERR,"ERROR:tm:t_write_req: while parsing "
					"'Record-Route:' header\n");
				goto error2;
			}
		}
		record_route = (rr_t*)p_hdr->parsed;
	} else {
		record_route = 0;
	}

	if( record_route ) {
		if ( (tmp_s.s=find_not_quoted(&record_route->nameaddr.uri,';'))!=0 &&
		tmp_s.s+1!=record_route->nameaddr.uri.s+
		record_route->nameaddr.uri.len) {
			/* Parse all parameters */
			tmp_s.len = record_route->nameaddr.uri.len - (tmp_s.s-
				record_route->nameaddr.uri.s);
			if (parse_params( &tmp_s, CLASS_URI, &hooks, 
			&record_route->params) < 0) {
				LOG(L_ERR,"ERROR:tm:t_write_req: error while parsing "
					"record route uri params\n");
				goto error3;
			}
			fproxy_lr = (hooks.uri.lr != 0);
			DBG("DEBUG:tm:t_write_req: record_route->nameaddr.uri: %.*s\n",
				record_route->nameaddr.uri.len,record_route->nameaddr.uri.s);
			if(fproxy_lr){
				DBG("DEBUG:tm:t_write_req: first proxy has loose routing.\n");
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
		}
		for(p_hdr = p_hdr->next;p_hdr;p_hdr = p_hdr->next) {
			/* filter out non-RR hdr and empty hdrs */
			if( (p_hdr->type!=HDR_RECORDROUTE) || p_hdr->body.len==0)
				continue;

			if(p_hdr->parsed==0) {
				/* if we are in failure route and we have to parse,
				 * remember to free before exiting */
				if (rmode==MODE_ONFAILURE)
					parse_flags |= TWRITE_RRTMP_PARSED;
				if ( parse_rr(p_hdr) ){
					LOG(L_ERR,"ERROR:tm:t_write_req: "
						"while parsing <Record-route:> header\n");
					goto error3;
				}
			}
			for(record_route=p_hdr->parsed; record_route;
			record_route=record_route->next){
				DBG("DEBUG:tm:t_write_req: record_route->nameaddr.uri: %.*s\n",
					record_route->nameaddr.uri.len,
					record_route->nameaddr.uri.s);
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
			if (parse_flags&TWRITE_RRTMP_PARSED)
				free_rr( ((rr_t**)&p_hdr->parsed) );
		}

		if(!fproxy_lr){
			copy_route(s,route.len,str_uri.s,str_uri.len);
			str_uri = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
		} else {
			next_hop = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
		}
	}

	DBG("DEBUG:tm:t_write_req: calculated route: %.*s\n",
		route.len,route.len ? route.s : "");
	DBG("DEBUG:tm:t_write_req: next r-uri: %.*s\n",
		str_uri.len,str_uri.len ? str_uri.s : "");

	if( REQ_LINE(msg).method_value==METHOD_INVITE ) {
		/* get body */
		if( (body.s = get_body(msg)) == 0 ){
			LOG(L_ERR, "ERROR:tm:t_write_req: get_body failed\n");
			goto error3;
		}
		body.len = msg->len - (body.s - msg->buf);

		/* get email (if any) */
		if ( (email_avp=search_avp( &email_attr ))!=0 &&
		email_avp->val_type!=AVP_TYPE_STR ) {
			LOG(L_WARN, "WARNING:tm:t_write_req: 'email' avp found but "
				"not string -> ignoring it\n");
			email_avp = 0;
		}
	}

	/* additional headers */
	hdrs.s = s = hdrs_buf;
	l = sizeof(flag_t);
	if (l+12+1 >= HDRS_BUFFER_MAX) {
		LOG(L_ERR,"ERROR:tm:t_write_req: buffer overflow "
			"while copying optional header\n");
		goto error3;
	}
	append_str(s,"P-MsgFlags: ",12); hdrs.len = 12;
	int2reverse_hex(&s, &l, (int)msg->msg_flags);
	hdrs.len += sizeof(flag_t) - l;
	append_chr(s,'\n'); hdrs.len++;

	for(p_hdr = msg->headers;p_hdr;p_hdr = p_hdr->next) {
		if( !(p_hdr->type&HDR_OTHER) )
			continue;

		if(hdrs.len+p_hdr->name.len+p_hdr->body.len+4 >= HDRS_BUFFER_MAX){
			LOG(L_ERR,"ERROR:tm:t_write_req: buffer overflow while "
				"copying optional header\n");
			goto error3;
		}
		append_str(s,p_hdr->name.s,p_hdr->name.len);
		hdrs.len += p_hdr->name.len;
		append_chr(s,':');append_chr(s,' '); hdrs.len+=2;
		append_str(s,p_hdr->body.s,p_hdr->body.len);
		hdrs.len += p_hdr->body.len;
		if(*(s-1) != '\n'){
			append_chr(s,'\n');
			hdrs.len++;
		}
	}

	append_chr(s,'.');
	hdrs.len++;

	eol_line(1).s = s = cmd_buf;
	if(strlen(action)+12 >= CMD_BUFFER_MAX){
		LOG(L_ERR,"ERROR:tm:t_write_req: buffer overflow while "
			"copying command name\n");
		goto error3;
	}
	append_str(s,"sip_request.",12);
	append_str(s,action,strlen(action));
	eol_line(1).len = s-eol_line(1).s;

	eol_line(2)=REQ_LINE(msg).method;     /* method type */
	eol_line(3)=msg->parsed_uri.user;     /* user from r-uri */
	eol_line(4)=email_avp?(email_avp->val.str_val):empty_param;  /* email */
	eol_line(5)=msg->parsed_uri.host;     /* domain */

	eol_line(6)=msg->rcv.bind_address->address_str; /* dst ip */

	eol_line(7)=msg->rcv.dst_port==SIP_PORT ?
			empty_param : msg->rcv.bind_address->port_no_str; /* port */

	/* r_uri ('Contact:' for next requests) */
	eol_line(8)=msg->first_line.u.request.uri;

	/* r_uri for subsequent requests */
	eol_line(9)=str_uri.len?str_uri:empty_param;

	eol_line(10)=get_from(msg)->body;		/* from */
	eol_line(11)=msg->to->body;			/* to */
	eol_line(12)=msg->callid->body;		/* callid */
	eol_line(13)=get_from(msg)->tag_value;	/* from tag */
	eol_line(14)=get_to(msg)->tag_value;	/* to tag */
	eol_line(15)=get_cseq(msg)->number;	/* cseq number */

	eol_line(16).s=id_buf;       /* hash:label */
	s = int2str(hash_index, &l);
	if (l+1>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR:tm:t_write_req: too big hash\n");
		goto error3;
	}
	memcpy(id_buf, s, l);
	id_buf[l]=':';
	eol_line(16).len=l+1;
	s = int2str(label, &l);
	if (l+1+eol_line(16).len>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR:tm:t_write_req: too big label\n");
		goto error3;
	}
	memcpy(id_buf+eol_line(16).len, s, l);
	eol_line(16).len+=l;

	eol_line(17) = route.len ? route : empty_param;
	eol_line(18) = next_hop;
	eol_line(19) = hdrs;
	eol_line(20) = body;

	if ( write_to_fifo(vm_fifo, TWRITE_PARAMS)==-1 ) {
		LOG(L_ERR, "ERROR:tm:t_write_req: write_to_fifo failed\n");
		goto error3;
	}

	/* make sure that if voicemail does not initiate a reply
	 * timely, a SIP timeout will be sent out */
	if( add_blind_uac()==-1 ) {
		LOG(L_ERR, "ERROR:tm:t_write_req: add_blind failed\n");
		goto error3;
	}

	/* success */
	ret = 1;

error3:
	if (parse_flags&TWRITE_RR_PARSED)
		free_rr( ((rr_t**)&msg->record_route->parsed) );
error2:
	if (parse_flags&TWRITE_CONTACT_PARSED)
		free_contact( ((contact_body_t**)&msg->contact->parsed) );
error1:
	if (parse_flags&TWRITE_FROM_PARSED) {
		free_from( msg->from->parsed );
		msg->from->parsed = 0;
	}
error:
	/* 0 would lead to immediate script exit -- -1 returns
	 * with 'false' to script processing */
	return ret;
}



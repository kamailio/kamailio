/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 *  2003-02-13  proto support added (andrei)
 *  2003-02-24  s/T_NULL/T_NULL_CELL/ to avoid redefinition conflict w/
 *              nameser_compat.h (andrei)
 *  2003-03-01  kr set through a function now (jiri)
 *  2003-03-06  callbacks renamed; "blind UAC" introduced, which makes
 *              transaction behave as if it was forwarded even if it was
 *              not -- good for local UAS, like VM (jiri)
 *  2003-03-19  replaced all the mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-30  we now watch downstream delivery and if it fails, send an
 *              error message upstream (jiri)
 *  2003-04-14  use protocol from uri (jiri)
 *  2003-12-04  global TM callbacks switched to per transaction callbacks
 *              (bogdan)
 *  2004-02-13: t->is_invite and t->local replaced with flags (bogdan)
 */

#include "../../dprint.h"
#include "../../config.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../timer.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "../../action.h"
#include "../../data_lump.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../parser/parser_f.h"
#include "t_funcs.h"
#include "t_hooks.h"
#include "t_msgbuilder.h"
#include "ut.h"
#include "t_cancel.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "fix_lumps.h"
#include "config.h"

/* route to execute for the branches */
static int goto_on_branch;

void t_on_branch( unsigned int go_to )
{
	struct cell *t = get_t();

	/* in MODE_REPLY and MODE_ONFAILURE T will be set to current transaction;
	 * in MODE_REQUEST T will be set only if the transaction was already 
	 * created; if not -> use the static variable */
	if (route_type==BRANCH_ROUTE || !t || t==T_UNDEFINED )
		goto_on_branch=go_to;
	else
		t->on_branch = go_to;
}


unsigned int get_on_branch()
{
	return goto_on_branch;
}


static inline int pre_print_uac_request( struct cell *t, int branch, 
		struct sip_msg *request)
{
	int backup_route_type;
	struct usr_avp **backup_list;
	char *p;


	/* ... we calculate branch ... */
	if (!t_calc_branch(t, branch, request->add_to_branch_s,
			&request->add_to_branch_len ))
	{
		LOG(L_ERR, "ERROR:pre_print_uac_request: branch computation failed\n");
		goto error;
	}

	/* from now on, flag all new lumps with LUMPFLAG_BRANCH flag in order to
	 * be able to remove them later --bogdan */
	set_init_lump_flags(LUMPFLAG_BRANCH);

	/********** run route & callback ************/

	/* run branch route, if any; run it before RURI's DNS lookup 
	 * to allow to be changed --bogdan */
	if (t->on_branch) {
		/* need to pkg_malloc the dst_uri */
		if ( request->dst_uri.len ) {
			if ( (p=pkg_malloc(request->dst_uri.len))==0 ) {
				LOG(L_ERR,"ERROR:tm:pre_print_uac_request: no more pkg mem\n");
				ser_error=E_OUT_OF_MEM;
				goto error;
			}
			memcpy( p, request->dst_uri.s, request->dst_uri.len);
			request->dst_uri.s = p;
		}
		/* need to pkg_malloc the new_uri */
		if ( (p=pkg_malloc(request->new_uri.len))==0 ) {
			LOG(L_ERR,"ERROR:tm:pre_print_uac_request: no more pkg mem\n");
			ser_error=E_OUT_OF_MEM;
			goto error;
		}
		memcpy( p, request->new_uri.s, request->new_uri.len);
		request->new_uri.s = p;
		/* make available the avp list from transaction */
		backup_list = set_avp_list( &t->user_avps );
		/* run branch route */
		swap_route_type( backup_route_type, BRANCH_ROUTE);
		run_actions(branch_rlist[t->on_branch], request);
		set_route_type( backup_route_type );
		/* restore original avp list */
		set_avp_list( backup_list );
	}

	/* run the specific callbacks for this transaction */
	run_trans_callbacks( TMCB_REQUEST_FWDED, t, request, 0,
			-request->REQ_METHOD);

	return 0;
error:
	return -1;
}

/* be aware and use it *all* the time between pre_* and post_* functions! */
static inline char *print_uac_request(struct sip_msg *i_req, unsigned int *len,
		struct socket_info *send_sock, enum sip_protos proto )
{
	char *buf, *shbuf;

	shbuf=0;

	/* build the shm buffer now */
	buf=build_req_buf_from_sip_req( i_req, len, send_sock, proto );
	if (!buf) {
		LOG(L_ERR, "ERROR:tm:print_uac_request: no pkg_mem\n"); 
		ser_error=E_OUT_OF_MEM;
		goto error01;
	}

	shbuf=(char *)shm_malloc(*len);
	if (!shbuf) {
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR:tm:print_uac_request: no shmem\n");
		goto error02;
	}
	memcpy( shbuf, buf, *len );

error02:
	pkg_free( buf );
error01:
	return shbuf;
}


static inline void post_print_uac_request(struct sip_msg *request,
		str *org_uri, str *org_dst)
{
	reset_init_lump_flags();
	/* delete inserted branch lumps */
	del_flaged_lumps( &request->add_rm, LUMPFLAG_BRANCH);
	del_flaged_lumps( &request->body_lumps, LUMPFLAG_BRANCH);
	/* free any potential new uri */
	if (request->new_uri.s!=org_uri->s) {
		pkg_free(request->new_uri.s);
		/* and just to be sure */
		request->new_uri.s = 0;
		request->new_uri.len = 0;
		request->parsed_uri_ok = 0;
	}
	/* free any potential dst uri */
	if (request->dst_uri.s!=org_dst->s) {
		pkg_free(request->dst_uri.s);
		/* and just to be sure */
		request->dst_uri.s = 0;
		request->dst_uri.len = 0;
	}
}


/* introduce a new uac, which is blind -- it only creates the
   data structures and starts FR timer, but that's it; it does
   not print messages and send anything anywhere; that is good
   for FIFO apps -- the transaction must look operationally
   and FR must be ticking, whereas the request is "forwarded"
   using a non-SIP way and will be replied the same way
*/
int add_blind_uac( /*struct cell *t*/ )
{
	unsigned short branch;
	struct cell *t;

	t=get_t();
	if (t==T_UNDEFINED || !t ) {
		LOG(L_ERR, "ERROR: add_blind_uac: no transaction context\n");
		return -1;
	}

	branch=t->nr_of_outgoings;	
	if (branch==MAX_BRANCHES) {
		LOG(L_ERR, "ERROR: add_blind_uac: "
			"maximum number of branches exceeded\n");
		return -1;
	}
	/* make sure it will be replied */
	t->flags |= T_NOISY_CTIMER_FLAG;
	t->nr_of_outgoings++;
	/* start FR timer -- protocol set by default to PROTO_NONE,
       which means retransmission timer will not be started
    */
	start_retr(&t->uac[branch].request);
	/* we are on a timer -- don't need to put on wait on script
	   clean-up	
	*/
	set_kr(REQ_FWDED); 

	return 1; /* success */
}



/* introduce a new uac to transaction; returns its branch id (>=0)
   or error (<0); it doesn't send a message yet -- a reply to it
   might interfere with the processes of adding multiple branches
*/
int add_uac( struct cell *t, struct sip_msg *request, str *uri, str* next_hop,
	struct proxy_l *proxy, int proto )
{
	int ret;
	short temp_proxy;
	union sockaddr_union to;
	unsigned short branch;
	struct socket_info* send_sock;
	char *shbuf;
	unsigned int len;

	branch=t->nr_of_outgoings;
	if (branch==MAX_BRANCHES) {
		LOG(L_ERR, "ERROR:tm:add_uac: maximum number of branches exceeded\n");
		ret=E_CFG;
		goto error;
	}

	/* check existing buffer -- rewriting should never occur */
	if (t->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR:tm:add_uac: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}

	/* set proper RURI to request to reflect the branch */
	request->new_uri=*uri;
	request->parsed_uri_ok=0;
	request->dst_uri=*next_hop;

	if ( pre_print_uac_request( t, branch, request)!= 0 ) {
		ret = -1;
		goto error01;
	}

	/* check DNS resolution */
	if (proxy){
		temp_proxy=0;
		proto=get_proto(proto, proxy->proto);
	}else {
		proxy=uri2proxy( request->dst_uri.len ?
			&request->dst_uri:&request->new_uri, proto );
		if (proxy==0)  {
			ret=E_BAD_ADDRESS;
			goto error01;
		}
		proto=proxy->proto; /* uri2proxy will fix it for us */
		temp_proxy=1;
	}

	if (proxy->ok==0) {
		if (proxy->host.h_addr_list[proxy->addr_idx+1])
			proxy->addr_idx++;
		else proxy->addr_idx=0;
		proxy->ok=1;
	}

	hostent2su( &to, &proxy->host, proxy->addr_idx, 
		proxy->port ? proxy->port:SIP_PORT);

	send_sock=get_send_socket( request, &to , proto);
	if (send_sock==0) {
		LOG(L_ERR, "ERROR:tm:add_uac: can't fwd to af %d, proto %d "
			" (no corresponding listening socket)\n",
			to.s.sa_family, proto );
		ret=ser_error=E_NO_SOCKET;
		goto error02;
	}

	/* now message printing starts ... */
	shbuf=print_uac_request( request, &len, send_sock, proto );
	if (!shbuf) {
		ret=ser_error=E_OUT_OF_MEM;
		goto error02;
	}

	/* things went well, move ahead and install new buffer! */
	t->uac[branch].request.dst.to=to;
	t->uac[branch].request.dst.send_sock=send_sock;
	t->uac[branch].request.dst.proto=proto;
	t->uac[branch].request.dst.proto_reserved1=0;
	t->uac[branch].request.buffer=shbuf;
	t->uac[branch].request.buffer_len=len;
	t->uac[branch].uri.s=t->uac[branch].request.buffer+
		request->first_line.u.request.method.len+1;
	t->uac[branch].uri.len=request->new_uri.len;
	t->uac[branch].sc_flags = request->flags;
	t->nr_of_outgoings++;

	/* update stats */
	proxy->tx++;
	proxy->tx_bytes+=len;

	/* done! */
	ret=branch;

error02:
	if (temp_proxy) {
		free_proxy( proxy );
		pkg_free( proxy );
	}
error01:
	post_print_uac_request( request, uri, next_hop);
error:
	return ret;
}


int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, 
	struct cell *t_invite, int branch )
{
	int ret;
	char *shbuf;
	unsigned int len;
	str bk_dst_uri;

	if (t_cancel->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: e2e_cancel_branch: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}

	/* note -- there is a gap in proxy stats -- we don't update 
	   proxy stats with CANCEL (proxy->ok, proxy->tx, etc.) */
	cancel_msg->new_uri = t_invite->uac[branch].uri;
	cancel_msg->parsed_uri_ok=0;
	bk_dst_uri = cancel_msg->dst_uri;

	if ( pre_print_uac_request( t_cancel, branch, cancel_msg)!= 0 ) {
		ret = -1;
		goto error01;
	}

	/* force same uri as in INVITE */
	if (cancel_msg->new_uri.s!=t_invite->uac[branch].uri.s) {
		pkg_free(cancel_msg->new_uri.s);
		cancel_msg->new_uri = t_invite->uac[branch].uri;
		/* and just to be sure */
		cancel_msg->parsed_uri_ok = 0;
	}

	/* print */
	shbuf=print_uac_request( cancel_msg, &len,
		t_invite->uac[branch].request.dst.send_sock,
		t_invite->uac[branch].request.dst.proto);
	if (!shbuf) {
		LOG(L_ERR, "ERROR: e2e_cancel_branch: printing e2e cancel failed\n");
		ret=ser_error=E_OUT_OF_MEM;
		goto error01;
	}

	/* install buffer */
	t_cancel->uac[branch].request.dst=t_invite->uac[branch].request.dst;
	t_cancel->uac[branch].request.buffer=shbuf;
	t_cancel->uac[branch].request.buffer_len=len;
	t_cancel->uac[branch].uri.s=t_cancel->uac[branch].request.buffer+
		cancel_msg->first_line.u.request.method.len+1;
	t_cancel->uac[branch].uri.len=t_invite->uac[branch].uri.len;

	/* success */
	ret=1;

error01:
	post_print_uac_request( cancel_msg, &t_invite->uac[branch].uri,
		&bk_dst_uri);
	cancel_msg->dst_uri = bk_dst_uri;
error:
	return ret;
}

void e2e_cancel( struct sip_msg *cancel_msg, 
	struct cell *t_cancel, struct cell *t_invite )
{
	branch_bm_t cancel_bm;
	branch_bm_t dummy_bm;
	int i;
	int lowest_error;
	str backup_uri;
	int ret;

	cancel_bm=0;
	lowest_error=0;

	/* e2e_cancel_branch() makes no RURI parsing, so no need to 
	 * save the ->parse_uri_ok */
	backup_uri = cancel_msg->new_uri;

	/* determine which branches to cancel ... */
	which_cancel( t_invite, &cancel_bm );
	t_cancel->nr_of_outgoings=t_invite->nr_of_outgoings;
	t_cancel->first_branch=t_invite->first_branch;
	/* fix label -- it must be same for reply matching */
	t_cancel->label=t_invite->label;
	/* ... and install CANCEL UACs */
	for (i=t_invite->first_branch; i<t_invite->nr_of_outgoings; i++) {
		if (cancel_bm & (1<<i)) {
			ret=e2e_cancel_branch(cancel_msg, t_cancel, t_invite, i);
			if (ret<0) cancel_bm &= ~(1<<i);
			if (ret<lowest_error) lowest_error=ret;
		}
	}
	/* restore new_uri */
	cancel_msg->new_uri = backup_uri;
	cancel_msg->parsed_uri_ok = 0;

	/* send them out */
	for (i=t_cancel->first_branch; i<t_cancel->nr_of_outgoings; i++) {
		if (cancel_bm & (1<<i)) {
			if (SEND_BUFFER( &t_cancel->uac[i].request)==-1) {
				LOG(L_ERR, "ERROR: e2e_cancel: send failed\n");
			}
			start_retr( &t_cancel->uac[i].request );
		}
	}

	/* internally cancel branches with no received reply */
	for (i=t_invite->first_branch; i<t_invite->nr_of_outgoings; i++) {
		if (t_invite->uac[i].last_received==0){
			/* mark as cancelled */
			t_invite->uac[i].flags |= T_UAC_TO_CANCEL_FLAG;
			/* reset the "request" timers */
			reset_timer(&t_invite->uac[i].request.retr_timer);
			reset_timer(&t_invite->uac[i].request.fr_timer);
			LOCK_REPLIES( t_invite );
			if (RPS_ERROR==relay_reply(t_invite,FAKED_REPLY,i,487,&dummy_bm))
				lowest_error = -1; /* force sending 500 error */
		}
	}

	/* if error occurred, let it know upstream (final reply
	   will also move the transaction on wait state
	*/
	if (lowest_error<0) {
		LOG(L_ERR, "ERROR: cancel error\n");
		t_reply( t_cancel, cancel_msg, 500, "cancel error");
	/* if there are pending branches, let upstream know we
	   are working on it
	*/
	} else if (cancel_bm) {
		DBG("DEBUG: e2e_cancel: e2e cancel proceeding\n");
		t_reply( t_cancel, cancel_msg, 200, CANCELING );
	/* if the transaction exists, but there is no more pending
	   branch, tell upstream we're done
	*/
	} else {
		DBG("DEBUG: e2e_cancel: e2e cancel -- no more pending branches\n");
		t_reply( t_cancel, cancel_msg, 200, CANCEL_DONE );
	}

#ifdef LOCAL_487

	/* local 487s have been deprecated -- it better handles
	 * race conditions (UAS sending 200); hopefully there are
	 * no longer UACs who go crazy waiting for the 487 whose
	 * forwarding is being blocked by other unresponsive branch
	 */

	/* we could await downstream UAS's 487 replies; however,
	   if some of the branches does not do that, we could wait
	   long time and annoy upstream UAC which wants to see 
	   a result of CANCEL quickly
	*/
	DBG("DEBUG: e2e_cancel: sending 487\n");
	/* in case that something in the meantime has been sent upstream
	   (like if FR hit at the same time), don't try to send */
	if (t_invite->uas.status>=200) return;
	/* there is still a race-condition -- the FR can hit now; that's
	   not too bad -- we take care in t_reply's REPLY_LOCK; in
	   the worst case, both this t_reply and other replier will
	   try, and the later one will result in error message 
	   "can't reply twice"
	*/
	t_reply(t_invite, t_invite->uas.request, 487, CANCELED );
#endif
}


/* function returns:
 *       1 - forward successful
 *      -1 - error during forward
 */
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg , 
	struct proxy_l * proxy, int proto)
{
	str backup_uri;
	str backup_dst;
	int branch_ret, lowest_ret;
	str current_uri;
	branch_bm_t  added_branches;
	int i, q;
	struct cell *t_invite;
	int success_branch;
	int try_new;
	str dst_uri;
	struct socket_info *bk_sock;
	int bk_flags;
	int idx;

	/* make -Wall happy */
	current_uri.s=0;

	set_kr(REQ_FWDED);

	if (p_msg->REQ_METHOD==METHOD_CANCEL) {
		t_invite=t_lookupOriginalT(  p_msg );
		if (t_invite!=T_NULL_CELL) {
			t_invite->flags |= T_WAS_CANCELLED_FLAG;
			e2e_cancel( p_msg, t, t_invite );
			UNREF(t_invite);
			return 1;
		}
	}

	/* do not fooward requests which were already cancelled*/
	if (was_cancelled(t)) {
		LOG(L_ERR,"ERROR:tm:t_forward_nonack: discarding fwd for "
				"a cancelled transaction\n");
		return -1;
	}

	/* backup current uri, sock and flags ... add_uac changes it */
	backup_uri = p_msg->new_uri;
	backup_dst = p_msg->dst_uri;
	bk_sock = p_msg->force_send_socket;
	bk_flags = p_msg->flags;

	/* if no more specific error code is known, use this */
	lowest_ret=E_BUG;
	/* branches added */
	added_branches=0;
	/* branch to begin with */
	t->first_branch=t->nr_of_outgoings;

	/* on first-time forwarding, use current uri, later only what
	   is in additional branches (which may be continuously refilled)
	*/
	if (t->first_branch==0) {
		try_new=1;
		current_uri = *GET_RURI(p_msg);
		branch_ret=add_uac( t, p_msg, &current_uri, &backup_dst, proxy, proto );
		if (branch_ret>=0)
			added_branches |= 1<<branch_ret;
		else
			lowest_ret=branch_ret;
	} else try_new=0;

	for( idx=0; (current_uri.s=get_branch( idx, &current_uri.len, &q,
	&dst_uri.s, &dst_uri.len, &p_msg->force_send_socket))!=0 ; idx++ ) {
		try_new++;
		branch_ret=add_uac( t, p_msg, &current_uri, &dst_uri, proxy, proto);
		/* pick some of the errors in case things go wrong;
		   note that picking lowest error is just as good as
		   any other algorithm which picks any other negative
		   branch result */
		if (branch_ret>=0) 
			added_branches |= 1<<branch_ret;
		else
			lowest_ret=branch_ret;
	}
	/* consume processed branches */
	clear_branches();

	/* restore original stuff */
	p_msg->new_uri=backup_uri;
	p_msg->parsed_uri_ok = 0;/* just to be sure; add_uac may parse other uris*/
	p_msg->dst_uri = backup_dst;
	p_msg->force_send_socket = bk_sock;
	p_msg->flags = bk_flags;
	/* update on_branch, if modified */
	t->on_branch = get_on_branch();

	/* don't forget to clear all branches processed so far */

	/* things went wrong ... no new branch has been fwd-ed at all */
	if (added_branches==0) {
		if (try_new==0) {
			LOG(L_ERR, "ERROR: t_forward_nonack: no branched for forwarding\n");
			return -1;
		}
		LOG(L_ERR, "ERROR: t_forward_nonack: failure to add branches\n");
		return lowest_ret;
	}

	/* send them out now */
	success_branch=0;
	for (i=t->first_branch; i<t->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			if (SEND_BUFFER( &t->uac[i].request)==-1) {
				LOG(L_ERR, "ERROR: t_forward_nonack: sending request failed\n");
				if (proxy) { proxy->errors++; proxy->ok=0; }
			} else {
				success_branch++;
			}
			start_retr( &t->uac[i].request );
		}
	}
	if (success_branch<=0) {
		ser_error=E_SEND;
		return -1;
	}
	return 1;
}


int t_replicate(struct sip_msg *p_msg,  struct proxy_l *proxy, int proto )
{
	/* this is a quite horrible hack -- we just take the message
	   as is, including Route-s, Record-route-s, and Vias ,
	   forward it downstream and prevent replies received
	   from relaying by setting the replication/local_trans bit;

		nevertheless, it should be good enough for the primary
		customer of this function, REGISTER replication

		if we want later to make it thoroughly, we need to
		introduce delete lumps for all the header fields above
	*/
	return t_relay_to(p_msg, proxy, proto, 1 /* replicate */);
}

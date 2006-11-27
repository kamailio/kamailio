/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  2005-08-04  msg->parsed_uri and parsed_uri_ok are no saved & restored
 *               before & after handling the branches (andrei)
 *  2005-12-11  onsend_route support added for forwarding (andrei)
 *  2006-01-27  t_forward_no_ack will return error if a forward on an 
 *              already canceled transaction is attempted (andrei)
 *  2006-02-07  named routes support (andrei)
 *  2006-04-18  add_uac simplified + switched to struct dest_info (andrei)
 *  2006-04-20  pint_uac_request uses now struct dest_info (andrei)
 *  2006-08-11  dns failover support (andrei)
 *              t_forward_non_ack won't start retransmission on send errors
 *               anymore (WARNING: callers should release/kill the transaction
 *               if error is returned) (andrei)
 *  2006-09-15  e2e_cancel uses t_reply_unsafe when called from the 
 *               failure_route and replying to a cancel (andrei)
 *  2006-10-10  e2e_cancel update for the new/modified 
 *               which_cancel()/should_cancel() (andrei)
 *  2006-10-11  don't fork a new branch if the transaction or branch was
 *               canceled, or a 6xx was received
 *              stop retr. timers fix on cancel for non-invites     (andrei)
 *  2006-11-20  new_uri is no longer saved/restore across add_uac calls, since
 *              print_uac_request is now uri safe (andrei)
 */

#include "defs.h"


#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../dset.h"
#include "../../action.h"
#include "../../data_lump.h"
#include "../../onsend.h"
#include "t_funcs.h"
#include "t_hooks.h"
#include "t_msgbuilder.h"
#include "ut.h"
#include "t_cancel.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "fix_lumps.h"
#include "config.h"
#ifdef USE_DNS_FAILOVER
#include "../../dns_cache.h"
#endif
#ifdef USE_DST_BLACKLIST
#include "../../dst_blacklist.h"
#endif



static int goto_on_branch = 0, branch_route = 0;

void t_on_branch( unsigned int go_to )
{
	struct cell *t = get_t();

       /* in MODE_REPLY and MODE_ONFAILURE T will be set to current transaction;
        * in MODE_REQUEST T will be set only if the transaction was already
        * created; if not -> use the static variable */
	if (!t || t==T_UNDEFINED ) {
		goto_on_branch=go_to;
	} else {
		get_t()->on_branch = go_to;
	}
}

unsigned int get_on_branch(void)
{
	return goto_on_branch;
}


static char *print_uac_request( struct cell *t, struct sip_msg *i_req,
	int branch, str *uri, unsigned int *len, struct dest_info* dst)
{
	char *buf, *shbuf;
	str* msg_uri;
	struct lump* add_rm_backup, *body_lumps_backup;
	struct sip_uri parsed_uri_bak;
	int parsed_uri_ok_bak, uri_backed_up;
	str msg_uri_bak;

	shbuf=0;
	msg_uri_bak.s=0; /* kill warnings */
	msg_uri_bak.len=0;
	parsed_uri_ok_bak=0;
	uri_backed_up=0;

	/* ... we calculate branch ... */	
	if (!t_calc_branch(t, branch, i_req->add_to_branch_s,
			&i_req->add_to_branch_len ))
	{
		LOG(L_ERR, "ERROR: print_uac_request: branch computation failed\n");
		goto error00;
	}

	/* ... update uri ... */
	msg_uri=GET_RURI(i_req);
	if ((msg_uri->s!=uri->s) || (msg_uri->len!=uri->len)){
		msg_uri_bak=i_req->new_uri;
		parsed_uri_ok_bak=i_req->parsed_uri_ok;
		parsed_uri_bak=i_req->parsed_uri;
		i_req->new_uri=*uri;
		i_req->parsed_uri_ok=0;
		uri_backed_up=1;
	}

	add_rm_backup = i_req->add_rm;
	body_lumps_backup = i_req->body_lumps;
	i_req->add_rm = dup_lump_list(i_req->add_rm);
	i_req->body_lumps = dup_lump_list(i_req->body_lumps);

	if (branch_route) {
		     /* run branch_route actions if provided */
		if (run_actions(branch_rt.rlist[branch_route], i_req) < 0) {
			LOG(L_ERR, "ERROR: print_uac_request: Error in run_actions\n");
               }
	}

	/* run the specific callbacks for this transaction */
	run_trans_callbacks( TMCB_REQUEST_FWDED , t, i_req, 0, -i_req->REQ_METHOD);

	/* ... and build it now */
	buf=build_req_buf_from_sip_req( i_req, len, dst);
#ifdef DBG_MSG_QA
	if (buf[*len-1]==0) {
		LOG(L_ERR, "ERROR: print_uac_request: sanity check failed\n");
		abort();
	}
#endif
	if (!buf) {
		LOG(L_ERR, "ERROR: print_uac_request: no pkg_mem\n"); 
		ser_error=E_OUT_OF_MEM;
		goto error01;
	}

	shbuf=(char *)shm_malloc(*len);
	if (!shbuf) {
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: print_uac_request: no shmem\n");
		goto error02;
	}
	memcpy( shbuf, buf, *len );

error02:
	pkg_free( buf );
error01:
	     /* Delete the duplicated lump lists, this will also delete
	      * all lumps created here, such as lumps created in per-branch
	      * routing sections, Via, and Content-Length headers created in
	      * build_req_buf_from_sip_req
	      */
	free_duped_lump_list(i_req->add_rm);
	free_duped_lump_list(i_req->body_lumps);
	     /* Restore the lists from backups */
	i_req->add_rm = add_rm_backup;
	i_req->body_lumps = body_lumps_backup;
	/* restore the new_uri from the backup */
	if (uri_backed_up){
		i_req->new_uri=msg_uri_bak;
		i_req->parsed_uri=parsed_uri_bak;
		i_req->parsed_uri_ok=parsed_uri_ok_bak;
	}

 error00:
	return shbuf;
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
	if (start_retr(&t->uac[branch].request)!=0)
		LOG(L_CRIT, "BUG: add_blind_uac: start retr failed for %p\n",
				&t->uac[branch].request);
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
	unsigned short branch;
	char *shbuf;
	unsigned int len;

	branch=t->nr_of_outgoings;
	if (branch==MAX_BRANCHES) {
		LOG(L_ERR, "ERROR: add_uac: maximum number of branches exceeded\n");
		ret=E_CFG;
		goto error;
	}

	/* check existing buffer -- rewriting should never occur */
	if (t->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: add_uac: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}

	/* check DNS resolution */
	if (proxy){
		/* dst filled from the proxy */
		init_dest_info(&t->uac[branch].request.dst);
		t->uac[branch].request.dst.proto=get_proto(proto, proxy->proto);
		proxy2su(&t->uac[branch].request.dst.to, proxy);
		/* fill dst send_sock */
		t->uac[branch].request.dst.send_sock =
		get_send_socket( request, &t->uac[branch].request.dst.to,
								t->uac[branch].request.dst.proto);
	}else {
#ifdef USE_DNS_FAILOVER
		if (uri2dst(&t->uac[branch].dns_h, &t->uac[branch].request.dst,
					request, next_hop?next_hop:uri, proto) == 0)
#else
		/* dst filled from the uri & request (send_socket) */
		if (uri2dst(&t->uac[branch].request.dst, request,
						next_hop ? next_hop: uri, proto)==0)
#endif
		{
			ret=E_BAD_ADDRESS;
			goto error;
		}
	}
	
	/* check if send_sock is ok */
	if (t->uac[branch].request.dst.send_sock==0) {
		LOG(L_ERR, "ERROR: add_uac: can't fwd to af %d, proto %d "
			" (no corresponding listening socket)\n",
			t->uac[branch].request.dst.to.s.sa_family, 
			t->uac[branch].request.dst.proto );
		ret=ser_error=E_NO_SOCKET;
		goto error01;
	}

	/* now message printing starts ... */
	shbuf=print_uac_request( t, request, branch, uri, 
							&len, &t->uac[branch].request.dst);
	if (!shbuf) {
		ret=ser_error=E_OUT_OF_MEM;
		goto error01;
	}

	/* things went well, move ahead and install new buffer! */
	t->uac[branch].request.buffer=shbuf;
	t->uac[branch].request.buffer_len=len;
	t->uac[branch].uri.s=t->uac[branch].request.buffer+
		request->first_line.u.request.method.len+1;
	t->uac[branch].uri.len=uri->len;
	t->nr_of_outgoings++;

	/* update stats */
	if (proxy){
		proxy_mark(proxy, 1);
	}
	/* done! */
	ret=branch;
		
error01:
error:
	return ret;
}



#ifdef USE_DNS_FAILOVER
/* introduce a new uac to transaction, based on old_uac and a possible
 *  new ip address (if the dns name resolves to more ips). If no more
 *   ips are found => returns -1.
 *  returns its branch id (>=0)
   or error (<0); it doesn't send a message yet -- a reply to it
   might interfere with the processes of adding multiple branches
   if lock_replies is 1 replies will be locked for t until the new branch
   is added (to prevent add branches races). Use 0 if the reply lock is
   already held, e.g. in failure route/handlers (WARNING: using 1 in a 
   failure route will cause a deadlock).
*/
int add_uac_dns_fallback( struct cell *t, struct sip_msg* msg, 
									struct ua_client* old_uac,
									int lock_replies)
{
	int ret;
	
	ret=-1;
	if (use_dns_failover && 
			!((t->flags & T_DONT_FORK) || uac_dont_fork(old_uac)) &&
			dns_srv_handle_next(&old_uac->dns_h, 0)){
			if (lock_replies){
				/* use reply lock to guarantee nobody is adding a branch
				 * in the same time */
				LOCK_REPLIES(t);
				/* check again that we can fork */
				if ((t->flags & T_DONT_FORK) || uac_dont_fork(old_uac)){
					UNLOCK_REPLIES(t);
					DBG("add_uac_dns_fallback: no forking on => no new"
							" branches\n");
					return ret;
				}
			}
			if (t->nr_of_outgoings >= MAX_BRANCHES){
				LOG(L_ERR, "ERROR: add_uac_dns_fallback: maximum number of "
							"branches exceeded\n");
				if (lock_replies)
					UNLOCK_REPLIES(t);
				return E_CFG;
			}
			/* copy the dns handle into the new uac */
			dns_srv_handle_cpy(&t->uac[t->nr_of_outgoings].dns_h,
								&old_uac->dns_h);
			/* add_uac will use dns_h => next_hop will be ignored.
			 * Unfortunately we can't reuse the old buffer, the branch id
			 *  must be changed and the send_socket might be different =>
			 *  re-create the whole uac */
			ret=add_uac(t,  msg, &old_uac->uri, 0, 0, 
							old_uac->request.dst.proto);
			if (ret<0){
				/* failed, delete the copied dns_h */
				dns_srv_handle_put(&t->uac[t->nr_of_outgoings].dns_h);
			}
			if (lock_replies){
				UNLOCK_REPLIES(t);
			}
	}
	return ret;
}

#endif

int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, 
	struct cell *t_invite, int branch )
{
	int ret;
	char *shbuf;
	unsigned int len;

	ret=-1;
	if (t_cancel->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: e2e_cancel_branch: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}
	if (t_invite->uac[branch].request.buffer==0){
		/* inactive / deleted  branch */
		goto error;
	}

	/* note -- there is a gap in proxy stats -- we don't update 
	   proxy stats with CANCEL (proxy->ok, proxy->tx, etc.)
	*/

	/* print */
	shbuf=print_uac_request( t_cancel, cancel_msg, branch, 
							&t_invite->uac[branch].uri, &len, 
							&t_invite->uac[branch].request.dst);
	if (!shbuf) {
		LOG(L_ERR, "ERROR: e2e_cancel_branch: printing e2e cancel failed\n");
		ret=ser_error=E_OUT_OF_MEM;
		goto error;
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


error:
	return ret;
}

void e2e_cancel( struct sip_msg *cancel_msg, 
	struct cell *t_cancel, struct cell *t_invite )
{
	branch_bm_t cancel_bm, tmp_bm;
	int i;
	int lowest_error;
	int ret;

	cancel_bm=0;
	lowest_error=0;

	/* first check if there are any branches */
	if (t_invite->nr_of_outgoings==0){
		t_invite->flags|=T_CANCELED;
		/* no branches yet => force a reply to the invite */
		t_reply( t_invite, t_invite->uas.request, 487, CANCELED );
		DBG("DEBUG: e2e_cancel: e2e cancel -- no more pending branches\n");
		t_reply( t_cancel, cancel_msg, 200, CANCEL_DONE );
		return;
	}
	
	/* determine which branches to cancel ... */
	which_cancel( t_invite, &cancel_bm );
	t_cancel->nr_of_outgoings=t_invite->nr_of_outgoings;
	/* fix label -- it must be same for reply matching */
	t_cancel->label=t_invite->label;
	/* ... and install CANCEL UACs */
	for (i=0; i<t_invite->nr_of_outgoings; i++)
		if ((cancel_bm & (1<<i)) && (t_invite->uac[i].last_received>=100)) {
			ret=e2e_cancel_branch(cancel_msg, t_cancel, t_invite, i);
			if (ret<0) cancel_bm &= ~(1<<i);
			if (ret<lowest_error) lowest_error=ret;
		}

	/* send them out */
	for (i = 0; i < t_cancel->nr_of_outgoings; i++) {
		if (cancel_bm & (1 << i)) {
			if (t_invite->uac[i].last_received>=100){
				/* Provisional reply received on this branch, send CANCEL */
				/* we do need to stop the retr. timers if the request is not 
				 * an invite and since the stop_rb_retr() cost is lower then
				 * the invite check we do it always --andrei */
				stop_rb_retr(&t_invite->uac[i].request);
				if (SEND_BUFFER(&t_cancel->uac[i].request) == -1) {
					LOG(L_ERR, "ERROR: e2e_cancel: send failed\n");
				}
				if (start_retr( &t_cancel->uac[i].request )!=0)
					LOG(L_CRIT, "BUG: e2e_cancel: failed to start retr."
							" for %p\n", &t_cancel->uac[i].request);
			} else {
				/* No provisional response received, stop
				 * retransmission timers */
				stop_rb_retr(&t_invite->uac[i].request);
				/* no need to stop fr, it will be stoped by relay_reply
				 * put_on_wait -- andrei */
				/* Generate faked reply */
				LOCK_REPLIES(t_invite);
				if (relay_reply(t_invite, FAKED_REPLY, i, 487, &tmp_bm) == 
						RPS_ERROR) {
					lowest_error = -1;
				}
			}
		}
	}

	/* if error occurred, let it know upstream (final reply
	   will also move the transaction on wait state
	*/
	if (lowest_error<0) {
		LOG(L_ERR, "ERROR: cancel error\n");
		/* if called from failure_route, make sure that the unsafe version
		 * is called (we are already hold the reply mutex for the cancel
		 * transaction).
		 */
		if ((rmode==MODE_ONFAILURE) && (t_cancel==get_t()))
			t_reply_unsafe( t_cancel, cancel_msg, 500, "cancel error");
		else
			t_reply( t_cancel, cancel_msg, 500, "cancel error");
	} else if (cancel_bm) {
		/* if there are pending branches, let upstream know we
		   are working on it
		*/
		DBG("DEBUG: e2e_cancel: e2e cancel proceeding\n");
		/* if called from failure_route, make sure that the unsafe version
		 * is called (we are already hold the reply mutex for the cancel
		 * transaction).
		 */
		if ((rmode==MODE_ONFAILURE) && (t_cancel==get_t()))
			t_reply_unsafe( t_cancel, cancel_msg, 200, CANCELING );
		else
			t_reply( t_cancel, cancel_msg, 200, CANCELING );
	} else {
		/* if the transaction exists, but there is no more pending
		   branch, tell upstream we're done
		*/
		DBG("DEBUG: e2e_cancel: e2e cancel -- no more pending branches\n");
		/* if called from failure_route, make sure that the unsafe version
		 * is called (we are already hold the reply mutex for the cancel
		 * transaction).
		 */
		if ((rmode==MODE_ONFAILURE) && (t_cancel==get_t()))
			t_reply_unsafe( t_cancel, cancel_msg, 200, CANCEL_DONE );
		else
			t_reply( t_cancel, cancel_msg, 200, CANCEL_DONE );
	}
}



/* sends one uac/branch buffer and fallbacks to other ips if
 *  the destination resolves to several addresses
 *  Takes care of starting timers a.s.o. (on send success)
 *  returns: -2 on error, -1 on drop,  current branch id on success,
 *   new branch id on send error/blacklist, when failover is possible
 *    (ret>=0 && ret!=branch)
 *    if lock_replies is 1, the replies for t will be locked when adding
 *     new branches (to prevent races). Use 0 from failure routes or other
 *     places where the reply lock is already held, to avoid deadlocks. */
int t_send_branch( struct cell *t, int branch, struct sip_msg* p_msg ,
					struct proxy_l * proxy, int lock_replies)
{
	struct ip_addr ip; /* debugging */
	int ret;
	struct ua_client* uac;
	
	uac=&t->uac[branch];
	ret=branch;
	if (run_onsend(p_msg,	&uac->request.dst, uac->request.buffer,
					uac->request.buffer_len)==0){
		/* disable the current branch: set a "fake" timeout
		 *  reply code but don't set uac->reply, to avoid overriding 
		 *  a higly unlikely, perfectly timed fake reply (to a message
		 *   we never sent).
		 * (code=final reply && reply==0 => t_pick_branch won't ever pick it)*/
			uac->last_received=408;
			su2ip_addr(&ip, &uac->request.dst.to);
			DBG("t_send_branch: onsend_route dropped msg. to %s:%d (%d)\n",
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
#ifdef USE_DNS_FAILOVER
			/* if the destination resolves to more ips, add another
			 *  branch/uac */
			if (use_dns_failover){
				ret=add_uac_dns_fallback(t, p_msg, uac, lock_replies);
				if (ret>=0){
					su2ip_addr(&ip, &uac->request.dst.to);
					DBG("t_send_branch: send on branch %d failed "
							"(onsend_route), trying another ip %s:%d (%d)\n",
							branch, ip_addr2a(&ip),
							su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
					/* success, return new branch */
					return ret;
				}
			}
#endif /* USE_DNS_FAILOVER*/
		return -1; /* drop, try next branch */
	}
#ifdef USE_DST_BLACKLIST
	if (use_dst_blacklist){
		if (dst_is_blacklisted(&uac->request.dst)){
			su2ip_addr(&ip, &uac->request.dst.to);
			DBG("t_send_branch: blacklisted destination: %s:%d (%d)\n",
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
			/* disable the current branch: set a "fake" timeout
			 *  reply code but don't set uac->reply, to avoid overriding 
			 *  a higly unlikely, perfectly timed fake reply (to a message
			 *   we never sent).  (code=final reply && reply==0 => 
			 *   t_pick_branch won't ever pick it)*/
			uac->last_received=408;
#ifdef USE_DNS_FAILOVER
			/* if the destination resolves to more ips, add another
			 *  branch/uac */
			if (use_dns_failover){
				ret=add_uac_dns_fallback(t, p_msg, uac, lock_replies);
				if (ret>=0){
					su2ip_addr(&ip, &uac->request.dst.to);
					DBG("t_send_branch: send on branch %d failed (blacklist),"
							" trying another ip %s:%d (%d)\n", branch,
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
					/* success, return new branch */
					return ret;
				}
			}
#endif /* USE_DNS_FAILOVER*/
			return -1; /* don't send */
		}
	}
#endif /* USE_DST_BLACKLIST */
	if (SEND_BUFFER( &uac->request)==-1) {
		/* disable the current branch: set a "fake" timeout
		 *  reply code but don't set uac->reply, to avoid overriding 
		 *  a higly unlikely, perfectly timed fake reply (to a message
		 *  we never sent).
		 * (code=final reply && reply==0 => t_pick_branch won't ever pick it)*/
		uac->last_received=408;
		su2ip_addr(&ip, &uac->request.dst.to);
		DBG("t_send_branch: send to %s:%d (%d) failed\n",
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
#ifdef USE_DST_BLACKLIST
		if (use_dst_blacklist)
			dst_blacklist_add(BLST_ERR_SEND, &uac->request.dst);
#endif
#ifdef USE_DNS_FAILOVER
		/* if the destination resolves to more ips, add another
		 *  branch/uac */
		if (use_dns_failover){
			ret=add_uac_dns_fallback(t, p_msg, uac, lock_replies);
			if (ret>=0){
				/* success, return new branch */
				DBG("t_send_branch: send on branch %d failed, adding another"
						" branch with another ip\n", branch);
				return ret;
			}
		}
#endif
		LOG(L_ERR, "ERROR: t_send_branch: sending request on branch %d "
				"failed\n", branch);
		if (proxy) { proxy->errors++; proxy->ok=0; }
		return -2;
	} else {
		/* start retr. only if the send succeeded */
		if (start_retr( &uac->request )!=0){
			LOG(L_CRIT, "BUG: t_send_branch: retr. already started for %p\n",
					&uac->request);
			return -2;
		}
	}
	return ret;
}



/* function returns:
 *       1 - forward successful
 *      -1 - error during forward
 */
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg , 
	struct proxy_l * proxy, int proto)
{
	int branch_ret, lowest_ret;
	str current_uri;
	branch_bm_t	added_branches;
	int first_branch;
	int i, q;
	struct cell *t_invite;
	int success_branch;
	int try_new;
	int lock_replies;
	str dst_uri;
	struct socket_info* si, *backup_si;

	/* make -Wall happy */
	current_uri.s=0;

	set_kr(REQ_FWDED);

	if (p_msg->REQ_METHOD==METHOD_CANCEL) {
		t_invite=t_lookupOriginalT(  p_msg );
		if (t_invite!=T_NULL_CELL) {
			e2e_cancel( p_msg, t, t_invite );
			UNREF(t_invite);
			return 1;
		}
	}
	if (t->flags & T_CANCELED){
		DBG("t_forward_non_ack: no forwarding on canceled branch\n");
		ser_error=E_CANCELED;
		return -1;
	}

	backup_si = p_msg->force_send_socket;
	/* if no more specific error code is known, use this */
	lowest_ret=E_BUG;
	/* branches added */
	added_branches=0;
	/* branch to begin with */
	first_branch=t->nr_of_outgoings;

	if (t->on_branch) {
		/* tell add_uac that it should run branch route actions */
		branch_route = t->on_branch;
		/* reset the flag before running the actions (so that it
		 * could be set again in branch_route if needed
		 */
		t_on_branch(0);
	} else {
		branch_route = 0;
	}
	
	/* on first-time forwarding, use current uri, later only what
	   is in additional branches (which may be continuously refilled
	*/
	if (first_branch==0) {
		try_new=1;
		branch_ret=add_uac( t, p_msg, GET_RURI(p_msg), GET_NEXT_HOP(p_msg), proxy, proto );
		if (branch_ret>=0) 
			added_branches |= 1<<branch_ret;
		else
			lowest_ret=branch_ret;
	} else try_new=0;

	init_branch_iterator();
	while((current_uri.s=next_branch( &current_uri.len, &q, &dst_uri.s, &dst_uri.len, &si))) {
		try_new++;
		p_msg->force_send_socket = si;
		branch_ret=add_uac( t, p_msg, &current_uri, 
				    (dst_uri.len) ? (&dst_uri) : &current_uri, 
				    proxy, proto);
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

	p_msg->force_send_socket = backup_si;

	/* don't forget to clear all branches processed so far */

	/* things went wrong ... no new branch has been fwd-ed at all */
	if (added_branches==0) {
		if (try_new==0) {
			LOG(L_ERR, "ERROR: t_forward_nonack: no branches for forwarding\n");
			return -1;
		}
		LOG(L_ERR, "ERROR: t_forward_nonack: failure to add branches\n");
		return lowest_ret;
	}

	/* send them out now */
	success_branch=0;
	lock_replies= ! ((rmode==MODE_ONFAILURE) && (t==get_t()));
	for (i=first_branch; i<t->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			
			branch_ret=t_send_branch(t, i, p_msg , proxy, lock_replies);
			if (branch_ret>=0){ /* some kind of success */
				if (branch_ret==i) /* success */
					success_branch++;
				else /* new branch added */
					added_branches |= 1<<branch_ret;
			}
		}
	}
	if (success_branch<=0) {
		ser_error=E_SEND;
		/* the caller should take care and delete the transaction */
		return -1;
	}
	return 1;
}



/* WARNING: doesn't work from failure route (deadlock, uses t_relay_to which
 *  is failure route unsafe) */
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

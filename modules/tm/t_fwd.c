/*
 * $Id$
 *
 */

#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "../../dset.h"
#include "t_funcs.h"
#include "t_hooks.h"
#include "t_msgbuilder.h"
#include "ut.h"
#include "t_cancel.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "fix_lumps.h"
#include "config.h"


#ifdef _OBSOLETED
#define shm_free_lump( _lmp) \
	do{\
		if ((_lmp)) {\
			if ((_lmp)->op==LUMP_ADD && (_lmp)->u.value )\
				shm_free((_lmp)->u.value);\
			shm_free((_lmp));\
		}\
	}while(0);
#endif

char *print_uac_request( struct cell *t, struct sip_msg *i_req,
	int branch, str *uri, int *len, struct socket_info *send_sock )
{
	char *buf, *shbuf;

	shbuf=0;

	/* ... we calculate branch ... */	
	if (!t_setbranch( t, i_req, branch )) {
		LOG(L_ERR, "ERROR: print_uac_request: branch computation failed\n");
		goto error01;
	}

	/* ... update uri ... */
	i_req->new_uri=*uri;

	/* ... give apps a chance to change things ... */
	callback_event( TMCB_REQUEST_OUT, t, i_req, -i_req->REQ_METHOD);

	/* ... and build it now */
	buf=build_req_buf_from_sip_req( i_req, len, send_sock );
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
	/*	clean Via's we created now -- they would accumulate for
		other branches  and for  shmem i_req they would mix up
	 	shmem with pkg_mem
	*/
#ifdef OBSOLETED
	if (branch) for(b=i_req->add_rm,b1=0;b;b1=b,b=b->next)
		if (b->type==HDR_VIA) {
			for(a=b->before;a;)
				{c=a->before;free_lump(a);pkg_free(a);a=c;}
			for(a=b->after;a;)
				{c=a->after;free_lump(a);pkg_free(a);a=c;}
			if (b1) b1->next = b->next;
			else i_req->add_rm = b->next;
			free_lump(b);pkg_free(b);
		}
#endif
	free_via_lump(&i_req->add_rm);

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
	return shbuf;
}

/* introduce a new uac to transaction; returns its branch id (>=0)
   or error (<0); it doesn't send a message yet -- a reply to it
   might itnerfere with the processes of adding multiple branches
*/
int add_uac( struct cell *t, struct sip_msg *request, str *uri, 
	struct proxy_l *proxy )
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
	if (proxy) temp_proxy=0; else {
		proxy=uri2proxy( uri );
		if (proxy==0)  {
			ret=E_BAD_ADDRESS;
			goto error;
		}
		temp_proxy=1;
	}

	if (proxy->ok==0) {
		if (proxy->host.h_addr_list[proxy->addr_idx+1])
			proxy->addr_idx++;
		else proxy->addr_idx=0;
		proxy->ok=1;
	}

	hostent2su( &to, &proxy->host, proxy->addr_idx, 
		proxy->port ? htons(proxy->port):htons(SIP_PORT));

	send_sock=get_send_socket( &to );
	if (send_sock==0) {
		LOG(L_ERR, "ERROR: add_uac: can't fwd to af %d "
			" (no corresponding listening socket)\n",
			to.s.sa_family );
		ret=ser_error=E_NO_SOCKET;
		goto error01;
	}

	/* now message printing starts ... */
	shbuf=print_uac_request( t, request, branch, uri, 
		&len, send_sock );
	if (!shbuf) {
		ret=ser_error=E_OUT_OF_MEM;
		goto error01;
	}

	/* things went well, move ahead and install new buffer! */
	t->uac[branch].request.to=to;
	t->uac[branch].request.send_sock=send_sock;
	t->uac[branch].request.buffer=shbuf;
	t->uac[branch].request.buffer_len=len;
	t->uac[branch].uri.s=t->uac[branch].request.buffer+
		request->first_line.u.request.method.len+1;
	t->uac[branch].uri.len=uri->len;
	t->nr_of_outgoings++;

	/* update stats */
	proxy->tx++;
	proxy->tx_bytes+=len;

	/* done! */	
	ret=branch;
		
error01:
	if (temp_proxy) {
		free_proxy( proxy );
		free( proxy );
	}
error:
	return ret;
}

int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, 
	struct cell *t_invite, int branch )
{
	int ret;
	char *shbuf;
	int len;

	if (t_cancel->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: e2e_cancel_branch: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}	

	/* note -- there is a gap in proxy stats -- we don't update 
	   proxy stats with CANCEL (proxy->ok, proxy->tx, etc.)
	*/

	/* print */
	shbuf=print_uac_request( t_cancel, cancel_msg, branch, 
		&t_invite->uac[branch].uri, &len, 
		t_invite->uac[branch].request.send_sock);
	if (!shbuf) {
		LOG(L_ERR, "ERROR: e2e_cancel_branch: printing e2e cancel failed\n");
		ret=ser_error=E_OUT_OF_MEM;
		goto error;
	}
	
	/* install buffer */
	t_cancel->uac[branch].request.to=t_invite->uac[branch].request.to;
	t_cancel->uac[branch].request.send_sock=t_invite->uac[branch].request.send_sock;
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
	branch_bm_t cancel_bm;
	int i;
	int lowest_error;
	str backup_uri;
	int ret;

	cancel_bm=0;
	lowest_error=0;

	backup_uri=cancel_msg->new_uri;
	/* determine which branches to cancel ... */
	which_cancel( t_invite, &cancel_bm );
	t_cancel->nr_of_outgoings=t_invite->nr_of_outgoings;
	/* fix label -- it must be same for reply matching */
	t_cancel->label=t_invite->label;
	/* ... and install CANCEL UACs */
	for (i=0; i<t_invite->nr_of_outgoings; i++)
		if (cancel_bm & (1<<i)) {
			ret=e2e_cancel_branch(cancel_msg, t_cancel, t_invite, i);
			if (ret<0) cancel_bm &= ~(1<<i);
			if (ret<lowest_error) lowest_error=ret;
		}
	cancel_msg->new_uri=backup_uri;

	/* send them out */
	for (i=0; i<t_cancel->nr_of_outgoings; i++) {
		if (cancel_bm & (1<<i)) {
			if (SEND_BUFFER( &t_cancel->uac[i].request)==-1) {
				LOG(L_ERR, "ERROR: e2e_cancel: send failed\n");
			}
			start_retr( &t_cancel->uac[i].request );
		}
	}


	/* if error occured, let it know upstream (final reply
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
		t_reply( t_cancel, cancel_msg, 200, CANCELLING );
	/* if the transaction exists, but there is no more pending
	   branch, tell usptream we're done
	*/
	} else {
		DBG("DEBUG: e2e_cancel: e2e cancel -- no more pending branches\n");
		t_reply( t_cancel, cancel_msg, 200, CANCEL_DONE );
	}
	/* we could await downstream UAS's 487 replies; however,
	   if some of the branches does not do that, we could wait
	   long time and annoy upstream UAC which wants to see 
	   a result of CANCEL quickly
	*/
	DBG("DEBUG: e2e_cancel: sending 487\n");
	t_reply(t_invite, t_invite->uas.request, 487, CANCELLED );
}


/* function returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg , 
	struct proxy_l * proxy )
{
	str          backup_uri;
	int branch_ret, lowest_ret;
	str current_uri;
	branch_bm_t	added_branches;
	int first_branch;
	int i;
	struct cell *t_invite;

	/* make -Wall happy */
	current_uri.s=0;

	t->kr|=REQ_FWDED;

	if (p_msg->REQ_METHOD==METHOD_CANCEL) {
		t_invite=t_lookupOriginalT(  p_msg );
		if (t_invite!=T_NULL) {
			e2e_cancel( p_msg, t, t_invite );
			UNREF(t_invite);
			return 1;
		}
	}

	/* backup current uri ... add_uac changes it */
	backup_uri = p_msg->new_uri;
	/* if no more specific error code is known, use this */
	lowest_ret=E_BUG;
	/* branches added */
	added_branches=0;
	/* branch to begin with */
	first_branch=t->nr_of_outgoings;

	/* on first-time forwarding, use current uri, later only what
	   is in additional branches (which may be continuously refilled
	*/
	if (first_branch==0) {
		branch_ret=add_uac( t, p_msg, 
			p_msg->new_uri.s ? &p_msg->new_uri :  
				&p_msg->first_line.u.request.uri,
			proxy );
		if (branch_ret>=0) 
			added_branches |= 1<<branch_ret;
		else
			lowest_ret=branch_ret;
	}

	init_branch_iterator();
	while((current_uri.s=next_branch( &current_uri.len))) {
		branch_ret=add_uac( t, p_msg, &current_uri, proxy );
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

	/* restore original URI */
	p_msg->new_uri=backup_uri;

	/* don't forget to clear all branches processed so far */

	/* things went wrong ... no new branch has been fwd-ed at all */
	if (added_branches==0)
		return lowest_ret;

	/* if someone set on_negative, store in in T-context */
	t->on_negative=get_on_negative();

	/* send them out now */
	for (i=first_branch; i<t->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			if (SEND_BUFFER( &t->uac[i].request)==-1) {
				LOG(L_ERR, "ERROR: add_uac: sending request failed\n");
				if (proxy) { proxy->errors++; proxy->ok=0; }
			}
			start_retr( &t->uac[i].request );
		}
	}
	return 1;
}	

int t_replicate(struct sip_msg *p_msg,  struct proxy_l *proxy )
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
	return t_relay_to(p_msg, proxy, 1 /* replicate */);
}

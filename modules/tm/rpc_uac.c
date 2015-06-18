/* 
 * Copyright (C) 2009 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * modules/tm/rpc_uac.c
 */

#include "rpc_uac.h"
#include "../../rpc.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "../../parser/parse_from.h"
#include "../../str_list.h"
#include "ut.h"
#include "dlg.h"
#include "uac.h"
#include "callid.h"



/* RPC substitution char (used in rpc_t_uac headers) */
#define SUBST_CHAR '!'



/** make sure the rpc user created the msg properly.
 * Make sure that the FIFO user created the message
 * correctly and fill some extra parameters in function
 * of the message contents.
 * @param rpc - rpc handle
 * @param  c  - rpc context handle
 * @param msg - faked sip msg
 * @param method 
 * @param body
 * @param fromtag - filled on success (1 if fromtag present, 0 if not)
 * @param cseq_is - filled on success (1 if cseq present, 0 if not)
 * @param cseq    - filled on success with the cseq number
 * @callid        - filled on success with a pointer to the callid in the msg.
 * @return -1 on error (and sends the rpc reply), 0 on success
 */
static int rpc_uac_check_msg(rpc_t *rpc, void* c,
								struct sip_msg* msg,
								str* method, str* body, 
								int* fromtag, int *cseq_is, int* cseq,
								str* callid)
{
	struct to_body* parsed_from;
	struct cseq_body *parsed_cseq;
	int i;
	char ch;

	if (body->len && !msg->content_type) {
		rpc->fault(c, 400, "Content-Type missing");
		goto err;
	}

	if (body->len && msg->content_length) {
		rpc->fault(c, 400, "Content-Length disallowed");
		goto err;
	}

	if (!msg->to) {
		rpc->fault(c, 400,  "To missing");
		goto err;
	}

	if (!msg->from) {
		rpc->fault(c, 400, "From missing");
		goto err;
	}

	/* we also need to know if there is from-tag and add it otherwise */
	if (parse_from_header(msg) < 0) {
		rpc->fault(c, 400, "Error in From");
		goto err;
	}

	parsed_from = (struct to_body*)msg->from->parsed;
	*fromtag = parsed_from->tag_value.s && parsed_from->tag_value.len;

	*cseq = 0;
	if (msg->cseq && (parsed_cseq = get_cseq(msg))) {
		*cseq_is = 1;
		for (i = 0; i < parsed_cseq->number.len; i++) {
			ch = parsed_cseq->number.s[i];
			if (ch >= '0' && ch <= '9' ) {
				*cseq = (*cseq) * 10 + ch - '0';
			} else {
			 	DBG("check_msg: Found non-numerical in CSeq: <%i>='%c'\n",
							(unsigned int)ch, ch);
				rpc->fault(c, 400,  "Non-numerical CSeq");
				goto err;
			}
		}
		
		if (parsed_cseq->method.len != method->len || 
				memcmp(parsed_cseq->method.s, method->s, method->len) !=0 ) {
			rpc->fault(c, 400, "CSeq method mismatch");
			goto err;
		}
	} else {
		*cseq_is = 0;
	}

	if (msg->callid) {
		callid->s = msg->callid->body.s;
		callid->len = msg->callid->body.len;
	} else {
		callid->s = 0;
		callid->len = 0;
	}
	return 0;

err:
	return -1;
}





/** construct a "header block" from a header list.
  *
  * @return pkg_malloc'ed header block on success (with *l set to its length),
  *         0 on error.
  */
static char *get_hfblock(str *uri, struct hdr_field *hf, int proto,
							struct socket_info* ssock, int* l)
{
	struct str_list sl, *last, *i, *foo;
	int p, frag_len, total_len;
	char *begin, *needle, *dst, *ret, *d;
	str *sock_name, *portname;
	struct dest_info di;

	ret = 0; /* pessimist: assume failure */
	total_len = 0;
	last = &sl;
	last->next = 0;
	sock_name = 0;
	portname = 0;
	if (ssock){
		si_get_signaling_data(ssock, &sock_name, &portname);
	}

	for (; hf; hf = hf->next) {
		if (tm_skip_hf(hf)) continue;

		begin = needle = hf->name.s; 
		p = hf->len;

		     /* substitution loop */
		while(p) {
			d = q_memchr(needle, SUBST_CHAR, p);
			if (!d || d + 1 >= needle + p) { /* nothing to substitute */
				if (!append_str_list(begin, p, &last, &total_len)) goto error;
				break;
			} else {
				frag_len = d - begin;
				d++; /* d not at the second substitution char */
				switch(*d) {
				case SUBST_CHAR: /* double SUBST_CHAR: IP */
					     /* string before substitute */
					if (!append_str_list(begin, frag_len, &last, &total_len))
						goto error;
					     /* substitute */
					if (!sock_name) {
						if (
#ifdef USE_DNS_FAILOVER
								uri2dst(0, &di, 0, uri, proto)
#else
								uri2dst(&di, 0, uri, proto)
#endif /* USE_DNS_FAILOVER */
									== 0 ){
							LOG(L_ERR, "ERROR: get_hfblock: send_sock"
										" failed\n");
							goto error;
						}
						si_get_signaling_data(di.send_sock, &sock_name, &portname);
					}
					if (!append_str_list(sock_name->s, sock_name->len, &last,
									&total_len))
						goto error;
					/* inefficient - FIXME --andrei*/
					if (!append_str_list(":", 1, &last, &total_len)) goto error;
					if (!append_str_list(portname->s, portname->len, &last,
								&total_len)) goto error;
					/* keep going ... */
					begin = needle = d + 1;
					p -= frag_len + 2;
					continue;
				default:
					/* no valid substitution char -- keep going */
					p -= frag_len + 1;
					needle = d;
				}
			} /* possible substitute */
		} /* substitution loop */
		DBG("get_hfblock: one more hf processed\n");
	} /* header loop */
	
	if(total_len==0) {
		LM_DBG("empty result for headers block\n");
		goto error;
	}

	/* construct a single header block now */
	ret = pkg_malloc(total_len);
	if (!ret) {
		LOG(L_ERR, "get_hfblock: no pkg mem for hf block\n");
		goto error;
	}
	i = sl.next;
	dst = ret;
	while(i) {
		foo = i;
		i = i->next;
		memcpy(dst, foo->s.s, foo->s.len);
		dst += foo->s.len;
		pkg_free(foo);
	}
	*l = total_len;
	return ret;
	
 error:
	i = sl.next;
	while(i) {
		foo = i;
		i = i->next;
		pkg_free(foo);
	}
	*l = 0;
	return 0;
}


#define RPC_ROUTE_PREFIX	"Route: "
#define RPC_ROUTE_PREFIX_LEN	(sizeof(RPC_ROUTE_PREFIX)-1)
#define RPC_ROUTE_SEPARATOR	", "
#define RPC_ROUTE_SEPARATOR_LEN	(sizeof(RPC_ROUTE_SEPARATOR)-1)


/** internal print routes into rpc reply function.
 *  Prints the dialog routes. It's used internally by
 *  rpx_print_uris (called from rpc_uac_callback()).
 *  @param rpc
 *  @param c - rpc context
 *  @param reply - sip reply
 */
static void  rpc_print_routes(rpc_t* rpc, void* c,
								dlg_t* d)
{
	rr_t* ptr;
	int size;
	char* buf;
	char* p;
	
	
	if (d->hooks.first_route == 0){
		rpc->add(c, "s", "");
		return;
	}
	size=RPC_ROUTE_PREFIX_LEN;
	for (ptr=d->hooks.first_route; ptr; ptr=ptr->next)
		size+=ptr->len+(ptr->next!=0)*RPC_ROUTE_SEPARATOR_LEN;
	if (d->hooks.last_route)
		size+=RPC_ROUTE_SEPARATOR_LEN + 1 /* '<' */ + 
				d->hooks.last_route->len +1 /* '>' */;

	buf=pkg_malloc(size+1);
	if (buf==0){
		ERR("out of memory\n");
		rpc->add(c, "s", "");
		return;
	}
	p=buf;
	memcpy(p, RPC_ROUTE_PREFIX, RPC_ROUTE_PREFIX_LEN);
	p+=RPC_ROUTE_PREFIX_LEN;
	for (ptr=d->hooks.first_route; ptr; ptr=ptr->next){
		memcpy(p, ptr->nameaddr.name.s, ptr->len);
		p+=ptr->len;
		if (ptr->next!=0){
			memcpy(p, RPC_ROUTE_SEPARATOR, RPC_ROUTE_SEPARATOR_LEN);
			p+=RPC_ROUTE_SEPARATOR_LEN;
		}
	}
	if (d->hooks.last_route){
		memcpy(p, RPC_ROUTE_SEPARATOR, RPC_ROUTE_SEPARATOR_LEN);
		p+=RPC_ROUTE_SEPARATOR_LEN;
		*p='<';
		p++;
		memcpy(p, d->hooks.last_route->s, d->hooks.last_route->len);
		p+=d->hooks.last_route->len;
		*p='>';
		p++;
	}
	*p=0;
	rpc->add(c, "s", buf);
	pkg_free(buf);
	return;
}


/** internal print uri into rpc reply function.
 *  Prints the uris into rpc reply. It's used internally by
 *  rpc_uac_callback().
 *  @param rpc
 *  @param c - rpc context
 *  @param reply - sip reply
 */
static void  rpc_print_uris(rpc_t* rpc, void* c, struct sip_msg* reply)
{
	dlg_t* dlg;
	dlg=shm_malloc(sizeof(dlg_t));
	if (dlg==0){
		ERR("out of memory (shm)\n");
		return;
	}
	memset(dlg, 0, sizeof(dlg_t));
	if (dlg_response_uac(dlg, reply, TARGET_REFRESH_UNKNOWN) < 0) {
		ERR("failure while filling dialog structure\n");
		free_dlg(dlg);
		return;
	}
	if (dlg->state != DLG_CONFIRMED) {
		free_dlg(dlg);
		return;
	}
	if (dlg->hooks.request_uri->s){
		rpc->add(c, "S", dlg->hooks.request_uri);
	}else{
		rpc->add(c, "s", "");
	}
	if (dlg->hooks.next_hop->s) {
		rpc->add(c, "S", dlg->hooks.next_hop);
	} else {
		rpc->add(c, "s", "");
	}
	rpc_print_routes(rpc, c, dlg);
	free_dlg(dlg);
	return;
}



/* t_uac callback */
static void rpc_uac_callback(struct cell* t, int type, struct tmcb_params* ps)
{
	rpc_delayed_ctx_t* dctx;
	str text;
	rpc_t* rpc;
	void* c;
	int code;
	str* preason;
	
	dctx=(rpc_delayed_ctx_t*)*ps->param;
	*ps->param=0;
	if (dctx==0){
		BUG("null delayed reply ctx\n");
		return;
	}
	rpc=&dctx->rpc;
	c=dctx->reply_ctx;
	if (ps->rpl==FAKED_REPLY) {
		text.s=error_text(ps->code);
		text.len=strlen(text.s);
		code=ps->code;
		preason=&text;
		rpc->add(c, "dS", code, preason);
		rpc->add(c, "s", ""); /* request uri (rpc_print_uris)*/
		rpc->add(c, "s", ""); /* next hop (rpc_print_uris) */
		rpc->add(c, "s", ""); /* dialog routes (rpc_print_routes) */
		rpc->add(c, "s", ""); /* rest of the reply */
	}else{
		code=ps->rpl->first_line.u.reply.statuscode;
		preason=&ps->rpl->first_line.u.reply.reason;
		rpc->add(c, "dS", code, preason);
		rpc_print_uris(rpc, c, ps->rpl);
		/* print all the reply (from the first header) */
		rpc->add(c, "s", ps->rpl->headers->name.s);
	}
	rpc->delayed_ctx_close(dctx);
	ps->param=0;
}



/** rpc t_uac version-
  * It expects the following list of strings as parameters:
  *  method
  *  request_uri
  *  dst_uri (next hop) -- can be empty (either "" or ".", which is still
  *                        supported for backwards compatibility with fifo)
  *  send_socket (socket from which the message will be sent)
  *  headers (message headers separated by CRLF, at least From and To
  *           must be present)
  *  body (optional, might be null or completely missing)
  *
  * If all the parameters are ok it will call t_uac() using them.
  * Note: this version will  wait for the transaction final reply
  * only if reply_wait is set to 1. Otherwise the rpc reply will be sent 
  * immediately and it will be success if the paremters were ok and t_uac did
  * not report any error.
  * Note: reply waiting (reply_wait==1) is not yet supported.
  * @param rpc - rpc handle
  * @param  c - rpc current context
  * @param reply_wait - if 1 do not generate a rpc reply until final response
  *                     for the transaction arrives, if 0 immediately send
  *                     an rpc reply (see above).
  */
static void rpc_t_uac(rpc_t* rpc, void* c, int reply_wait)
{
	/* rpc params */
	str method, ruri, nexthop, send_socket, headers, body;
	/* other internal vars.*/
	str hfb, callid;
	struct sip_uri p_uri, pnexthop;
	struct sip_msg faked_msg;
	struct socket_info* ssock;
	str saddr;
	int sport, sproto;
	int ret, sip_error, err_ret, fromtag, cseq_is, cseq;
	char err_buf[MAX_REASON_LEN];
	dlg_t dlg;
	uac_req_t uac_req;
	rpc_delayed_ctx_t* dctx;
	
	body.s=0;
	body.len=0;
	dctx=0;
	if (reply_wait && (rpc->capabilities == 0 ||
						!(rpc->capabilities(c) & RPC_DELAYED_REPLY))) {
		rpc->fault(c, 600, "Reply wait/async mode not supported"
							" by this rpc transport");
		return;
	}
	ret=rpc->scan(c, "SSSSS*S",
					&method, &ruri, &nexthop, &send_socket, &headers, &body);
	if (ret<5 && ! (-ret == 5)){
		rpc->fault(c, 400, "too few parameters (%d/5)", ret?ret:-ret);
		return;
	}
	/* check and parse parameters */
	if (method.len==0){
		rpc->fault(c, 400, "Empty method");
		return;
	}
	if (parse_uri(ruri.s, ruri.len, &p_uri)<0){
		rpc->fault(c, 400, "Invalid request uri \"%s\"", ruri.s);
		return;
	}
	/* old fifo & unixsock backwards compatibility for nexthop: '.' is still
	   allowed */
	if (nexthop.len==1 && nexthop.s[0]=='.'){
		/* empty nextop */
		nexthop.len=0;
		nexthop.s=0;
	}else if (nexthop.len==0){
		nexthop.s=0;
	}else if (parse_uri(nexthop.s, nexthop.len, &pnexthop)<0){
		rpc->fault(c, 400, "Invalid next-hop uri \"%s\"", nexthop.s);
		return;
	}
	/* kamailio backwards compatibility for send_socket: '.' is still
	   allowed for an empty socket */
	ssock=0;
	saddr.s=0;
	saddr.len=0;
	if (send_socket.len==1 && send_socket.s[0]=='.'){
		/* empty send socket */
		send_socket.len=0;
	}else if (send_socket.len &&
				(parse_phostport(send_socket.s, &saddr.s, &saddr.len,
								&sport, &sproto)!=0 ||
				 				/* check also if it's not a MH addr. */
				 				saddr.len==0 || saddr.s[0]=='(')
				){
		rpc->fault(c, 400, "Invalid send socket \"%s\"", send_socket.s);
		return;
	}else if (saddr.len && (ssock=grep_sock_info(&saddr, sport, sproto))==0){
		rpc->fault(c, 400, "No local socket for \"%s\"", send_socket.s);
		return;
	}
	/* check headers using the SIP parser to look in the header list */
	memset(&faked_msg, 0, sizeof(struct sip_msg));
	faked_msg.len=headers.len;
	faked_msg.buf=faked_msg.unparsed=headers.s;
	if (parse_headers(&faked_msg, HDR_EOH_F, 0)==-1){
		rpc->fault(c, 400, "Invalid headers");
		return;
	}
	/* at this moment all the parameters are parsed => more sanity checks */
	if (rpc_uac_check_msg(rpc, c, &faked_msg, &method, &body, &fromtag,
							&cseq_is, &cseq, &callid)<0)
		goto error;
	hfb.s=get_hfblock(nexthop.len? &nexthop: &ruri, faked_msg.headers,
						PROTO_NONE, ssock, &hfb.len);
	if (hfb.s==0){
		rpc->fault(c, 500, "out of memory");
		goto error;
	}
	/* proceed to transaction creation */
	memset(&dlg, 0, sizeof(dlg_t));
	/* fill call-id if call-id present or else generate a callid */
	if (callid.s && callid.len) dlg.id.call_id=callid;
	else generate_callid(&dlg.id.call_id);
	
	/* We will not fill in dlg->id.rem_tag because
	 * if present it will be printed within To HF
	 */
	
	/* Generate fromtag if not present */
	if (!fromtag) {
		generate_fromtag(&dlg.id.loc_tag, &dlg.id.call_id);
	}
	
	/* Fill in CSeq */
	if (cseq_is) dlg.loc_seq.value = cseq;
	else dlg.loc_seq.value = DEFAULT_CSEQ;
	dlg.loc_seq.is_set = 1;
	
	dlg.loc_uri = faked_msg.from->body;
	dlg.rem_uri = faked_msg.to->body;
	dlg.rem_target = ruri;
	dlg.dst_uri = nexthop;
	dlg.send_sock=ssock;
	
	memset(&uac_req, 0, sizeof(uac_req));
	uac_req.method=&method;
	uac_req.headers=&hfb;
	uac_req.body=body.len?&body:0;
	uac_req.dialog=&dlg;
	if (reply_wait){
		dctx=rpc->delayed_ctx_new(c);
		if (dctx==0){
			rpc->fault(c, 500, "internal error: failed to create context");
			return;
		}
		uac_req.cb=rpc_uac_callback;
		uac_req.cbp=dctx;
		uac_req.cb_flags=TMCB_LOCAL_COMPLETED;
		/* switch to dctx, in case adding the callback fails and we
		   want to still send a reply */
		rpc=&dctx->rpc;
		c=dctx->reply_ctx;
	}
	ret = t_uac(&uac_req);
	
	if (ret <= 0) {
		err_ret = err2reason_phrase(ret, &sip_error, err_buf,
			sizeof(err_buf), "RPC/UAC") ;
		if (err_ret > 0 )
		{
			rpc->fault(c, sip_error, "%s", err_buf);
		} else {
			rpc->fault(c, 500, "RPC/UAC error");
		}
		if (dctx)
			rpc->delayed_ctx_close(dctx);
		goto error01;
	}
error01:
	if (hfb.s) pkg_free(hfb.s);
error:
	if (faked_msg.headers) free_hdr_field_lst(faked_msg.headers);
}



/** t_uac with no reply waiting.
  * @see rpc_t_uac.
  */
void rpc_t_uac_start(rpc_t* rpc, void* c)
{
	rpc_t_uac(rpc, c, 0);
}

/** t_uac with reply waiting.
  * @see rpc_t_uac.
  */
void rpc_t_uac_wait(rpc_t* rpc, void* c)
{
	rpc_t_uac(rpc, c, 1);
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */

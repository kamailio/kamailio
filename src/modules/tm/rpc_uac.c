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
#include "../../core/rpc.h"
#include "../../core/socket_info.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_from.h"
#include "../../core/str_list.h"
#include "../../core/timer_proc.h"
#include "../../core/utils/sruid.h"
#include "ut.h"
#include "dlg.h"
#include "uac.h"
#include "callid.h"


/* RPC substitution char (used in rpc_t_uac headers) */
#define SUBST_CHAR '!'

#define TM_RPC_RESPONSE_LIFETIME 300
#define TM_RPC_RESPONSE_TIMERSTEP 10

void tm_rpc_response_list_clean(unsigned int ticks, void *param);

typedef struct tm_rpc_response {
	str ruid;
	int flags;
	int rcode;
	str rtext;
	time_t rtime;
	struct tm_rpc_response *next;
} tm_rpc_response_t;

typedef struct tm_rpc_response_list {
	gen_lock_t rlock;
	tm_rpc_response_t *rlist;
} tm_rpc_response_list_t;

static tm_rpc_response_list_t *_tm_rpc_response_list = NULL;

static sruid_t _tm_rpc_sruid;

/**
 *
 */
int tm_rpc_response_list_init(void)
{
	if(_tm_rpc_response_list != NULL) {
		return 0;
	}
	if(sruid_init(&_tm_rpc_sruid, '-', "tmrp", SRUID_INC)<0) {
		LM_ERR("failed to init sruid\n");
		return -1;
	}
	if(sr_wtimer_add(tm_rpc_response_list_clean, 0,
				TM_RPC_RESPONSE_TIMERSTEP)<0) {
		LM_ERR("failed to register timer routine\n");
		return -1;
	}
	_tm_rpc_response_list = shm_malloc(sizeof(tm_rpc_response_list_t));
	if(_tm_rpc_response_list == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	memset(_tm_rpc_response_list, 0, sizeof(tm_rpc_response_list_t));

	lock_init(&_tm_rpc_response_list->rlock);

	return 0;
}

/**
 *
 */
int tm_rpc_response_list_destroy(void)
{
	tm_rpc_response_t *rl0 = NULL;
	tm_rpc_response_t *rl1 = NULL;

	if(_tm_rpc_response_list == NULL) {
		return 0;
	}

	rl1 = _tm_rpc_response_list->rlist;

	while(rl1!=NULL) {
		rl0 = rl1;
		rl1 = rl1->next;
		shm_free(rl0);
	}
	lock_destroy(&_tm_rpc_response_list->rlock);
	shm_free(_tm_rpc_response_list);
	_tm_rpc_response_list = NULL;

	return 0;
}

/**
 *
 */
int tm_rpc_response_list_add(str *ruid, int rcode, str *rtext)
{
	size_t rsize = 0;
	tm_rpc_response_t *ri = NULL;
	if(_tm_rpc_response_list == NULL) {
		LM_ERR("rpc response list not initialized\n");
		return -1;
	}

	rsize = sizeof(tm_rpc_response_t) + ruid->len + 2
				+ ((rtext!=NULL)?rtext->len:0);

	ri = (tm_rpc_response_t*)shm_malloc(rsize);
	if(ri==NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(ri, 0, rsize);

	ri->ruid.s = (char*)ri + sizeof(tm_rpc_response_t);
	ri->ruid.len = ruid->len;
	memcpy(ri->ruid.s, ruid->s, ruid->len);
	ri->rtime = time(NULL);
	ri->rcode = rcode;
	if(rtext!=NULL) {
		ri->rtext.s = ri->ruid.s + ri->ruid.len + 1;
		ri->rtext.len = rtext->len;
		memcpy(ri->rtext.s, rtext->s, rtext->len);
	}
	lock_get(&_tm_rpc_response_list->rlock);
	ri->next = _tm_rpc_response_list->rlist;
	_tm_rpc_response_list->rlist = ri;
	lock_release(&_tm_rpc_response_list->rlock);

	return 0;
}

/**
 *
 */
tm_rpc_response_t *tm_rpc_response_list_get(str *ruid)
{
	tm_rpc_response_t *ri0 = NULL;
	tm_rpc_response_t *ri1 = NULL;

	if(_tm_rpc_response_list == NULL) {
		LM_ERR("rpc response list not initialized\n");
		return NULL;
	}

	lock_get(&_tm_rpc_response_list->rlock);
	ri1 = _tm_rpc_response_list->rlist;
	while(ri1!=NULL) {
		if(ri1->ruid.len==ruid->len
				&& memcmp(ri1->ruid.s, ruid->s, ruid->len)==0) {
			if(ri0 == NULL) {
				_tm_rpc_response_list->rlist = ri1->next;
			} else {
				ri0->next = ri1->next;
			}
			lock_release(&_tm_rpc_response_list->rlock);
			return ri1;
		}
		ri0 = ri1;
		ri1 = ri1->next;
	}
	lock_release(&_tm_rpc_response_list->rlock);
	return NULL;
}

/**
 *
 */
void tm_rpc_response_list_clean(unsigned int ticks, void *param)
{
	tm_rpc_response_t *ri0 = NULL;
	tm_rpc_response_t *ri1 = NULL;
	time_t tnow;

	if(_tm_rpc_response_list == NULL) {
		return;
	}

	tnow = time(NULL);
	lock_get(&_tm_rpc_response_list->rlock);
	ri1 = _tm_rpc_response_list->rlist;
	while(ri1!=NULL) {
		if(ri1->rtime < tnow - TM_RPC_RESPONSE_LIFETIME) {
			LM_DBG("freeing item [%.*s]\n", ri1->ruid.len, ri1->ruid.s);
			if(ri0 == NULL) {
				_tm_rpc_response_list->rlist = ri1->next;
				shm_free(ri1);
				ri1 = _tm_rpc_response_list->rlist;
			} else {
				ri0->next = ri1->next;
				shm_free(ri1);
				ri1 = ri0->next;
			}
		} else {
			ri0 = ri1;
			ri1 = ri1->next;
		}
	}
	lock_release(&_tm_rpc_response_list->rlock);
}

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
		str* fromtag, int *cseq_is, int* cseq,
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
	if(parsed_from->tag_value.s && parsed_from->tag_value.len) {
		fromtag->s = parsed_from->tag_value.s;
		fromtag->len = parsed_from->tag_value.len;
	} else {
		fromtag->s = NULL;
		fromtag->len = 0;
	}

	*cseq = 0;
	if (msg->cseq && (parsed_cseq = get_cseq(msg))) {
		*cseq_is = 1;
		for (i = 0; i < parsed_cseq->number.len; i++) {
			ch = parsed_cseq->number.s[i];
			if (ch >= '0' && ch <= '9' ) {
				*cseq = (*cseq) * 10 + ch - '0';
			} else {
				LM_DBG("Found non-numerical in CSeq: <%i>='%c'\n",
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
static int get_hfblock(str *uri, struct hdr_field *hf, int proto,
		struct socket_info* ssock, str* hout)
{
	struct str_list sl, *last, *i, *foo;
	int p, frag_len, total_len;
	char *begin, *needle, *dst, *ret, *d;
	str *sock_name, *portname;
	struct dest_info di;

	hout->s = NULL;
	hout->len = 0;
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
								LM_ERR("send_sock failed\n");
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
		LM_DBG("one more hf processed\n");
	} /* header loop */

	if(total_len==0) {
		LM_DBG("empty result for headers block\n");
		return 1;;
	}

	/* construct a single header block now */
	ret = pkg_malloc(total_len);
	if (!ret) {
		PKG_MEM_ERROR;
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
	hout->len = total_len;
	hout->s = ret;
	return 0;

error:
	i = sl.next;
	while(i) {
		foo = i;
		i = i->next;
		pkg_free(foo);
	}
	return -1;
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
		PKG_MEM_ERROR;
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
		SHM_MEM_ERROR;
		return;
	}
	memset(dlg, 0, sizeof(dlg_t));
	if (dlg_response_uac(dlg, reply, TARGET_REFRESH_UNKNOWN) < 0) {
		LM_ERR("failure while filling dialog structure\n");
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


/* t_uac callback */
static void rpc_uac_block_callback(struct cell* t, int type,
		struct tmcb_params* ps)
{
	str *ruid;
	str rtext;

	ruid = (str*)(*ps->param);
	*ps->param=0;
	if (ps->rpl==FAKED_REPLY) {
		rtext.s = error_text(ps->code);
		rtext.len = strlen(rtext.s);
	} else {
		rtext = ps->rpl->first_line.u.reply.reason;
	}
	tm_rpc_response_list_add(ruid, ps->code, &rtext);
	shm_free(ruid);
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
 * immediately and it will be success if the parameters were ok and t_uac did
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
	int ret, sip_error, err_ret, cseq_is, cseq;
	str fromtag;
	char err_buf[MAX_REASON_LEN];
	dlg_t dlg;
	uac_req_t uac_req;
	rpc_delayed_ctx_t* dctx;
	str *ruid = NULL;
	tm_rpc_response_t *ritem = NULL;
	int rcount = 0;
	void* th = NULL;

	body.s=0;
	body.len=0;
	dctx=0;
	if (reply_wait==1 && (rpc->capabilities == 0 ||
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
	if(get_hfblock(nexthop.len? &nexthop: &ruri, faked_msg.headers,
			PROTO_NONE, ssock, &hfb)<0) {
		rpc->fault(c, 500, "Failed to build headers block");
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
	if (fromtag.s && fromtag.len) {
		dlg.id.loc_tag = fromtag;
	} else {
		generate_fromtag(&dlg.id.loc_tag, &dlg.id.call_id, &ruri);
	}

	/* Fill in CSeq */
	if (cseq_is) dlg.loc_seq.value = cseq;
	else dlg.loc_seq.value = DEFAULT_CSEQ;
	dlg.loc_seq.is_set = 1;

	dlg.loc_uri = get_from(&faked_msg)->uri;
	dlg.rem_uri = get_to(&faked_msg)->uri;
	if(get_to(&faked_msg)->tag_value.len > 0) {
		dlg.id.rem_tag = get_to(&faked_msg)->tag_value;
	}
	dlg.rem_target = ruri;
	dlg.dst_uri = nexthop;
	dlg.send_sock=ssock;

	memset(&uac_req, 0, sizeof(uac_req));
	uac_req.method=&method;
	if(hfb.s!=NULL && hfb.len>0) uac_req.headers=&hfb;
	uac_req.body=body.len?&body:0;
	uac_req.dialog=&dlg;
	if (reply_wait==1){
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
	} else if (reply_wait==2) {
		sruid_next(&_tm_rpc_sruid);
		uac_req.cb = rpc_uac_block_callback;
		ruid = shm_str_dup_block(&_tm_rpc_sruid.uid);
		uac_req.cbp = ruid;
		uac_req.cb_flags = TMCB_LOCAL_COMPLETED;
	}

	ret = t_uac(&uac_req);

	if (ret <= 0) {
		err_ret = err2reason_phrase(ret, &sip_error, err_buf,
				sizeof(err_buf), "RPC/UAC") ;
		if (err_ret > 0 ) {
			rpc->fault(c, sip_error, "%s", err_buf);
		} else {
			rpc->fault(c, 500, "RPC/UAC error");
		}
		if (dctx) {
			rpc->delayed_ctx_close(dctx);
		}
		if(ruid) {
			shm_free(ruid);
		}
		goto error01;
	}

	if(reply_wait==2) {
		while(ritem==NULL && rcount<800) {
			sleep_us(100000);
			rcount++;
			ritem = tm_rpc_response_list_get(&_tm_rpc_sruid.uid);
		}
		if(ritem == NULL) {
			rpc->fault(c, 500, "No response");
		} else {
			/* add structure node */
			if (rpc->add(c, "{", &th) < 0) {
				rpc->fault(c, 500, "Structure error");
			} else {
				if(rpc->struct_add(th, "dS",
						"code", 	ritem->rcode,
						"text", 	&ritem->rtext)<0) {
					rpc->fault(c, 500, "Fields error");
					return;
				}
			}
			shm_free(ritem);
		}
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

/** t_uac with blocking for reply waiting.
 * @see rpc_t_uac.
 */
void rpc_t_uac_wait_block(rpc_t* rpc, void* c)
{
	rpc_t_uac(rpc, c, 2);
}


static int t_uac_check_msg(struct sip_msg* msg,
		str* method, str* body,
		str *fromtag, int *cseq_is, int* cseq,
		str* callid)
{
	struct to_body* parsed_from;
	struct cseq_body *parsed_cseq;
	int i;
	char ch;

	if (body->len && !msg->content_type) {
		LM_ERR("Content-Type missing\n");
		goto err;
	}

	if (body->len && msg->content_length) {
		LM_ERR("Content-Length disallowed\n");
		goto err;
	}

	if (!msg->to) {
		LM_ERR("To missing\n");
		goto err;
	}

	if (!msg->from) {
		LM_ERR("From missing\n");
		goto err;
	}

	/* we also need to know if there is from-tag and add it otherwise */
	if (parse_from_header(msg) < 0) {
		LM_ERR("Error in From\n");
		goto err;
	}

	parsed_from = (struct to_body*)msg->from->parsed;
	if(parsed_from->tag_value.s && parsed_from->tag_value.len) {
		fromtag->s = parsed_from->tag_value.s;
		fromtag->len = parsed_from->tag_value.len;
	} else {
		fromtag->s = NULL;
		fromtag->len = 0;
	}

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
				LM_ERR("Non-numerical CSeq\n");
				goto err;
			}
		}

		if (parsed_cseq->method.len != method->len ||
				memcmp(parsed_cseq->method.s, method->s, method->len) !=0 ) {
			LM_ERR("CSeq method mismatch\n");
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

int t_uac_send(str *method, str *ruri, str *nexthop, str *send_socket,
		str *headers, str *body)
{
	str hfb, callid;
	struct sip_uri p_uri, pnexthop;
	struct sip_msg faked_msg;
	struct socket_info* ssock;
	str saddr;
	int sport, sproto;
	int ret, cseq_is, cseq;
	str fromtag;
	dlg_t dlg;
	uac_req_t uac_req;

	ret = -1;

	/* check and parse parameters */
	if (method->len<=0){
		LM_ERR("Empty method\n");
		return -1;
	}
	if (parse_uri(ruri->s, ruri->len, &p_uri)<0){
		LM_ERR("Invalid request uri \"%s\"", ruri->s);
		return -1;
	}
	if (nexthop->len==1 && nexthop->s[0]=='.'){
		/* empty nextop */
		nexthop->len=0;
		nexthop->s=0;
	}else if (nexthop->len==0){
		nexthop->s=0;
	}else if (parse_uri(nexthop->s, nexthop->len, &pnexthop)<0){
		LM_ERR("Invalid next-hop uri \"%s\"", nexthop->s);
		return -1;
	}
	ssock=0;
	saddr.s=0;
	saddr.len=0;
	if (send_socket->len==1 && send_socket->s[0]=='.'){
		/* empty send socket */
		send_socket->len=0;
	}else if (send_socket->len &&
			(parse_phostport(send_socket->s, &saddr.s, &saddr.len,
							 &sport, &sproto)!=0 ||
			 /* check also if it's not a MH addr. */
			 saddr.len==0 || saddr.s[0]=='(')
			){
		LM_ERR("Invalid send socket \"%s\"", send_socket->s);
		return -1;
	}else if (saddr.len && (ssock=grep_sock_info(&saddr, sport, sproto))==0){
		LM_ERR("No local socket for \"%s\"", send_socket->s);
		return -1;
	}
	/* check headers using the SIP parser to look in the header list */
	memset(&faked_msg, 0, sizeof(struct sip_msg));
	faked_msg.len=headers->len;
	faked_msg.buf=faked_msg.unparsed=headers->s;
	if (parse_headers(&faked_msg, HDR_EOH_F, 0)==-1){
		LM_ERR("Invalid headers\n");
		return -1;
	}
	/* at this moment all the parameters are parsed => more sanity checks */
	if (t_uac_check_msg(&faked_msg, method, body, &fromtag,
				&cseq_is, &cseq, &callid)<0) {
		LM_ERR("checking values failed\n");
		goto error;
	}
	if(get_hfblock(nexthop->len? nexthop: ruri, faked_msg.headers,
			PROTO_NONE, ssock, &hfb)<0) {
		LM_ERR("failed to get the block of headers\n");
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
	if (fromtag.s && fromtag.len) {
		dlg.id.loc_tag = fromtag;
	} else {
		generate_fromtag(&dlg.id.loc_tag, &dlg.id.call_id, ruri);
	}

	/* Fill in CSeq */
	if (cseq_is) dlg.loc_seq.value = cseq;
	else dlg.loc_seq.value = DEFAULT_CSEQ;
	dlg.loc_seq.is_set = 1;

	dlg.loc_uri = get_from(&faked_msg)->uri;
	dlg.rem_uri = get_to(&faked_msg)->uri;
	if(get_to(&faked_msg)->tag_value.len > 0) {
		dlg.id.rem_tag = get_to(&faked_msg)->tag_value;
	}
	dlg.rem_target = *ruri;
	dlg.dst_uri = *nexthop;
	dlg.send_sock=ssock;

	memset(&uac_req, 0, sizeof(uac_req));
	uac_req.method=method;
	if(hfb.s!=NULL && hfb.len>0) uac_req.headers=&hfb;
	uac_req.body=body->len?body:0;
	uac_req.dialog=&dlg;

	ret = t_uac(&uac_req);

	if (ret <= 0) {
		LM_ERR("UAC error\n");
		goto error01;
	}
error01:
	if (hfb.s) pkg_free(hfb.s);
error:
	if (faked_msg.headers) free_hdr_field_lst(faked_msg.headers);

	return ret;
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */

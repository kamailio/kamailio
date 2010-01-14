/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */


#include <stdlib.h>
#ifdef EXTRA_DEBUG
#include <assert.h>
#endif

#include "ut.h"
#include "config.h"
#include "sr_module.h"
#include "parser/parse_rr.h"
#include "parser/parse_from.h"
#include "rpc.h"
#include "modules/tm/dlg.h" /* dlg_t */
/* define TM's branch_bm_t used in t_cancel.h (FIXME!!!) */
#include "modules/tm/t_reply.h"
#include "modules/tm/t_cancel.h" /* F_CANCEL_B_FAKE_REPLY */

#include "tid.h"
#include "binds.h" /* for tmb.t_uac_with_ids() */
#include "appsrv.h" /* for dispatch_tm_reply() */
#include "digest.h" /* for ASI_DSGT_IDS */

#define STX	0x02
#define ETX	0x03
#define SUB	0x1A

enum ASI_REQ_FLAGS {
	ASIREQ_GEN_ACK_FLG	= 1 << 0, /* wanna generate ACK */
	ASIREQ_GET_FIN_FLG	= 1 << 1, /* wanna see final reply */
	ASIREQ_GET_PRV_FLG	= 1 << 2, /* wanna see provisional reply */
	ASIREQ_RUN_ORR_FLG	= 1 << 3, /* run 'the' on reply route */
	ASIREQ_DEL_1ST_FLG	= 1 << 4, /* 1st entry in route set, is the dst */
};

typedef struct {
	unsigned int flags;
	int as_id;
	str method;
	str opaque;
} tmcbprm_t;


int onreply_rt_idx = -1;


	/* RPC strings are 0-terminated */
#define STRIP_0TERM(_str, may0, _call) \
	do { \
		if (_str.len) \
			_str.len --; \
		if ((! _str.len) && (! may0)) { \
			rpc->add(ctx, "ds", 484, "Mandatory parameter empty."); \
			DEBUG("faulty %s() RPC: empty parameter (%s).\n", _call, #_str); \
			return; \
		} \
		DEBUG("%s(%s: `%.*s' [%d]).\n", _call, #_str, \
				_str.len, _str.s, _str.len); \
	} while (0)



/* TODO: WTF?! no such generic function[=print a URI] in all SER?!?!? */
/**
 * turn outbound socket into SIP URI.
 * @return Offset past <<scheme COLUMN>> (so that a <<user@>> can be
 * inserted) OR, on error, negative.
 */
static inline int outbound_as_uri(str *dst_uri, str *outbound_uri)
{
	struct dest_info dstinfo;
	struct socket_info* send_sock;
	const static str trans_tcp = STR_STATIC_INIT(TRANSPORT_PARAM "tcp");
	const static str sch_sip = STR_STATIC_INIT("sip:");
	const static str sch_sips = STR_STATIC_INIT("sips:");
	const str *param, *scheme;
	char *cursor;

/* TODO: having this conditional is IMO a bad ideea; why not only one
 * signature and NULL for the !USE_DNS_FAILOVER ? */
#ifdef USE_DNS_FAILOVER
	if (! uri2dst(NULL, &dstinfo, /*msg*/NULL, dst_uri, PROTO_NONE))
#else
	if (! uri2dst(&dstinfo, /*msg*/NULL, dst_uri, PROTO_NONE))
#endif
	{
		ERR("failed to resolve URI `%.*s' into socket info.\n", 
				STR_FMT(dst_uri));
		return -1;
	} else {
		send_sock = dstinfo.send_sock;
	}
	switch (send_sock->proto) {
		case PROTO_UDP:
			scheme = &sch_sip;
			param = NULL;
			break;
		case PROTO_TCP:
			scheme = &sch_sip;
			param = &trans_tcp;
			break;
		case PROTO_TLS:
			scheme = &sch_sips;
			/* TODO: does transport=tls make sense if already sips: ? */
			param = NULL;

		do {
		case PROTO_NONE: BUG("unknown protocol for outbound socket.\n"); break;
		/* TODO: sip/sips? */
		case PROTO_SCTP: BUG("unimplemented protocol (SCTP).\n"); break;
		default: BUG("invalid protocol %d.\n", send_sock->proto); break;
		} while (0);
			return -1;
	}

	if (outbound_uri->len < (scheme->len + send_sock->address_str.len + 
			/*`:'*/1 + send_sock->port_no_str.len + 
			(param ? param->len : 0))) {
		ERR("insufficient buffer to print outbound URI for destination URI"
				" `%.*s'.\n", dst_uri->len, dst_uri->s);
		return -1;
	}
	cursor = outbound_uri->s;
	memcpy(cursor, scheme->s, scheme->len);
	cursor += scheme->len;
	memcpy(cursor, send_sock->address_str.s, send_sock->address_str.len);
	cursor += send_sock->address_str.len;
	/* TODO: if  UDP/TCP & 5060 || TLS & 5071, skip port */
	*cursor = ':'; cursor ++;
	memcpy(cursor, send_sock->port_no_str.s, send_sock->port_no_str.len);
	cursor += send_sock->port_no_str.len;
	if (param) {
		memcpy(cursor, param->s, param->len);
		cursor += param->len;
	}

	outbound_uri->len = cursor - outbound_uri->s;
	return scheme->len;
}

/**
 * Replace SUB or <<STX user_name ETX>> with SIP URI of outbound socket
 * (through which the message will be dispatched).
 */
int fix_local_uris(str *uri, str old_hdrs, str *new_hdrs)
{
	char *old_end, *new_end;
	char *old_curs, *new_curs;
	char *marker, *tmp;
	char buff[MAX_URI_SIZE];
	str local_uri = {buff, sizeof(buff)};
	off_t scheme_len = 0; /* if 0, local_uri had not been resolved */
	enum {
		PARSE_SKIP,	/* cursor in 'casual' header text */
		PARSE_TEXT,	/* cursor between STX and ETX */
	} parse_state;


#define GET_LOCAL_URI \
	do { \
		if (scheme_len != 0) \
			break; \
		if ((scheme_len = outbound_as_uri(uri, &local_uri)) < 0) \
			return -1; \
	} while (0)

#define WRITE_TO_HDRS(_src_, _len) \
	do { \
		size_t _len_ = _len; \
		if (new_end < new_curs + _len_) { \
			ERR("insufficient buffer space (%d !) to fix local URIs.\n", \
					new_hdrs->len); \
			return -1; \
		} \
		memcpy(new_curs, _src_, _len_); \
		new_curs += _len_; \
	} while (0)

	for (	old_curs = old_hdrs.s, old_end = old_hdrs.s + old_hdrs.len,
			new_curs = new_hdrs->s, new_end = new_hdrs->s + new_hdrs->len,
			marker = old_hdrs.s, parse_state = PARSE_SKIP;
			old_curs < old_end; old_curs ++) {
		switch (parse_state) {
			case PARSE_SKIP:
				switch (*old_curs) {
					case STX:
						WRITE_TO_HDRS(marker, old_curs - marker);
						marker = old_curs + /*past current position*/1;
						parse_state = PARSE_TEXT;
						break;
					case ETX:
						ERR("unexpected ETX marker (after `%.*s'): no leading "
								"STX.\n", (int)(old_curs - old_hdrs.s), 
								old_hdrs.s);
						return -1;
					case SUB:
						GET_LOCAL_URI;
						WRITE_TO_HDRS(marker, old_curs - marker);
						marker = old_curs + /*past current position*/1;
						WRITE_TO_HDRS(local_uri.s, local_uri.len);
						break;
					case 0:
						WRITE_TO_HDRS(marker, old_curs - marker);
						new_hdrs->len = new_curs - new_hdrs->s;
						break; /* formal */
				}
				break;

			case PARSE_TEXT:
				switch (*old_curs) {
					do {
						case SUB: tmp = "SUB"; break;
						case STX: tmp = "STX"; break;
						case 0: tmp = "0-terminator"; break;
					} while (0);
						ERR("unexpected %s marker (after `%.*s'): in text "\
								"mode.\n", tmp, (int)(old_curs - old_hdrs.s), 
								old_hdrs.s);
						return -1;

					case ETX:
						GET_LOCAL_URI;
						WRITE_TO_HDRS(local_uri.s, scheme_len);
						WRITE_TO_HDRS(marker, old_curs - marker);
						WRITE_TO_HDRS("@", 1);
						WRITE_TO_HDRS(local_uri.s + scheme_len, 
								local_uri.len - scheme_len);
						marker = old_curs + /*past current position*/1;
						parse_state = PARSE_SKIP;
						break; /* formal */
				}
				break;

			default:
				BUG("invalid parsing state %d.\n", parse_state);
				abort();
		}
	}
	return 0;
#undef WRITE_TO_HDRS
#undef GET_LOCAL_URI
}

#ifdef ASI_FAKE_408
static inline void free_faked_408(struct sip_msg *faked)
{
#ifdef EXTRA_DEBUG
	if ((! faked) || (! faked->buf) || (! faked->len)) {
		BUG("freeing NULL faked 408.\n");
		abort();
	}
#endif
#ifdef DYN_BUF
/* DYN_BUF is deprecated (until using threads :-) ) */
#error "dynamic buffer not supported (anymore?) by ASI module"
#else
	pkg_free(faked->buf);
#endif
	free_sip_msg(faked);
	DEBUG("faked 408 free'd.\n");
}

/**
 * Fake a 408 SIP reply.
 *
 * NOTE:
 * Scenario: A->B call, through B2B. B doesn't reply (at all or only with
 * 100).
 * Problem: there is a race between the first inbound (A->SER->B2B) transaction
 * and the outbound (B2B->SER->B) one: as the inbound one is started first, it
 * will also timeout first and will reply itself to A with a 408. 
 * When the second one times out, B2B will try to reply to the first one with
 * its own 408, but will get back and error, since a 408 was already generated
 * in this (first) transaction.
 *
 * 1 Fix: make the first transaction time out slower, relative to second one
 * (in script; see t_set_fr()).
 */
static inline void copy_transport_meta(sip_msg_t *msg, struct dest_info dst)
{
	msg->rcv.src_su = dst.to;
	if (dst.send_sock) {
		msg->rcv.bind_address = dst.send_sock;
		msg->rcv.src_ip = dst.send_sock->address;
		msg->rcv.src_port = dst.send_sock->port_no;
		/* the 408 is local, generated and consumed locally. */
		msg->rcv.dst_ip = msg->rcv.src_ip;
		msg->rcv.dst_port = msg->rcv.src_port;
	}
}

#define TOTAG_MAXLEN	128
#define TOTAG_PREFIX	"408."
#define TOTAG_PREFIX_LEN	(sizeof(TOTAG_PREFIX) - /*0-term*/1)

static struct sip_msg *fake_408(struct cell *trans)
{
	char *buff_408;
	unsigned len_408;
	sip_msg_t orig;
	static sip_msg_t faked;
	struct bookmark dummy_bm;
	char totag_buff[TOTAG_MAXLEN + TOTAG_PREFIX_LEN];
	str *_to_tag, to_tag;

	DEBUG("faking 408 SIP message for UAC#0/T@%p.\n", trans);

	memset(&orig, 0, sizeof(struct sip_msg));
	orig.buf = trans->uac[0].request.buffer;
	orig.len = trans->uac[0].request.buffer_len;
#ifdef EXTRA_DEBUG
	assert(0 < orig.len);
#endif
	/* needed for parse_msg to work */
	copy_transport_meta(&orig, trans->uac[0].request.dst);
	DEBUG("re-parsing original request.\n");
	if (parse_msg(orig.buf,orig.len, &orig)!=0) {
		ERR("failed to parse original request that 408'ed.\n");
		return NULL;
	}

	/* if don't have To tag, build it by prefixing the From tag with 408.*/
	if (	(parse_headers(&orig, HDR_TO_F, 0) == 0) && 
			orig.to && (get_to(&orig)->tag_value.s == NULL) && 
			(parse_headers(&orig, HDR_FROM_F, 0) == 0) &&
			/* From must be explicitly parsed (but implicitly freed X-() ) */
			orig.from && (parse_from_header(&orig) == 0) && 
			(get_from(&orig)->tag_value.s != NULL)) {
		int totag_len = get_from(&orig)->tag_value.len < TOTAG_MAXLEN ?
				get_from(&orig)->tag_value.len : TOTAG_MAXLEN;
		memcpy(totag_buff, TOTAG_PREFIX, TOTAG_PREFIX_LEN);
		memcpy(totag_buff + TOTAG_PREFIX_LEN, get_from(&orig)->tag_value.s,
				totag_len);
		to_tag.s = totag_buff;
		to_tag.len = TOTAG_PREFIX_LEN + totag_len;
		_to_tag = &to_tag;
	} else {
		/* copy original (if exists) */
		_to_tag = NULL;
	}

	buff_408 = build_res_buf_from_sip_req(408, "Request Timedout", _to_tag, 
			&orig, &len_408, &dummy_bm);
	free_sip_msg(&orig);
	if (! buff_408) {
		ERR("failed to build 408 reply to timed out request.\n");
		return NULL;
	}
#ifdef EXTRA_DEBUG
	assert(len_408);
#endif

	memset(&faked, 0, sizeof(struct sip_msg));
	faked.buf = buff_408;
	faked.len = len_408;
	
	/* AS might want some info about message's IP level, so we copy/make up
	 * some metadata here */
	/* TODO: is this enough/safe? */
	copy_transport_meta(&faked, trans->uac[0].request.dst);

	DEBUG("parsing faked 408 reply.\n");
	if (parse_msg(faked.buf,faked.len, &faked) != 0) {
		ERR("failed to parse faked 408.\n");
		pkg_free(faked.buf);
		return NULL;
	}

	return &faked;
}

#endif

inline static tmcbprm_t *tmcbprm_new(int as_id, unsigned int flags, 
		str method, str opaque)
{
	tmcbprm_t *cbp;
	char *pos;
	size_t len;

	/* aggregate strings with structure */
	len = sizeof(tmcbprm_t);
	len += method.len;
	len += opaque.len;

	if (! (cbp = (tmcbprm_t *)shm_malloc(len))) {
		ERR("out of shm memroy.\n");
		return NULL;
	}
	memset(cbp, 0, sizeof(tmcbprm_t));
	cbp->flags = flags;
	cbp->as_id = as_id;

	pos = (char *)cbp + sizeof(tmcbprm_t);
	cbp->method.s = pos;
	memcpy(pos, method.s, method.len);
	cbp->method.len = method.len;

	if (opaque.len) {
		pos += method.len;
		cbp->opaque.s = pos;
		memcpy(pos, opaque.s, opaque.len);
		cbp->opaque.len = opaque.len;
	}

	return cbp;
}

inline static void tmcbprm_free(tmcbprm_t *cbp)
{
	shm_free(cbp);
}

static void tm_callback(struct cell *trans, int type, struct tmcb_params *cbp)
{
	enum ASI_DSGT_IDS discr;
	tmcbprm_t *mycbp;
	struct sip_msg *sipmsg;
	int ret;

#ifdef EXTRA_DEBUG
	assert(trans != T_NULL_CELL);
	assert(trans != T_UNDEFINED);
	assert(cbp);
#endif

	if (! (mycbp = (tmcbprm_t *)*cbp->param)) {
		/* TODO: this is kind of dumb: the transaction moves to terminated
		 * only once => review if really needed this guard. */
		BUG("TM callback invoked after TMCB_DESTROY.\n");
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	} else {
		DEBUG("running ASI TM CB; T@%p [%d:%d], type=%d; flags=0x%X.\n", trans,
				trans->hash_index, trans->label, type, mycbp->flags);
	}
	switch (type) {
		case TMCB_LOCAL_RESPONSE_IN:
#ifdef EXTRA_DEBUG
			assert(mycbp->flags & ASIREQ_GET_PRV_FLG);
			assert(cbp->rpl != FAKED_REPLY);
#endif
			if (200 <= cbp->rpl->REPLY_STATUS)
				/* B/c the CB will be later invoked with T._L._COMPLETED 
				 * TLRI won't filter retransmissions, while TLC will. */
				return;
			sipmsg = cbp->rpl;
			discr = ASI_DGST_ID_PRV;
			break;
		case TMCB_LOCAL_COMPLETED:
#ifdef EXTRA_DEBUG
			assert(mycbp->flags & ASIREQ_GET_FIN_FLG);
#endif
			if (cbp->rpl == FAKED_REPLY) {
#ifdef ASI_FAKE_408
				if (! (sipmsg = fake_408(trans))) {
					ERR("failed to build a faked 408 SIP message.\n");
					return;
				}
#else
				sipmsg = cbp->rpl;
#endif
			} else {
				sipmsg = cbp->rpl;
			}
			discr = ASI_DGST_ID_FIN;
			break;
		case TMCB_DESTROY:
			tmcbprm_free(mycbp);
			DEBUG("ASI TM CB param freed for T@%p.\n", mycbp);
			*cbp->param = NULL; /* retransmission guard */
			return;
		default:
			BUG("unexpected TM callback (type:%d) handed by ASI (T@0x%p) - "
					"ignoring.\n", type, trans);
#ifdef EXTRA_DEBUG
			abort();
#endif
			return;
	}

	/* this could be done when creating the transaction (by setting the 
	 * 'on_reply' field), but there is no clean way to do that safely:
	 * if T obtained from TM HT ID (slot:index), T gets ref'ed (and there is
	 * no way to unref it w/o a message); if set_t() is used when creating and
	 * get_t() afterwards, the obtained T might already be terminated/free'd.
	 */
	if (mycbp->flags & ASIREQ_RUN_ORR_FLG) {
		if (onreply_rt_idx < 0) {
			WARN("reply route flag set, but no route specified as "
					"module parameter (\"onreply_route\").\n");
		} else {
			struct run_act_ctx ra_ctx;
			int ret;
			
			init_run_actions_ctx(&ra_ctx);
			ret = run_actions(&ra_ctx, onreply_rt.rlist[onreply_rt_idx], 
					sipmsg);
			if (ret < 0)
				ERR("failed to execute reply route for local request "
								"(failed with %d).\n", ret);
			else
				DEBUG("reply route of local request returned with: %d.\n",ret);
		}
	}
	
	if ((ret = dispatch_tm_reply(mycbp->as_id, sipmsg, &mycbp->method,
			discr, tid2str(trans->hash_index, trans->label),
			mycbp->opaque.len ? &mycbp->opaque : NULL)) < 0)
		ERR("running TM reply callback failed with %d.\n", ret);

#if ASI_FAKE_408
	if (cbp->rpl == FAKED_REPLY)
		free_faked_408(sipmsg);
#endif
}


static const char* request_doc[] = { "Initiate a UAC transaction.", NULL };

/**
 * Expect RPC parameters:
 * 	AS ID (integer)
 * 	flags (integer)
 * 	method (string) : SIP method name
 * 	RURI (string) : SIP URI
 * 	From header value (string) : SIP From; must contain the tag
 * 	To header value (string) : SIP To
 * 	cseq (u integer) : numeric part only
 * 	Call-ID header value (string) : SIP Call-ID
 * 	route set (string) : SIP URIs, comma&/space separated (may be empty/NULL)
 * 	headers (string) : aditional fully built headers (maybe be empty/NULL)
 * 	body (string) : SIP message body (may be empty/NULL)
 * 	opaque (string) : to be returned in replies (may be empty/NULL)
 * 	TODO:
 * 	? headers (<name_string:values_string[]>[] or <name_string:value_string>[])
 * 	? routes  ( - || - )
 *
 * Return:
 * 	code (integer) [2xx for sucees]
 * 	SER's opaque or failure reason (string)
 */
static void rpc_uac_request(rpc_t* rpc, void* ctx)
{
	const static str invite = STR_STATIC_INIT("INVITE");
	str method, ruri, rtset, to, from, callid, headers, body, opaque;
	int as_id, flags, scseq;
	uint32_t cseq;
	dlg_t dialog;
	char hdrs_buff[BUF_SIZE];
	str hdrs = {hdrs_buff, sizeof(hdrs_buff)};
	str *headers_p, *body_p;
	rr_t *route_set;
	uac_req_t cb_req;
	int cb_flags;
	tmcbprm_t *cb_param;
	int uacret;
	unsigned int h_index, h_label;
	char err_buff[MAX_REASON_LEN], *err_p;
	int sip_err;


#define STRIP_0TERM_C(_str, may0)	STRIP_0TERM(_str,may0,"asi.uac.request()")

	if (rpc->scan(ctx, "ddSSSSdSSSSS", &as_id, &flags,
			&method, &ruri, &from, &to, 
			&scseq,
			&callid, &rtset, &headers, &body, &opaque) != 12) {
		rpc->fault(ctx, 493, "Failed to extract request parameters.");
		DEBUG("faulty asi.uac.request() RPC: failed to read params.\n");
		return;
	}
	
	/* sanity checks */

	if ((as_id < 0) || (appsrvs_count() <= as_id)) {
		rpc->add(ctx, "ds", 488, "Invalid AS ID.");
		DEBUG("faulty asi.uac.request() RPC: invalid AS ID: %d.\n", as_id);
		return;
	}

	STRIP_0TERM_C(method, false);
	STRIP_0TERM_C(ruri, false);
	STRIP_0TERM_C(from, false);
	STRIP_0TERM_C(to, false);
	STRIP_0TERM_C(callid, false);

	/* SIP's CSeq is an unsigned 32bits integer, while BINRPC streams signed */
	cseq = (uint32_t)scseq;
	/* ACK flag only makes sens for INVITE */
	if (flags & ASIREQ_GEN_ACK_FLG) {
		if (! ((method.len == invite.len) && 
				(strncmp(method.s, invite.s, invite.len) == 0))) {
			rpc->add(ctx, "ds", 606, "ACK flg can only be used with INVITEs.");
			DEBUG("faulty asi.uac.request() RPC (from AS#%d): ACK flag used "
					"with `%.*s' method.\n", as_id, STR_FMT(&method));
			return;
		}
		if ((flags & ASIREQ_GET_FIN_FLG) == 0) {
			/* enforce AS to set it, so that it is aware that it has to handle
			 * the final reply, also. */
			rpc->add(ctx, "ds", 606, "The ACK flag must be accompanied by the "
					"FINAL flag.");
			DEBUG("faulty asi.uac.request() RPC (from AS#%d): ACK without "
					"FINAL.\n", as_id);
			return;
		}
	}

	/* seems sane */

	STRIP_0TERM_C(rtset, true);
	/* headers not trimmed */
	STRIP_0TERM_C(body, true);
	STRIP_0TERM_C(opaque, true);
	
	DEBUG("new asi.uac.request() RPC call "
			"(AS#%d;flags:%d;rtset:%.*s;opaque:%.*s):"
			" %.*s %.*s "
			"<%.*s,%.*s,%u,%.*s>"
			" [%.*s] {%.*s}.\n", 
			as_id, flags, STR_FMT(&rtset), STR_FMT(&opaque),
			STR_FMT(&method), STR_FMT(&ruri), 
			STR_FMT(&from), STR_FMT(&to), cseq, STR_FMT(&callid),
			STR_FMT(&headers), STR_FMT(&body));

	/* TODO: the dialog is built manaually (no API used) b/c it's only used 
	 * for retransmission buffer building => add some API for the case 
	 * The new_dlg_uac() nearly fits the purpose BUT:
	 * 	- it allocates (<> stack variable enough)
	 * 	- it expects From tag (the From value as received from AS contains it
	 * 	and extracting it is not needed: it would only be used to print it)
	 * */
	memset(&dialog, 0, sizeof(dlg_t));
	dialog.loc_seq = (dlg_seq_t){cseq, 1};
	dialog.loc_uri = from;
	dialog.rem_uri = to;
	dialog.id.call_id = callid;
	dialog.rem_target = ruri;

	if (rtset.len) {
		if (parse_rr_body(rtset.s, rtset.len, &route_set) != 0) {
			rpc->fault(ctx, 493, "Failed to parse route set.");
			DEBUG("failed to parse route set '%.*s'.\n", STR_FMT(&rtset));
			return;
		}
		if (flags & ASIREQ_DEL_1ST_FLG) {
			dialog.dst_uri = route_set->nameaddr.uri;
			dialog.route_set = route_set->next;
		} else {
			dialog.route_set = route_set;
		}
	} else {
		route_set = 0;
	}

	if (/*0-term*/1 < headers.len) {
		str dst_uri = dialog.dst_uri.len ? dialog.dst_uri : dialog.rem_target;
		if (fix_local_uris(&dst_uri, headers, &hdrs) < 0) {
			rpc->fault(ctx, 500, "Failed to fix local URIs.\n");
			return;
		}
		headers_p = &hdrs;
	} else {
		headers_p = NULL;
	}
	body_p = body.len ? &body : NULL;

	cb_flags = TMCB_DESTROY;
	if (flags) {
		if (! (cb_param = (void *)tmcbprm_new(as_id, flags, method, opaque))) {
			ERR("RPC dropped due to callback param aggregating failure.\n");
			goto end;
		}
		if (flags & ASIREQ_GET_FIN_FLG)
			cb_flags |= TMCB_LOCAL_COMPLETED;
		if (flags & ASIREQ_GET_PRV_FLG)
			cb_flags |= TMCB_LOCAL_RESPONSE_IN;
		if (flags & ASIREQ_GEN_ACK_FLG)
			cb_flags |= TMCB_DONT_ACK;
	} else {
		cb_param = NULL;
	}
	set_uac_req(&cb_req, &method, headers_p, body_p, &dialog, 
			cb_flags, tm_callback, cb_param);

	/* have everything set: start T & dispatch away */

	/* TODO: collect error from SEND_BUFFER & retr arming; OR call TERMINATED
	 * callback; OTHERWISE the shm param remains leaking */
	if ((uacret = tmb.t_uac_with_ids(&cb_req, &h_index, &h_label)) < 0) {
		if (cb_param)
			tmcbprm_free(cb_param);
		if (err2reason_phrase(uacret, &sip_err, err_buff, sizeof(err_buff), 
				"ASI/UAC") <= 0) {
			sip_err = 500;
			err_p = "Failed to create ASI/UAC";
		} else {
			err_p = err_buff;
		}
		rpc->fault(ctx, sip_err, err_p);
	} else {
		rpc->add(ctx, "dS", 200, tid2str(h_index, h_label));
		DEBUG("successfully created new ASI/UAC (%.*s to %.*s (%.*s)).\n",
				STR_FMT(&method), STR_FMT(dialog.hooks.next_hop), 
				STR_FMT(&ruri));
	}

end:
	if (route_set)
		free_rr(&route_set);
#undef STRIP_0TERM_C
}

/**
 * Note: if returns non-0, the returned T is ref'ed!
 */
inline static struct cell *tmhtid2trans(const str *tmhtid)
{
	str idx_str, label_str;
	char *column;
	unsigned int h_index, h_label;
	struct cell *trans;

	if (! (column = q_memchr(tmhtid->s, ':', tmhtid->len))) {
		DEBUG("no column found in TMHTID `%.*s'.\n", STR_FMT(tmhtid));
		return NULL;
	}
	
	idx_str.s = tmhtid->s;
	idx_str.len = column - idx_str.s;
	label_str.s = column + 1;
	label_str.len = tmhtid->len - /*0-term*/1 - idx_str.len - /*`:'*/1;
	if ((str2int(&idx_str, &h_index) < 0) || 
			(str2int(&label_str, &h_label) < 0)) {
		DEBUG("failed to convert identifiers from TMHTID `%.*s' into unsigned"
				" integers.\n", STR_FMT(tmhtid));
		return NULL;
	}

	if (tmb.t_lookup_ident(&trans, h_index, h_label) < 0) {
		DEBUG("no transaction found for identifiers %u:%u.\n", 
				h_index, h_label);
		return NULL;
	}
	
	return trans;
}


static const char* cancel_doc[] = {"Cancel an ongoing UAC transaction.", NULL};

/**
 * Expected RPC parameters:
 * 	TM HT ID (string)
 *
 * Return:
 * 	code (integer) [2xx for sucess]
 * 	reason (string)

 */
static void rpc_uac_cancel(rpc_t* rpc, void* ctx)
{
	str tmhtid;
	struct cell *trans;

	if (rpc->scan(ctx, "S", &tmhtid) != 1) {
		rpc->fault(ctx, 493, "Failed to extract request parameters.");
		DEBUG("faulty asi.uac.cancel() RPC: failed to read params.\n");
		return;
	}
	DEBUG("asi.uac.cancel(TMHTID: `%.*s').\n", STR_FMT(&tmhtid));

#ifndef E2E_CANCEL_HOP_BY_HOP
#error "ASI module can only cancel local UACs in E2E_CANCEL_HOP_BY_HOP tm mode"
#endif
	if (! (trans = tmhtid2trans(&tmhtid))) {
		rpc->add(ctx, "ds", 404, "No transaction found for identifiers.");
		DEBUG("No transaction found for identifiers `%.*s'.\n",STR_FMT(&tmhtid));
		return;
	}

	/* TODO: if SER rcvd no reply for REQ, the final CB will be fired only
	 * after initial T expires (<>as fast as canceling) */

	/* TODO: cancel with F_CANCEL_B_FAKE_REPLY crashes SER if no 
	 * reply received */
	if (tmb.cancel_all_uacs(trans, F_CANCEL_UNREF) < 0) {
		rpc->fault(ctx, 500, "Internal Server Error");
		BUG("cancel_uac returned negative.\n");
		/* TODO: can this return negative?
		 * FIXME: if yes, unref the T.
		 */
	} else {
		rpc->add(ctx, "ds", 200, "Transaction successfully canceled.\n");
		DEBUG("Successfully canceled transaction.\n");
	}
}


static const char* ack_doc[] = { "ACK a received reply.", NULL };

/**
 * Expected RPC parameters:
 * 	TM HT ID (string)
 * 	headers (string)
 * 	body (string)
 *
 * Return:
 * 	code (integer) [2xx for sucess]
 * 	reason (string)
 */
static void rpc_uac_ack(rpc_t* rpc, void* ctx)
{
	str tmhtid, body, hdrs;
	struct cell *trans;
	int ret;

	if (rpc->scan(ctx, "SSS", &tmhtid, &hdrs, &body) != 3) {
		rpc->fault(ctx, 493, "Failed to extract requst parameters.");
		DEBUG("faulty asi.uac.ack() RPC: failed to read params.\n");
		return;
	}

	if (! (trans = tmhtid2trans(&tmhtid))) {
		rpc->add(ctx, "ds", 404, "No transaction found for identifiers.");
		DEBUG("No transaction found for identifiers `%.*s'.\n",STR_FMT(&tmhtid));
		return;
	}

	STRIP_0TERM(hdrs, true, "asi.uac.ack()");
	STRIP_0TERM(body, true, "asi.uac.ack()");

#ifndef WITH_AS_SUPPORT
#error "no AS support enabled (WITH_AS_SUPPORT missing)"
#endif
	if ((ret = tmb.ack_local_uac(trans, &hdrs, &body)) < 0) {
		if (ret == -2)
			rpc->add(ctx, "ds", 400, "Invalid call (illegal state)");
		else
			rpc->add(ctx, "ds", 500, "Internal Server Error");
		ERR("ack_local_uac failed (with %d).\n", ret);
	} else {
		rpc->add(ctx, "ds", 200, "Transaction successfully ACKed.\n");
		DEBUG("Successfully ACKed transaction.\n");
	}
}


static const char* reply_doc[] = { "Reply to a received request.", NULL };

/**
 * Expect RPC params:
 * 	TM HT ID (string)
 * 	code (int)
 * 	reason (string)
 * 	totag (string)
 * 	headers (string)
 * 	body (string)
 *
 * Return:
 * 	code (integer) [2xx for sucess]
 * 	reason (string)
 */
static void rpc_uas_reply(rpc_t* rpc, void* ctx)
{
	int code, ret;
	str tmhtid, reason, totag, headers, body;
	struct cell *trans;

	if (rpc->scan(ctx, "SdSSSS", &tmhtid, &code, &reason, &totag, &headers, 
			&body) != 6) {
		rpc->fault(ctx, 493, "Failed to extract request parameters.");
		DEBUG("faulty asi.uas.reply(): failed to read params.\n");
		return;
	}
#ifdef EXTRA_DEBUG
	DEBUG("asi.uas.reply(tmhtid:%.*s,code:%d,reason:%.*s,totag:%.*s,"
			"headers:%.*s,body:%.*s) invoked.\n", STR_FMT(&tmhtid), code, 
			STR_FMT(&reason), STR_FMT(&totag), STR_FMT(&headers), 
			STR_FMT(&body));
#endif

	/* sanity checks */
	if ((code < 100) || (700 <= code)) {
		rpc->add(ctx, "ds", 606, "Illegal SIP code.");
		DEBUG("faulty asi.uas.reply() RPC: illegal SIP code %d.\n", code);
		return;
	}

	if (! (trans = tmhtid2trans(&tmhtid))) {
		rpc->add(ctx, "ds", 404, "No transaction found for identifiers.");
		DEBUG("No transaction found for identifiers `%.*s'.\n",STR_FMT(&tmhtid));
		return;
	}

	if (is_local(trans)) {
		rpc->add(ctx, "ds", 400, "Can not reply to a local transaction.");
		DEBUG("Trying to reply to a locally initiated transaction with "
				"identifiers. `%.*s'.\n",STR_FMT(&tmhtid));
		return;
	}

	/* looks sane */
	
	/* TODO: check if makes sens to fix any local URIs in outgoing replies */

	ret = tmb.t_reply_with_body(trans, code, reason.s, body.s, headers.s,
			totag.s);
	if (rpc->add(ctx, "ds", (ret < 0) ? 500 : 200, "Replying failed.") < 0) {
		ERR("failed to build RPC reply.\n");
	} else {
		DEBUG("successfully replied.\n");
	}
}


#ifdef ASI_WITH_RESYNC
static const char* resync_doc[] = { "Set resync flag forcing AS reconnecting", 
		NULL };

static void rpc_resync(rpc_t *rpc, void *ctx)
{
	str as_uri;
	int ret, as_id;
	int serial, proto;
	char *as_id_str;
	int as_id_len;

	if (rpc->scan(ctx, "dSd", &proto, &as_uri, &serial) != 3) {
		rpc->fault(ctx, 493, "Failed to extract call parameters "
				"(proto, URI, serial).\n");
		DEBUG("faulty as.resync(): failed to read params.\n");
		return;
	}
	DEBUG("Requesting connection renewing with AS@%s, serial:%d; proto:%d.\n", 
			as_uri.s, serial, proto);
	if (proto != ASI_VERSION) {
		ERR("unsupported SASI version %d.\n", proto);
		ret = 505;
		rpc->add(ctx, "ds", ret, "SASI version not supported.");
	} else {
		switch ((ret = appsrv_set_resync(as_uri.s, serial, &as_id))) {
			case EBADMSG:
				ret = 493;
				rpc->add(ctx, "ds", ret, "failed to parse URI.");
				break;
			case EADDRNOTAVAIL:
				ret = 404;
				rpc->add(ctx, "ds", ret, "no AS for URI.");
				break;
			case EINPROGRESS:
			case 0:
				as_id_str = int2str(as_id, &as_id_len);
#ifdef EXTRA_DEBUG
				/* INT2STR_MAX_LEN must be large enough to hold a 0-term */
				assert(as_id_len < INT2STR_MAX_LEN); 
#endif
				as_id_str[as_id_len] = 0;
				/* 202 can occur when multiple AS threads push a resync */
				ret = 200 + (ret ? 2 : 0);
				rpc->add(ctx, "ds", ret, as_id_str);
				break;
			default:
				BUG("unexpected ret val %d.\n", ret);
#ifdef EXTRA_DEBUG
				abort();
#endif
				ret = 500;
				rpc->add(ctx, "ds", ret, "server interal error.");
		}
	}
	DEBUG("as.resync(%s, %d) -> %d.\n", as_uri.s, serial, ret);
}
#endif /* ASI_WITH_RESYNC */

rpc_export_t mod_rpc[] = {
	{"asi.uac.request",	rpc_uac_request,	request_doc,	/*flags*/0},
	{"asi.uac.cancel",	rpc_uac_cancel	,	cancel_doc,		/*flags*/0},
	{"asi.uac.ack",		rpc_uac_ack	,		ack_doc,		/*flags*/0},
	{"asi.uas.reply",	rpc_uas_reply,		reply_doc,		/*flags*/0},
#ifdef ASI_WITH_RESYNC
	{"asi.resync",		rpc_resync,			resync_doc,		/*flags*/0},
#endif
	{0, 0, 0, 0}
};


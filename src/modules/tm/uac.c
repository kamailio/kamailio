/*
 * simple UAC for things such as SUBSCRIBE or SMS gateway;
 * no authentication and other UAC features -- just send
 * a message, retransmit and await a reply; forking is not
 * supported during client generation, in all other places
 * it is -- adding it should be simple
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <string.h>
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../core/globals.h"
#include "../../core/md5.h"
#include "../../core/crc.h"
#include "../../core/ip_addr.h"
#include "../../core/dset.h"
#include "../../core/socket_info.h"
#include "../../core/compiler_opt.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/rand/kam_rand.h"
#include "config.h"
#include "ut.h"
#include "h_table.h"
#include "t_hooks.h"
#include "t_funcs.h"
#include "t_msgbuilder.h"
#include "callid.h"
#include "uac.h"
#include "t_stats.h"
#ifdef USE_DNS_FAILOVER
#include "../../core/dns_cache.h"
#include "../../core/cfg_core.h" /* cfg_get(core, core_cfg, use_dns_failover) */
#endif
#ifdef WITH_EVENT_LOCAL_REQUEST
#include "../../core/data_lump.h"
#include "../../core/receive.h"
#include "../../core/route.h"
#include "../../core/action.h"
#include "../../core/onsend.h"
#include "t_lookup.h"
#include "t_fwd.h"
#endif

#define FROM_TAG_LEN (MD5_LEN + 1 /* - */ + CRC16_LEN) /* length of FROM tags */

#ifdef WITH_EVENT_LOCAL_REQUEST
/* where to go for the local request route ("tm:local-request") */
int goto_on_local_req=-1; /* default disabled */
#endif /* WITH_EVEN_LOCAL_REQuEST */

static char from_tag[FROM_TAG_LEN + 1];

extern str tm_event_callback;
/*
 * Initialize UAC
 */
int uac_init(void)
{
	str src[3];
	struct socket_info *si;

	if (KAM_RAND_MAX < TABLE_ENTRIES) {
		LM_WARN("uac does not spread across the whole hash table\n");
	}
	/* on tcp/tls bind_address is 0 so try to get the first address we listen
	 * on no matter the protocol */
	si=bind_address?bind_address:get_first_socket();
	if (si==0){
		LM_CRIT("BUG - null socket list\n");
		return -1;
	}

	/* calculate the initial From tag */
	src[0].s = "Long live " NAME " server";
	src[0].len = strlen(src[0].s);
	src[1].s = si->address_str.s;
	src[1].len = strlen(src[1].s);
	src[2].s = si->port_no_str.s;
	src[2].len = strlen(src[2].s);

	MD5StringArray(from_tag, src, 3);
	from_tag[MD5_LEN] = '-';
	return 1;
}


/*
 * Generate a From tag
 */
void generate_fromtag(str* tag, str* callid)
{
	     /* calculate from tag from callid */
	crcitt_string_array(&from_tag[MD5_LEN + 1], callid, 1);
	tag->s = from_tag;
	tag->len = FROM_TAG_LEN;
}


/*
 * Check value of parameters
 */
static inline int check_params(uac_req_t *uac_r, str* to, str* from)
{
	if (!uac_r || !uac_r->method || !to || !from) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	if (!uac_r->method->s || !uac_r->method->len) {
		LM_ERR("Invalid request method\n");
		return -2;
	}

	if (!to->s || !to->len) {
		LM_ERR("Invalid To URI\n");
		return -4;
	}

	if (!from->s || !from->len) {
		LM_ERR("Invalid From URI\n");
		return -5;
	}
	return 0;
}

static inline unsigned int dlg2hash( dlg_t* dlg )
{
	str cseq_nr;
	unsigned int hashid;

	cseq_nr.s=int2str(dlg->loc_seq.value, &cseq_nr.len);
	hashid=hash(dlg->id.call_id, cseq_nr);
	LM_DBG("hashid %d\n", hashid);
	return hashid;
}

/**
 * refresh hdr shortcuts inside new buffer
 */
int uac_refresh_hdr_shortcuts(tm_cell_t *tcell, char *buf, int buf_len)
{
	sip_msg_t lreq;
	struct cseq_body *cs;

	if(likely(build_sip_msg_from_buf(&lreq, buf, buf_len, inc_msg_no())<0)) {
		LM_ERR("failed to parse msg buffer\n");
		return -1;
	}
	if(parse_headers(&lreq,HDR_CSEQ_F|HDR_CALLID_F|HDR_FROM_F|HDR_TO_F,0)<0) {
		LM_ERR("failed to parse headers in new message\n");
		goto error;
	}
	tcell->from.s = lreq.from->name.s;
	tcell->from.len = lreq.from->len;
	tcell->to.s = lreq.to->name.s;
	tcell->to.len = lreq.to->len;
	tcell->callid.s = lreq.callid->name.s;
	tcell->callid.len = lreq.callid->len;

	cs = get_cseq(&lreq);
	tcell->cseq_n.s = lreq.cseq->name.s;
	tcell->cseq_n.len = (int)(cs->number.s + cs->number.len - lreq.cseq->name.s);

	LM_DBG("cseq: [%.*s]\n", tcell->cseq_n.len, tcell->cseq_n.s);
	lreq.buf=0; /* covers the obsolete DYN_BUF */
	free_sip_msg(&lreq);
	return 0;

error:
	lreq.buf=0; /* covers the obsolete DYN_BUF */
	free_sip_msg(&lreq);
	return -1;
}


#if defined(USE_DNS_FAILOVER) || defined(WITH_EVENT_LOCAL_REQUEST)
static inline int t_build_msg_from_buf(
			struct sip_msg *msg, char *buf, int buf_len,
			uac_req_t *uac_r, struct dest_info *dst)
{
	if (unlikely(build_sip_msg_from_buf(msg, buf, buf_len, inc_msg_no()) != 0)) {
		return -1;
	}
	msg->force_send_socket = uac_r->dialog->send_sock;
	msg->rcv.proto = dst->send_sock->proto;
	msg->rcv.src_ip = dst->send_sock->address;
	su2ip_addr(&msg->rcv.dst_ip, &dst->to);
	msg->rcv.src_port = dst->send_sock->port_no;
	msg->rcv.dst_port = su_getport(&dst->to);
	msg->rcv.src_su=dst->send_sock->su;
	msg->rcv.bind_address=dst->send_sock;
#ifdef USE_COMP
	msg->rcv.comp=dst->comp;
#endif /* USE_COMP */

	return 0;
}

#ifdef WITH_EVENT_LOCAL_REQUEST
static inline int t_run_local_req(
		char **buf, int *buf_len,
		uac_req_t *uac_r,
		struct cell *new_cell, struct retr_buf *request)
{
	struct sip_msg lreq = {0};
	struct onsend_info onsnd_info;
	tm_xlinks_t backup_xd;
	int sflag_bk;
	char *buf1;
	int buf_len1;
	int backup_route_type;
	struct cell *backup_t;
	int backup_branch;
	unsigned int backup_msgid;
	int refresh_shortcuts = 0;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("tm:local-request");

	LM_DBG("executing event_route[tm:local-request]\n");
	if (unlikely(t_build_msg_from_buf(&lreq, *buf, *buf_len, uac_r, &request->dst))) {
		return -1;
	}
	if (unlikely(set_dst_uri(&lreq, uac_r->dialog->hooks.next_hop))) {
		LM_ERR("failed to set dst_uri");
		free_sip_msg(&lreq);
		return -1;
	}
	sflag_bk = getsflags();
	tm_xdata_swap(new_cell, &backup_xd, 0);

	onsnd_info.to=&request->dst.to;
	onsnd_info.send_sock=request->dst.send_sock;
	onsnd_info.buf=*buf;
	onsnd_info.len=*buf_len;
	p_onsend=&onsnd_info;

	/* run the route */
	backup_route_type = get_route_type();
	set_route_type(LOCAL_ROUTE);
	/* set T to the current transaction */
	backup_t=get_t();
	backup_branch=get_t_branch();
	backup_msgid=global_msg_id;
	/* fake transaction and message id */
	global_msg_id=lreq.id;
	set_t(new_cell, T_BR_UNDEFINED);
	if(goto_on_local_req>=0) {
		run_top_route(event_rt.rlist[goto_on_local_req], &lreq, 0);
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			LM_WARN("event callback (%s) set, but no cfg engine\n",
					tm_event_callback.s);
		} else {
			if(keng->froute(&lreq, EVENT_ROUTE,
						&tm_event_callback, &evname)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	/* restore original environment */
	set_t(backup_t, backup_branch);
	global_msg_id=backup_msgid;
	set_route_type( backup_route_type );
	p_onsend=0;

	/* restore original environment */
	tm_xdata_swap(new_cell, &backup_xd, 1);
	setsflagsval(sflag_bk);

	/* rebuild the new message content */
	if(lreq.force_send_socket != uac_r->dialog->send_sock) {
		LM_DBG("Send socket updated to: %.*s",
				lreq.force_send_socket->address_str.len,
				lreq.force_send_socket->address_str.s);

		/* rebuild local Via - remove previous value
			* and add the one for the new send socket */
		if (!del_lump(&lreq, lreq.h_via1->name.s - lreq.buf,
					lreq.h_via1->len, 0)) {
			LM_ERR("Failed to remove previous local Via\n");
			/* attempt a normal update to give it a chance */
			goto normal_update;
		}

		/* reuse same branch value from previous local Via */
		memcpy(lreq.add_to_branch_s, lreq.via1->branch->value.s,
				lreq.via1->branch->value.len);
		lreq.add_to_branch_len = lreq.via1->branch->value.len;

		/* update also info about new destination and send sock */
		uac_r->dialog->send_sock=lreq.force_send_socket;
		request->dst.send_sock = lreq.force_send_socket;
		request->dst.proto = lreq.force_send_socket->proto;

		LM_DBG("apply new updates with Via to sip msg\n");
		buf1 = build_req_buf_from_sip_req(&lreq,
				(unsigned int*)&buf_len1, &request->dst, BUILD_IN_SHM);
		if (likely(buf1)){
			shm_free(*buf);
			*buf = buf1;
			*buf_len = buf_len1;
			/* a possible change of the method is not handled! */
			refresh_shortcuts = 1;
		}

	} else {
normal_update:
		if (unlikely(lreq.add_rm || lreq.body_lumps || lreq.new_uri.s)) {
			LM_DBG("apply new updates without Via to sip msg\n");
			buf1 = build_req_buf_from_sip_req(&lreq,
					(unsigned int*)&buf_len1,
					&request->dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE|
					BUILD_IN_SHM);
			if (likely(buf1)){
				shm_free(*buf);
				*buf = buf1;
				*buf_len = buf_len1;
				/* a possible change of the method is not handled! */
				refresh_shortcuts = 1;
			}
		}
	}

	/* clean local msg structure */
	if (unlikely(lreq.new_uri.s))
	{
		pkg_free(lreq.new_uri.s);
		lreq.new_uri.s=0;
		lreq.new_uri.len=0;
	}
	if (unlikely(lreq.dst_uri.s))
	{
		pkg_free(lreq.dst_uri.s);
		lreq.dst_uri.s=0;
		lreq.dst_uri.len=0;
	}
	lreq.buf=0; /* covers the obsolete DYN_BUF */
	free_sip_msg(&lreq);
	return refresh_shortcuts;
}
#endif /* WITH_EVENT_LOCAL_REQUEST */
#endif /* defined(USE_DNS_FAILOVER) || defined(WITH_EVENT_LOCAL_REQUEST) */


/* WARNING: - dst_cell contains the created cell, but it is un-referenced
 *            (before using it make sure you REF() it first)
 *          - if  ACK (method==ACK), a cell will be created but it will not
 *            be added in the hash table (should be either deleted by the
 *            caller)
 */
static inline int t_uac_prepare(uac_req_t *uac_r,
		struct retr_buf **dst_req,
		struct cell **dst_cell)
{
	struct dest_info dst;
	struct cell *new_cell;
	struct retr_buf *request;
	char *buf;
	int buf_len, ret;
	unsigned int hi;
	int is_ack;
	ticks_t lifetime;
	long nhtype;
	snd_flags_t snd_flags;
	tm_xlinks_t backup_xd;
	tm_xdata_t local_xd;
	int refresh_shortcuts = 0;
	int sip_msg_len;
#ifdef USE_DNS_FAILOVER
	static struct sip_msg lreq;
#endif /* USE_DNS_FAILOVER */

	ret=-1;
	hi=0; /* make gcc happy */
	/*if (dst_req) *dst_req = NULL;*/
	is_ack = (((uac_r->method->len == 3) && (memcmp("ACK",
						uac_r->method->s, 3)==0)) ? 1 : 0);

	/*** added by dcm
	 * - needed by external ua to send a request within a dlg
	 */
	if ((nhtype = w_calculate_hooks(uac_r->dialog)) < 0)
		/* if err's returned, the message is incorrect */
		goto error3;

	if (!uac_r->dialog->loc_seq.is_set) {
		/* this is the first request in the dialog,
		set cseq to default value now - Miklos */
		uac_r->dialog->loc_seq.value = DEFAULT_CSEQ;
		uac_r->dialog->loc_seq.is_set = 1;
	}

	/* build cell sets X/AVP lists to new transaction structure
	 * => backup in a tmp struct and restore afterwards */
	memset(&local_xd, 0, sizeof(tm_xdata_t));
	tm_xdata_replace(&local_xd, &backup_xd);
	new_cell = build_cell(0);
	tm_xdata_replace(0, &backup_xd);

	if (!new_cell) {
		ret=E_OUT_OF_MEM;
		LM_ERR("short of cell shmem\n");
		goto error3;
	}

	LM_DBG("next_hop=<%.*s>\n",uac_r->dialog->hooks.next_hop->len,
			uac_r->dialog->hooks.next_hop->s);
	/* new message => take the dialog send_socket if set, or the default
	  send_socket if not*/
	SND_FLAGS_INIT(&snd_flags);

	if (uac_r->dialog->send_sock != NULL)
	{
		snd_flags.f |= SND_F_FORCE_SOCKET;
	}

#ifdef USE_DNS_FAILOVER
	if ((uri2dst2(cfg_get(core, core_cfg, use_dns_failover) ? &new_cell->uac[0].dns_h : 0,
			&dst, uac_r->dialog->send_sock, snd_flags,
			uac_r->dialog->hooks.next_hop, PROTO_NONE)==0)
				|| (dst.send_sock==0)){
#else /* USE_DNS_FAILOVER */
	if ((uri2dst2(&dst, uac_r->dialog->send_sock, snd_flags,
					uac_r->dialog->hooks.next_hop, PROTO_NONE)==0) ||
			(dst.send_sock==0)){
#endif /* USE_DNS_FAILOVER */
		ser_error = E_NO_SOCKET;
		ret=ser_error;
		LM_ERR("no socket found\n");
		goto error2;
	}

	if (uac_r->method->len==INVITE_LEN && memcmp(uac_r->method->s, INVITE, INVITE_LEN)==0){
		new_cell->flags |= T_IS_INVITE_FLAG;
		new_cell->flags|=T_AUTO_INV_100 &
				(!cfg_get(tm, tm_cfg, tm_auto_inv_100) -1);
#ifdef WITH_AS_SUPPORT
		if (uac_r->cb_flags & TMCB_DONT_ACK)
			new_cell->flags |= T_NO_AUTO_ACK;
#endif
		lifetime=cfg_get(tm, tm_cfg, tm_max_inv_lifetime);
	}else
		lifetime=cfg_get(tm, tm_cfg, tm_max_noninv_lifetime);
	new_cell->flags |= T_IS_LOCAL_FLAG;
	/* init timers hack, new_cell->fr_timer and new_cell->fr_inv_timer
	 * must be set, or else the fr will happen immediately
	 * we can't call init_new_t() because we don't have a sip msg
	 * => we'll ignore t_set_fr() or avp timer value and will use directly the
	 * module params fr_inv_timer and fr_timer -- andrei */
	new_cell->fr_timeout=cfg_get(tm, tm_cfg, fr_timeout);
	new_cell->fr_inv_timeout=cfg_get(tm, tm_cfg, fr_inv_timeout);
	new_cell->end_of_life=get_ticks_raw()+lifetime;
#ifdef TM_DIFF_RT_TIMEOUT
	/* same as above for retransmission intervals */
	new_cell->rt_t1_timeout_ms = cfg_get(tm, tm_cfg, rt_t1_timeout_ms);
	new_cell->rt_t2_timeout_ms = cfg_get(tm, tm_cfg, rt_t2_timeout_ms);
#endif

	set_kr(REQ_FWDED);

	request = &new_cell->uac[0].request;
	request->dst = dst;
	request->flags |= nhtype;

#ifdef SO_REUSEPORT
	if (cfg_get(tcp, tcp_cfg, reuse_port) && 
			uac_r->ssock!=NULL && uac_r->ssock->len>0 &&
			request->dst.send_sock->proto == PROTO_TCP) {
		request->dst.send_flags.f |= SND_F_FORCE_SOCKET;
	}
#endif

	if (!is_ack) {
#ifdef TM_DEL_UNREF
		INIT_REF(new_cell, 1); /* ref'ed only from the hash */
#endif
		hi=dlg2hash(uac_r->dialog);
		LOCK_HASH(hi);
		insert_into_hash_table_unsafe(new_cell, hi);
		UNLOCK_HASH(hi);
	}

	buf = build_uac_req(uac_r->method, uac_r->headers, uac_r->body, uac_r->dialog, 0, new_cell,
		&buf_len, &dst);
	if (!buf) {
		LM_ERR("Error while building message\n");
		ret=E_OUT_OF_MEM;
		goto error1;
	}

#ifdef WITH_EVENT_LOCAL_REQUEST
	if (unlikely(goto_on_local_req>=0 || tm_event_callback.len>0)) {
		refresh_shortcuts = t_run_local_req(&buf, &buf_len, uac_r, new_cell, request);
	}
#endif

#ifdef USE_DNS_FAILOVER
	/* Set the outgoing message as UAS, so the failover code has something to work with */
	if(cfg_get(core, core_cfg, use_dns_failover)) {
		if(likely(t_build_msg_from_buf(&lreq, buf, buf_len, uac_r, &dst) == 0)) {
			if (parse_headers(&lreq, HDR_EOH_F, 0) == -1) {
				LM_ERR("failed to parse headers on uas for failover\n");
			} else {
				new_cell->uas.request = sip_msg_cloner(&lreq, &sip_msg_len);
				lreq.buf=0; /* covers the obsolete DYN_BUF */
				free_sip_msg(&lreq);
				if (!new_cell->uas.request) {
					LM_ERR("no more shmem\n");
					goto error1;
				}
				new_cell->uas.end_request=((char*)new_cell->uas.request)+sip_msg_len;
			}
		} else {
			LM_WARN("failed to build uas for failover\n");
		}
	}
#endif /* USE_DNS_FAILOVER */

	new_cell->uac[0].on_reply = new_cell->on_reply;
	new_cell->uac[0].on_failure = new_cell->on_failure;

	new_cell->method.s = buf;
	new_cell->method.len = uac_r->method->len;

	request->buffer = buf;
	request->buffer_len = buf_len;
	if(unlikely(refresh_shortcuts==1)) {
		if(uac_refresh_hdr_shortcuts(new_cell, buf, buf_len)<0) {
			LM_ERR("failed to refresh header shortcuts\n");
			goto error1;
		}
	}
	new_cell->nr_of_outgoings++;

	/* Register the callbacks after everything is successful and nothing can fail.
	Otherwise the callback parameter would be freed twise, once from TMCB_DESTROY,
	and again because of the negative return code. */
	if(uac_r->cb && insert_tmcb(&(new_cell->tmcb_hl), uac_r->cb_flags,
								*(uac_r->cb), uac_r->cbp, NULL)!=1){
		ret=E_OUT_OF_MEM;
		LM_ERR("short of tmcb shmem\n");
		goto error1;
	}
	if (has_local_reqin_tmcbs())
			run_local_reqin_callbacks(new_cell, 0, 0);
#ifdef DIALOG_CALLBACKS
	run_trans_dlg_callbacks(uac_r->dialog, new_cell, request);
#endif /* DIALOG_CALLBACKS */
	if (dst_req) *dst_req = request;
	if (dst_cell) *dst_cell = new_cell;
	else if(is_ack && dst_req==0){
		free_cell(new_cell);
	}

	return 1;

 error1:
 	if (!is_ack) {
		LOCK_HASH(hi);
		remove_from_hash_table_unsafe(new_cell);
		UNLOCK_HASH(hi);
	}

error2:
#ifdef TM_DEL_UNREF
	if (!is_ack) {
		UNREF_FREE(new_cell);
	}else
#endif
		free_cell(new_cell);
error3:
	return ret;
}

/*
 * Prepare a message within a dialog
 */
int prepare_req_within(uac_req_t *uac_r,
		struct retr_buf **dst_req)
{
	if (!uac_r || !uac_r->method || !uac_r->dialog) {
		LM_ERR("Invalid parameter value\n");
		goto err;
	}

	if (uac_r->dialog->state != DLG_CONFIRMED) {
		LM_ERR("Dialog is not confirmed yet\n");
		goto err;
	}

	if ((uac_r->method->len == 3) && (!memcmp("ACK", uac_r->method->s, 3))) goto send;
	if ((uac_r->method->len == 6) && (!memcmp("CANCEL", uac_r->method->s, 6))) goto send;
	uac_r->dialog->loc_seq.value++; /* Increment CSeq */
 send:
	return t_uac_prepare(uac_r, dst_req, 0);

 err:
	/* if (cbp) shm_free(cbp); */
	/* !! never free cbp here because if t_uac_prepare fails, cbp is not freed
	 * and thus caller has no chance to discover if it is freed or not !! */
	return -1;
}

static inline int send_prepared_request_impl(struct retr_buf *request, int retransmit, int branch)
{
	struct cell *t;
	struct sip_msg *p_msg;
	struct ua_client *uac;
	struct ip_addr ip; /* logging */
	int ret;

	t = request->my_T;
	uac = &t->uac[branch];
	p_msg = t->uas.request;

	if (SEND_BUFFER(request) == -1) {
		LM_ERR("Attempt to send to precreated request failed\n");
	}
	else if (unlikely(has_tran_tmcbs(t, TMCB_REQUEST_SENT)))
		/* we don't know the method here */
		run_trans_callbacks_with_buf(TMCB_REQUEST_SENT, &uac->request, 0, 0,
			TMCB_LOCAL_F);

	su2ip_addr(&ip, &uac->request.dst.to);
	LM_DBG("uac: %p  branch: %d  to %s:%d\n",
			uac, branch, ip_addr2a(&ip), su_getport(&uac->request.dst.to));

	if (run_onsend(p_msg, &uac->request.dst, uac->request.buffer,
			uac->request.buffer_len)==0){
		uac->last_received=408;
		su2ip_addr(&ip, &uac->request.dst.to);
		LM_DBG("onsend_route dropped msg. to %s:%d (%d)\n",
						ip_addr2a(&ip), su_getport(&uac->request.dst.to),
						uac->request.dst.proto);
#ifdef USE_DNS_FAILOVER
		/* if the destination resolves to more ips, add another
			*  branch/uac */
		ret = add_uac_dns_fallback(t, p_msg, uac, retransmit);
		if (ret > 0) {
			su2ip_addr(&ip, &uac->request.dst.to);
			LM_DBG("send on branch %d failed "
					"(onsend_route), trying another ip %s:%d (%d)\n",
					branch, ip_addr2a(&ip),
					su_getport(&uac->request.dst.to),
					uac->request.dst.proto);
			/* success, return new branch */
			return ret;
		}
#endif /* USE_DNS_FAILOVER*/
		return -1;
	}

	if (retransmit && (start_retr(&uac->request)!=0))
		LM_CRIT("BUG: failed to start retr. for %p\n", &uac->request);
	return 0;
}

void send_prepared_request(struct retr_buf *request)
{
	send_prepared_request_impl(request, 1 /* retransmit */, 0);
}

/*
 * Send a request using data from the dialog structure
 */
int t_uac(uac_req_t *uac_r)
{
	return t_uac_with_ids(uac_r, NULL, NULL);
}

/*
 * Send a request using data from the dialog structure
 * ret_index and ret_label will identify the new cell
 */
int t_uac_with_ids(uac_req_t *uac_r,
	unsigned int *ret_index, unsigned int *ret_label)
{
	struct retr_buf *request;
	struct cell *cell;
	int ret;
	int is_ack;
	int branch_ret;
	int i;
	branch_bm_t added_branches = 1;

	ret = t_uac_prepare(uac_r, &request, &cell);
	if (ret < 0) return ret;
	is_ack = (uac_r->method->len == 3) && (memcmp("ACK", uac_r->method->s, 3)==0) ? 1 : 0;

	/* equivalent loop to the one in t_forward_nonack */
	for (i=0; i<cell->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			branch_ret=send_prepared_request_impl(request, !is_ack /* retransmit */, i);
			if (branch_ret>=0){ /* some kind of success */
				if (branch_ret>i) {
					/* new branch added */
					added_branches |= 1<<branch_ret;
				}
			}
		}
	}

	if (is_ack) {
		free_cell(cell);
		if (ret_index && ret_label)
			*ret_index = *ret_label = 0;
	} else {
		if (ret_index && ret_label) {
			*ret_index = cell->hash_index;
			*ret_label = cell->label;
		}
	}
	return ret;
}

#ifdef WITH_AS_SUPPORT
struct retr_buf *local_ack_rb(sip_msg_t *rpl_2xx, struct cell *trans,
					unsigned int branch, str *hdrs, str *body)
{
	struct retr_buf *lack;
	unsigned int buf_len;
	char *buffer;
	struct dest_info dst;

	buf_len = (unsigned)sizeof(struct retr_buf);
	if (! (buffer = build_dlg_ack(rpl_2xx, trans, branch, hdrs, body,
			&buf_len, &dst))) {
		return 0;
	} else {
		/* 'buffer' now points into a contiguous chunk of memory with enough
		 * room to hold both the retr. buffer and the string raw buffer: it
		 * points to the begining of the string buffer; we iterate back to get
		 * the begining of the space for the retr. buffer. */
		lack = &((struct retr_buf *)buffer)[-1];
		lack->buffer = buffer;
		lack->buffer_len = buf_len;
		lack->dst = dst;
	}

	/* TODO: need next 2? */
	lack->activ_type = TYPE_LOCAL_ACK;
	lack->my_T = trans;

	return lack;
}

void free_local_ack(struct retr_buf *lack)
{
	shm_free(lack);
}

void free_local_ack_unsafe(struct retr_buf *lack)
{
	shm_free_unsafe(lack);
}

/**
 * @return:
 * 	0: success
 * 	-1: internal error
 * 	-2: insane call :)
 */
int ack_local_uac(struct cell *trans, str *hdrs, str *body)
{
	struct retr_buf *local_ack, *old_lack;
	int ret;
	struct tmcb_params onsend_params;

	/* sanity checks */

#ifdef EXTRA_DEBUG
	if (! trans) {
		LM_BUG("no transaction to ACK.\n");
		abort();
	}
#endif

#define RET_INVALID \
		ret = -2; \
		goto fin

	if (! is_local(trans)) {
		LM_ERR("trying to ACK non local transaction (T@%p).\n", trans);
		RET_INVALID;
	}
	if (! is_invite(trans)) {
		LM_ERR("trying to ACK non INVITE local transaction (T@%p).\n", trans);
		RET_INVALID;
	}
	if (! trans->uac[0].reply) {
		LM_ERR("trying to ACK un-completed INVITE transaction (T@%p).\n", trans);
		RET_INVALID;
	}

	if (! (trans->flags & T_NO_AUTO_ACK)) {
		LM_ERR("trying to ACK an auto-ACK transaction (T@%p).\n", trans);
		RET_INVALID;
	}
	if (trans->uac[0].local_ack) {
		LM_ERR("trying to rebuild ACK retransmission buffer (T@%p).\n", trans);
		RET_INVALID;
	}

	/* looks sane: build the retransmission buffer */

	if (! (local_ack = local_ack_rb(trans->uac[0].reply, trans, /*branch*/0,
			hdrs, body))) {
		LM_ERR("failed to build ACK retransmission buffer");
		RET_INVALID;
	} else {
		/* set the new buffer, but only if not already set (conc. invok.) */
		if ((old_lack = (struct retr_buf *)atomic_cmpxchg_long(
				(void *)&trans->uac[0].local_ack, 0, (long)local_ack))) {
			/* buffer already set: deny current attempt */
			LM_ERR("concurrent ACKing for local INVITE detected (T@%p).\n",trans);
			free_local_ack(local_ack);
			RET_INVALID;
		}
	}

	if (msg_send(&local_ack->dst, local_ack->buffer, local_ack->buffer_len)<0){
		/* hopefully will succeed on next 2xx retransmission */
		LM_ERR("failed to send local ACK (T@%p).\n", trans);
		ret = -1;
		goto fin;
	}
	else {
		INIT_TMCB_ONSEND_PARAMS(onsend_params, 0, 0, &trans->uac[0].request,
								&local_ack->dst,
								local_ack->buffer, local_ack->buffer_len,
								TMCB_LOCAL_F, 0 /* branch */, TYPE_LOCAL_ACK);
		run_trans_callbacks_off_params(TMCB_REQUEST_SENT, trans, &onsend_params);
	}

	ret = 0;
fin:
	/* TODO: ugly! */
	/* FIXME: the T had been obtain by t_lookup_ident()'ing for it, so, it is
	 * ref-counted. The t_unref() can not be used, as it requests a valid SIP
	 * message (all available might be the reply, but if AS goes wrong and
	 * tries to ACK before the final reply is received, we still have to
	 * lookup the T to find this out). */
	UNREF( trans );
	return ret;

#undef RET_INVALID
}
#endif /* WITH_AS_SUPPORT */


/*
 * Send a message within a dialog
 */
int req_within(uac_req_t *uac_r)
{
	int ret;
	char nbuf[MAX_URI_SIZE];
#define REQ_DST_URI_SIZE	80
	char dbuf[REQ_DST_URI_SIZE];
	str ouri = {0, 0};
	str nuri = {0, 0};
	str duri = {0, 0};

	if (!uac_r || !uac_r->method || !uac_r->dialog) {
		LM_ERR("Invalid parameter value\n");
		goto err;
	}

	if(uac_r->ssock!=NULL && uac_r->ssock->len>0
			&& uac_r->dialog->send_sock==NULL) {
		/* set local send socket */
		uac_r->dialog->send_sock = lookup_local_socket(uac_r->ssock);
	}

	/* handle alias parameter in uri
	 * - only if no dst uri and no route set - */
	if(uac_r->dialog && uac_r->dialog->rem_target.len>0
			&& uac_r->dialog->dst_uri.len==0
			&& uac_r->dialog->route_set==NULL) {
		ouri = uac_r->dialog->rem_target;
		/*restore alias parameter*/
		nuri.s = nbuf;
		nuri.len = MAX_URI_SIZE;
		duri.s = dbuf;
		duri.len = REQ_DST_URI_SIZE;
		if(uri_restore_rcv_alias(&ouri, &nuri, &duri)<0) {
			nuri.len = 0;
			duri.len = 0;
		}
		if(nuri.len>0 && duri.len>0) {
			uac_r->dialog->rem_target = nuri;
			uac_r->dialog->dst_uri    = duri;
		} else {
			ouri.len = 0;
		}
	}

	if ((uac_r->method->len == 3) && (!memcmp("ACK", uac_r->method->s, 3))) goto send;
	if ((uac_r->method->len == 6) && (!memcmp("CANCEL", uac_r->method->s, 6))) goto send;
	uac_r->dialog->loc_seq.value++; /* Increment CSeq */
 send:
	ret = t_uac(uac_r);
	if(ouri.len>0) {
		uac_r->dialog->rem_target = ouri;
		uac_r->dialog->dst_uri.s = 0;
		uac_r->dialog->dst_uri.len = 0;
	}
	return ret;

 err:
	/* callback parameter must be freed outside of tm module
	if (cbp) shm_free(cbp); */
	if(ouri.len>0) {
		uac_r->dialog->rem_target = ouri;
		uac_r->dialog->dst_uri.s = 0;
		uac_r->dialog->dst_uri.len = 0;
	}
	return -1;
}


/*
 * Send an initial request that will start a dialog
 * WARNING: writes uac_r->dialog
 */
int req_outside(uac_req_t *uac_r, str* ruri, str* to, str* from, str *next_hop)
{
	str callid, fromtag;

	if (check_params(uac_r, to, from) < 0) goto err;

	generate_callid(&callid);
	generate_fromtag(&fromtag, &callid);

	if (new_dlg_uac(&callid, &fromtag, DEFAULT_CSEQ, from, to, &uac_r->dialog) < 0) {
		LM_ERR("Error while creating new dialog\n");
		goto err;
	}

	if (ruri) {
		uac_r->dialog->rem_target.s = ruri->s;
		uac_r->dialog->rem_target.len = ruri->len;
		/* hooks will be set from w_calculate_hooks */
	}

	if (next_hop) uac_r->dialog->dst_uri = *next_hop;
	w_calculate_hooks(uac_r->dialog);

	if(uac_r->ssock!=NULL && uac_r->ssock->len>0
			&& uac_r->dialog->send_sock==NULL) {
		/* set local send socket */
		uac_r->dialog->send_sock = lookup_local_socket(uac_r->ssock);
	}

	return t_uac(uac_r);

 err:
	/* callback parameter must be freed outside of tm module
	if (cbp) shm_free(cbp); */
	return -1;
}


/*
 * Send a transactional request, no dialogs involved
 * WARNING: writes uac_r->dialog
 */
int request(uac_req_t *uac_r, str* ruri, str* to, str* from, str *next_hop)
{
	str callid, fromtag;
	dlg_t* dialog;
	int res;

	if (check_params(uac_r, to, from) < 0) goto err;

	if (uac_r->callid == NULL || uac_r->callid->len <= 0)
	    generate_callid(&callid);
	else
	    callid = *uac_r->callid;
	generate_fromtag(&fromtag, &callid);

	if (new_dlg_uac(&callid, &fromtag, DEFAULT_CSEQ, from, to, &dialog) < 0) {
		LM_ERR("Error while creating temporary dialog\n");
		goto err;
	}

	if (ruri) {
		dialog->rem_target.s = ruri->s;
		dialog->rem_target.len = ruri->len;
		/* hooks will be set from w_calculate_hooks */
	}

	if (next_hop) dialog->dst_uri = *next_hop;
	w_calculate_hooks(dialog);

	/* WARNING:
	 * to be clean it should be called
	 *   set_dlg_target(dialog, ruri, next_hop);
	 * which sets both uris if given [but it duplicates them in shm!]
	 *
	 * but in this case the _ruri parameter in set_dlg_target
	 * must be optional (it is needed now) and following hacks
	 *   dialog->rem_target.s = 0;
	 *   dialog->dst_uri.s = 0;
	 * before freeing dialog here must be removed
	 */
	uac_r->dialog = dialog;

	if(uac_r->ssock!=NULL && uac_r->ssock->len>0
			&& uac_r->dialog->send_sock==NULL) {
		/* set local send socket */
		uac_r->dialog->send_sock = lookup_local_socket(uac_r->ssock);
	}

	res = t_uac(uac_r);
	dialog->rem_target.s = 0;
	dialog->dst_uri.s = 0;
	free_dlg(dialog);
	uac_r->dialog = 0;
	return res;

 err:
	/* callback parameter must be freed outside of tm module
	if (cp) shm_free(cp); */
	return -1;
}

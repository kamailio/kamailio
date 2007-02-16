/*
 * $Id$
 *
 * simple UAC for things such as SUBSCRIBE or SMS gateway;
 * no authentication and other UAC features -- just send
 * a message, retransmit and await a reply; forking is not
 * supported during client generation, in all other places
 * it is -- adding it should be simple
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
 *
 * History:
 * --------
 *  2003-01-23  t_uac_dlg now uses get_out_socket (jiri)
 *  2003-01-27  fifo:t_uac_dlg completed (jiri)
 *  2003-01-29  scratchpad removed (jiri)
 *  2003-02-13  t_uac, t _uac_dlg, gethfblock, uri2proxy changed to use 
 *               proto & rb->dst (andrei)
 *  2003-02-27  FIFO/UAC now dumps reply -- good for CTD (jiri)
 *  2003-02-28  scratchpad compatibility abandoned (jiri)
 *  2003-03-01  kr set through a function now (jiri)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-02  port_no_str does not contain a leading ':' anymore (andrei)
 *  2003-07-08  appropriate log messages in check_params(...), 
 *               call calculate_hooks if next_hop==NULL in t_uac (dcm) 
 *  2003-10-24  updated to the new socket_info lists (andrei)
 *  2003-12-03  completion filed removed from transaction and uac callbacks
 *              merged in transaction callbacks as LOCAL_COMPLETED (bogdan)
 *  2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 *  2004-02-13  t->is_invite, t->local, t->noisy_ctimer replaced (bogdan)
 *  2004-08-23  avp support in t_uac (bogdan)
 *  2005-12-16  t_uac will set the new_cell timers to the default values,
 *               fixes 0 fr_timer bug (andrei)
 *  2006-08-11  t_uac uses dns failover until it finds a send socket (andrei)
 */

#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../globals.h"
#include "../../md5.h"
#include "../../crc.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "ut.h"
#include "h_table.h"
#include "t_hooks.h"
#include "t_funcs.h"
#include "t_msgbuilder.h"
#include "callid.h"
#include "uac.h"


#define FROM_TAG_LEN (MD5_LEN + 1 /* - */ + CRC16_LEN) /* length of FROM tags */

static char from_tag[FROM_TAG_LEN + 1];

char* uac_from = "sip:foo@foo.bar"; /* Module parameter */

/* Enable/disable passing of provisional replies to FIFO applications */
int pass_provisional_replies = 0;

/*
 * Initialize UAC
 */
int uac_init(void) 
{
	str src[3];
	struct socket_info *si;

	if (RAND_MAX < TABLE_ENTRIES) {
		LOG(L_WARN, "Warning: uac does not spread "
		    "across the whole hash table\n");
	}
	/* on tcp/tls bind_address is 0 so try to get the first address we listen
	 * on no matter the protocol */
	si=bind_address?bind_address:get_first_socket();
	if (si==0){
		LOG(L_CRIT, "BUG: uac_init: null socket list\n");
		return -1;
	}

	/* calculate the initial From tag */
	src[0].s = "Long live SER server";
	src[0].len = strlen(src[0].s);
	src[1].s = si->address_str.s;
	src[1].len = strlen(src[1].s);
	src[2].s = si->port_no_str.s;
	src[2].len = strlen(src[2].s);

	MDStringArray(from_tag, src, 3);
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
static inline int check_params(str* method, str* to, str* from, dlg_t** dialog)
{
	if (!method || !to || !from || !dialog) {
		LOG(L_ERR, "check_params(): Invalid parameter value\n");
		return -1;
	}

	if (!method->s || !method->len) {
		LOG(L_ERR, "check_params(): Invalid request method\n");
		return -2;
	}

	if (!to->s || !to->len) {
		LOG(L_ERR, "check_params(): Invalid To URI\n");
		return -4;
	}

	if (!from->s || !from->len) {
		LOG(L_ERR, "check_params(): Invalid From URI\n");
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
	DBG("DEBUG: dlg2hash: %d\n", hashid);
	return hashid;
}

static inline int t_uac_prepare(str* method, str* headers, str* body, 
		dlg_t* dialog, transaction_cb cb, void* cbp, struct retr_buf **dst_req,
		struct cell **dst_cell)
{
	struct dest_info dst;
	struct cell *new_cell;
	struct retr_buf *request;
	char* buf;
        int buf_len, ret, flags;
	unsigned int hi;
	int is_ack;
#ifdef USE_DNS_FAILOVER
	struct dns_srv_handle dns_h;
#endif

	ret=-1;
	hi=0; /* make gcc happy */
	/*if (dst_req) *dst_req = NULL;*/
	is_ack = (((method->len == 3) && (memcmp("ACK", method->s, 3)==0)) ? 1 : 0);
	
	/*** added by dcm 
	 * - needed by external ua to send a request within a dlg
	 */
	if (w_calculate_hooks(dialog)<0 && !dialog->hooks.next_hop)
		goto error2;

	if (!dialog->loc_seq.is_set) {
		/* this is the first request in the dialog,
		set cseq to default value now - Miklos */
		dialog->loc_seq.value = DEFAULT_CSEQ;
		dialog->loc_seq.is_set = 1;
	}

	DBG("DEBUG:tm:t_uac: next_hop=<%.*s>\n",dialog->hooks.next_hop->len,
			dialog->hooks.next_hop->s);
	/* it's a new message, so we will take the default socket */
#ifdef USE_DNS_FAILOVER
	if (use_dns_failover){
		dns_srv_handle_init(&dns_h);
		if ((uri2dst(&dns_h, &dst, 0, dialog->hooks.next_hop, PROTO_NONE)==0)
				|| (dst.send_sock==0)){
			dns_srv_handle_put(&dns_h);
			ser_error = E_NO_SOCKET;
			ret=ser_error;
			LOG(L_ERR, "t_uac: no socket found\n");
			goto error2;
		}
		dns_srv_handle_put(&dns_h); /* not needed anymore */
	}else{
		if ((uri2dst(0, &dst, 0, dialog->hooks.next_hop, PROTO_NONE)==0) ||
				(dst.send_sock==0)){
			ser_error = E_NO_SOCKET;
			ret=ser_error;
			LOG(L_ERR, "t_uac: no socket found\n");
			goto error2;
		}
	}
#else
	if ((uri2dst(&dst, 0, dialog->hooks.next_hop, PROTO_NONE)==0) ||
			(dst.send_sock==0)){
		ser_error = E_NO_SOCKET;
		ret=ser_error;
		LOG(L_ERR, "t_uac: no socket found\n");
		goto error2;
	}
#endif

	new_cell = build_cell(0); 
	if (!new_cell) {
		ret=E_OUT_OF_MEM;
		LOG(L_ERR, "t_uac: short of cell shmem\n");
		goto error2;
	}
	/* init timers hack, new_cell->fr_timer and new_cell->fr_inv_timer
	 * must be set, or else the fr will happen immediately
	 * we can't call init_new_t() because we don't have a sip msg
	 * => we'll ignore t_set_fr() or avp timer value and will use directly the
	 * module params fr_inv_timer and fr_timer -- andrei */
	new_cell->fr_timeout=fr_timeout;
	new_cell->fr_inv_timeout=fr_inv_timeout;

	/* better reset avp list now - anyhow, it's useless from
	 * this point (bogdan) */
	reset_avps();

	/* add the callback the the transaction for LOCAL_COMPLETED event */
 
	flags = TMCB_LOCAL_COMPLETED;
	/* Add also TMCB_LOCAL_REPLY_OUT if provisional replies are desired */
	if (pass_provisional_replies) flags |= TMCB_LOCAL_RESPONSE_OUT;

	if(cb && insert_tmcb(&(new_cell->tmcb_hl), flags, cb, cbp)!=1){
		ret=E_OUT_OF_MEM; 
		LOG(L_ERR, "t_uac: short of tmcb shmem\n");
		goto error2;
	}

	if (method->len==INVITE_LEN && memcmp(method->s, INVITE, INVITE_LEN)==0)
		new_cell->flags |= T_IS_INVITE_FLAG;
	new_cell->flags |= T_IS_LOCAL_FLAG;
	set_kr(REQ_FWDED);

	request = &new_cell->uac[0].request;
	
	request->dst = dst;

	if (!is_ack) {
		hi=dlg2hash(dialog);
		LOCK_HASH(hi);
		insert_into_hash_table_unsafe(new_cell, hi);
		UNLOCK_HASH(hi);
	}

	buf = build_uac_req(method, headers, body, dialog, 0, new_cell,
		&buf_len, &dst);
	if (!buf) {
		LOG(L_ERR, "t_uac: Error while building message\n");
		ret=E_OUT_OF_MEM;
		goto error1;
	}

	new_cell->method.s = buf;
	new_cell->method.len = method->len;

	request->buffer = buf;
	request->buffer_len = buf_len;
	new_cell->nr_of_outgoings++;
	
	if (dst_req) *dst_req = request;
	if (dst_cell) *dst_cell = new_cell;
	
	return 1;

 error1:
 	if (!is_ack) {
		LOCK_HASH(hi);
		remove_from_hash_table_unsafe(new_cell);
		UNLOCK_HASH(hi);
	}
	free_cell(new_cell);
error2:
	return ret;
}

/*
 * Prepare a message within a dialog
 */
int prepare_req_within(str* method, str* headers, 
		str* body, dlg_t* dialog, transaction_cb completion_cb, 
		void* cbp, struct retr_buf **dst_req)
{
	if (!method || !dialog) {
		LOG(L_ERR, "req_within: Invalid parameter value\n");
		goto err;
	}

	if (dialog->state != DLG_CONFIRMED) {
		LOG(L_ERR, "req_within: Dialog is not confirmed yet\n");
		goto err;
	}

	if ((method->len == 3) && (!memcmp("ACK", method->s, 3))) goto send;
	if ((method->len == 6) && (!memcmp("CANCEL", method->s, 6))) goto send;
	dialog->loc_seq.value++; /* Increment CSeq */
 send:
	return t_uac_prepare(method, headers, body, dialog, completion_cb, cbp, dst_req, 0);

 err:
	/* if (cbp) shm_free(cbp); */
	/* !! never free cbp here because if t_uac_prepare fails, cbp is not freed
	 * and thus caller has no chance to discover if it is freed or not !! */
	return -1;
}

static inline void send_prepared_request_impl(struct retr_buf *request, int retransmit)
{
	if (SEND_BUFFER(request) == -1) {
		LOG(L_ERR, "t_uac: Attempt to send to precreated request failed\n");
	}
	
	if (retransmit && (start_retr(request)!=0))
		LOG(L_CRIT, "BUG: t_uac: failed to start retr. for %p\n", request);
}

void send_prepared_request(struct retr_buf *request)
{
	send_prepared_request_impl(request, 1 /* retransmit */);
}

/*
 * Send a request using data from the dialog structure
 */
int t_uac(str* method, str* headers, str* body, dlg_t* dialog,
	  transaction_cb cb, void* cbp)
{
	struct retr_buf *request;
	struct cell *cell;
	int ret;
	int is_ack;

	ret = t_uac_prepare(method, headers, body, dialog, cb, cbp, &request, &cell);
	if (ret < 0) return ret;
	is_ack = (method->len == 3) && (memcmp("ACK", method->s, 3)==0) ? 1 : 0;
	send_prepared_request_impl(request, !is_ack /* retransmit */);
	if (cell && is_ack)
		free_cell(cell);
	return ret;
}

/*
 * Send a request using data from the dialog structure
 * ret_index and ret_label will identify the new cell
 */
int t_uac_with_ids(str* method, str* headers, str* body, dlg_t* dialog,
	transaction_cb cb, void* cbp,
	unsigned int *ret_index, unsigned int *ret_label)
{
	struct retr_buf *request;
	struct cell *cell;
	int ret;
	int is_ack;

	ret = t_uac_prepare(method, headers, body, dialog, cb, cbp, &request, &cell);
	if (ret < 0) return ret;
	is_ack = (method->len == 3) && (memcmp("ACK", method->s, 3)==0) ? 1 : 0;
	send_prepared_request_impl(request, !is_ack /* retransmit */);
	if (is_ack) {
		if (cell) free_cell(cell);
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

/*
 * Send a message within a dialog
 */
int req_within(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb completion_cb, void* cbp)
{
	if (!method || !dialog) {
		LOG(L_ERR, "req_within: Invalid parameter value\n");
		goto err;
	}

	if ((method->len == 3) && (!memcmp("ACK", method->s, 3))) goto send;
	if ((method->len == 6) && (!memcmp("CANCEL", method->s, 6))) goto send;
	dialog->loc_seq.value++; /* Increment CSeq */
 send:
	return t_uac(method, headers, body, dialog, completion_cb, cbp);

 err:
	if (cbp) shm_free(cbp);
	return -1;
}


/*
 * Send an initial request that will start a dialog
 */
int req_outside(str* method, str* to, str* from, str* headers, str* body, dlg_t** dialog, transaction_cb cb, void* cbp)
{
	str callid, fromtag;

	if (check_params(method, to, from, dialog) < 0) goto err;
	
	generate_callid(&callid);
	generate_fromtag(&fromtag, &callid);

	if (new_dlg_uac(&callid, &fromtag, DEFAULT_CSEQ, from, to, dialog) < 0) {
		LOG(L_ERR, "req_outside(): Error while creating new dialog\n");
		goto err;
	}

	return t_uac(method, headers, body, *dialog, cb, cbp);

 err:
	if (cbp) shm_free(cbp);
	return -1;
}


/*
 * Send a transactional request, no dialogs involved
 */
int request(str* m, str* ruri, str* to, str* from, str* h, str* b, str *next_hop, transaction_cb c, void* cp)
{
	str callid, fromtag;
	dlg_t* dialog;
	int res;

	if (check_params(m, to, from, &dialog) < 0) goto err;

	generate_callid(&callid);
	generate_fromtag(&fromtag, &callid);

	if (new_dlg_uac(&callid, &fromtag, DEFAULT_CSEQ, from, to, &dialog) < 0) {
		LOG(L_ERR, "request(): Error while creating temporary dialog\n");
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

	res = t_uac(m, h, b, dialog, c, cp);
	dialog->rem_target.s = 0;
	dialog->dst_uri.s = 0;
	free_dlg(dialog);
	return res;

 err:
	if (cp) shm_free(cp);
	return -1;
}

/*
 * $Id$
 *
 * simple UAC for things such as SUBSCRIBE or SMS gateway;
 * no authentication and other UAC features -- just send
 * a message, retransmit and await a reply; forking is not
 * supported during client generation, in all other places
 * it is -- adding it should be simple
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
 */

#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../globals.h"
#include "../../md5.h"
#include "../../crc.h"
#include "../../ip_addr.h"
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


/*
 * Initialize UAC
 */
int uac_init(void) 
{
	str src[3];

	if (RAND_MAX < TABLE_ENTRIES) {
		LOG(L_WARN, "Warning: uac does not spread "
		    "accross the whole hash table\n");
	}

	/* calculate the initial From tag */
	src[0].s = "Long live SER server";
	src[0].len = strlen(src[0].s);
	src[1].s = sock_info[bind_idx].address_str.s;
	src[1].len = strlen(src[1].s);
	src[2].s = sock_info[bind_idx].port_no_str.s;
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
		LOG(L_ERR, "check_params(): Invalid request method\n");
		return -4;
	}

	if (!from->s || !from->len) {
		LOG(L_ERR, "check_params(): Invalid request method\n");
		return -5;
	}
	return 0;
}


/*
 * Send a request using data from the dialog structure
 */
int t_uac(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb cb, void* cbp)
{
	struct socket_info* send_sock;
	union sockaddr_union to_su;
	struct cell *new_cell;
	struct retr_buf *request;
	char* buf;
	int buf_len;
	int ret;

	ret=-1;

	send_sock = uri2sock(dialog->hooks.next_hop, &to_su, PROTO_NONE);
	if (!send_sock) {
		ret=ser_error;
		LOG(L_ERR, "t_uac: no socket found\n");
		goto error2;
	}	

	new_cell = build_cell(0); 
	if (!new_cell) {
		ret=E_OUT_OF_MEM;
		LOG(L_ERR, "t_uac: short of cell shmem\n");
		goto error2;
	}

	new_cell->completion_cb = cb;
	new_cell->cbp = cbp;
	
	     /* cbp is installed -- tell error handling bellow not to free it */
	cbp = 0;

	new_cell->is_invite = method->len == INVITE_LEN && memcmp(method->s, INVITE, INVITE_LEN) == 0;
	new_cell->local= 1;
	set_kr(REQ_FWDED);
	
	request = &new_cell->uac[0].request;
	request->dst.to = to_su;
	request->dst.send_sock = send_sock;
	request->dst.proto = send_sock->proto;
	request->dst.proto_reserved1 = 0;

	     /* need to put in table to calculate label which is needed for printing */
	LOCK_HASH(new_cell->hash_index);
	insert_into_hash_table_unsafe(new_cell);
	UNLOCK_HASH(new_cell->hash_index);

	buf = build_uac_req(method, headers, body, dialog, 0, new_cell, &buf_len, send_sock);
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
	
	if (SEND_BUFFER(request) == -1) {
		LOG(L_ERR, "t_uac: Attempt to send to '%.*s' failed\n", 
		    dialog->hooks.next_hop->len,
		    dialog->hooks.next_hop->s
		    );
	}
	
	start_retr(request);
	return 1;

 error1:
	LOCK_HASH(new_cell->hash_index);
	remove_from_hash_table_unsafe(new_cell);
	UNLOCK_HASH(new_cell->hash_index);
	free_cell(new_cell);

 error2:
	     /* if we did not install cbp, release it now */
	if (cbp) shm_free(cbp);
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

	if (dialog->state != DLG_CONFIRMED) {
		LOG(L_ERR, "req_within: Dialog is not confirmed yet\n");
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
int request(str* m, str* ruri, str* to, str* from, str* h, str* b, transaction_cb c, void* cp)
{
	str callid, fromtag;
	dlg_t* dialog;
	int res;

	if (check_params(m, to, from, &dialog) < 0) goto err;

	generate_callid(&callid);
	generate_fromtag(&fromtag, &callid);

	if (new_dlg_uac(&callid, &fromtag, DEFAULT_CSEQ, from, to, &dialog) < 0) {
		LOG(L_ERR, "req_outside(): Error while creating temorary dialog\n");
		goto err;
	}

	if (ruri) {
		dialog->rem_target.s = ruri->s;
		dialog->rem_target.len = ruri->len;
		dialog->hooks.request_uri = &dialog->rem_target;
	}

	res = t_uac(m, h, b, dialog, c, cp);
	dialog->rem_target.s = 0;
	free_dlg(dialog);
	return res;

 err:
	if (cp) shm_free(cp);
	return -1;
}

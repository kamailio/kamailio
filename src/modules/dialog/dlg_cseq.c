/*
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


/*!
 * \file
 * \brief Cseq handling
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "../../core/events.h"
#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/data_lump.h"
#include "../../core/srapi.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_cseq.h"
#include "../../modules/tm/tm_load.h"

#include "dlg_handlers.h"
#include "dlg_var.h"
#include "dlg_cseq.h"

extern struct tm_binds d_tmb;

static str _dlg_cseq_diff_var_name = str_init("cseq_diff");

typedef struct dlg_cseq_edit
{
	char *start;
	char *end;
	str value;
} dlg_cseq_edit_t;

/**
 *
 */
static hdr_field_t *dlg_cseq_get_rack_hdr(sip_msg_t *msg)
{
	hdr_field_t *hdr = NULL;

	for(hdr = msg->headers; hdr != NULL; hdr = hdr->next) {
		if(hdr->name.len == 4 && strncasecmp(hdr->name.s, "RAck", 4) == 0) {
			return hdr;
		}
	}

	return NULL;
}

/**
 *
 */
static int dlg_cseq_get_rack_cseq(hdr_field_t *hdr, str *tok, unsigned int *val)
{
	str body;
	char *p = NULL;
	char *end = NULL;

	if(hdr == NULL || tok == NULL || val == NULL) {
		return -1;
	}

	body = hdr->body;
	trim(&body);
	if(body.len <= 0) {
		return -1;
	}

	p = body.s;
	end = body.s + body.len;

	/* response-num */
	if(p >= end || !isdigit((unsigned char)*p)) {
		return -1;
	}
	while(p < end && isdigit((unsigned char)*p)) {
		p++;
	}
	while(p < end && (*p == ' ' || *p == '\t')) {
		p++;
	}

	/* CSeq-num */
	if(p >= end || !isdigit((unsigned char)*p)) {
		return -1;
	}
	tok->s = p;
	while(p < end && isdigit((unsigned char)*p)) {
		p++;
	}
	tok->len = (int)(p - tok->s);

	if(str2int(tok, val) < 0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
static int dlg_cseq_add_edit(
		dlg_cseq_edit_t *edits, int *ecnt, char *start, char *end, str *value)
{
	if(edits == NULL || ecnt == NULL || value == NULL || start == NULL
			|| end == NULL || end < start || *ecnt >= 4) {
		return -1;
	}

	edits[*ecnt].start = start;
	edits[*ecnt].end = end;
	edits[*ecnt].value = *value;
	(*ecnt)++;

	return 0;
}

/**
 *
 */
static void dlg_cseq_sort_edits(dlg_cseq_edit_t *edits, int ecnt)
{
	dlg_cseq_edit_t tmp;
	int i;
	int j;

	for(i = 1; i < ecnt; i++) {
		tmp = edits[i];
		j = i - 1;
		while(j >= 0
				&& (edits[j].start > tmp.start
						|| (edits[j].start == tmp.start
								&& edits[j].end > tmp.end))) {
			edits[j + 1] = edits[j];
			j--;
		}
		edits[j + 1] = tmp;
	}
}

/**
 *
 */
static int dlg_cseq_apply_edits(
		sip_msg_t *msg, dlg_cseq_edit_t *edits, int ecnt, str *obuf)
{
	char tbuf[BUF_SIZE];
	char *src = NULL;
	char *msgend = NULL;
	int tbuf_len = 0;
	int i;
	int clen;
	str nbuf = STR_NULL;

	if(msg == NULL || obuf == NULL || (ecnt > 0 && edits == NULL)) {
		return -1;
	}

	src = msg->buf;
	msgend = msg->buf + msg->len;

	for(i = 0; i < ecnt; i++) {
		if(edits[i].start < src || edits[i].start > edits[i].end
				|| edits[i].end > msgend || edits[i].value.len < 0) {
			LM_ERR("invalid cseq rewrite edit\n");
			return -1;
		}
		clen = (int)(edits[i].start - src);
		if(tbuf_len + clen + edits[i].value.len >= BUF_SIZE) {
			LM_ERR("new message is too big\n");
			return -1;
		}
		if(clen > 0) {
			memcpy(tbuf + tbuf_len, src, clen);
			tbuf_len += clen;
		}
		if(edits[i].value.len > 0) {
			memcpy(tbuf + tbuf_len, edits[i].value.s, edits[i].value.len);
			tbuf_len += edits[i].value.len;
		}
		src = edits[i].end;
	}

	clen = (int)(msgend - src);
	if(tbuf_len + clen >= BUF_SIZE) {
		LM_ERR("new message is too big\n");
		return -1;
	}
	if(clen > 0) {
		memcpy(tbuf + tbuf_len, src, clen);
		tbuf_len += clen;
	}

	nbuf.s = pkg_malloc((tbuf_len + 1) * sizeof(char));
	if(nbuf.s == NULL) {
		LM_ERR("not enough memory for new message\n");
		return -1;
	}

	pkg_free(obuf->s);
	obuf->s = nbuf.s;
	memcpy(obuf->s, tbuf, tbuf_len);
	obuf->s[tbuf_len] = '\0';
	obuf->len = tbuf_len;

	return 0;
}

/**
 *
 */
static int dlg_cseq_prepare_msg(sip_msg_t *msg)
{
	LM_DBG("prepare msg for cseq update operations\n");

	if(msg->first_line.type == SIP_REQUEST) {
		if(!IS_SIP(msg)) {
			LM_DBG("non sip request message\n");
			return 1;
		}
	} else if(msg->first_line.type == SIP_REPLY) {
		if(!IS_SIP_REPLY(msg)) {
			LM_DBG("non sip reply message\n");
			return 1;
		}
	} else {
		LM_DBG("non sip message\n");
		return 1;
	}

	if((!msg->cseq && (parse_headers(msg, HDR_CSEQ_F, 0) < 0 || !msg->cseq))
			|| !msg->cseq->parsed) {
		LM_DBG("parsing cseq header failed\n");
		return 2;
	}

	if(msg->first_line.type == SIP_REPLY) {
		/* reply to local transaction -- nothing to do */
		if(parse_headers(msg, HDR_VIA2_F, 0) == -1 || (msg->via2 == 0)
				|| (msg->via2->error != PARSE_OK)) {
			if(get_cseq(msg)->method_id != METHOD_CANCEL) {
				LM_DBG("no second via in this message \n");
				return 3;
			}
		}
	}

	if(parse_from_header(msg) < 0) {
		LM_ERR("cannot parse FROM header\n");
		return 3;
	}

	if(parse_to_header(msg) < 0 || msg->to == NULL) {
		LM_ERR("cannot parse TO header\n");
		return 3;
	}

	if(get_to(msg) == NULL) {
		LM_ERR("cannot get TO header\n");
		return 3;
	}

	return 0;
}

/**
 *
 */
static int dlg_cseq_prepare_new_msg(sip_msg_t *msg)
{
	LM_DBG("prepare new msg for cseq update operations\n");
	if(parse_msg(msg->buf, msg->len, msg) != 0) {
		LM_DBG("outbuf buffer parsing failed!\n");
		return 1;
	}
	return dlg_cseq_prepare_msg(msg);
}

/**
 *
 */
int dlg_cseq_update(sip_msg_t *msg)
{
	dlg_cell_t *dlg = NULL;
	unsigned int direction;
	unsigned int ninc = 0;
	unsigned int vinc = 0;
	str nval;
	sr_cfgenv_t *cenv = NULL;

	if(dlg_cseq_prepare_msg(msg) != 0) {
		return -1;
	}
	if(msg->first_line.type == SIP_REPLY) {
		/* nothing to do for outgoing replies */
		goto done;
	}

	LM_DBG("initiating cseq updates\n");

	direction = DLG_DIR_NONE;
	dlg = dlg_lookup_msg_dialog(msg, &direction);

	if(dlg == NULL) {
		LM_DBG("no dialog for this request\n");
		goto done;
	}

	/* supported only for downstream direction */
	if(direction != DLG_DIR_DOWNSTREAM) {
		LM_DBG("request not going downstream (%u)\n", direction);
		goto done;
	}

	cenv = sr_cfgenv_get();
	ninc = 1;

	/* take the increment value from dialog */
	if((dlg->iflags & DLG_IFLAG_CSEQ_DIFF) == DLG_IFLAG_CSEQ_DIFF) {
		/* get dialog variable holding cseq diff */
		if(get_dlg_variable_uintval(dlg, &_dlg_cseq_diff_var_name, &vinc) < 0) {
			LM_DBG("dialog marked with cseq diff but no variable set yet\n");
			goto done;
		}
	}
	vinc += ninc;
	if(vinc == 0) {
		LM_DBG("nothing to increment\n");
		goto done;
	}
	if(set_dlg_variable_uintval(dlg, &_dlg_cseq_diff_var_name, vinc) < 0) {
		LM_ERR("failed to set the dlg cseq diff var\n");
		goto done;
	}
	str2int(&get_cseq(msg)->number, &ninc);
	vinc += ninc;
	nval.s = int2str(vinc, &nval.len);
	trim(&nval);

	LM_DBG("adding auth cseq header value: %.*s\n", nval.len, nval.s);
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse all headers\n");
	}
	sr_hdr_add_zs(msg, cenv->uac_cseq_auth.s, &nval);

done:
	if(dlg != NULL)
		dlg_release(dlg);
	return 0;
}


/**
 *
 */
int dlg_cseq_refresh(sip_msg_t *msg, dlg_cell_t *dlg, unsigned int direction)
{
	unsigned int ninc = 0;
	unsigned int vinc = 0;
	str nval;
	sr_cfgenv_t *cenv = NULL;

	if(dlg_cseq_prepare_msg(msg) != 0) {
		goto error;
	}
	if(msg->first_line.type == SIP_REPLY) {
		/* nothing to do for outgoing replies */
		goto done;
	}

	LM_DBG("initiating cseq refresh\n");

	/* supported only for downstream direction */
	if(direction != DLG_DIR_DOWNSTREAM) {
		LM_DBG("request not going downstream (%u)\n", direction);
		goto done;
	}

	/* take the increment value from dialog */
	if(!((dlg->iflags & DLG_IFLAG_CSEQ_DIFF) == DLG_IFLAG_CSEQ_DIFF)) {
		LM_DBG("no cseq refresh required\n");
		goto done;
	}

	/* get the value of the dialog variable holding cseq diff */
	if(get_dlg_variable_uintval(dlg, &_dlg_cseq_diff_var_name, &vinc) < 0) {
		LM_DBG("dialog marked with cseq diff but no variable set yet\n");
		goto done;
	}

	if(vinc == 0) {
		LM_DBG("nothing to increment\n");
		goto done;
	}

	str2int(&get_cseq(msg)->number, &ninc);
	vinc += ninc;
	nval.s = int2str(vinc, &nval.len);
	trim(&nval);

	LM_DBG("adding cseq refresh header value: %.*s\n", nval.len, nval.s);
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse all headers\n");
	}
	cenv = sr_cfgenv_get();
	sr_hdr_add_zs(msg, cenv->uac_cseq_refresh.s, &nval);

done:
	return 0;

error:
	return -1;
}

/**
 *
 */
int dlg_cseq_msg_received(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	struct via_body *via;
	str vcseq;

	obuf = (str *)evp->data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(dlg_cseq_prepare_new_msg(&msg) != 0) {
		goto done;
	}

	if(msg.first_line.type == SIP_REQUEST) {
		/* nothing to do for incoming requests */
		goto done;
	}

	if(!msg.h_via1) {
		goto done;
	}

	via = (struct via_body *)msg.h_via1->parsed;

	if(via->branch == NULL || via->branch->value.len <= 0) {
		LM_DBG("no branch parameter in top via\n");
		goto done;
	}
	vcseq.s = via->branch->value.s + via->branch->value.len - 1;
	vcseq.len = 0;
	while(vcseq.s > via->branch->value.s) {
		if(*vcseq.s == '.') {
			if(vcseq.len < 3) {
				LM_DBG("no matching the starting point length\n");
				goto done;
			}
			if(vcseq.s[1] != 'c' || vcseq.s[2] != 's') {
				LM_DBG("no matching the starting placeholder\n");
				goto done;
			}
			vcseq.len++;
			break;
		}
		vcseq.len++;
		vcseq.s--;
	}
	LM_DBG("via cseq cookie [%.*s] val [%.*s]\n", vcseq.len, vcseq.s,
			vcseq.len - 3, vcseq.s + 3);
	if(vcseq.len - 3 > get_cseq(&msg)->number.len) {
		/* higher length to update - wrong */
		LM_DBG("cseq in message (%d) shorter than in via (%d)\n",
				get_cseq(&msg)->number.len, vcseq.len - 3);
		goto done;
	}
	if(vcseq.len - 3 == get_cseq(&msg)->number.len) {
		/* same length - overwrite in the buffer */
		memcpy(get_cseq(&msg)->number.s, vcseq.s + 3, vcseq.len - 3);
	} else {
		/* pad beginning with space */
		strncpy(get_cseq(&msg)->number.s, "                  ",
				get_cseq(&msg)->number.len - vcseq.len + 3);
		memcpy(get_cseq(&msg)->number.s + get_cseq(&msg)->number.len - vcseq.len
						+ 3,
				vcseq.s + 3, vcseq.len - 3);
	}
	/* our via, will be removed anyhow - let's change cseq part of branch
	 * to a z parameter instead of shifting content to remove it */
	vcseq.s[0] = ';';
	vcseq.s[1] = 'z';
	vcseq.s[2] = '=';

done:
	free_sip_msg(&msg);
	return 0;
}

/**
 *
 */
int dlg_cseq_msg_sent(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	unsigned int direction;
	unsigned int old_cseq = 0;
	unsigned int new_cseq = 0;
	unsigned int cseq_diff = 0;
	unsigned int rack_cseq = 0;
	dlg_cell_t *dlg = NULL;
	str nval = STR_NULL;
	str emval = STR_NULL;
	str rack_tok = STR_NULL;
	struct via_body *via;
	hdr_field_t *hfk = NULL;
	hdr_field_t *rack = NULL;
	sr_cfgenv_t *cenv = NULL;
	str via_cookie = STR_NULL;
	str rack_nval = STR_NULL;
	dlg_cseq_edit_t edits[4];
	int ecnt = 0;
	char via_cookie_buf[64];
	char rack_nval_buf[INT2STR_MAX_LEN];

	obuf = (str *)evp->data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;
	cenv = sr_cfgenv_get();

	if(dlg_cseq_prepare_new_msg(&msg) != 0) {
		goto done;
	}

	if(msg.first_line.type == SIP_REPLY) {
		/* nothing to do for outgoing replies */
		goto done;
	}

	if(!IS_SIP(&msg)) {
		/* nothing to do for non-sip requests */
		goto done;
	}

	if(get_to(&msg)->tag_value.len <= 0) {
		/* initial request - handle only INVITEs, ACKs and CANCELs */
		if(!(msg.first_line.u.request.method_value
				   & (METHOD_INVITE | METHOD_ACK | METHOD_CANCEL))) {
			goto done;
		}
	}

	LM_DBG("tracking cseq updates\n");
	via = (struct via_body *)msg.h_via1->parsed;

	if(via->branch == NULL || via->branch->value.len <= 0) {
		LM_DBG("no branch parameter in top via\n");
		goto done;
	}

	direction = DLG_DIR_NONE;
	dlg = dlg_lookup_msg_dialog(&msg, &direction);

	if(dlg == NULL) {
		LM_DBG("no dialog for this request\n");
		goto done;
	}

	/* supported only for downstream direction */
	if(direction != DLG_DIR_DOWNSTREAM) {
		LM_DBG("request not going downstream (%u)\n", direction);
		goto done;
	}

	if(parse_headers(&msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse all headers\n");
	}

	/* check if transaction is marked for a new increment */
	hfk = sr_hdr_get_z(&msg, cenv->uac_cseq_auth.s);
	if(hfk != NULL) {
		LM_DBG("new cseq inc requested\n");
		nval = hfk->body;
		trim(&nval);
	} else {
		LM_DBG("new cseq inc not requested\n");
	}

	if(nval.len <= 0) {
		hfk = sr_hdr_get_z(&msg, cenv->uac_cseq_refresh.s);
		if(hfk != NULL) {
			LM_DBG("cseq refresh requested\n");
			nval = hfk->body;
			trim(&nval);
		} else {
			LM_DBG("cseq refresh not requested\n");
		}
	}
	if(nval.len <= 0) {
		LM_DBG("cseq refresh requested, but no new value found\n");
		goto done;
	}

	if(str2int(&get_cseq(&msg)->number, &old_cseq) < 0) {
		LM_ERR("failed to parse original cseq number\n");
		goto done;
	}
	if(str2int(&nval, &new_cseq) < 0 || new_cseq < old_cseq) {
		LM_ERR("failed to parse new cseq number\n");
		goto done;
	}
	cseq_diff = new_cseq - old_cseq;
	LM_DBG("updating cseq to: %.*s\n", nval.len, nval.s);

	/* new cseq value */
	dlg->iflags |= DLG_IFLAG_CSEQ_DIFF;

	if(sizeof(via_cookie_buf) <= (size_t)(3 + get_cseq(&msg)->number.len)) {
		LM_ERR("via cseq cookie buffer too small\n");
		goto done;
	}
	memcpy(via_cookie_buf, ".cs", 3);
	memcpy(via_cookie_buf + 3, get_cseq(&msg)->number.s,
			get_cseq(&msg)->number.len);
	via_cookie.s = via_cookie_buf;
	via_cookie.len = 3 + get_cseq(&msg)->number.len;

	if(dlg_cseq_add_edit(edits, &ecnt,
			   via->branch->value.s + via->branch->value.len,
			   via->branch->value.s + via->branch->value.len, &via_cookie)
			< 0) {
		LM_ERR("failed to add via cseq cookie edit\n");
		goto done;
	}

	if(dlg_cseq_add_edit(edits, &ecnt, get_cseq(&msg)->number.s,
			   get_cseq(&msg)->number.s + get_cseq(&msg)->number.len, &nval)
			< 0) {
		LM_ERR("failed to add cseq rewrite edit\n");
		goto done;
	}

	if(msg.first_line.u.request.method_value == METHOD_PRACK && cseq_diff > 0) {
		rack = dlg_cseq_get_rack_hdr(&msg);
		if(rack != NULL
				&& dlg_cseq_get_rack_cseq(rack, &rack_tok, &rack_cseq) == 0) {
			if(UINT_MAX - rack_cseq < cseq_diff) {
				LM_ERR("rack cseq would overflow on rewrite\n");
				goto done;
			}
			rack_nval.s = int2str(rack_cseq + cseq_diff, &rack_nval.len);
			if(rack_nval.len <= 0
					|| rack_nval.len >= (int)sizeof(rack_nval_buf)) {
				LM_ERR("invalid rack cseq rewrite value\n");
				goto done;
			}
			memcpy(rack_nval_buf, rack_nval.s, rack_nval.len);
			rack_nval_buf[rack_nval.len] = '\0';
			rack_nval.s = rack_nval_buf;
			if(dlg_cseq_add_edit(edits, &ecnt, rack_tok.s,
					   rack_tok.s + rack_tok.len, &rack_nval)
					< 0) {
				LM_ERR("failed to add rack rewrite edit\n");
				goto done;
			}
			LM_DBG("updating rack cseq to: %.*s\n", rack_nval.len, rack_nval.s);
		}
	}

	if(hfk != NULL) {
		if(dlg_cseq_add_edit(
				   edits, &ecnt, hfk->name.s, hfk->name.s + hfk->len, &emval)
				< 0) {
			LM_ERR("failed to add helper header removal edit\n");
			goto done;
		}
	}

	dlg_cseq_sort_edits(edits, ecnt);
	if(dlg_cseq_apply_edits(&msg, edits, ecnt, obuf) < 0) {
		goto done;
	}

done:
	if(dlg != NULL)
		dlg_release(dlg);
	free_sip_msg(&msg);
	return 0;
}

/**
 *
 */
int dlg_register_cseq_callbacks(void)
{
	sr_event_register_cb(SREV_NET_DATA_IN, dlg_cseq_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, dlg_cseq_msg_sent);
	return 0;
}

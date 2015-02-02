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

#include "../../events.h"
#include "../../ut.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../modules/tm/tm_load.h"

#include "dlg_handlers.h"
#include "dlg_var.h"
#include "dlg_cseq.h"

extern struct tm_binds d_tmb;

static str _dlg_cseq_diff_var_name = str_init("cseq_diff");

/**
 *
 */
int dlg_cseq_prepare_msg(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg)!=0) {
		LM_DBG("outbuf buffer parsing failed!");
		return 1;
	}

	if(msg->first_line.type==SIP_REQUEST) {
		if(!IS_SIP(msg))
		{
			LM_DBG("non sip request message\n");
			return 1;
		}
	} else if(msg->first_line.type!=SIP_REPLY) {
		LM_DBG("non sip message\n");
		return 1;
	}

	if (parse_headers(msg, HDR_CSEQ_F, 0)==-1) {
		LM_DBG("parsing cseq header failed\n");
		return 2;
	}

	if(msg->first_line.type==SIP_REPLY) {
		/* reply to local transaction -- nothing to do */
		if (parse_headers(msg, HDR_VIA2_F, 0)==-1
				|| (msg->via2==0) || (msg->via2->error!=PARSE_OK)) {
			LM_DBG("no second via in this message \n");
			return 3;
		}
	}

	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse FROM header\n");
		return 3;
	}

	if(parse_to_header(msg)<0 || msg->to==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return 3;
	}

	if(get_to(msg)==NULL)
	{
		LM_ERR("cannot get TO header\n");
		return 3;
	}

	return 0;
}

/**
 *
 */
int dlg_cseq_msg_received(void *data)
{
	sip_msg_t msg;
	str *obuf;
	struct via_body *via;
	str vcseq;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(dlg_cseq_prepare_msg(&msg)!=0) {
		goto done;
	}

	if(msg.first_line.type==SIP_REQUEST) {
		/* nothing to do for incoming requests */
		goto done;
	}

	via = (struct via_body*)msg.h_via1->parsed;

	if(via->branch==NULL || via->branch->value.len<=0) {
		LM_DBG("no branch parameter in top via\n");
		goto done;
	}
	vcseq.s = via->branch->value.s + via->branch->value.len - 1;
	vcseq.len = 0;
	while(vcseq.s>via->branch->value.s) {
		if(*vcseq.s=='.') {
			if(vcseq.len<3) {
				LM_DBG("no matching the starting point length\n");
				goto done;
			}
			if(vcseq.s[1]!='c' || vcseq.s[2]!='s') {
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
			vcseq.len-3, vcseq.s+3);
	if(vcseq.len-3<get_cseq(&msg)->number.len) {
		/* higher lenght to update - wrong */
		LM_DBG("cseq in message (%d) longer than in via (%d)\n",
				get_cseq(&msg)->number.len, vcseq.len-3);
		goto done;
	}
	if(vcseq.len-3==get_cseq(&msg)->number.len) {
		/* same length - overwrite in the buffer */
		memcpy(get_cseq(&msg)->number.s, vcseq.s+3, vcseq.len-3);
	} else {
		/* pad beginning with space */
		strncpy(get_cseq(&msg)->number.s, "                  ",
				get_cseq(&msg)->number.len - vcseq.len + 3);
		memcpy(get_cseq(&msg)->number.s + get_cseq(&msg)->number.len
				- vcseq.len + 3, vcseq.s+3, vcseq.len-3);
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
int dlg_cseq_msg_sent(void *data)
{
	sip_msg_t msg;
	str *obuf;
	unsigned int direction;
	tm_cell_t *t;
	unsigned int ninc = 0;
	unsigned int vinc = 0;
	dlg_cell_t *dlg = NULL;
	str nval;
	str *pval;
	char tbuf[BUF_SIZE];
	int tbuf_len = 0;
	struct via_body *via;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(dlg_cseq_prepare_msg(&msg)!=0) {
		goto done;
	}

	if(msg.first_line.type==SIP_REPLY) {
		/* nothing to do for outgoing replies */
		goto done;
	}

	LM_DBG("traking cseq updates\n");
	via = (struct via_body*)msg.h_via1->parsed;

	if(via->branch==NULL || via->branch->value.len<=0) {
		LM_DBG("no branch parameter in top via\n");
		goto done;
	}

	direction = DLG_DIR_NONE;
	dlg = dlg_lookup_msg_dialog(&msg, &direction);
	
	if(dlg == NULL) {
		LM_DBG("no dialog for this request\n");
		goto done;
	}

	/* supported only for downstrem direction */
	if(direction != DLG_DIR_DOWNSTREAM) {
		LM_DBG("request not going downstream (%u)\n", direction);
		goto done;
	}

	/* check if transaction is marked for a new increment */
	if(get_cseq(&msg)->method_id!=METHOD_ACK) {
		t = d_tmb.t_gett();
		if(t==NULL || t==T_UNDEFINED) {
			LM_DBG("no transaction for request\n");
			goto done;
		}
		if(t->uas.request==0) {
			LM_DBG("no uas request - no cseq update needed\n");
			goto done;
		}
		if((t->uas.request->msg_flags&FL_UAC_AUTH)==FL_UAC_AUTH) {
			LM_DBG("uac auth request - cseq inc needed\n");
			ninc = 1;
		}
	}

	/* take the increment value from dialog */
	if((dlg->iflags&DLG_IFLAG_CSEQ_DIFF)==DLG_IFLAG_CSEQ_DIFF) {
		/* get dialog variable holding cseq diff */
		pval = get_dlg_variable(dlg, &_dlg_cseq_diff_var_name);
		if(pval==NULL || pval->s==NULL || pval->len<=0) {
			LM_DBG("dialog marked with cseq diff but no variable set yet\n");
			goto done;
		}
		if(str2int(pval, &vinc)<0) {
			LM_ERR("invalid dlg cseq diff var value: %.*s\n",
					pval->len, pval->s);
			goto done;
		}
	}
	vinc += ninc;
	if(vinc==0) {
		LM_DBG("nothing to increment\n");
		goto done;
	}
	nval.s = int2str(vinc, &nval.len);
	if(msg.len + 3 + 2*nval.len>=BUF_SIZE) {
		LM_ERR("new messages is too big\n");
		goto done;
	}
	if(set_dlg_variable(dlg, &_dlg_cseq_diff_var_name, &nval) <0) {
		LM_ERR("failed to set the dlg cseq diff var\n");
		goto done;
	}
	str2int(&get_cseq(&msg)->number, &ninc);
	vinc += ninc;
	nval.s = int2str(vinc, &nval.len);

	/* new cseq value */
	dlg->iflags |= DLG_IFLAG_CSEQ_DIFF;

	if(via->branch->value.s<get_cseq(&msg)->number.s) {
		/* Via is before CSeq */
		/* copy first part till after via branch */
		tbuf_len = via->branch->value.s + via->branch->value.len - msg.buf;
		memcpy(tbuf, msg.buf, tbuf_len);
		/* complete via branch */
		tbuf[tbuf_len++] = '.';
		tbuf[tbuf_len++] = 'c';
		tbuf[tbuf_len++] = 's';
		memcpy(tbuf+tbuf_len, get_cseq(&msg)->number.s, get_cseq(&msg)->number.len);
		tbuf_len += get_cseq(&msg)->number.len;
		/* copy till beginning of cseq number */
		memcpy(tbuf+tbuf_len, via->branch->value.s + via->branch->value.len,
				get_cseq(&msg)->number.s - via->branch->value.s
				- via->branch->value.len);
		tbuf_len += get_cseq(&msg)->number.s - via->branch->value.s
					- via->branch->value.len;
		/* add new value */
		memcpy(tbuf+tbuf_len, nval.s, nval.len);
		tbuf_len += nval.len;
		/* copy from after cseq number to the end of sip message */
		memcpy(tbuf+tbuf_len, get_cseq(&msg)->number.s+get_cseq(&msg)->number.len,
				msg.buf + msg.len - get_cseq(&msg)->number.s
				- get_cseq(&msg)->number.len);
		tbuf_len += msg.buf+msg.len - get_cseq(&msg)->number.s
				- get_cseq(&msg)->number.len;
	} else {
		/* CSeq is before Via */
		/* copy till beginning of cseq number */
		tbuf_len = get_cseq(&msg)->number.s - msg.buf;
		memcpy(tbuf, msg.buf, tbuf_len);
		/* add new value */
		memcpy(tbuf+tbuf_len, nval.s, nval.len);
		tbuf_len += nval.len;
		/* copy from after cseq number to the after via branch */
		memcpy(tbuf+tbuf_len, get_cseq(&msg)->number.s+get_cseq(&msg)->number.len,
				via->branch->value.s + via->branch->value.len
				- get_cseq(&msg)->number.s - get_cseq(&msg)->number.len);
		tbuf_len += via->branch->value.s + via->branch->value.len
				- get_cseq(&msg)->number.s - get_cseq(&msg)->number.len;
		/* complete via branch */
		tbuf[tbuf_len++] = '.';
		tbuf[tbuf_len++] = 'c';
		tbuf[tbuf_len++] = 's';
		memcpy(tbuf+tbuf_len, get_cseq(&msg)->number.s, get_cseq(&msg)->number.len);
		tbuf_len += get_cseq(&msg)->number.len;
		/* copy from after via to the end of sip message */
		memcpy(tbuf+tbuf_len, via->branch->value.s + via->branch->value.len,
				msg.buf + msg.len - via->branch->value.s
				- via->branch->value.len);
		tbuf_len += msg.buf+msg.len - via->branch->value.s
				- via->branch->value.len;
	}
	/* replace old msg content */
	obuf->s = pkg_malloc((tbuf_len+1)*sizeof(char));
	if(obuf->s==NULL) {
		LM_ERR("not enough memory for new message\n");
		goto done;
	}
	memcpy(obuf->s, tbuf, tbuf_len);
	obuf->s[tbuf_len] = 0;
	obuf->len = tbuf_len;

done:
	if(dlg!=NULL) dlg_release(dlg);
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

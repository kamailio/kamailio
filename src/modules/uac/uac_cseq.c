/*
 * CSeq update tracking backed by the UAC restore htable.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/data_lump.h"
#include "../../core/events.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/srapi.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../rr/api.h"

#include "replace.h"
#include "uac_cseq.h"

extern struct rr_binds uac_rrb;

typedef struct uac_cseq_edit
{
	char *start;
	char *end;
	str value;
} uac_cseq_edit_t;

static str _uac_cseq_rr_param = str_init(";uacc=1");

static hdr_field_t *uac_cseq_get_rack_hdr(sip_msg_t *msg)
{
	hdr_field_t *hdr;

	for(hdr = msg->headers; hdr != NULL; hdr = hdr->next) {
		if(hdr->name.len == 4 && strncasecmp(hdr->name.s, "RAck", 4) == 0)
			return hdr;
	}
	return NULL;
}

static int uac_cseq_get_rack_cseq(hdr_field_t *hdr, str *tok, unsigned int *val)
{
	str body;
	char *p;
	char *end;

	if(hdr == NULL || tok == NULL || val == NULL)
		return -1;
	body = hdr->body;
	trim(&body);
	p = body.s;
	end = body.s + body.len;
	if(p >= end || !isdigit((unsigned char)*p))
		return -1;
	while(p < end && isdigit((unsigned char)*p))
		p++;
	while(p < end && (*p == ' ' || *p == '\t'))
		p++;
	if(p >= end || !isdigit((unsigned char)*p))
		return -1;
	tok->s = p;
	while(p < end && isdigit((unsigned char)*p))
		p++;
	tok->len = (int)(p - tok->s);
	return str2int(tok, val);
}

static int uac_cseq_add_edit(
		uac_cseq_edit_t *edits, int *count, char *start, char *end, str *value)
{
	if(edits == NULL || count == NULL || start == NULL || end == NULL
			|| value == NULL || end < start || *count >= 4)
		return -1;
	edits[*count].start = start;
	edits[*count].end = end;
	edits[*count].value = *value;
	(*count)++;
	return 0;
}

static void uac_cseq_sort_edits(uac_cseq_edit_t *edits, int count)
{
	uac_cseq_edit_t tmp;
	int i;
	int j;

	for(i = 1; i < count; i++) {
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

static int uac_cseq_apply_edits(
		sip_msg_t *msg, uac_cseq_edit_t *edits, int count, str *obuf)
{
	char tbuf[BUF_SIZE];
	char *src;
	char *end;
	char *nbuf;
	int len = 0;
	int copy;
	int i;

	if(msg == NULL || obuf == NULL || (count > 0 && edits == NULL))
		return -1;
	src = msg->buf;
	end = msg->buf + msg->len;
	for(i = 0; i < count; i++) {
		if(edits[i].start < src || edits[i].start > edits[i].end
				|| edits[i].end > end || edits[i].value.len < 0)
			return -1;
		copy = (int)(edits[i].start - src);
		if(copy > BUF_SIZE - len - edits[i].value.len - 1) {
			LM_ERR("new message is too big\n");
			return -1;
		}
		if(copy > 0) {
			memcpy(tbuf + len, src, copy);
			len += copy;
		}
		if(edits[i].value.len > 0) {
			memcpy(tbuf + len, edits[i].value.s, edits[i].value.len);
			len += edits[i].value.len;
		}
		src = edits[i].end;
	}
	copy = (int)(end - src);
	if(copy > BUF_SIZE - len - 1) {
		LM_ERR("new message is too big\n");
		return -1;
	}
	memcpy(tbuf + len, src, copy);
	len += copy;
	nbuf = pkg_malloc(len + 1);
	if(nbuf == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(nbuf, tbuf, len);
	nbuf[len] = '\0';
	pkg_free(obuf->s);
	obuf->s = nbuf;
	obuf->len = len;
	return 0;
}

static int uac_cseq_prepare_msg(sip_msg_t *msg)
{
	if(msg->first_line.type == SIP_REQUEST) {
		if(!IS_SIP(msg))
			return 1;
	} else if(msg->first_line.type == SIP_REPLY) {
		if(!IS_SIP_REPLY(msg))
			return 1;
	} else {
		return 1;
	}
	if((msg->cseq == NULL
			   && (parse_headers(msg, HDR_CSEQ_F, 0) < 0 || msg->cseq == NULL))
			|| msg->cseq->parsed == NULL)
		return 2;
	if(msg->first_line.type == SIP_REPLY
			&& (parse_headers(msg, HDR_VIA2_F, 0) < 0 || msg->via2 == NULL
					|| msg->via2->error != PARSE_OK)
			&& get_cseq(msg)->method_id != METHOD_CANCEL)
		return 3;
	if(parse_from_header(msg) < 0 || parse_to_header(msg) < 0 || msg->to == NULL
			|| get_to(msg) == NULL)
		return 3;
	return 0;
}

static int uac_cseq_prepare_new_msg(sip_msg_t *msg)
{
	if(parse_msg(msg->buf, msg->len, msg) != 0)
		return 1;
	return uac_cseq_prepare_msg(msg);
}

static int uac_cseq_add_helper(sip_msg_t *msg, char *name, unsigned int diff)
{
	unsigned int cseq;
	str value;
	char value_buf[INT2STR_MAX_LEN];

	if(str2int(&get_cseq(msg)->number, &cseq) < 0 || UINT_MAX - cseq < diff) {
		LM_ERR("invalid cseq value or update overflow\n");
		return -1;
	}
	value.s = int2str(cseq + diff, &value.len);
	if(value.len <= 0 || value.len >= (int)sizeof(value_buf))
		return -1;
	memcpy(value_buf, value.s, value.len);
	value.s = value_buf;
	if(parse_headers(msg, HDR_EOH_F, 0) < 0)
		return -1;
	return sr_hdr_add_zs(msg, name, &value);
}

int uac_cseq_update(sip_msg_t *msg)
{
	sr_cfgenv_t *cenv;
	unsigned int diff;
	int upstream;

	if(uac_cseq_prepare_msg(msg) != 0 || msg->first_line.type != SIP_REQUEST)
		return -1;
	if(uac_htable_cseq_update(msg, &diff, &upstream) < 0)
		return -1;
	if(upstream || diff == 0)
		return 0;
	if(get_to(msg)->tag_value.len == 0
			&& msg->first_line.u.request.method_value == METHOD_INVITE
			&& uac_rrb.add_rr_param(msg, &_uac_cseq_rr_param) != 0)
		LM_WARN("failed to add uac cseq Record-Route marker\n");
	cenv = sr_cfgenv_get();
	return uac_cseq_add_helper(msg, cenv->uac_cseq_auth.s, diff);
}

int uac_cseq_refresh(sip_msg_t *msg)
{
	sr_cfgenv_t *cenv;
	unsigned int diff;
	int upstream;
	int ret;

	if(uac_cseq_prepare_msg(msg) != 0 || msg->first_line.type != SIP_REQUEST)
		return -1;
	ret = uac_htable_cseq_get(msg, 1, &diff, &upstream);
	if(ret > 0 || upstream || diff == 0)
		return 0;
	if(ret < 0)
		return -1;
	cenv = sr_cfgenv_get();
	return uac_cseq_add_helper(msg, cenv->uac_cseq_refresh.s, diff);
}

static int uac_cseq_msg_received(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	struct via_body *via;
	str cookie;
	int found = 0;
	int i;

	obuf = (str *)evp->data;
	memset(&msg, 0, sizeof(msg));
	msg.buf = obuf->s;
	msg.len = obuf->len;
	if(uac_cseq_prepare_new_msg(&msg) != 0 || msg.first_line.type == SIP_REQUEST
			|| msg.h_via1 == NULL)
		goto done;
	via = (struct via_body *)msg.h_via1->parsed;
	if(via == NULL || via->branch == NULL || via->branch->value.len <= 0)
		goto done;
	cookie.s = via->branch->value.s + via->branch->value.len - 1;
	cookie.len = 0;
	while(cookie.s > via->branch->value.s) {
		if(*cookie.s == '.') {
			if(cookie.len < 3 || cookie.s[1] != 'c' || cookie.s[2] != 's')
				goto done;
			cookie.len++;
			found = 1;
			break;
		}
		cookie.len++;
		cookie.s--;
	}
	if(!found || cookie.len < 4 || cookie.len - 3 > get_cseq(&msg)->number.len)
		goto done;
	for(i = 3; i < cookie.len; i++) {
		if(!isdigit((unsigned char)cookie.s[i]))
			goto done;
	}
	if(cookie.len - 3 < get_cseq(&msg)->number.len)
		memset(get_cseq(&msg)->number.s, ' ',
				get_cseq(&msg)->number.len - cookie.len + 3);
	memcpy(get_cseq(&msg)->number.s + get_cseq(&msg)->number.len - cookie.len
					+ 3,
			cookie.s + 3, cookie.len - 3);
	cookie.s[0] = ';';
	cookie.s[1] = 'z';
	cookie.s[2] = '=';

done:
	free_sip_msg(&msg);
	return 0;
}

static int uac_cseq_msg_sent(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	str nval = STR_NULL;
	str empty = STR_NULL;
	str via_cookie;
	str rack_tok;
	str rack_value;
	hdr_field_t *helper = NULL;
	hdr_field_t *rack;
	struct via_body *via;
	sr_cfgenv_t *cenv;
	uac_cseq_edit_t edits[4];
	unsigned int old_cseq;
	unsigned int new_cseq;
	unsigned int rack_cseq;
	unsigned int diff;
	unsigned int stored_diff;
	int upstream;
	int state;
	int count = 0;
	char via_buf[64];
	char rack_buf[INT2STR_MAX_LEN];

	obuf = (str *)evp->data;
	memset(&msg, 0, sizeof(msg));
	msg.buf = obuf->s;
	msg.len = obuf->len;
	if(uac_cseq_prepare_new_msg(&msg) != 0 || msg.first_line.type != SIP_REQUEST
			|| !IS_SIP(&msg) || msg.h_via1 == NULL)
		goto done;
	via = (struct via_body *)msg.h_via1->parsed;
	if(via == NULL || via->branch == NULL || via->branch->value.len <= 0)
		goto done;
	if(parse_headers(&msg, HDR_EOH_F, 0) < 0)
		goto done;
	cenv = sr_cfgenv_get();
	helper = sr_hdr_get_z(&msg, cenv->uac_cseq_auth.s);
	if(helper == NULL)
		helper = sr_hdr_get_z(&msg, cenv->uac_cseq_refresh.s);
	if(helper == NULL)
		goto done;
	nval = helper->body;
	trim(&nval);
	if(nval.len <= 0 || str2int(&get_cseq(&msg)->number, &old_cseq) < 0
			|| str2int(&nval, &new_cseq) < 0 || new_cseq < old_cseq)
		goto strip_helper;
	diff = new_cseq - old_cseq;
	state = uac_htable_cseq_get(&msg, 0, &stored_diff, &upstream);
	if(state != 0 || upstream || stored_diff != diff) {
		LM_WARN("ignoring unverified uac cseq helper header\n");
		goto strip_helper;
	}
	if((size_t)(3 + get_cseq(&msg)->number.len) >= sizeof(via_buf))
		goto strip_helper;
	memcpy(via_buf, ".cs", 3);
	memcpy(via_buf + 3, get_cseq(&msg)->number.s, get_cseq(&msg)->number.len);
	via_cookie.s = via_buf;
	via_cookie.len = 3 + get_cseq(&msg)->number.len;
	if(uac_cseq_add_edit(edits, &count,
			   via->branch->value.s + via->branch->value.len,
			   via->branch->value.s + via->branch->value.len, &via_cookie)
					< 0
			|| uac_cseq_add_edit(edits, &count, get_cseq(&msg)->number.s,
					   get_cseq(&msg)->number.s + get_cseq(&msg)->number.len,
					   &nval)
					   < 0)
		goto strip_helper;
	if(msg.first_line.u.request.method_value == METHOD_PRACK && diff > 0) {
		rack = uac_cseq_get_rack_hdr(&msg);
		if(rack != NULL
				&& uac_cseq_get_rack_cseq(rack, &rack_tok, &rack_cseq) == 0) {
			if(UINT_MAX - rack_cseq < diff)
				goto strip_helper;
			rack_value.s = int2str(rack_cseq + diff, &rack_value.len);
			if(rack_value.len <= 0 || rack_value.len >= (int)sizeof(rack_buf))
				goto strip_helper;
			memcpy(rack_buf, rack_value.s, rack_value.len);
			rack_value.s = rack_buf;
			if(uac_cseq_add_edit(edits, &count, rack_tok.s,
					   rack_tok.s + rack_tok.len, &rack_value)
					< 0)
				goto strip_helper;
		}
	}
	if(uac_cseq_add_edit(edits, &count, helper->name.s,
			   helper->name.s + helper->len, &empty)
			< 0)
		goto done;
	uac_cseq_sort_edits(edits, count);
	uac_cseq_apply_edits(&msg, edits, count, obuf);
	goto done;

strip_helper:
	count = 0;
	if(helper != NULL
			&& uac_cseq_add_edit(edits, &count, helper->name.s,
					   helper->name.s + helper->len, &empty)
					   == 0)
		uac_cseq_apply_edits(&msg, edits, count, obuf);

done:
	free_sip_msg(&msg);
	return 0;
}

int uac_cseq_register_callbacks(void)
{
	if(sr_event_register_cb(SREV_NET_DATA_IN, uac_cseq_msg_received) < 0
			|| sr_event_register_cb(SREV_NET_DATA_OUT, uac_cseq_msg_sent) < 0) {
		LM_ERR("failed to register cseq network callbacks\n");
		return -1;
	}
	return 0;
}

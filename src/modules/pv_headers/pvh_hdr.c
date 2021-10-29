/*
 * pv_headers
 *
 * Copyright (C)
 * 2020 Victor Seva <vseva@sipwise.com>
 * 2018 Kirill Solomko <ksolomko@sipwise.com>
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

#include "../../core/data_lump.h"
#include "../../core/dset.h"

#include "pvh_hdr.h"

int pvh_hdrs_collected(struct sip_msg *msg)
{
	if(msg->first_line.type == SIP_REPLY) {
		if(isflagset(msg, FL_PV_HDRS_COLLECTED) == 1) {
			return 1;
		}
	} else {
		if(isbflagset(_branch, FL_PV_HDRS_COLLECTED) == 1) {
			return 1;
		}
	}
	return 0;
}

int pvh_hdrs_applied(struct sip_msg *msg)
{
	if(msg->first_line.type == SIP_REPLY) {
		if(isflagset(msg, FL_PV_HDRS_APPLIED) == 1) {
			return 1;
		}
	} else {
		if(isbflagset(_branch, FL_PV_HDRS_APPLIED) == 1) {
			return 1;
		}
	}
	return 0;
}

void pvh_hdrs_set_applied(struct sip_msg *msg)
{
	if(msg->first_line.type == SIP_REPLY) {
		setflag(msg, FL_PV_HDRS_APPLIED);
	} else {
		setbflag(_branch, FL_PV_HDRS_APPLIED);
	}
}

void pvh_hdrs_set_collected(struct sip_msg *msg)
{
	if(msg->first_line.type == SIP_REPLY) {
		setflag(msg, FL_PV_HDRS_COLLECTED);
	} else {
		setbflag(_branch, FL_PV_HDRS_COLLECTED);
	}
}

void pvh_hdrs_reset_flags(struct sip_msg *msg)
{
	if(msg->first_line.type == SIP_REPLY) {
		resetflag(msg, FL_PV_HDRS_COLLECTED);
		resetflag(msg, FL_PV_HDRS_APPLIED);
	} else {
		resetbflag(_branch, FL_PV_HDRS_COLLECTED);
		resetbflag(_branch, FL_PV_HDRS_APPLIED);
	}
}

int pvh_real_hdr_append(struct sip_msg *msg, str *hname, str *hvalue)
{
	struct lump *anchor = NULL;
	hdr_field_t *hf = NULL;
	hdr_field_t *m_hf = NULL;
	str new_h = STR_NULL;

	if(hname->s == NULL || hvalue->s == NULL) {
		LM_ERR("header name/value cannot be empty");
		return -1;
	}

	// find last header matching the name
	for(hf = msg->headers; hf; hf = hf->next) {
		if(hf->name.len == hname->len
				&& strncasecmp(hf->name.s, hname->s, hname->len) == 0) {
			m_hf = hf;
		}
		if(!hf->next)
			break;
	}

	if(m_hf == NULL)
		anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	else
		anchor = anchor_lump(msg, m_hf->name.s + m_hf->len - msg->buf, 0, 0);

	if(anchor == 0) {
		LM_ERR("unable to find header lump\n");
		return -1;
	}

	if(pvh_create_hdr_str(hname, hvalue, &new_h) <= 0)
		return -1;

	if(insert_new_lump_after(anchor, new_h.s, new_h.len, 0) == 0) {
		LM_ERR("cannot insert header lump\n");
		pkg_free(new_h.s);
		return -1;
	}

	LM_DBG("append header: %.*s\n", new_h.len, new_h.s);

	return 1;
}

int pvh_real_hdr_remove_display(struct sip_msg *msg, str *hname)
{
	hdr_field_t *hf = NULL;
	struct to_body *d_hf = NULL;
	int disp_len = 0;

	for(hf = msg->headers; hf; hf = hf->next) {
		if(hf->name.len == hname->len
				&& strncasecmp(hf->name.s, hname->s, hname->len) == 0) {
			d_hf = (struct to_body *)hf->parsed;
			if((disp_len = d_hf->display.len) > 0) {
				LM_DBG("remove display[%.*s]: %.*s\n", hf->name.len, hf->name.s,
						disp_len, d_hf->display.s);
				if(strncmp(d_hf->display.s + disp_len, " ", 1) == 0)
					disp_len++;
				del_lump(msg, d_hf->display.s - msg->buf, disp_len, 0);
			}
		}
	}
	return 1;
}

int pvh_real_replace_reply_reason(struct sip_msg *msg, str *value)
{
	struct lump *anchor = NULL;
	char *reason = NULL;

	anchor = del_lump(msg, msg->first_line.u.reply.reason.s - msg->buf,
			msg->first_line.u.reply.reason.len, 0);
	if(!anchor) {
		LM_ERR("set reply: failed to del lump\n");
		goto err;
	}

	reason = (char *)pkg_malloc(value->len);
	if(reason == NULL) {
		PKG_MEM_ERROR;
		goto err;
	}
	memcpy(reason, value->s, value->len);

	if(insert_new_lump_after(anchor, reason, value->len, 0) == 0) {
		LM_ERR("set reply: failed to add lump: %.*s\n", value->len, value->s);
		goto err;
	}

	return 1;

err:
	if(reason)
		pkg_free(reason);
	return -1;
}

int pvh_create_hdr_str(str *hname, str *hvalue, str *dst)
{
	int os;
	if(hname->s == NULL || hvalue->s == NULL) {
		LM_ERR("header name/value cannot be empty");
		return -1;
	}

	if(dst == NULL) {
		LM_ERR("new header str cannot be null");
		return -1;
	}

	dst->len = hname->len + 2 + hvalue->len + CRLF_LEN;
	dst->s = (char *)pkg_malloc(dst->len + 1);
	if(dst->s == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(dst->s, 0, dst->len + 1);

	os = 0;
	memcpy(dst->s, hname->s, hname->len);
	os += hname->len;
	memcpy(dst->s + os, ": ", 2);
	os += 2;
	memcpy(dst->s + os, hvalue->s, hvalue->len);
	os += hvalue->len;
	memcpy(dst->s + os, CRLF, CRLF_LEN);
	os += CRLF_LEN;
	dst->s[dst->len] = '\0';

	return 1;
}

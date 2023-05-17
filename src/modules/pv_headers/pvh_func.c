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
#include "../../core/pvapi.h"
#include "../../core/strutils.h"

#include "pv_headers.h"
#include "pvh_func.h"
#include "pvh_xavp.h"
#include "pvh_str.h"
#include "pvh_hash.h"
#include "pvh_hdr.h"

static str xavi_helper_name = str_init("xavi_name");

int pvh_parse_msg(sip_msg_t *msg)
{
	if(msg->first_line.type == SIP_REQUEST) {
		if(!IS_SIP(msg)) {
			LM_DBG("non SIP request message\n");
			return 1;
		}
	} else if(msg->first_line.type == SIP_REPLY) {
		if(!IS_SIP_REPLY(msg)) {
			LM_DBG("non SIP reply message\n");
			return 1;
		}
	} else {
		LM_DBG("non SIP message\n");
		return 1;
	}

	return 0;
}

int pvh_collect_headers(struct sip_msg *msg)
{
	struct hdr_field *hf = NULL;
	str name = STR_NULL;
	str val = STR_NULL;
	char hvals[header_name_size][header_value_size];
	int idx = 0, d_size = 0;
	str val_part = STR_NULL;
	char *marker = NULL;

	if(pvh_hdrs_collected(msg)) {
		LM_ERR("headers are already collected\n");
		return -1;
	}

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	for(hf = msg->headers; hf; hf = hf->next) {
		LM_DBG("collect header[%.*s]: %.*s\n", hf->name.len, hf->name.s,
				hf->body.len, hf->body.s);

		switch(hf->type) {
			case HDR_FROM_T:
				name.len = _hdr_from.len;
				name.s = _hdr_from.s;
				LM_DBG("force [From] as key\n");
				break;
			case HDR_TO_T:
				name.len = _hdr_to.len;
				name.s = _hdr_to.s;
				LM_DBG("force [To] as key\n");
				break;
			default:
				name.len = hf->name.len;
				name.s = hf->name.s;
		}
		val.len = hf->body.len;
		val.s = hf->body.s;

		if((marker = pvh_detect_split_char(val.s)) != NULL
				&& str_hash_case_get(&split_headers, name.s, name.len)) {

			if(pvh_split_values(&val, hvals, &d_size, 1, marker) < 0) {
				LM_ERR("could not parse %.*s header comma separated "
					   "value",
						name.len, name.s);
				return -1;
			}

			for(idx = 0; idx < d_size; idx++) {
				val_part.s = hvals[idx];
				val_part.len = strlen(hvals[idx]);
				if(pvh_set_xavi(msg, &xavi_name, &name, &val_part, SR_XTYPE_STR,
						   0, 1)
						< 0)
					return -1;
			}
			continue;
		}
		if(pvh_set_xavi(msg, &xavi_name, &name, &val, SR_XTYPE_STR, 0, 1) < 0)
			return -1;
	}

	if(pvh_set_xavi(msg, &xavi_helper_xname, &xavi_helper_name, &xavi_name,
			   SR_XTYPE_STR, 0, 0)
			< 0)
		return -1;

	pvh_hdrs_set_collected(msg);

	return 1;
}

int pvh_apply_headers(struct sip_msg *msg)
{
	sr_xavp_t *xavi = NULL;
	sr_xavp_t *sub = NULL;
	struct str_hash_table rm_hdrs;
	int from_cnt = 0, to_cnt = 0;
	char t[header_name_size];
	char tv[2][header_value_size];
	str display = {tv[0], header_value_size};
	str uri = {tv[1], header_value_size};
	str br_xname = {t, header_name_size};
	int skip_from_to = 0, keys_count = 0;
	int res = -1;

	memset(&rm_hdrs, 0, sizeof(struct str_hash_table));

	if(pvh_hdrs_applied(msg)) {
		LM_ERR("headers are already applied\n");
		return -1;
	}

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	pvh_get_branch_xname(msg, &xavi_name, &br_xname);

	if((xavi = xavi_get(&br_xname, NULL)) == NULL
			&& (xavi = xavi_get(&xavi_name, NULL)) == NULL) {
		LM_ERR("missing xavi %.*s, run pv_collect_headers() first\n",
				xavi_name.len, xavi_name.s);
		return -1;
	}
	if(xavi->val.type != SR_XTYPE_XAVP) {
		LM_ERR("not xavp child type %.*s\n", xavi_name.len, xavi_name.s);
		return -1;
	}

	if((sub = xavi->val.v.xavp) == NULL) {
		LM_ERR("invalid xavp structure: %.*s\n", xavi_name.len, xavi_name.s);
		return -1;
	}
	keys_count = pvh_xavi_keys_count(&sub);
	if(keys_count <= 0) {
		LM_ERR("no keys found: %.*s\n", xavi_name.len, xavi_name.s);
		return -1;
	}
	if(str_hash_alloc(&rm_hdrs, keys_count) < 0) {
		PKG_MEM_ERROR;
		return -1;
	}
	LM_DBG("xavi->name:%.*s br_xname:%.*s keys_count: %d\n", xavi->name.len,
			xavi->name.s, br_xname.len, br_xname.s, keys_count);
	str_hash_init(&rm_hdrs);

	if(msg->first_line.type == SIP_REPLY
			|| msg->first_line.u.request.method_value == METHOD_ACK
			|| msg->first_line.u.request.method_value == METHOD_PRACK
			|| msg->first_line.u.request.method_value == METHOD_BYE) {
		skip_from_to = 1;
		if(msg->to == NULL) {
			LM_DBG("no To header, can't store To info in parsed\n");
		} else {
			if(pvh_set_parsed(msg, &_hdr_to, &msg->to->body, NULL) == NULL)
				LM_ERR("can't store To info in parsed\n");
		}
	}

	do {
		if(pvh_skip_header(&sub->name))
			continue;

		if(cmpi_str(&sub->name, &_hdr_from) == 0) {
			if(skip_from_to) {
				LM_DBG("skip From header change in reply messages\n");
				continue;
			}
			if(cmp_str(&sub->val.v.s, &msg->from->body) == 0) {
				LM_DBG("skip unchanged From header\n");
				continue;
			}
			if(from_cnt > 0)
				continue;

			memset(display.s, 0, header_value_size);
			memset(uri.s, 0, header_value_size);

			if(pvh_extract_display_uri(sub->val.v.s.s, &display, &uri) < 0) {
				LM_ERR("error parsing From header\n");
				goto err;
			}

			if(uac.replace_from != NULL) {
				LM_DBG("replace_from[%s]: %s %s\n", sub->name.s, display.s,
						uri.s);
				if(display.len == 0)
					pvh_real_hdr_remove_display(msg, &sub->name);
				uac.replace_from(msg, &display, &uri);
			}

			from_cnt++;
			continue;
		}

		if(cmpi_str(&sub->name, &_hdr_to) == 0) {
			if(skip_from_to) {
				LM_DBG("skip To header change in reply messages\n");
				continue;
			}
			if(cmp_str(&sub->val.v.s, &msg->to->body) == 0) {
				LM_DBG("skip unchanged To header\n");
				continue;
			}
			if(to_cnt > 0)
				continue;

			memset(display.s, 0, header_value_size);
			memset(uri.s, 0, header_value_size);

			if(pvh_extract_display_uri(sub->val.v.s.s, &display, &uri) < 0) {
				LM_ERR("error parsing To header\n");
				goto err;
			}

			if(uac.replace_to != NULL) {
				LM_DBG("replace_to[%s]: %s %s\n", sub->name.s, display.s,
						uri.s);
				if(display.len == 0)
					pvh_real_hdr_remove_display(msg, &sub->name);
				uac.replace_to(msg, &display, &uri);
			}

			to_cnt++;
			continue;
		}

		if(cmpi_str(&sub->name, &_hdr_reply_reason) == 0) {
			if(str_hash_case_get(&rm_hdrs, sub->name.s, sub->name.len))
				continue;
			pvh_real_replace_reply_reason(msg, &sub->val.v.s);
			pvh_str_hash_add_key(&rm_hdrs, &sub->name);
			continue;
		}

		if(!str_hash_case_get(&rm_hdrs, sub->name.s, sub->name.len)) {
			LM_DBG("remove header[%s]: %s\n", sub->name.s, sub->val.v.s.s);
			pvh_real_hdr_del_by_name(msg, &sub->name);
			pvh_str_hash_add_key(&rm_hdrs, &sub->name);
		}

		if(!pvh_avp_is_null(sub) && !pvh_single_header(&sub->name)) {
			pvh_real_hdr_append(msg, &sub->name, &sub->val.v.s);
			LM_DBG("append header[%s]: %s\n", sub->name.s, sub->val.v.s.s);
		}
	} while((sub = sub->next) != NULL);

	pvh_hdrs_set_applied(msg);

	res = 1;

err:
	if(rm_hdrs.table)
		pvh_str_hash_free(&rm_hdrs);
	return res;
}

int pvh_reset_headers(struct sip_msg *msg)
{
	char t[header_name_size];
	str br_xname = {t, header_name_size};

	pvh_get_branch_xname(msg, &xavi_name, &br_xname);
	LM_DBG("clean xavi:%.*s\n", br_xname.len, br_xname.s);
	xavi_rm_by_name(&br_xname, 1, NULL);
	pvh_get_branch_xname(msg, &xavi_parsed_xname, &br_xname);
	LM_DBG("clean xavi:%.*s\n", br_xname.len, br_xname.s);
	xavi_rm_by_name(&br_xname, 1, NULL);

	pvh_hdrs_reset_flags(msg);

	return 1;
}

int pvh_check_header(struct sip_msg *msg, str *hname)
{

	if(pvh_xavi_get_child(msg, &xavi_name, hname) == NULL)
		return -1;

	return 1;
}

int pvh_append_header(struct sip_msg *msg, str *hname, str *hvalue)
{
	return pvh_set_xavi(msg, &xavi_name, hname, hvalue, SR_XTYPE_STR, 0, 1);
}

int pvh_modify_header(struct sip_msg *msg, str *hname, str *hvalue, int indx)
{
	return pvh_set_xavi(msg, &xavi_name, hname, hvalue, SR_XTYPE_STR, indx, 0);
}

int pvh_remove_header(struct sip_msg *msg, str *hname, int indx)
{
	sr_xavp_t *avp = NULL;
	int count = 0;

	if((avp = pvh_xavi_get_child(msg, &xavi_name, hname)) == NULL)
		return 1;

	if(indx < 0) {
		count = xavi_count(hname, &avp);
		do {
			if(pvh_set_xavi(
					   msg, &xavi_name, hname, NULL, SR_XTYPE_STR, indx++, 0)
					< 1)
				return -1;
		} while(indx < count);
	} else {
		if(pvh_set_xavi(msg, &xavi_name, hname, NULL, SR_XTYPE_STR, indx, 0)
				< 1)
			return -1;
	}

	return 1;
}

int pvh_header_param_exists(struct sip_msg *msg, str *hname, str *hvalue)
{
	sr_xavp_t *avi = NULL;
	char head_name[header_name_size];
	str br_xname = {head_name, header_name_size};

	avi = xavi_get(&xavi_name, NULL);
	pvh_get_branch_xname(msg, &xavi_name, &br_xname);

	avi = xavi_get_child(&br_xname, hname);

	while(avi) {
		if(avi->val.type == SR_XTYPE_STR && avi->val.v.s.s != NULL
				&& _strnstr(avi->val.v.s.s, hvalue->s, avi->val.v.s.len)
						   != NULL) {
			return 1;
		}
		avi = xavi_get_next(avi);
	}

	return -1;
}

int pvh_remove_header_param_helper(str *orig, const str *toRemove, str *dst)
{
	int notTarget;
	int writtenChars = 0;
	int offset = 0;
	char *saveptr = NULL;
	char *token;
	char t[header_value_size];
	char *result = pv_get_buffer();
	int maxSize = pv_get_buffer_size();

	memset(result, 0, maxSize);
	LM_DBG("orig:'%.*s' toRemove:'%.*s'\n", STR_FMT(orig), STR_FMT(toRemove));
	strncpy(t, orig->s, orig->len);
	t[orig->len] = '\0';
	token = strtok_r(t, ", ", &saveptr);
	dst->s = NULL;
	dst->len = -1;
	while(token) {
		notTarget = strncasecmp(token, toRemove->s, toRemove->len);
		LM_DBG("offset:%d token:%s notTarget:%d\n", offset, token, notTarget);
		if(notTarget) {
			writtenChars =
					snprintf(result + offset, maxSize - offset, "%s, ", token);
			if(writtenChars < 0)
				break;
			offset += writtenChars;
		} else {
			dst->len = 0; /* we found a token */
		}
		token = strtok_r(NULL, ", ", &saveptr);
	}

	if(offset > 0) {
		dst->s = result;
		if(offset > 2 && result[offset - 2] == ','
				&& result[offset - 1] == ' ') {
			LM_DBG("remove last separator\n");
			offset = offset - 2;
			result[offset] = '\0';
		}
		dst->len = offset;
		LM_DBG("offset:%d result:'%.*s'[%d]\n", offset, STR_FMT(dst), dst->len);
	}
	return offset;
}

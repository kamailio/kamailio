/*
 * pv_headers
 *
 * Copyright (C) 2018 Kirill Solomko <ksolomko@sipwise.com>
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../core/dset.h"

#include "pv_headers.h"
#include "pvh_str.h"
#include "pvh_hdr.h"
#include "pvh_hash.h"
#include "pvh_xavp.h"

static str xavp_helper_xname = str_init("modparam_pv_headers");
static str xavp_helper_name = str_init("xavp_name");

int pvh_collect_headers(struct sip_msg *msg, int is_auto)
{
	struct hdr_field *hf = NULL;
	str name = STR_NULL;
	str val = STR_NULL;
	char hvals[header_name_size][header_value_size];
	int idx = 0, d_size = 0;
	str val_part = STR_NULL;
	int br_idx;

	pvh_get_branch_index(msg, &br_idx);
	LM_DBG("br_idx: %d\n", br_idx);
	if(!is_auto) {
		if(msg->first_line.type == SIP_REPLY) {
			if(isflagset(msg, FL_PV_HDRS_COLLECTED) == 1) {
				LM_ERR("headers are already collected\n");
				return -1;
			}
		} else {
			if(isbflagset(br_idx, FL_PV_HDRS_COLLECTED)) {
				LM_ERR("headers are already collected\n");
				return -1;
			}
		}
	}

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	if(pvh_str_new(&name, header_name_size) < 0)
		goto err;
	if(pvh_str_new(&val, header_value_size) < 0)
		goto err;

	if(name.s == NULL || val.s == NULL)
		goto err;

	for(hf = msg->headers; hf; hf = hf->next) {
		LM_DBG("collect header[%.*s]: %.*s\n", hf->name.len, hf->name.s,
				hf->body.len, hf->body.s);

		switch(hf->type) {
			case HDR_FROM_T:
				pvh_str_copy(&name, &_hdr_from, header_name_size);
				LM_DBG("force [From] as key\n");
				break;
			case HDR_TO_T:
				pvh_str_copy(&name, &_hdr_to, header_name_size);
				LM_DBG("force [To] as key\n");
				break;
			default:
				pvh_str_copy(&name, &hf->name, header_name_size);
		}
		pvh_str_copy(&val, &hf->body, header_value_size);

		if(str_hash_get(&split_headers, name.s, name.len)
				&& strchr(val.s, ',') != NULL) {

			if(pvh_split_values(&val, hvals, &d_size, 1) < 0) {
				LM_ERR("could not parse Diversion header comma separated "
					   "value");
				return -1;
			}

			for(idx = 0; idx < d_size; idx++) {
				val_part.s = hvals[idx];
				val_part.len = strlen(hvals[idx]);
				if(pvh_set_xavp(msg, &xavp_name, &name, &val_part, SR_XTYPE_STR,
						   0, 1)
						< 0)
					goto err;
			}
			continue;
		}
		if(pvh_set_xavp(msg, &xavp_name, &name, &val, SR_XTYPE_STR, 0, 1) < 0)
			goto err;
	}

	if(pvh_set_xavp(msg, &xavp_helper_xname, &xavp_helper_name, &xavp_name,
			   SR_XTYPE_STR, 0, 0)
			< 0)
		goto err;

	pvh_str_free(&name);
	pvh_str_free(&val);

	msg->first_line.type == SIP_REPLY ? setflag(msg, FL_PV_HDRS_COLLECTED)
									  : setbflag(br_idx, FL_PV_HDRS_COLLECTED);

	return 1;

err:
	pvh_str_free(&name);
	pvh_str_free(&val);
	return -1;
}

int pvh_apply_headers(struct sip_msg *msg, int is_auto)
{
	sr_xavp_t *xavp = NULL;
	sr_xavp_t *sub = NULL;
	str display = STR_NULL;
	str uri = STR_NULL;
	struct str_hash_table rm_hdrs;
	int from_cnt = 0, to_cnt = 0;
	str br_xname = STR_NULL;
	int br_idx, keys_count;
	int res = -1;

	rm_hdrs.size = 0;

	pvh_get_branch_index(msg, &br_idx);

	if(!is_auto) {
		if(msg->first_line.type == SIP_REPLY) {
			if(isflagset(msg, FL_PV_HDRS_APPLIED) == 1) {
				LM_ERR("headers are already applied\n");
				return -1;
			}
		} else {
			if(isbflagset(br_idx, FL_PV_HDRS_APPLIED) == 1) {
				LM_ERR("headers are already applied\n");
				return -1;
			}
		}
	}

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	if(pvh_str_new(&display, header_value_size) < 0)
		goto err;
	if(pvh_str_new(&uri, header_value_size) < 0)
		goto err;
	if(pvh_str_new(&br_xname, header_value_size) < 0)
		goto err;

	pvh_get_branch_xname(msg, &xavp_name, &br_xname);

	if((xavp = xavp_get(&br_xname, NULL)) == NULL
			&& (xavp = xavp_get(&xavp_name, NULL)) == NULL) {
		LM_ERR("missing xavp %s, run pv_collect_headers() first\n",
				xavp_name.s);
		goto err;
	}
	if(xavp->val.type != SR_XTYPE_XAVP) {
		LM_ERR("not xavp child type %s\n", xavp_name.s);
		goto err;
	}

	if((sub = xavp->val.v.xavp) == NULL) {
		LM_ERR("invalid xavp structure: %s\n", xavp_name.s);
		goto err;
	}
	keys_count = pvh_xavp_keys_count(&sub);
	if(str_hash_alloc(&rm_hdrs, keys_count) < 0) {
		PKG_MEM_ERROR;
		goto err;
	}
	LM_DBG("xavp->name:%.*s br_xname:%.*s keys_count: %d\n", xavp->name.len,
			xavp->name.s, br_xname.len, br_xname.s, keys_count);
	str_hash_init(&rm_hdrs);

	do {
		if(pvh_skip_header(&sub->name))
			continue;

		if(strncasecmp(sub->name.s, _hdr_from.s, sub->name.len) == 0) {
			if(msg->first_line.type == SIP_REPLY
					|| msg->first_line.u.request.method_value == METHOD_ACK
					|| msg->first_line.u.request.method_value == METHOD_PRACK
					|| msg->first_line.u.request.method_value == METHOD_BYE) {
				LM_DBG("skip From header change in reply messages\n");
				continue;
			}
			if(strncmp(sub->val.v.s.s, msg->from->body.s, sub->val.v.s.len)
					== 0) {
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

		if(strncasecmp(sub->name.s, _hdr_to.s, sub->name.len) == 0) {
			if(msg->first_line.type == SIP_REPLY
					|| msg->first_line.u.request.method_value == METHOD_ACK
					|| msg->first_line.u.request.method_value == METHOD_PRACK
					|| msg->first_line.u.request.method_value == METHOD_BYE) {
				LM_DBG("skip To header change in reply messages\n");
				continue;
			}
			if(strncmp(sub->val.v.s.s, msg->to->body.s, sub->val.v.s.len)
					== 0) {
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

		if(strncasecmp(sub->name.s, "@Reply-Reason", sub->name.len) == 0) {
			if(str_hash_get(&rm_hdrs, sub->name.s, sub->name.len))
				continue;
			pvh_real_replace_reply_reason(msg, &sub->val.v.s);
			pvh_str_hash_add_key(&rm_hdrs, &sub->name);
			continue;
		}

		if(!str_hash_get(&rm_hdrs, sub->name.s, sub->name.len)) {
			if(!pvh_xavp_is_null(sub) && xavp_count(&sub->name, &sub) == 1) {
				LM_DBG("replace header[%s]: %s\n", sub->name.s, sub->val.v.s.s);
				pvh_real_hdr_replace(msg, &sub->name, &sub->val.v.s);
				pvh_str_hash_add_key(&rm_hdrs, &sub->name);
				continue;
			}
			LM_DBG("remove header[%s]: %s\n", sub->name.s, sub->val.v.s.s);
			pvh_real_hdr_del_by_name(msg, &sub->name);
			pvh_str_hash_add_key(&rm_hdrs, &sub->name);
		}

		if(!pvh_xavp_is_null(sub) && !pvh_single_header(&sub->name)) {
			pvh_real_hdr_append(msg, &sub->name, &sub->val.v.s);
			LM_DBG("append header[%s]: %s\n", sub->name.s, sub->val.v.s.s);
		}
	} while((sub = sub->next) != NULL);

	msg->first_line.type == SIP_REPLY ? setflag(msg, FL_PV_HDRS_APPLIED)
									  : setbflag(br_idx, FL_PV_HDRS_APPLIED);

	res = 1;

err:
	pvh_str_free(&display);
	pvh_str_free(&uri);
	pvh_str_free(&br_xname);
	if(rm_hdrs.size)
		pvh_str_hash_free(&rm_hdrs);
	return res;
}

int pvh_reset_headers(struct sip_msg *msg)
{
	str br_xname = STR_NULL;
	int br_idx;

	if(pvh_str_new(&br_xname, header_name_size) < 0)
		return -1;

	pvh_get_branch_index(msg, &br_idx);
	pvh_get_branch_xname(msg, &xavp_name, &br_xname);

	pvh_free_xavp(&br_xname);
	pvh_get_branch_xname(msg, &xavp_parsed_xname, &br_xname);
	pvh_free_xavp(&br_xname);

	if(msg->first_line.type == SIP_REPLY) {
		resetflag(msg, FL_PV_HDRS_COLLECTED);
		resetflag(msg, FL_PV_HDRS_APPLIED);
	} else {
		resetbflag(br_idx, FL_PV_HDRS_COLLECTED);
		resetbflag(br_idx, FL_PV_HDRS_APPLIED);
	}

	pvh_str_free(&br_xname);

	return 1;
}

int pvh_check_header(struct sip_msg *msg, str *hname)
{

	if(pvh_xavp_get_child(msg, &xavp_name, hname) == NULL)
		return -1;

	return 1;
}

int pvh_append_header(struct sip_msg *msg, str *hname, str *hvalue)
{
	return pvh_set_xavp(msg, &xavp_name, hname, hvalue, SR_XTYPE_STR, 0, 1);
}

int pvh_modify_header(struct sip_msg *msg, str *hname, str *hvalue, int indx)
{
	return pvh_set_xavp(msg, &xavp_name, hname, hvalue, SR_XTYPE_STR, indx, 0);
}

int pvh_remove_header(struct sip_msg *msg, str *hname, int indx)
{
	sr_xavp_t *avp = NULL;
	int count = 0;

	if((avp = pvh_xavp_get_child(msg, &xavp_name, hname)) == NULL)
		return 1;

	if(indx < 0) {
		count = xavp_count(hname, &avp);
		do {
			if(pvh_set_xavp(
					   msg, &xavp_name, hname, NULL, SR_XTYPE_STR, indx++, 0)
					< 1)
				return -1;
		} while(indx < count);
	} else {
		if(pvh_set_xavp(msg, &xavp_name, hname, NULL, SR_XTYPE_STR, indx, 0)
				< 1)
			return -1;
	}

	return 1;
}

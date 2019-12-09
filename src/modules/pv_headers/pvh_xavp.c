/*
 *
 * PV Headers
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

#include <stdlib.h>

#include "../../core/hashes.h"
#include "../../core/route_struct.h"

#include "pvh_xavp.h"
#include "pvh_str.h"
#include "pvh_hash.h"

sr_xavp_t *pvh_xavp_new_value(str *name, sr_xval_t *val)
{
	sr_xavp_t *avp = NULL;
	int size;
	unsigned int id;

	if(name == NULL || name->s == NULL || val == NULL)
		return NULL;
	id = get_hash1_raw(name->s, name->len);

	size = sizeof(sr_xavp_t) + name->len + 1;
	if(val->type == SR_XTYPE_STR)
		size += val->v.s.len + 1;
	avp = (sr_xavp_t *)shm_malloc(size);
	if(avp == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(avp, 0, size);
	avp->id = id;
	avp->name.s = (char *)avp + sizeof(sr_xavp_t);
	memcpy(avp->name.s, name->s, name->len);
	avp->name.s[name->len] = '\0';
	avp->name.len = name->len;
	memcpy(&avp->val, val, sizeof(sr_xval_t));
	if(val->type == SR_XTYPE_STR) {
		avp->val.v.s.s = avp->name.s + avp->name.len + 1;
		memcpy(avp->val.v.s.s, val->v.s.s, val->v.s.len);
		avp->val.v.s.s[val->v.s.len] = '\0';
		avp->val.v.s.len = val->v.s.len;
	}

	return avp;
}

int pvh_xavp_append_value(str *name, sr_xval_t *val, sr_xavp_t **start)
{
	sr_xavp_t *last = NULL;
	sr_xavp_t *xavp = NULL;

	if((xavp = pvh_xavp_new_value(name, val)) == NULL)
		return -1;

	if(*start == NULL) {
		xavp->next = *start;
		*start = xavp;
		return 1;
	}

	last = *start;
	while(last->next)
		last = last->next;
	last->next = xavp;

	return 1;
}

int pvh_xavp_set_value(str *name, sr_xval_t *val, int idx, sr_xavp_t **start)
{
	int cnt = 0;

	if(idx < 0) {
		cnt = xavp_count(name, start);
		idx = idx + cnt;
		if(idx < 0)
			return -1;
	}
	LM_DBG("xavp name: %.*s\n", name->len, name->s);
	if(xavp_set_value(name, idx, val, start) == NULL)
		return -1;

	return 1;
}

sr_xval_t *pvh_xavp_get_value(
		struct sip_msg *msg, str *xname, str *name, int idx)
{
	sr_xavp_t *xavp = NULL;
	sr_xavp_t *sub = NULL;
	str br_xname = STR_NULL;

	if(pvh_str_new(&br_xname, header_name_size) < 0)
		return NULL;

	pvh_get_branch_xname(msg, xname, &br_xname);
	if((xavp = xavp_get(&br_xname, NULL)) == NULL
			&& (xavp = xavp_get(xname, NULL)) == NULL) {
		goto err;
	}

	if(xavp->val.type != SR_XTYPE_XAVP) {
		LM_ERR("not xavp child type %s\n", br_xname.s);
		goto err;
	}

	sub = xavp_get_by_index(name, idx, &xavp->val.v.xavp);

	pvh_str_free(&br_xname);
	return sub ? &sub->val : NULL;

err:
	pvh_str_free(&br_xname);
	return NULL;
}

sr_xavp_t *pvh_xavp_get_child(struct sip_msg *msg, str *xname, str *name)
{
	sr_xavp_t *xavp = NULL;
	str br_xname = STR_NULL;

	if(pvh_str_new(&br_xname, header_name_size) < 0)
		return NULL;

	pvh_get_branch_xname(msg, xname, &br_xname);
	xavp = xavp_get_child(&br_xname, name);
	if(xavp == NULL)
		xavp = xavp_get_child(xname, name);

	pvh_str_free(&br_xname);
	return xavp;
}

int pvh_xavp_is_null(sr_xavp_t *avp)
{
	if(avp == NULL)
		return 1;

	if(avp->val.type == SR_XTYPE_NULL
			|| (avp->val.type == SR_XTYPE_STR
					   && (strncasecmp(avp->val.v.s.s, "NULL", 4) == 0))) {
		return 1;
	}

	return 0;
}

void pvh_xavp_free_data(void *p, sr_xavp_sfree_f sfree)
{
	xavp_c_data_t *c_data = NULL;

	if((c_data = (xavp_c_data_t *)p) != NULL) {
		pvh_free_to_params(c_data->to_params, sfree);
		sfree(c_data->value.s);
		c_data->value.s = NULL;
		sfree(c_data);
		c_data = NULL;
	}
}

int pvh_xavp_keys_count(sr_xavp_t **start)
{
	sr_xavp_t *xavp = NULL;
	int cnt = 0;

	if(*start == NULL)
		return 0;

	xavp = *start;

	while(xavp) {
		cnt++;
		xavp = xavp->next;
	}

	return cnt;
}

void pvh_free_to_params(struct to_param *param, sr_xavp_sfree_f sfree)
{
	struct to_param *n = NULL;

	while(param) {
		n = param->next;
		sfree(param);
		param = n;
	}
	param = NULL;
}

int pvh_parse_header_name(pv_spec_p sp, str *hname)
{
	pv_spec_p psp = NULL;

	if(hname->s == NULL || hname->len == 0) {
		LM_ERR("empty header name\n");
		return -1;
	}

	if(hname->len >= header_name_size) {
		LM_ERR("header name is too long\n");
		return -1;
	}

	if(*hname->s == PV_MARKER) {
		psp = (pv_spec_p)pkg_malloc(sizeof(pv_spec_t));
		if(psp == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		if(pv_parse_spec(hname, psp) == NULL) {
			LM_ERR("invalid avp name [%.*s]\n", hname->len, hname->s);
			pv_spec_free(psp);
			return -1;
		}
		sp->pvp.pvn.type = PV_NAME_PVAR;
		sp->pvp.pvn.u.dname = (void *)psp;
		sp->pvp.pvn.u.isname.name.s = *hname;
		return 0;
	}

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *hname;

	return 0;
}

int pvh_set_xavp(struct sip_msg *msg, str *xname, str *name, void *data,
		sr_xtype_t type, int idx, int append)
{
	sr_xavp_t **xavp = NULL;
	sr_xavp_t *root = NULL;
	sr_xval_t root_xval;
	sr_xval_t xval;
	str br_xname = STR_NULL;
	int br_idx;

	if(xname == NULL || name == NULL) {
		LM_ERR("missing xavp/pv name\n");
		return -1;
	}

	pvh_get_branch_index(msg, &br_idx);
	if(pvh_str_new(&br_xname, header_name_size) < 0)
		return -1;
	pvh_get_branch_xname(msg, xname, &br_xname);
	LM_DBG("br_xname: %.*s name: %.*s\n", br_xname.len, br_xname.s, name->len,
			name->s);
	memset(&xval, 0, sizeof(sr_xval_t));
	if(data == NULL || SR_XTYPE_NULL) {
		xval.type = SR_XTYPE_NULL;
	} else if(type == SR_XTYPE_STR) {
		xval.type = SR_XTYPE_STR;
		xval.v.s = *(str *)data;
	} else if(type == SR_XTYPE_DATA) {
		xval.type = SR_XTYPE_DATA;
		xval.v.data = (sr_data_t *)shm_malloc(sizeof(sr_data_t));
		if(xval.v.data == NULL) {
			SHM_MEM_ERROR;
			goto err;
		}
		memset(xval.v.data, 0, sizeof(sr_data_t));
		xval.v.data->p = data;
		xval.v.data->pfree = pvh_xavp_free_data;
	}

	root = xavp_get(&br_xname, NULL);

	if(root == NULL && br_idx > 0) {
		pvh_clone_branch_xavp(msg, xname);
		root = xavp_get(&br_xname, NULL);
	}

	xavp = root ? &root->val.v.xavp : &root;

	if(root == NULL) {
		append = 1;
		memset(&root_xval, 0, sizeof(sr_xval_t));
		root_xval.type = SR_XTYPE_XAVP;
		root_xval.v.xavp = NULL;

		if((root = xavp_add_value(&br_xname, &root_xval, NULL)) == NULL) {
			LM_ERR("error create xavp %s\n", br_xname.s);
			goto err;
		}
		xavp = &root->val.v.xavp;
	} else if(xavp_get_child(&br_xname, name) == NULL) {
		append = 1;
	}

	if(append) {
		if(pvh_xavp_append_value(name, &xval, xavp) < 0) {
			LM_ERR("error append xavp=>name %s=>%.*s\n", br_xname.s, name->len,
					name->s);
			goto err;
		}
	} else {
		if(pvh_xavp_set_value(name, &xval, idx, xavp) < 0) {
			LM_ERR("error modify xavp=>name %s=>%.*s idx=%d\n", br_xname.s,
					name->len, name->s, idx);
			goto err;
		}
	}

	pvh_str_free(&br_xname);
	return 1;

err:
	pvh_str_free(&br_xname);
	return -1;
}

int pvh_free_xavp(str *xname)
{
	sr_xavp_t *xavp = NULL;
	xavp_rm_by_name(xname, 1, NULL);
	if((xavp = xavp_get(xname, NULL)) != NULL)
		xavp_rm(xavp, NULL);
	return 1;
}

int pvh_get_branch_index(struct sip_msg *msg, int *br_idx)
{
	int os = 0;
	int len = 0;
	char parsed_br_idx[header_value_size];

	if(msg->add_to_branch_len > header_value_size) {
		LM_ERR("branch name is too long\n");
		return -1;
	}

	os = msg->add_to_branch_len;
	while(os > 0 && memcmp(msg->add_to_branch_s + os - 1, ".", 1))
		os--;
	len = msg->add_to_branch_len - os;
	if(os > 0 && len > 0) {
		memcpy(parsed_br_idx, msg->add_to_branch_s + os, len);
		parsed_br_idx[len] = '\0';
		*br_idx = atoi(parsed_br_idx) + 1;
	} else {
		*br_idx = 0;
	}

	return 1;
}

int pvh_get_branch_xname(struct sip_msg *msg, str *xname, str *dst)
{
	int br_idx;
	int os = 0;
	char br_idx_s[32];
	char br_idx_len = 0;

	if(dst == NULL)
		return -1;

	memset(dst->s, 0, dst->len);
	memcpy(dst->s, xname->s, xname->len);
	os += xname->len;

	pvh_get_branch_index(msg, &br_idx);
	if(br_idx > 0) {
		snprintf(br_idx_s, 32, "%d", br_idx - 1);
		br_idx_len = strlen(br_idx_s);
		memcpy(dst->s + os, ".", 1);
		os += 1;
		memcpy(dst->s + os, br_idx_s, br_idx_len);
		os += br_idx_len;
	}
	if(msg->first_line.type == SIP_REPLY) {
		memcpy(dst->s + os, ".r", 2);
		os += 2;
	}
	dst->len = os;
	dst->s[dst->len] = '\0';

	return 1;
}

int pvh_clone_branch_xavp(struct sip_msg *msg, str *xname)
{
	sr_xavp_t *xavp = NULL;
	sr_xavp_t *br_xavp = NULL;
	sr_xavp_t *sub = NULL;
	sr_xval_t root_xval;
	str br_xname = STR_NULL;

	if((xavp = xavp_get(xname, NULL)) == NULL) {
		LM_ERR("cannot clone xavp from non existing %s\n", xname->s);
		return -1;
	}

	if(xavp->val.type != SR_XTYPE_XAVP) {
		LM_ERR("not xavp child type %s\n", xavp_name.s);
		return -1;
	}

	if((sub = xavp->val.v.xavp) == NULL) {
		LM_ERR("invalid xavp structure: %s\n", xavp_name.s);
		return -1;
	}

	if(pvh_str_new(&br_xname, header_name_size) < 0)
		return -1;
	pvh_get_branch_xname(msg, xname, &br_xname);

	memset(&root_xval, 0, sizeof(sr_xval_t));
	root_xval.type = SR_XTYPE_XAVP;
	root_xval.v.xavp = NULL;

	if((br_xavp = xavp_add_value(&br_xname, &root_xval, NULL)) == NULL) {
		LM_ERR("error create xavp %s\n", br_xname.s);
		goto err;
	}

	if(strncmp(xname->s, xavp_parsed_xname.s, xname->len) == 0) {
		pvh_str_free(&br_xname);
		return 1;
	}

	do {
		if(pvh_skip_header(&sub->name))
			continue;
		if(sub->val.type == SR_XTYPE_DATA)
			continue;
		if(pvh_xavp_append_value(&sub->name, &sub->val, &br_xavp->val.v.xavp)
				< 0) {
			LM_ERR("cannot clone xavp %s\n", sub->name.s);
			goto err;
		}
	} while((sub = sub->next) != NULL);

	pvh_str_free(&br_xname);
	return 1;

err:
	pvh_str_free(&br_xname);
	return -1;
}

int pvh_get_header(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	sr_xavp_t *xavp = NULL;
	sr_xval_t *xval = NULL;
	pv_value_t tv;
	str hname = STR_NULL;
	int idx = 0;
	int cnt = 0;

	idx = param->pvi.u.ival;

	if(param->pvn.type == PV_NAME_PVAR) {
		if(pv_get_spec_value(msg, (pv_spec_p)(param->pvn.u.dname), &tv) != 0) {
			LM_ERR("cannot get avp value\n");
			return -1;
		}
		if(!(tv.flags & PV_VAL_STR)) {
			return pv_get_null(msg, param, res);
		}
		hname = tv.rs;
	} else if(param->pvn.u.isname.type == AVP_NAME_STR) {
		hname = param->pvn.u.isname.name.s;
	} else {
		return pv_get_null(msg, param, res);
	}

	if(idx < 0) {
		if((xavp = pvh_xavp_get_child(msg, &xavp_name, &hname)) == NULL)
			cnt = 0;
		else
			cnt = xavp_count(&hname, &xavp);
		idx = idx + cnt;
		if(idx < 0)
			pv_get_null(msg, param, res);
	}

	xval = pvh_xavp_get_value(msg, &xavp_name, &hname, idx);

	if(xval == NULL || !xval->v.s.s)
		return pv_get_null(msg, param, res);

	return pv_get_strval(msg, param, res, &xval->v.s);
}

int pvh_set_header(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val)
{
	sr_xavp_t *xavp = NULL;
	pv_elem_p pv_format = NULL;
	pv_value_t tv;
	str hname = STR_NULL;
	str orig_hname = STR_NULL;
	str fval;
	int idx = 0;
	int cnt = 0;
	int itype;

	idx = param->pvi.u.ival;
	itype = param->pvi.type;

	if(param->pvn.type == PV_NAME_PVAR) {
		if(pv_get_spec_value(msg, (pv_spec_p)(param->pvn.u.dname), &tv) != 0) {
			LM_ERR("cannot get avp value\n");
			return -1;
		}
		if(!(tv.flags & PV_VAL_STR)) {
			LM_ERR("invalid avp value, must be a string\n");
			return -1;
		}
		hname = tv.rs;
		orig_hname = param->pvn.u.isname.name.s;
	} else if(param->pvn.u.isname.type == AVP_NAME_STR) {
		hname = param->pvn.u.isname.name.s;
		orig_hname = hname;
	} else {
		LM_ERR("invalid header name, must be a string\n");
		return -1;
	}

	if((xavp = pvh_xavp_get_child(msg, &xavp_name, &hname)) == NULL)
		idx = 0;
	else if(idx < 0)
		idx = idx + xavp_count(&hname, &xavp);

	if(val == NULL || (val->flags & PV_VAL_NULL)) {
		if(itype == PV_IDX_ALL) {
			for(idx = xavp_count(&hname, &xavp) - 1; idx >= 0; idx--) {
				if(pvh_set_xavp(
						   msg, &xavp_name, &hname, NULL, SR_XTYPE_STR, idx, 0)
						< 0)
					goto err;
			}
		} else {
			if(pvh_set_xavp(msg, &xavp_name, &hname, NULL, SR_XTYPE_STR, idx, 0)
					< 0)
				goto err;
		}
	} else if(val->flags & (PV_VAL_STR | PV_TYPE_INT | PV_VAL_INT)) {
		if(val->flags & (PV_TYPE_INT | PV_VAL_INT)) {
			if(pv_get_sintval(msg, param, val, val->ri) < 0)
				goto err;
		}
		if(pv_parse_format(&val->rs, &pv_format) < 0) {
			LM_ERR("cannot parse format: %.*s\n", val->rs.len, val->rs.s);
			goto err;
		}

		if(pv_printf_s(msg, pv_format, &fval) < 0) {
			LM_ERR("cannot parse format: %.*s\n", val->rs.len, val->rs.s);
			goto err;
		}
		if(strlen(orig_hname.s) > 1
				&& strcmp(orig_hname.s + strlen(orig_hname.s) - 2, "])") != 0) {
			if(pvh_set_xavp(msg, &xavp_name, &hname, &fval, SR_XTYPE_STR, 0, 1)
					< 0)
				goto err;
		} else if(itype == PV_IDX_ALL) {
			idx = 0;
			cnt = xavp_count(&hname, &xavp);
			while(idx < cnt) {
				if(pvh_set_xavp(msg, &xavp_name, &hname, NULL, SR_XTYPE_STR,
						   idx++, 0)
						< 1)
					goto err;
			}
			if(pvh_set_xavp(msg, &xavp_name, &hname, &fval, SR_XTYPE_STR, 0,
					   cnt ? 0 : 1)
					< 0)
				goto err;
		} else {
			if(pvh_set_xavp(
					   msg, &xavp_name, &hname, &fval, SR_XTYPE_STR, idx, 0)
					< 0)
				goto err;
		}
		if(pv_format)
			pv_elem_free_all(pv_format);
	} else {
		LM_ERR("x_hdr %.*s value can be either string, integer or null\n",
				hname.len, hname.s);
		goto err;
	}
	return 1;

err:
	if(pv_format)
		pv_elem_free_all(pv_format);
	return -1;
}

int pvh_get_uri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	sr_xval_t *xval = NULL;
	sr_xval_t *xval_pd = NULL;
	xavp_c_data_t *c_data = NULL;
	int p_no = 0;
	str sval = STR_NULL;
	int ival = 0;
	int is_strint = 0;
	str hname = STR_NULL;

	p_no = param->pvn.u.isname.name.n;
	if(pvh_str_new(&hname, header_name_size) < 0)
		goto err;

	if(p_no >= 1 && p_no <= 5)
		pvh_str_copy(&hname, &_hdr_from, header_name_size);
	else if(p_no >= 6 && p_no <= 10)
		pvh_str_copy(&hname, &_hdr_to, header_name_size);

	xval = pvh_xavp_get_value(msg, &xavp_name, &hname, 0);
	if(xval == NULL || !xval->v.s.s)
		goto err;

	xval_pd = pvh_xavp_get_value(msg, &xavp_parsed_xname, &hname, 0);

	if(xval_pd)
		c_data = (xavp_c_data_t *)xval_pd->v.data->p;

	if(c_data != NULL
			&& strncmp(xval->v.s.s, c_data->value.s, c_data->value.len) != 0) {
		c_data = NULL;
	}

	if(c_data == NULL) {
		c_data = (xavp_c_data_t *)shm_malloc(sizeof(xavp_c_data_t));
		if(c_data == NULL) {
			SHM_MEM_ERROR;
			goto err;
		}
		memset(c_data, 0, sizeof(xavp_c_data_t));
		if(pvh_merge_uri(msg, SET_URI_T, &xval->v.s, &xval->v.s, c_data) < 0)
			goto err;
		if(pvh_set_xavp(
				   msg, &xavp_parsed_xname, &hname, c_data, SR_XTYPE_DATA, 0, 0)
				< 0)
			goto err;
	}

	switch(p_no) {
		case 1: // full from
		case 6: // full to
			sval = c_data->to_b.uri;
			break;
		case 2: // username from
		case 7: // username to
			sval = c_data->to_b.parsed_uri.user;
			break;
		case 3: // domain from
		case 8: // domain to
			sval = c_data->to_b.parsed_uri.host;
			break;
		case 4: // displayname from
		case 9: // displayname to
			sval = c_data->to_b.display;
			break;
		case 5:  // from tag
		case 10: // to tag
			sval = c_data->to_b.tag_value;
			break;
		default:
			LM_ERR("unknown get uri op\n");
	}

	pvh_str_free(&hname);
	return sval.s ? is_strint ? pv_get_strintval(msg, param, res, &sval, ival)
							  : pv_get_strval(msg, param, res, &sval)
				  : pv_get_null(msg, param, res);

err:
	pvh_str_free(&hname);
	return pv_get_null(msg, param, res);
}

int pvh_set_uri(struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val)
{
	sr_xval_t *xval = NULL;
	xavp_c_data_t *c_data = NULL;
	pv_elem_p pv_format = NULL;
	int p_no = 0;
	enum action_type a_type;
	str hname;
	str fval;

	p_no = param->pvn.u.isname.name.n;
	if(pvh_str_new(&hname, header_name_size) < 0)
		goto err;
	if(p_no >= 1 && p_no <= 5)
		pvh_str_copy(&hname, &_hdr_from, header_name_size);
	else if(p_no >= 6 && p_no <= 10)
		pvh_str_copy(&hname, &_hdr_to, header_name_size);

	switch(p_no) {
		case 1: // uri from
		case 6: // uri to
			a_type = SET_URI_T;
			break;
		case 2: // username from
		case 7: // username to
			a_type = SET_USER_T;
			break;
		case 3: // domain from
		case 8: // domain to
			a_type = SET_HOST_T;
			break;
		case 4: // displayname from
		case 9: // displayname to
			a_type = SET_USERPHONE_T;
			break;
		default:
			LM_ERR("unknown set uri op\n");
			goto err;
	}

	if(val->flags & (PV_TYPE_INT | PV_VAL_INT)) {
		if(pv_get_sintval(msg, param, val, val->ri) < 0)
			goto err;
	}

	if(pv_parse_format(&val->rs, &pv_format) < 0) {
		LM_ERR("cannot parse format: %.*s\n", val->rs.len, val->rs.s);
		goto err;
	}

	if(pv_printf_s(msg, pv_format, &fval) < 0) {
		LM_ERR("cannot parse format: %.*s\n", val->rs.len, val->rs.s);
		goto err;
	}

	xval = pvh_xavp_get_value(msg, &xavp_name, &hname, 0);
	if(xval == NULL || !xval->v.s.s)
		goto err;

	c_data = (xavp_c_data_t *)shm_malloc(sizeof(xavp_c_data_t));
	if(c_data == NULL) {
		SHM_MEM_ERROR;
		goto err;
	}
	memset(c_data, 0, sizeof(xavp_c_data_t));
	if(pvh_merge_uri(msg, a_type, &xval->v.s, &fval, c_data) < 0)
		goto err;

	if(pvh_set_xavp(msg, &xavp_name, &hname, &c_data->value, SR_XTYPE_STR, 0, 0)
			< 0)
		goto err;

	if(pvh_set_xavp(
			   msg, &xavp_parsed_xname, &hname, c_data, SR_XTYPE_DATA, 0, 0)
			< 0)
		goto err;

	pvh_str_free(&hname);
	if(pv_format)
		pv_elem_free_all(pv_format);
	return 1;

err:
	pvh_str_free(&hname);
	if(pv_format)
		pv_elem_free_all(pv_format);
	return -1;
}

int pvh_merge_uri(struct sip_msg *msg, enum action_type type, str *cur,
		str *new, xavp_c_data_t *c_data)
{
	struct sip_uri puri;
	struct to_body tb;
	struct to_param *param = NULL;
	struct to_param *sparam_start = NULL;
	struct to_param **sparam = NULL;
	str *merged = NULL;
	char *c_ptr = NULL;
	str uri_t;
	int os = 0;
	int t_len = 0;

	parse_addr_spec(cur->s, cur->s + cur->len, &tb, 0);
	if(!tb.uri.s) {
		LM_ERR("cannot parse addr spec\n");
		goto err;
	}

	if(parse_uri(tb.uri.s, tb.uri.len, &tb.parsed_uri) < 0) {
		LM_ERR("cannot parse uri %.*s\n", tb.uri.len, tb.uri.s);
		goto err;
	}
	puri = tb.parsed_uri;

	c_data->value.s = (char *)shm_malloc(header_value_size);
	if(c_data->value.s == NULL) {
		SHM_MEM_ERROR;
		goto err;
	}
	merged = &c_data->value;

	if(type == SET_URI_T && strchr(new->s, '<')) {
		pvh_str_copy(merged, new, header_value_size);
		goto reparse;
	}

	os = 0;
	if(type == SET_USERPHONE_T) {
		memcpy(merged->s + os, new->s, new->len);
		os += new->len;
		memcpy(merged->s + os, " ", 1);
		os += 1;
	} else if(tb.display.len > 0) {
		memcpy(merged->s + os, tb.display.s, tb.display.len);
		os += tb.display.len;
		memcpy(merged->s + os, " ", 1);
		os += 1;
	}
	memcpy(merged->s + os, "<", 1);
	os += 1;
	if(type != SET_URI_T) {
		uri_type_to_str(puri.type, &uri_t);
		t_len = uri_t.len + 1;
		memcpy(merged->s + os, uri_t.s, uri_t.len);
		os += uri_t.len;
		memcpy(merged->s + os, ":", 1);
		os += 1;
	}
	switch(type) {
		case SET_USERPHONE_T:
			memcpy(merged->s + os, tb.uri.s + t_len, tb.uri.len - t_len);
			os += tb.uri.len - t_len;
			break;
		case SET_URI_T:
			memcpy(merged->s + os, new->s, new->len);
			os += new->len;
			break;
		case SET_USER_T:
			memcpy(merged->s + os, new->s, new->len);
			os += new->len;
			memcpy(merged->s + os, tb.uri.s + t_len + puri.user.len,
					tb.uri.len - t_len - puri.user.len);
			os += tb.uri.len - t_len - puri.user.len;
			break;
		case SET_HOST_T:
			if((c_ptr = strchr(tb.uri.s, '@')) == NULL) {
				LM_ERR("invalid uri: %.*s\n", tb.uri.len, tb.uri.s);
				goto err;
			}
			memcpy(merged->s + os, tb.uri.s + t_len,
					c_ptr - tb.uri.s - t_len + 1);
			os += c_ptr - tb.uri.s - t_len + 1;
			memcpy(merged->s + os, new->s, new->len);
			os += new->len;
			memcpy(merged->s + os, c_ptr + puri.host.len + 1,
					tb.uri.s + tb.uri.len - c_ptr - puri.host.len - 1);
			os += tb.uri.s + tb.uri.len - c_ptr - puri.host.len - 1;
			break;
		default:
			LM_ERR("unknown set uri op\n");
			goto err;
	}
	memcpy(merged->s + os, ">", 1);
	os += 1;
	if((param = tb.param_lst) != NULL) {
		while(param) {
			memcpy(merged->s + os, ";", 1);
			os += 1;
			memcpy(merged->s + os, param->name.s, param->name.len);
			os += param->name.len;
			memcpy(merged->s + os, "=", 1);
			os += 1;
			memcpy(merged->s + os, param->value.s, param->value.len);
			os += param->value.len;
			param = param->next;
		}
	}
	merged->len = os;
	merged->s[merged->len] = '\0';

reparse:

	parse_addr_spec(merged->s, merged->s + merged->len, &c_data->to_b, 0);
	if(!c_data->to_b.uri.s) {
		LM_ERR("cannot parse addr spec\n");
		goto err;
	}

	if((param = tb.param_lst) != NULL) {
		while(param) {
			if(sparam == NULL)
				sparam = &sparam_start;
			*sparam = (struct to_param *)shm_malloc(sizeof(struct to_param));
			if(*sparam == NULL) {
				SHM_MEM_ERROR;
				goto err;
			}
			memset(*sparam, 0, sizeof(struct to_param));
			memcpy(*sparam, param, sizeof(struct to_param));
			(*sparam)->next = NULL;
			sparam = &(*sparam)->next;
			param = param->next;
		}
		c_data->to_params = sparam_start;
	}

	if(parse_uri(c_data->to_b.uri.s, c_data->to_b.uri.len,
			   &c_data->to_b.parsed_uri)
			< 0) {
		LM_ERR("cannot parse uri %.*s\n", c_data->to_b.uri.len,
				c_data->to_b.uri.s);
		goto err;
	}

	free_to_params(&tb);
	free_to_params(&c_data->to_b);
	return 1;

err:
	free_to_params(&tb);
	free_to_params(&c_data->to_b);
	return -1;
}

int pvh_get_reply_sr(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	sr_xval_t *xval = NULL;
	int p_no = 0;
	str rhname = {"@Reply-Reason", 13};

	p_no = param->pvn.u.isname.name.n;

	if(msg->first_line.type != SIP_REPLY)
		return pv_get_null(msg, param, res);

	switch(p_no) {
		case 1: // status
			return pv_get_intstrval(msg, param, res,
					(int)msg->first_line.u.reply.statuscode,
					&msg->first_line.u.reply.status);
			break;
		case 2: // reason
			xval = pvh_xavp_get_value(msg, &xavp_name, &rhname, 0);
			return pv_get_strval(msg, param, res,
					xval && xval->v.s.s ? &xval->v.s
										: &msg->first_line.u.reply.reason);
			break;
		default:
			LM_ERR("unknown get reply op\n");
	}

	return pv_get_null(msg, param, res);
}

int pvh_set_reply_sr(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val)
{
	pv_elem_p pv_format = NULL;
	int p_no = 0;
	unsigned int code = 0;
	str rhname = {"@Reply-Reason", 13};
	str fval;

	p_no = param->pvn.u.isname.name.n;

	if(msg->first_line.type != SIP_REPLY) {
		LM_ERR("set reply: not a reply message\n");
		goto err;
	}

	if(val->flags & (PV_VAL_NULL)) {
		LM_ERR("set reply: value cannot be null\n");
		goto err;
	}

	if(val->flags & (PV_TYPE_INT | PV_VAL_INT)) {
		if(pv_get_sintval(msg, param, val, val->ri) < 0)
			goto err;
	}

	if(pv_parse_format(&val->rs, &pv_format) < 0) {
		LM_ERR("cannot parse format: %.*s\n", val->rs.len, val->rs.s);
		goto err;
	}

	if(pv_printf_s(msg, pv_format, &fval) < 0) {
		LM_ERR("cannot parse format: %.*s\n", val->rs.len, val->rs.s);
		goto err;
	}

	switch(p_no) {
		case 1: // status
			code = atoi(fval.s);
			if(code < 100 || code > 699) {
				LM_ERR("set reply: wrong status code: %d\n", code);
				goto err;
			}
			if((code < 300 || msg->REPLY_STATUS < 300)
					&& (code / 100 != msg->REPLY_STATUS / 100)) {
				LM_ERR("set reply: 1xx or 2xx replies cannot be changed or set "
					   "to\n");
				goto err;
			}
			msg->first_line.u.reply.statuscode = code;
			msg->first_line.u.reply.status.s[2] = code % 10 + '0';
			code /= 10;
			msg->first_line.u.reply.status.s[1] = code % 10 + '0';
			code /= 10;
			msg->first_line.u.reply.status.s[0] = code + '0';
			break;
		case 2: // reason
			if(pvh_set_xavp(msg, &xavp_name, &rhname, &fval, SR_XTYPE_STR, 0, 0)
					< 0) {
				LM_ERR("set reply: cannot set reply reason\n");
				goto err;
			}
			break;
		default:
			LM_ERR("unknown set reply op\n");
			goto err;
	}

	if(pv_format)
		pv_elem_free_all(pv_format);
	return 1;

err:
	if(pv_format)
		pv_elem_free_all(pv_format);
	return -1;
}

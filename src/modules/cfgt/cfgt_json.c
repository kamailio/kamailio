/**
 *
 * Copyright (C) 2013-2015 Victor Seva (sipwise.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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
#include <stdio.h>

#include "../../core/pvar.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/xavp.h"
#include "../pv/pv_xavp.h"

#include "cfgt_json.h"

int _cfgt_get_array_avp_vals(struct sip_msg *msg, pv_param_t *param,
		srjson_doc_t *jdoc, srjson_t **jobj, str *item_name)
{
	struct usr_avp *avp;
	unsigned short name_type;
	int_str avp_name;
	int_str avp_value;
	struct search_state state;
	srjson_t *jobjt;
	memset(&state, 0, sizeof(struct search_state));

	if(pv_get_avp_name(msg, param, &avp_name, &name_type) != 0) {
		LM_ERR("invalid name\n");
		return -1;
	}
	if(name_type == 0 && avp_name.n == 0) {
		LM_DBG("skip name_type:%d avp_name:%ld\n", name_type, avp_name.n);
		return 0;
	}
	*jobj = srjson_CreateArray(jdoc);
	if(*jobj == NULL) {
		LM_ERR("cannot create json object\n");
		return -1;
	}
	if((avp = search_first_avp(name_type, avp_name, &avp_value, &state)) == 0) {
		goto ok;
	}
	do {
		if(avp->flags & AVP_VAL_STR) {
			jobjt = srjson_CreateStr(jdoc, avp_value.s.s, avp_value.s.len);
			if(jobjt == NULL) {
				LM_ERR("cannot create json object\n");
				srjson_Delete(jdoc, *jobj);
				*jobj = NULL;
				return -1;
			}
		} else {
			jobjt = srjson_CreateNumber(jdoc, avp_value.n);
			if(jobjt == NULL) {
				LM_ERR("cannot create json object\n");
				srjson_Delete(jdoc, *jobj);
				*jobj = NULL;
				return -1;
			}
		}
		srjson_AddItemToArray(jdoc, *jobj, jobjt);
	} while((avp = search_next_avp(&state, &avp_value)) != 0);
ok:
	item_name->s = avp_name.s.s;
	item_name->len = avp_name.s.len;
	return 0;
}
#define CFGT_XAVP_DUMP_SIZE 32
static str *_cfgt_xavp_dump[CFGT_XAVP_DUMP_SIZE];
int _cfgt_xavp_dump_lookup(pv_param_t *param)
{
	unsigned int i = 0;
	pv_xavp_name_t *xname;

	if(param == NULL)
		return -1;

	xname = (pv_xavp_name_t *)param->pvn.u.dname;

	while(i < CFGT_XAVP_DUMP_SIZE && _cfgt_xavp_dump[i] != NULL) {
		if(_cfgt_xavp_dump[i]->len == xname->name.len) {
			if(strncmp(_cfgt_xavp_dump[i]->s, xname->name.s, xname->name.len)
					== 0)
				return 1; /* already dump before */
		}
		i++;
	}
	if(i == CFGT_XAVP_DUMP_SIZE) {
		LM_WARN("full _cfgt_xavp_dump cache array\n");
		return 0; /* end cache names */
	}
	_cfgt_xavp_dump[i] = &xname->name;
	return 0;
}

void _cfgt_get_obj_xavp_val(sr_xavp_t *avp, srjson_doc_t *jdoc, srjson_t **jobj)
{
	static char _pv_xavp_buf[128];
	int result = 0;

	switch(avp->val.type) {
		case SR_XTYPE_NULL:
			*jobj = srjson_CreateNull(jdoc);
			if(*jobj == NULL) {
				LM_ERR("cannot create json object\n");
				return;
			}
			break;
		case SR_XTYPE_LONG:
			*jobj = srjson_CreateNumber(jdoc, avp->val.v.l);
			if(*jobj == NULL) {
				LM_ERR("cannot create json object\n");
				return;
			}
			break;
		case SR_XTYPE_STR:
			*jobj = srjson_CreateStr(jdoc, avp->val.v.s.s, avp->val.v.s.len);
			if(*jobj == NULL) {
				LM_ERR("cannot create json object\n");
				return;
			}
			break;
		case SR_XTYPE_TIME:
			result = snprintf(
					_pv_xavp_buf, 128, "%lu", (long unsigned)avp->val.v.t);
			break;
		case SR_XTYPE_LLONG:
			result = snprintf(_pv_xavp_buf, 128, "%lld", avp->val.v.ll);
			break;
		case SR_XTYPE_XAVP:
			result =
					snprintf(_pv_xavp_buf, 128, "<<xavp:%p>>", avp->val.v.xavp);
			break;
		case SR_XTYPE_DATA:
			result =
					snprintf(_pv_xavp_buf, 128, "<<data:%p>>", avp->val.v.data);
			break;
		default:
			LM_WARN("unknown data type\n");
			*jobj = srjson_CreateNull(jdoc);
			if(*jobj == NULL) {
				LM_ERR("cannot create json object\n");
				return;
			}
	}
	if(result < 0) {
		LM_ERR("cannot convert to str\n");
		*jobj = srjson_CreateNull(jdoc);
		if(*jobj == NULL) {
			LM_ERR("cannot create json object\n");
			return;
		}
	} else if(*jobj == NULL) {
		*jobj = srjson_CreateStr(jdoc, _pv_xavp_buf, 128);
		if(*jobj == NULL) {
			LM_ERR("cannot create json object\n");
			return;
		}
	}
}

int _cfgt_get_obj_avp_vals(
		str name, sr_xavp_t *xavp, srjson_doc_t *jdoc, srjson_t **jobj)
{
	sr_xavp_t *avp = NULL;
	srjson_t *jobjt = NULL;

	*jobj = srjson_CreateArray(jdoc);
	if(*jobj == NULL) {
		LM_ERR("cannot create json object\n");
		return -1;
	}
	avp = xavp;
	while(avp != NULL && !STR_EQ(avp->name, name)) {
		avp = avp->next;
	}
	while(avp != NULL) {
		_cfgt_get_obj_xavp_val(avp, jdoc, &jobjt);
		if(jobjt == NULL) {
			return -1;
		}
		srjson_AddItemToArray(jdoc, *jobj, jobjt);
		jobjt = NULL;
		avp = xavp_get_next(avp);
	}

	return 0;
}

int _cfgt_get_obj_xavp_vals(struct sip_msg *msg, pv_param_t *param,
		srjson_doc_t *jdoc, srjson_t **jobjr, str *item_name)
{
	pv_xavp_name_t *xname = (pv_xavp_name_t *)param->pvn.u.dname;
	sr_xavp_t *xavp = NULL;
	sr_xavp_t *avp = NULL;
	srjson_t *jobj = NULL;
	srjson_t *jobjt = NULL;
	struct str_list *keys;
	struct str_list *k;

	*jobjr = srjson_CreateArray(jdoc);
	if(*jobjr == NULL) {
		LM_ERR("cannot create json object\n");
		return -1;
	}

	item_name->s = xname->name.s;
	item_name->len = xname->name.len;
	xavp = xavp_get_by_index(&xname->name, 0, NULL);
	if(xavp == NULL) {
		return 0; /* empty */
	}

	do {
		if(xavp->val.type == SR_XTYPE_XAVP) {
			avp = xavp->val.v.xavp;
			jobj = srjson_CreateObject(jdoc);
			if(jobj == NULL) {
				LM_ERR("cannot create json object\n");
				srjson_Delete(jdoc, *jobjr);
				jobjr = NULL;
				return -1;
			}
			keys = xavp_get_list_key_names(xavp);
			if(keys != NULL) {
				do {
					_cfgt_get_obj_avp_vals(keys->s, avp, jdoc, &jobjt);
					if(jobjt) {
						srjson_AddStrItemToObject(
								jdoc, jobj, keys->s.s, keys->s.len, jobjt);
					}
					k = keys;
					keys = keys->next;
					pkg_free(k);
					jobjt = NULL;
				} while(keys != NULL);
			}
		}
		if(jobj != NULL) {
			srjson_AddItemToArray(jdoc, *jobjr, jobj);
			jobj = NULL;
		}
	} while((xavp = xavp_get_next(xavp)) != 0);

	return 0;
}

int cfgt_get_json(struct sip_msg *msg, unsigned int mask, srjson_doc_t *jdoc,
		srjson_t *head)
{
	int i;
	pv_value_t value;
	pv_cache_t **_pv_cache = pv_cache_get_table();
	pv_cache_t *el = NULL;
	srjson_t *jobj = NULL;
	str item_name = STR_NULL;
	static char iname[128];

	if(_pv_cache == NULL) {
		LM_ERR("cannot access pv_cache\n");
		return -1;
	}
	if(jdoc == NULL) {
		LM_ERR("jdoc is null\n");
		return -1;
	}
	if(head == NULL) {
		LM_ERR("head is null\n");
		return -1;
	}

	memset(_cfgt_xavp_dump, 0, sizeof(str *) * CFGT_XAVP_DUMP_SIZE);
	for(i = 0; i < PV_CACHE_SIZE; i++) {
		el = _pv_cache[i];
		while(el) {
			if(jobj) {
				srjson_Delete(jdoc, jobj);
				jobj = NULL;
			}
			if(!(el->spec.type == PVT_AVP || el->spec.type == PVT_SCRIPTVAR
					   || el->spec.type == PVT_XAVP
					   || el->spec.type == PVT_OTHER)
					|| !((el->spec.type == PVT_AVP && mask & CFGT_DP_AVP)
							   || (el->spec.type == PVT_XAVP
										  && mask & CFGT_DP_XAVP)
							   || (el->spec.type == PVT_SCRIPTVAR
										  && mask & CFGT_DP_SCRIPTVAR)
							   || (el->spec.type == PVT_OTHER
										  && mask & CFGT_DP_OTHER))
					|| (el->spec.trans != NULL)) {
				el = el->next;
				continue;
			}
			item_name.len = 0;
			item_name.s = 0;
			iname[0] = '\0';
			if(el->spec.type == PVT_AVP) {
				if(el->spec.pvp.pvi.type == PV_IDX_ALL
						|| (el->spec.pvp.pvi.type == PV_IDX_INT
								   && el->spec.pvp.pvi.u.ival != 0)) {
					el = el->next;
					continue;
				} else {
					if(_cfgt_get_array_avp_vals(
							   msg, &el->spec.pvp, jdoc, &jobj, &item_name)
							!= 0) {
						LM_WARN("can't get value[%.*s]\n", el->pvname.len,
								el->pvname.s);
						el = el->next;
						continue;
					}
					if(jobj == NULL || (srjson_GetArraySize(jdoc, jobj) == 0
							&& !(mask & CFGT_DP_NULL))) {
						el = el->next;
						continue;
					}
					snprintf(iname, 128, "$avp(%.*s)", item_name.len,
							item_name.s);
				}
			} else if(el->spec.type == PVT_XAVP) {
				if(_cfgt_xavp_dump_lookup(&el->spec.pvp) != 0) {
					el = el->next;
					continue;
				}
				if(_cfgt_get_obj_xavp_vals(
						   msg, &el->spec.pvp, jdoc, &jobj, &item_name)
						!= 0) {
					LM_WARN("can't get value[%.*s]\n", el->pvname.len,
							el->pvname.s);
					el = el->next;
					continue;
				}
				if(srjson_GetArraySize(jdoc, jobj) == 0
						&& !(mask & CFGT_DP_NULL)) {
					el = el->next;
					continue;
				}
				snprintf(iname, 128, "$xavp(%.*s)", item_name.len, item_name.s);
			} else {
				if(el->pvname.len > 3 && strncmp("$T_", el->pvname.s, 3) == 0) {
					LM_DBG("skip tm var[%.*s]\n", el->pvname.len, el->pvname.s);
					el = el->next;
					continue;
				}
				if(strchr(el->pvname.s + 1, 36) != NULL) {
					LM_DBG("skip dynamic format [%.*s]\n", el->pvname.len, el->pvname.s);
					el = el->next;
					continue;
				}
				if(pv_get_spec_value(msg, &el->spec, &value) != 0) {
					LM_WARN("can't get value[%.*s]\n", el->pvname.len,
							el->pvname.s);
					el = el->next;
					continue;
				}
				if(value.flags & (PV_VAL_NULL | PV_VAL_EMPTY | PV_VAL_NONE)) {
					if(mask & CFGT_DP_NULL) {
						jobj = srjson_CreateNull(jdoc);
					} else {
						el = el->next;
						continue;
					}
				} else if(value.flags & (PV_VAL_INT)) {
					jobj = srjson_CreateNumber(jdoc, value.ri);
					if(jobj == NULL)
						LM_ERR("cannot create json object\n");
				} else if(value.flags & (PV_VAL_STR)) {
					jobj = srjson_CreateStr(jdoc, value.rs.s, value.rs.len);
					if(jobj == NULL)
						LM_ERR("cannot create json object\n");
				} else {
					LM_WARN("el->pvname[%.*s] value[%d] unhandled\n",
							el->pvname.len, el->pvname.s, value.flags);
					el = el->next;
					continue;
				}
				if(jobj == NULL) {
					LM_ERR("el->pvname[%.*s] empty json object\n",
							el->pvname.len, el->pvname.s);
					goto error;
				}
				snprintf(iname, 128, "%.*s", el->pvname.len, el->pvname.s);
			}
			if(jobj != NULL) {
				srjson_AddItemToObject(jdoc, head, iname, jobj);
				jobj = NULL;
			}
			el = el->next;
		}
	}
	return 0;

error:
	srjson_Delete(jdoc, head);
	return -1;
}

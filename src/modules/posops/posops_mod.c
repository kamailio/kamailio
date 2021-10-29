/**
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/kemi.h"


MODULE_VERSION

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_posops_pos_append(sip_msg_t* msg, char* p1idx, char* p2val);
static int w_posops_pos_insert(sip_msg_t* msg, char* p1idx, char* p2val);
static int w_posops_pos_rm(sip_msg_t* msg, char* p1idx, char* p2len);
static int w_posops_pos_headers_start(sip_msg_t* msg, char* p1, char* p2);
static int w_posops_pos_headers_end(sip_msg_t* msg, char* p1, char* p2);
static int w_posops_pos_body_start(sip_msg_t* msg, char* p1, char* p2);
static int w_posops_pos_body_end(sip_msg_t* msg, char* p1, char* p2);
static int w_posops_pos_find_str(sip_msg_t* msg, char* p1idx, char* p2val);
static int w_posops_pos_findi_str(sip_msg_t* msg, char* p1idx, char* p2val);
static int w_posops_pos_rfind_str(sip_msg_t* msg, char* p1idx, char* p2val);
static int w_posops_pos_rfindi_str(sip_msg_t* msg, char* p1idx, char* p2val);
static int w_posops_pos_search(sip_msg_t* msg, char* p1idx, char* p2re);
static int w_posops_pos_rsearch(sip_msg_t* msg, char* p1idx, char* p2re);

typedef struct posops_data {
	int ret;
	int idx;
	int len;
} posops_data_t;

static int posops_idx0 = -255;

static int pv_posops_get_pos(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int pv_posops_parse_pos_name(pv_spec_t *sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"pos", (sizeof("pos")-1)}, PVT_OTHER, pv_posops_get_pos, 0,
		pv_posops_parse_pos_name, 0, 0, 0 },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/**
 *
 */
static posops_data_t _posops_data = {0};

/* clang-format off */
static cmd_export_t cmds[]={
	{"pos_append", (cmd_function)w_posops_pos_append, 2, fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"pos_insert", (cmd_function)w_posops_pos_insert, 2, fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"pos_rm", (cmd_function)w_posops_pos_rm, 2, fixup_igp_igp,
		fixup_free_igp_igp, ANY_ROUTE},
	{"pos_headers_start", (cmd_function)w_posops_pos_headers_start, 0, 0,
		0, ANY_ROUTE},
	{"pos_headers_end", (cmd_function)w_posops_pos_headers_end, 0, 0,
		0, ANY_ROUTE},
	{"pos_body_start", (cmd_function)w_posops_pos_body_start, 0, 0,
		0, ANY_ROUTE},
	{"pos_body_end", (cmd_function)w_posops_pos_body_end, 0, 0,
		0, ANY_ROUTE},
	{"pos_find_str", (cmd_function)w_posops_pos_find_str, 2, fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"pos_findi_str", (cmd_function)w_posops_pos_findi_str, 2, fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"pos_rfind_str", (cmd_function)w_posops_pos_rfind_str, 2, fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"pos_rfindi_str", (cmd_function)w_posops_pos_rfindi_str, 2, fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"pos_search",    (cmd_function)w_posops_pos_search, 2, fixup_igp_regexp,
		fixup_free_igp_regexp, ANY_ROUTE},
	{"pos_rsearch",    (cmd_function)w_posops_pos_rsearch, 2, fixup_igp_regexp,
		fixup_free_igp_regexp, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"idx0",     PARAM_INT,   &posops_idx0},

	{0, 0, 0}
};

struct module_exports exports = {
	"posops",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* exported RPC methods */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* per child init function */
	mod_destroy      /* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
static void posops_data_init(void)
{
	memset(&_posops_data, 0, sizeof(posops_data_t));
	_posops_data.ret = -1;
}

/**
 *
 */
static int ki_posops_pos_append(sip_msg_t *msg, int idx, str *val)
{
	int offset;
	sr_lump_t *anchor = NULL;

	posops_data_init();
	if(val==NULL || val->s==NULL || val->len<=0) {
		LM_ERR("invalid val parameter\n");
		return -1;
	}

	if(idx<0) {
		offset = msg->len + idx;
	} else {
		offset = idx;
	}
	if(offset>msg->len) {
		LM_ERR("offset invalid: %d (msg-len: %d)\n", offset, msg->len);
		return -1;
	}

	anchor = anchor_lump(msg, offset, 0, 0);
	if(anchor == NULL) {
		LM_ERR("failed to create the anchor\n");
		return -1;
	}
	if (insert_new_lump_after(anchor, val->s, val->len, 0) == 0) {
		LM_ERR("unable to add lump\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_posops_pos_append(sip_msg_t* msg, char* p1idx, char* p2val)
{
	int idx = 0;
	str val = STR_NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2val, &val)!=0) {
		LM_ERR("unable to get val parameter\n");
		return -1;
	}

	return ki_posops_pos_append(msg, idx, &val);
}

/**
 *
 */
static int ki_posops_pos_insert(sip_msg_t *msg, int idx, str *val)
{
	int offset;
	sr_lump_t *anchor = NULL;

	posops_data_init();
	if(val==NULL || val->s==NULL || val->len<=0) {
		LM_ERR("invalid val parameter\n");
		return -1;
	}

	if(idx<0) {
		offset = msg->len + idx;
	} else {
		offset = idx;
	}
	if(offset>msg->len) {
		LM_ERR("offset invalid: %d (msg-len: %d)\n", offset, msg->len);
		return -1;
	}

	anchor = anchor_lump(msg, offset, 0, 0);
	if(anchor == NULL) {
		LM_ERR("failed to create the anchor\n");
		return -1;
	}
	if (insert_new_lump_before(anchor, val->s, val->len, 0) == 0) {
		LM_ERR("unable to add lump\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_posops_pos_insert(sip_msg_t* msg, char* p1idx, char* p2val)
{
	int idx = 0;
	str val = STR_NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2val, &val)!=0) {
		LM_ERR("unable to get val parameter\n");
		return -1;
	}

	return ki_posops_pos_insert(msg, idx, &val);
}


/**
 *
 */
static int ki_posops_pos_rm(sip_msg_t *msg, int idx, int len)
{
	int offset;
	sr_lump_t *anchor = NULL;

	posops_data_init();
	if(len<=0) {
		LM_ERR("length invalid: %d (msg-len: %d)\n", len, msg->len);
		return -1;
	}
	if(idx<0) {
		offset = msg->len + idx;
	} else {
		offset = idx;
	}
	if(offset>msg->len) {
		LM_ERR("offset invalid: %d (msg-len: %d)\n", offset, msg->len);
		return -1;
	}
	if(offset==msg->len) {
		LM_WARN("offset at the end of message: %d (msg-len: %d)\n",
				offset, msg->len);
		return 1;
	}
	if(offset + len > msg->len) {
		LM_WARN("offset + len over the end of message: %d + %d (msg-len: %d)\n",
				offset, len, msg->len);
		return -1;
	}
	anchor=del_lump(msg, offset, len, 0);
	if (anchor==0) {
		LM_ERR("cannot remove - offset: %d - len: %d - msg-len: %d\n",
				offset, len, msg->len);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_posops_pos_rm(sip_msg_t* msg, char* p1idx, char* p2len)
{
	int idx = 0;
	int len = 0;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)p2len, &len)!=0) {
		LM_ERR("unable to get len parameter\n");
		return -1;
	}

	return ki_posops_pos_rm(msg, idx, len);
}

/**
 *
 */
static int ki_posops_pos_headers_start(sip_msg_t* msg)
{
	posops_data_init();
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	_posops_data.idx = msg->first_line.len;
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_headers_start(sip_msg_t* msg, char* p1, char* p2)
{
	return ki_posops_pos_headers_start(msg);
}

/**
 *
 */
static int ki_posops_pos_headers_end(sip_msg_t* msg)
{
	posops_data_init();
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	_posops_data.idx = msg->unparsed - msg->buf;
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_headers_end(sip_msg_t* msg, char* p1, char* p2)
{
	return ki_posops_pos_headers_end(msg);
}

/**
 *
 */
static int ki_posops_pos_body_start(sip_msg_t* msg)
{
	char *body = 0;

	posops_data_init();
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	body = get_body(msg);
	if(body == NULL) {
		LM_DBG("no body\n");
		return -1;
	}
	_posops_data.idx = body - msg->buf;

	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_body_start(sip_msg_t* msg, char* p1, char* p2)
{
	return ki_posops_pos_body_start(msg);
}

/**
 *
 */
static int ki_posops_pos_body_end(sip_msg_t* msg)
{
	posops_data_init();
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if(get_body(msg) == NULL) {
		LM_DBG("no body\n");
		return -1;
	}

	_posops_data.idx = msg->len;
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_body_end(sip_msg_t* msg, char* p1, char* p2)
{
	return  ki_posops_pos_body_end(msg);
}

/**
 *
 */
static int ki_posops_pos_find_str(sip_msg_t *msg, int idx, str *val)
{
	char *p;
	str text;

	posops_data_init();
	if(val==NULL || val->s==NULL || val->len<=0) {
		return -1;
	}
	if(idx<0) {
		idx += msg->len;
	}
	if(idx<0 || idx > msg->len - val->len) {
		return -1;
	}
	text.s = msg->buf + idx;
	text.len = msg->len - idx;
	p = str_search(&text, val);
	if(p==NULL) {
		return -1;
	}

	_posops_data.idx = (int)(p - msg->buf);
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_find_str(sip_msg_t* msg, char* p1idx, char* p2val)
{
	int idx = 0;
	str val = STR_NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2val, &val)!=0) {
		LM_ERR("unable to get val parameter\n");
		return -1;
	}

	return ki_posops_pos_find_str(msg, idx, &val);
}

/**
 *
 */
static int ki_posops_pos_findi_str(sip_msg_t *msg, int idx, str *val)
{
	char *p;
	str text;

	posops_data_init();
	if(val==NULL || val->s==NULL || val->len<=0) {
		return -1;
	}
	if(idx<0) {
		idx += msg->len;
	}
	if(idx<0 || idx > msg->len - val->len) {
		return -1;
	}

	text.s = msg->buf + idx;
	text.len = msg->len - idx;
	p = str_casesearch(&text, val);
	if(p==NULL) {
		return -1;
	}

	_posops_data.idx = (int)(p - msg->buf);
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_findi_str(sip_msg_t* msg, char* p1idx, char* p2val)
{
	int idx = 0;
	str val = STR_NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2val, &val)!=0) {
		LM_ERR("unable to get val parameter\n");
		return -1;
	}

	return ki_posops_pos_findi_str(msg, idx, &val);
}



/**
 *
 */
static int ki_posops_pos_rfind_str(sip_msg_t *msg, int idx, str *val)
{
	char *p;
	str text;

	posops_data_init();
	if(val==NULL || val->s==NULL || val->len<=0) {
		return -1;
	}
	if(idx<0) {
		idx += msg->len;
	}
	if(idx<0 || idx > msg->len - val->len) {
		return -1;
	}
	text.s = msg->buf + idx;
	text.len = msg->len - idx;
	p = str_rsearch(&text, val);
	if(p==NULL) {
		return -1;
	}

	_posops_data.idx = (int)(p - msg->buf);
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_rfind_str(sip_msg_t* msg, char* p1idx, char* p2val)
{
	int idx = 0;
	str val = STR_NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2val, &val)!=0) {
		LM_ERR("unable to get val parameter\n");
		return -1;
	}

	return ki_posops_pos_rfind_str(msg, idx, &val);
}

/**
 *
 */
static int ki_posops_pos_rfindi_str(sip_msg_t *msg, int idx, str *val)
{
	char *p;
	str text;

	posops_data_init();
	if(val==NULL || val->s==NULL || val->len<=0) {
		return -1;
	}
	if(idx<0) {
		idx += msg->len;
	}
	if(idx<0 || idx > msg->len - val->len) {
		return -1;
	}

	text.s = msg->buf + idx;
	text.len = msg->len - idx;
	p = str_rcasesearch(&text, val);
	if(p==NULL) {
		return -1;
	}

	_posops_data.idx = (int)(p - msg->buf);
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;

	return _posops_data.ret;
}

/**
 *
 */
static int w_posops_pos_rfindi_str(sip_msg_t* msg, char* p1idx, char* p2val)
{
	int idx = 0;
	str val = STR_NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)p2val, &val)!=0) {
		LM_ERR("unable to get val parameter\n");
		return -1;
	}

	return ki_posops_pos_rfindi_str(msg, idx, &val);
}

/**
 *
 */
static int ki_posops_pos_search_helper(sip_msg_t *msg, int idx, regex_t *re)
{
	regmatch_t pmatch;

	if(idx<0) {
		idx += msg->len;
	}
	if(idx<0 || idx >= msg->len) {
		return -1;
	}

	if (regexec(re, msg->buf + idx, 1, &pmatch, 0)!=0) {
		return -1;
	}
	if (pmatch.rm_so==-1) {
		return -1;
	}

	_posops_data.idx = idx + pmatch.rm_so;
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;
	_posops_data.len = pmatch.rm_eo-pmatch.rm_so;

	return _posops_data.ret;
}

/**
 *
 */
static int ki_posops_pos_search(sip_msg_t* msg, int idx, str* sre)
{
	regex_t mre;
	int ret;

	posops_data_init();
	memset(&mre, 0, sizeof(regex_t));
	if (regcomp(&mre, sre->s, REG_EXTENDED|REG_ICASE|REG_NEWLINE)!=0) {
		LM_ERR("failed to compile regex: %.*s\n", sre->len, sre->s);
		return -1;
	}

	ret = ki_posops_pos_search_helper(msg, idx, &mre);

	regfree(&mre);

	return ret;
}

/**
 *
 */
static int w_posops_pos_search(sip_msg_t* msg, char* p1idx, char* p2re)
{
	int idx = 0;
	regex_t *re = NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}
	re = (regex_t*)p2re;

	return ki_posops_pos_search_helper(msg, idx, re);
}

/**
 *
 */
static int ki_posops_pos_rsearch_helper(sip_msg_t *msg, int idx, regex_t *re)
{
	regmatch_t pmatch;
	int i;
	int ret = -1;

	if(idx<0) {
		idx += msg->len;
	}
	if(idx<0 || idx >= msg->len) {
		return -1;
	}

	for(i=msg->len-1; i>=idx; i--) {
		ret = regexec(re, msg->buf + i, 1, &pmatch, 0);
		if(ret==0) {
			break;
		}
	}
	if (ret!=0) {
		return -1;
	}

	_posops_data.idx = i + pmatch.rm_so;
	_posops_data.ret = (_posops_data.idx==0)?posops_idx0:_posops_data.idx;
	_posops_data.len = pmatch.rm_eo-pmatch.rm_so;

	return _posops_data.ret;
}

/**
 *
 */
static int ki_posops_pos_rsearch(sip_msg_t* msg, int idx, str* sre)
{
	regex_t mre;
	int ret;

	posops_data_init();
	memset(&mre, 0, sizeof(regex_t));
	if (regcomp(&mre, sre->s, REG_EXTENDED|REG_ICASE|REG_NEWLINE)!=0) {
		LM_ERR("failed to compile regex: %.*s\n", sre->len, sre->s);
		return -1;
	}

	ret = ki_posops_pos_rsearch_helper(msg, idx, &mre);

	regfree(&mre);

	return ret;
}

/**
 *
 */
static int w_posops_pos_rsearch(sip_msg_t* msg, char* p1idx, char* p2re)
{
	int idx = 0;
	regex_t *re = NULL;

	posops_data_init();
	if(fixup_get_ivalue(msg, (gparam_t*)p1idx, &idx)!=0) {
		LM_ERR("unable to get idx parameter\n");
		return -1;
	}
	re = (regex_t*)p2re;

	return ki_posops_pos_rsearch_helper(msg, idx, re);
}

/**
 *
 */
static int pv_posops_get_pos(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n) {
		case 0: /* idx */
			return pv_get_sintval(msg, param, res, _posops_data.idx);
		case 1: /* ret */
			return pv_get_sintval(msg, param, res, _posops_data.ret);
		case 2: /* len */
			return pv_get_sintval(msg, param, res, _posops_data.len);
	}
	return pv_get_null(msg, param, res);
}

/**
 *
 */
static int pv_posops_parse_pos_name(pv_spec_t *sp, str *in)
{
	switch(in->len) {
		case 3:
			if(strncmp(in->s, "idx", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "ret", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "len", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;

		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown pv pos key: %.*s\n", in->len, in->s);
	return -1;
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_posops_exports[] = {
	{ str_init("posops"), str_init("pos_append"),
		SR_KEMIP_INT, ki_posops_pos_append,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_insert"),
		SR_KEMIP_INT, ki_posops_pos_insert,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_rm"),
		SR_KEMIP_INT, ki_posops_pos_rm,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_headers_start"),
		SR_KEMIP_INT, ki_posops_pos_headers_start,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_headers_end"),
		SR_KEMIP_INT, ki_posops_pos_headers_end,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_body_start"),
		SR_KEMIP_INT, ki_posops_pos_body_start,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_body_end"),
		SR_KEMIP_INT, ki_posops_pos_body_end,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_find_str"),
		SR_KEMIP_INT, ki_posops_pos_find_str,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_findi_str"),
		SR_KEMIP_INT, ki_posops_pos_findi_str,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_rfind_str"),
		SR_KEMIP_INT, ki_posops_pos_rfind_str,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_rfindi_str"),
		SR_KEMIP_INT, ki_posops_pos_rfindi_str,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_search"),
		SR_KEMIP_INT, ki_posops_pos_search,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("posops"), str_init("pos_rsearch"),
		SR_KEMIP_INT, ki_posops_pos_rsearch,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_posops_exports);
	return 0;
}

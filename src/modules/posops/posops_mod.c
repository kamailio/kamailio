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

typedef struct posops_data {
	int ret;
	int idx;
} pospos_data_t;

/**
 *
 */
static pospos_data_t _pospos_data = {0};

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

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={

	{0, 0, 0}
};

struct module_exports exports = {
	"posops",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
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
static int ki_posops_pos_append(sip_msg_t *msg, int idx, str *val)
{
	int offset;
	sr_lump_t *anchor = NULL;

	if(val==NULL || val->s==NULL || val->len<=0) {
		LM_ERR("invalid val parameter\n");
		return -1;
	}

	if(idx<0) {
		offset = msg->len + idx + 1;
	} else {
		offset = idx;
	}
	if(offset>msg->len) {
		LM_ERR("offset invalid: %d (msg-len: %d)\n", offset, msg->len);
		return -1;
	}

	anchor = anchor_lump(msg, offset, 0, 0);
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

	if(val==NULL || val->s==NULL || val->len<=0) {
		LM_ERR("invalid val parameter\n");
		return -1;
	}

	if(idx<0) {
		offset = msg->len + idx + 1;
	} else {
		offset = idx;
	}
	if(offset>msg->len) {
		LM_ERR("offset invalid: %d (msg-len: %d)\n", offset, msg->len);
		return -1;
	}

	anchor = anchor_lump(msg, offset, 0, 0);
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

	if(len<=0) {
		LM_ERR("length invalid: %d (msg-len: %d)\n", len, msg->len);
		return -1;
	}
	if(idx<0) {
		offset = msg->len + idx + 1;
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
	int ret = 0;

	memset(&_pospos_data, 0, sizeof(pospos_data_t));
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	ret = msg->first_line.len;
	_pospos_data.idx = ret;

	return (ret==0)?-255:ret;
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
	int ret = 0;

	memset(&_pospos_data, 0, sizeof(pospos_data_t));
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	ret = msg->unparsed - msg->buf;
	_pospos_data.idx = ret;

	return (ret==0)?-255:ret;
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
	int ret = 0;
	char *body = 0;

	memset(&_pospos_data, 0, sizeof(pospos_data_t));
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	body = get_body(msg);
	if(body == NULL) {
		LM_DBG("no body\n");
		return -1;
	}
	ret = body - msg->buf;
	_pospos_data.idx = ret;

	return (ret==0)?-255:ret;
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
	int ret = 0;

	memset(&_pospos_data, 0, sizeof(pospos_data_t));
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if(get_body(msg) == NULL) {
		LM_DBG("no body\n");
		return -1;
	}

	ret = msg->len;
	_pospos_data.idx = ret;

	return (ret==0)?-255:ret;
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

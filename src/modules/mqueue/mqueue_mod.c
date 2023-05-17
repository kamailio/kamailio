/**
 * Copyright (C) 2010 Elena-Ramona Modroiu (asipto.com)
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
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/parser/parse_param.h"
#include "../../core/shm_init.h"
#include "../../core/kemi.h"

#include "mqueue_api.h"
#include "mqueue_db.h"
#include "api.h"

MODULE_VERSION

static int mod_init(void);
static void mod_destroy(void);

static int w_mq_fetch(struct sip_msg *msg, char *mq, char *str2);
static int w_mq_size(struct sip_msg *msg, char *mq, char *str2);
static int w_mq_add(struct sip_msg *msg, char *mq, char *key, char *val);
static int w_mq_pv_free(struct sip_msg *msg, char *mq, char *str2);
int mq_param(modparam_t type, void *val);
int mq_param_name(modparam_t type, void *val);
static int fixup_mq_add(void **param, int param_no);
static int bind_mq(mq_api_t *api);

static int mqueue_rpc_init(void);

static int mqueue_size = 0;

int mqueue_addmode = 0;

static pv_export_t mod_pvs[] = {
		{{"mqk", sizeof("mqk") - 1}, PVT_OTHER, pv_get_mqk, 0, pv_parse_mq_name,
				0, 0, 0},
		{{"mqv", sizeof("mqv") - 1}, PVT_OTHER, pv_get_mqv, 0, pv_parse_mq_name,
				0, 0, 0},
		{{"mq_size", sizeof("mq_size") - 1}, PVT_OTHER, pv_get_mq_size, 0,
				pv_parse_mq_name, 0, 0, 0},
		{{0, 0}, 0, 0, 0, 0, 0, 0, 0}};


static cmd_export_t cmds[] = {{"mq_fetch", (cmd_function)w_mq_fetch, 1,
									  fixup_spve_null, 0, ANY_ROUTE},
		{"mq_add", (cmd_function)w_mq_add, 3, fixup_mq_add, 0, ANY_ROUTE},
		{"mq_pv_free", (cmd_function)w_mq_pv_free, 1, fixup_spve_null, 0,
				ANY_ROUTE},
		{"mq_size", (cmd_function)w_mq_size, 1, fixup_spve_null, 0, ANY_ROUTE},
		{"bind_mq", (cmd_function)bind_mq, 1, 0, 0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{"db_url", PARAM_STR, &mqueue_db_url},
		{"mqueue", PARAM_STRING | USE_FUNC_PARAM, (void *)mq_param},
		{"mqueue_name", PARAM_STRING | USE_FUNC_PARAM, (void *)mq_param_name},
		{"mqueue_size", INT_PARAM, &mqueue_size},
		{"mqueue_addmode", INT_PARAM, &mqueue_addmode}, {0, 0, 0}};

struct module_exports exports = {
		"mqueue", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, params, 0,		   /* exported RPC methods */
		mod_pvs,				   /* exported pseudo-variables */
		0,						   /* response function */
		mod_init,				   /* module initialization function */
		0,						   /* per child init function */
		mod_destroy				   /* destroy function */
};


/**
 * init module function
 */
static int mod_init(void)
{
	if(!mq_head_defined())
		LM_WARN("no mqueue defined\n");

	if(mqueue_rpc_init() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return 1;
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	mq_destroy();
}

static int w_mq_fetch(struct sip_msg *msg, char *mq, char *str2)
{
	int ret;
	str q;

	if(fixup_get_svalue(msg, (gparam_t *)mq, &q) < 0) {
		LM_ERR("cannot get the queue\n");
		return -1;
	}
	ret = mq_head_fetch(&q);
	if(ret < 0)
		return ret;
	return 1;
}

static int w_mq_size(struct sip_msg *msg, char *mq, char *str2)
{
	int ret;
	str q;

	if(fixup_get_svalue(msg, (gparam_t *)mq, &q) < 0) {
		LM_ERR("cannot get queue parameter\n");
		return -1;
	}

	ret = _mq_get_csize(&q);

	if(ret < 0)
		LM_ERR("mqueue %.*s not found\n", q.len, q.s);
	if(ret <= 0)
		ret--;

	return ret;
}

static int w_mq_add(struct sip_msg *msg, char *mq, char *key, char *val)
{
	str q;
	str qkey;
	str qval;

	if(fixup_get_svalue(msg, (gparam_t *)mq, &q) < 0) {
		LM_ERR("cannot get the queue\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)key, &qkey) < 0) {
		LM_ERR("cannot get the key\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)val, &qval) < 0) {
		LM_ERR("cannot get the val\n");
		return -1;
	}
	if(mq_item_add(&q, &qkey, &qval) < 0)
		return -1;
	return 1;
}

static int w_mq_pv_free(struct sip_msg *msg, char *mq, char *str2)
{
	str q;

	if(fixup_get_svalue(msg, (gparam_t *)mq, &q) < 0) {
		LM_ERR("cannot get the queue\n");
		return -1;
	}
	mq_pv_free(&q);
	return 1;
}

int mq_param(modparam_t type, void *val)
{
	str mqs;
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	str qname = {0, 0};
	int msize = 0;
	int dbmode = 0;
	int addmode = 0;

	if(val == NULL)
		return -1;

	if(!shm_initialized()) {
		LM_ERR("shm not initialized - cannot define mqueue now\n");
		return 0;
	}

	mqs.s = (char *)val;
	mqs.len = strlen(mqs.s);
	if(mqs.s[mqs.len - 1] == ';')
		mqs.len--;
	if(parse_params(&mqs, CLASS_ANY, &phooks, &params_list) < 0)
		return -1;
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncasecmp(pit->name.s, "name", 4) == 0) {
			qname = pit->body;
		} else if(pit->name.len == 4
				  && strncasecmp(pit->name.s, "size", 4) == 0) {
			str2sint(&pit->body, &msize);
		} else if(pit->name.len == 6
				  && strncasecmp(pit->name.s, "dbmode", 6) == 0) {
			str2sint(&pit->body, &dbmode);
		} else if(pit->name.len == 7
				  && strncasecmp(pit->name.s, "addmode", 7) == 0) {
			str2sint(&pit->body, &addmode);
		} else {
			LM_ERR("unknown param: %.*s\n", pit->name.len, pit->name.s);
			free_params(params_list);
			return -1;
		}
	}
	if(qname.len <= 0) {
		LM_ERR("mqueue name not defined: %.*s\n", mqs.len, mqs.s);
		free_params(params_list);
		return -1;
	}
	if(mq_head_add(&qname, msize, addmode) < 0) {
		LM_ERR("cannot add mqueue: %.*s\n", mqs.len, mqs.s);
		free_params(params_list);
		return -1;
	}
	LM_INFO("mqueue param: [%.*s|%d|%d]\n", qname.len, qname.s, dbmode,
			addmode);
	if(dbmode == 1 || dbmode == 2) {
		if(mqueue_db_load_queue(&qname) < 0) {
			LM_ERR("error loading mqueue: %.*s from DB\n", qname.len, qname.s);
			free_params(params_list);
			return -1;
		}
	}
	mq_set_dbmode(&qname, dbmode);
	free_params(params_list);
	return 0;
}

int mq_param_name(modparam_t type, void *val)
{
	str qname = {0, 0};
	int msize = 0;
	int addmode = 0;

	if(val == NULL)
		return -1;

	if(!shm_initialized()) {
		LM_ERR("shm not initialized - cannot define mqueue now\n");
		return 0;
	}

	qname.s = (char *)val;
	qname.len = strlen(qname.s);

	addmode = mqueue_addmode;
	msize = mqueue_size;

	if(qname.len <= 0) {
		LM_ERR("mqueue name not defined: %.*s\n", qname.len, qname.s);
		return -1;
	}
	if(mq_head_add(&qname, msize, addmode) < 0) {
		LM_ERR("cannot add mqueue: %.*s\n", qname.len, qname.s);
		return -1;
	}
	LM_INFO("mqueue param: [%.*s|%d]\n", qname.len, qname.s, msize);
	return 0;
}

static int fixup_mq_add(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2 || param_no == 3) {
		return fixup_spve_null(param, 1);
	}

	LM_ERR("invalid parameter number %d\n", param_no);
	return E_UNSPEC;
}

static int bind_mq(mq_api_t *api)
{
	if(!api)
		return -1;
	api->add = mq_item_add;
	return 0;
}

/* Return the size of the specified mqueue */
static void mqueue_rpc_get_size(rpc_t *rpc, void *ctx)
{
	void *vh;
	str mqueue_name;
	int mqueue_sz = 0;

	if(rpc->scan(ctx, "S", &mqueue_name) < 1) {
		rpc->fault(ctx, 400, "No queue name");
		return;
	}

	if(mqueue_name.len <= 0 || mqueue_name.s == NULL) {
		LM_ERR("bad mqueue name\n");
		rpc->fault(ctx, 400, "Invalid queue name");
		return;
	}

	mqueue_sz = _mq_get_csize(&mqueue_name);

	if(mqueue_sz < 0) {
		LM_ERR("no such mqueue\n");
		rpc->fault(ctx, 404, "No such queue");
		return;
	}

	if(rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "Sd", "name", &mqueue_name, "size", mqueue_sz);
}

static const char *mqueue_rpc_get_size_doc[2] = {"Get size of mqueue.", 0};

static void mqueue_rpc_get_sizes(rpc_t *rpc, void *ctx)
{
	mq_head_t *mh = mq_head_get(NULL);
	void *vh;
	int size;

	while(mh != NULL) {
		if(rpc->add(ctx, "{", &vh) < 0) {
			rpc->fault(ctx, 500, "Server error");
			return;
		}
		lock_get(&mh->lock);
		size = mh->csize;
		lock_release(&mh->lock);
		rpc->struct_add(vh, "Sd", "name", &mh->name, "size", size);
		mh = mh->next;
	}
}

static const char *mqueue_rpc_get_sizes_doc[2] = {
		"Get sizes of all mqueues.", 0};

static void mqueue_rpc_fetch(rpc_t *rpc, void *ctx)
{
	str mqueue_name;
	int mqueue_sz = 0;
	int ret = 0;
	void *th;
	str *key = NULL;
	str *val = NULL;

	if(rpc->scan(ctx, "S", &mqueue_name) < 1) {
		rpc->fault(ctx, 500, "No queue name");
		return;
	}

	if(mqueue_name.len <= 0 || mqueue_name.s == NULL) {
		LM_ERR("bad mqueue name\n");
		rpc->fault(ctx, 500, "Invalid queue name");
		return;
	}

	mqueue_sz = _mq_get_csize(&mqueue_name);

	if(mqueue_sz < 0) {
		LM_ERR("no such mqueue\n");
		rpc->fault(ctx, 500, "No such queue");
		return;
	}

	ret = mq_head_fetch(&mqueue_name);
	if(ret == -2) {
		rpc->fault(ctx, 404, "Empty queue");
		return;
	} else if(ret < 0) {
		LM_ERR("mqueue fetch\n");
		rpc->fault(ctx, 500, "Unexpected error (fetch)");
		return;
	}

	key = get_mqk(&mqueue_name);
	val = get_mqv(&mqueue_name);

	if(!val || !key) {
		rpc->fault(ctx, 500, "Unexpected error (result)");
		return;
	}

	/* add entry node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}

	if(rpc->struct_add(th, "SS", "key", key, "val", val) < 0) {
		rpc->fault(ctx, 500, "Server error appending (key/val)");
		return;
	}
}

static const char *mqueue_rpc_fetch_doc[2] = {
		"Fetch an element from the queue.", 0};

rpc_export_t mqueue_rpc[] = {
		{"mqueue.get_size", mqueue_rpc_get_size, mqueue_rpc_get_size_doc, 0},
		{"mqueue.get_sizes", mqueue_rpc_get_sizes, mqueue_rpc_get_sizes_doc,
				RET_ARRAY},
		{"mqueue.fetch", mqueue_rpc_fetch, mqueue_rpc_fetch_doc, 0},
		{0, 0, 0, 0}};

static int mqueue_rpc_init(void)
{
	if(rpc_register_array(mqueue_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static int ki_mq_add(sip_msg_t *msg, str *mq, str *key, str *val)
{
	if(mq_item_add(mq, key, val) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_mq_fetch(sip_msg_t *msg, str *mq)
{
	int ret;
	ret = mq_head_fetch(mq);
	if(ret < 0)
		return ret;
	return 1;
}

/**
 *
 */
static int ki_mq_size(sip_msg_t *msg, str *mq)
{
	int ret;

	ret = _mq_get_csize(mq);

	if(ret < 0 && mq != NULL)
		LM_ERR("mqueue %.*s not found\n", mq->len, mq->s);

	return ret;
}

/**
 *
 */
static int ki_mq_pv_free(sip_msg_t *msg, str *mq)
{
	mq_pv_free(mq);
	return 1;
}

/**
 *
 */
static sr_kemi_xval_t _sr_kemi_mqueue_xval = {0};

/**
 *
 */
static sr_kemi_xval_t *ki_mqx_get_mode(
		sip_msg_t *msg, str *qname, int qtype, int rmode)
{
	mq_pv_t *mp = NULL;

	memset(&_sr_kemi_mqueue_xval, 0, sizeof(sr_kemi_xval_t));
	mp = mq_pv_get(qname);
	if(mp == NULL || mp->item == NULL) {
		sr_kemi_xval_null(&_sr_kemi_mqueue_xval, 0);
		return &_sr_kemi_mqueue_xval;
	}
	_sr_kemi_mqueue_xval.vtype = SR_KEMIP_STR;
	if(qtype == 0) {
		_sr_kemi_mqueue_xval.v.s = mp->item->key;
	} else {
		_sr_kemi_mqueue_xval.v.s = mp->item->val;
	}
	return &_sr_kemi_mqueue_xval;
}

/**
 *
 */
static sr_kemi_xval_t *ki_mqk_get(sip_msg_t *msg, str *qname)
{
	return ki_mqx_get_mode(msg, qname, 0, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t *ki_mqk_gete(sip_msg_t *msg, str *qname)
{
	return ki_mqx_get_mode(msg, qname, 0, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t *ki_mqk_getw(sip_msg_t *msg, str *qname)
{
	return ki_mqx_get_mode(msg, qname, 0, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
static sr_kemi_xval_t *ki_mqv_get(sip_msg_t *msg, str *qname)
{
	return ki_mqx_get_mode(msg, qname, 1, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t *ki_mqv_gete(sip_msg_t *msg, str *qname)
{
	return ki_mqx_get_mode(msg, qname, 1, SR_KEMI_XVAL_NULL_EMPTY);
}

/**
 *
 */
static sr_kemi_xval_t *ki_mqv_getw(sip_msg_t *msg, str *qname)
{
	return ki_mqx_get_mode(msg, qname, 1, SR_KEMI_XVAL_NULL_PRINT);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_mqueue_exports[] = {
	{ str_init("mqueue"), str_init("mq_add"),
		SR_KEMIP_INT, ki_mq_add,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mq_fetch"),
		SR_KEMIP_INT, ki_mq_fetch,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mq_size"),
		SR_KEMIP_INT, ki_mq_size,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mq_pv_free"),
		SR_KEMIP_INT, ki_mq_pv_free,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mqk_get"),
		SR_KEMIP_XVAL, ki_mqk_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mqk_gete"),
		SR_KEMIP_XVAL, ki_mqk_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mqk_getw"),
		SR_KEMIP_XVAL, ki_mqk_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mqv_get"),
		SR_KEMIP_XVAL, ki_mqv_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mqv_gete"),
		SR_KEMIP_XVAL, ki_mqv_gete,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqueue"), str_init("mqv_getw"),
		SR_KEMIP_XVAL, ki_mqv_getw,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_mqueue_exports);
	return 0;
}

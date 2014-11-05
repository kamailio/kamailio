/**
 * $Id$
 *
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../lib/kmi/mi.h"
#include "../../parser/parse_param.h"
#include "../../shm_init.h"

#include "mqueue_api.h"
#include "api.h"

MODULE_VERSION

static int  mod_init(void);
static void mod_destroy(void);

static int w_mq_fetch(struct sip_msg* msg, char* mq, char* str2);
static int w_mq_size(struct sip_msg *msg, char *mq, char *str2);
static int w_mq_add(struct sip_msg* msg, char* mq, char* key, char* val);
static int w_mq_pv_free(struct sip_msg* msg, char* mq, char* str2);
int mq_param(modparam_t type, void *val);
static int fixup_mq_add(void** param, int param_no);
static int bind_mq(mq_api_t* api);

static struct mi_root *mq_mi_get_size(struct mi_root *, void *);

static pv_export_t mod_pvs[] = {
	{ {"mqk", sizeof("mqk")-1}, PVT_OTHER, pv_get_mqk, 0,
		pv_parse_mq_name, 0, 0, 0 },
	{ {"mqv", sizeof("mqv")-1}, PVT_OTHER, pv_get_mqv, 0,
		pv_parse_mq_name, 0, 0, 0 },
	{ {"mq_size", sizeof("mq_size")-1}, PVT_OTHER, pv_get_mq_size, 0,
		pv_parse_mq_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static mi_export_t mi_cmds[] = {
	{ "mq_get_size",	mq_mi_get_size,	0, 0, 0},
	{ 0, 0, 0, 0, 0}
};

static cmd_export_t cmds[]={
	{"mq_fetch", (cmd_function)w_mq_fetch, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"mq_add", (cmd_function)w_mq_add, 3, fixup_mq_add,
		0, ANY_ROUTE},
	{"mq_pv_free", (cmd_function)w_mq_pv_free, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"mq_size", (cmd_function) w_mq_size, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"bind_mq", (cmd_function)bind_mq, 1, 0,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"mqueue",          PARAM_STRING|USE_FUNC_PARAM, (void*)mq_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"mqueue",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	mi_cmds,        /* exported MI functions */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	0               /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(!mq_head_defined())
		LM_WARN("no mqueue defined\n");

	if(register_mi_mod(exports.name, mi_cmds) != 0) {
		LM_ERR("failed to register MI commands\n");
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

static int w_mq_fetch(struct sip_msg* msg, char* mq, char* str2)
{
	int ret;
	str q;

	if(fixup_get_svalue(msg, (gparam_t*)mq, &q)<0)
	{
		LM_ERR("cannot get the queue\n");
		return -1;
	}
	ret = mq_head_fetch(&q);
	if(ret<0)
		return ret;
	return 1;
}

static int w_mq_size(struct sip_msg *msg, char *mq, char *str2) 
{
	int ret;
	str q;

	if(fixup_get_svalue(msg, (gparam_t *) mq, &q) < 0) {
		LM_ERR("cannot get queue parameter\n");
		return -1;
	}

	ret = _mq_get_csize(&q);

	if(ret < 0)
		LM_ERR("mqueue %.*s not found\n", q.len, q.s);
	if(ret<=0) ret--;

	return ret;
}

static int w_mq_add(struct sip_msg* msg, char* mq, char* key, char* val)
{
	str q;
	str qkey;
	str qval;

	if(fixup_get_svalue(msg, (gparam_t*)mq, &q)<0)
	{
		LM_ERR("cannot get the queue\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)key, &qkey)<0)
	{
		LM_ERR("cannot get the key\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)val, &qval)<0)
	{
		LM_ERR("cannot get the val\n");
		return -1;
	}
	if(mq_item_add(&q, &qkey, &qval)<0)
		return -1;
	return 1;
}

static int w_mq_pv_free(struct sip_msg* msg, char* mq, char* str2)
{
	str q;

	if(fixup_get_svalue(msg, (gparam_t*)mq, &q)<0)
	{
		LM_ERR("cannot get the queue\n");
		return -1;
	}
	mq_pv_free(&q);
	return 1;
}

int mq_param(modparam_t type, void *val)
{
	str mqs;
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	str qname = {0, 0};
	int msize = 0;

	if(val==NULL)
		return -1;

	if(!shm_initialized())
	{
		LM_ERR("shm not intialized - cannot define mqueue now\n");
		return 0;
	}

	mqs.s = (char*)val;
	mqs.len = strlen(mqs.s);
	if(mqs.s[mqs.len-1]==';')
		mqs.len--;
	if (parse_params(&mqs, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==4
				&& strncasecmp(pit->name.s, "name", 4)==0) {
			qname = pit->body;
		} else if(pit->name.len==4
				&& strncasecmp(pit->name.s, "size", 4)==0) {
			str2sint(&pit->body, &msize);
		}  else {
			LM_ERR("unknown param: %.*s\n", pit->name.len, pit->name.s);
			free_params(params_list);
			return -1;
		}
	}
	if(qname.len<=0)
	{
		LM_ERR("mqueue name not defined: %.*s\n", mqs.len, mqs.s);
		free_params(params_list);
		return -1;
	}
	if(mq_head_add(&qname, msize)<0)
	{
		LM_ERR("cannot add mqueue: %.*s\n", mqs.len, mqs.s);
		free_params(params_list);
		return -1;
	}
	free_params(params_list);
	return 0;
}

static int fixup_mq_add(void** param, int param_no)
{
    if(param_no==1 || param_no==2 || param_no==3) {
		return fixup_spve_null(param, 1);
    }

    LM_ERR("invalid parameter number %d\n", param_no);
    return E_UNSPEC;
}

static int bind_mq(mq_api_t* api)
{
	if (!api)
		return -1;
	api->add = mq_item_add;
	return 0;
}

/* Return the size of the specified mqueue */

static struct mi_root *mq_mi_get_size(struct mi_root *cmd_tree, 
				      void *param)
{
	static struct mi_node	*node = NULL, *rpl = NULL;
	static struct mi_root	*rpl_tree = NULL;
	static struct mi_attr	*attr = NULL;
	str			mqueue_name;
	int			mqueue_sz = 0;
	char			*p = NULL;
	int			len = 0;

	if((node = cmd_tree->node.kids) == NULL) {
		return init_mi_tree(400, MI_MISSING_PARM_S, 
					 MI_MISSING_PARM_LEN);
	}

	mqueue_name = node->value;

	if(mqueue_name.len <= 0 || mqueue_name.s == NULL) {
		LM_ERR("bad mqueue name\n");
		return init_mi_tree(500, MI_SSTR("bad mqueue name"));
	}

	mqueue_sz = _mq_get_csize(&mqueue_name);

	if(mqueue_sz < 0) {
		LM_ERR("no such mqueue\n");
		return init_mi_tree(404, MI_SSTR("no such mqueue"));
	}

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);

	if(rpl_tree == NULL) 
		return 0;

	rpl = &rpl_tree->node;

	node = add_mi_node_child(rpl, MI_DUP_VALUE, "mqueue", strlen("mqueue"),
				 NULL, 0);

	if(node == NULL) {
		free_mi_tree(rpl_tree);
		return NULL;
	}

	attr = add_mi_attr(node, MI_DUP_VALUE, "name", strlen("name"),
			   mqueue_name.s, mqueue_name.len);

	if(attr == NULL) goto error;

	p = int2str((unsigned long) mqueue_sz, &len);	

	attr = add_mi_attr(node, MI_DUP_VALUE, "size", strlen("size"), 
			   p, len);

	if(attr == NULL) goto error;

	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
	return NULL;
}


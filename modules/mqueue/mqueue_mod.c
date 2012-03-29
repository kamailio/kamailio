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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../parser/parse_param.h"
#include "../../shm_init.h"

#include "mqueue_api.h"
#include "api.h"

MODULE_VERSION

static int  mod_init(void);
static void mod_destroy(void);

static int w_mq_fetch(struct sip_msg* msg, char* mq, char* str2);
static int w_mq_add(struct sip_msg* msg, char* mq, char* key, char* val);
static int w_mq_pv_free(struct sip_msg* msg, char* mq, char* str2);
int mq_param(modparam_t type, void *val);
static int fixup_mq_add(void** param, int param_no);
static int bind_mq(mq_api_t* api);

static pv_export_t mod_pvs[] = {
	{ {"mqk", sizeof("mqk")-1}, PVT_OTHER, pv_get_mqk, 0,
		pv_parse_mq_name, 0, 0, 0 },
	{ {"mqv", sizeof("mqv")-1}, PVT_OTHER, pv_get_mqv, 0,
		pv_parse_mq_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"mq_fetch", (cmd_function)w_mq_fetch, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"mq_add", (cmd_function)w_mq_add, 3, fixup_mq_add,
		0, ANY_ROUTE},
	{"mq_pv_free", (cmd_function)w_mq_pv_free, 1, fixup_str_null,
		0, ANY_ROUTE},
	{"bind_mq", (cmd_function)bind_mq, 1, 0,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"mqueue",          STR_PARAM|USE_FUNC_PARAM, (void*)mq_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"mqueue",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
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
	mq_pv_free((str*)mq);
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

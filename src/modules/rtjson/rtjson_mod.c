/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
#include "../../core/kemi.h"

#include "rtjson_routing.h"

MODULE_VERSION


str _rtjson_xavp_name = str_init("rtjson");

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_rtjson_init_routes(sip_msg_t *msg, char *rdoc, char *rflags);
static int w_rtjson_push_routes(sip_msg_t *msg, char *p1, char *p2);
static int w_rtjson_next_route(sip_msg_t *msg, char *p1, char *p2);
static int w_rtjson_update_branch(sip_msg_t *msg, char *p1, char *p2);

static cmd_export_t cmds[]={
	{"rtjson_init_routes", (cmd_function)w_rtjson_init_routes, 1, fixup_spve_null,
		0, REQUEST_ROUTE},
	{"rtjson_push_routes", (cmd_function)w_rtjson_push_routes,     0, 0,
		0, REQUEST_ROUTE},
	{"rtjson_next_route", (cmd_function)w_rtjson_next_route,       0, 0,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"rtjson_update_branch", (cmd_function)w_rtjson_update_branch, 0, 0,
		0, BRANCH_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"xavp_cfg", PARAM_STR, &_rtjson_xavp_name},
	{0, 0, 0}
};

struct module_exports exports = {
	"rtjson",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy     /* destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(_rtjson_xavp_name.s==NULL || _rtjson_xavp_name.len<=0) {
		LM_ERR("invalid xavp name\n");
		return -1;
	}
	if(rtjson_init()<0) {
		LM_ERR("failed to initialize\n");
		return -1;
	}
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
}

/**
 *
 */
static int w_rtjson_init_routes(sip_msg_t *msg, char *rdoc, char *rflags)
{
	str srdoc = {0};

	if(msg==NULL)
		return -1;

	if(fixup_get_svalue(msg, (gparam_t*)rdoc, &srdoc)!=0 || srdoc.len<=0) {
		LM_ERR("no routing information\n");
		return -1;
	}
	if(rtjson_init_routes(msg, &srdoc)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int w_rtjson_push_routes(sip_msg_t *msg, char *p1, char *p2)
{
	if(msg==NULL)
		return -1;

	if(rtjson_push_routes(msg)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int w_rtjson_next_route(sip_msg_t *msg, char *p1, char *p2)
{
	if(msg==NULL)
		return -1;

	if(rtjson_next_route(msg)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int w_rtjson_update_branch(sip_msg_t *msg, char *p1, char *p2)
{
	if(msg==NULL)
		return -1;

	if(rtjson_update_branch(msg)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_rtjson_init_routes(sip_msg_t *msg, str *srdoc)
{
	if(msg==NULL)
		return -1;

	if(rtjson_init_routes(msg, srdoc)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_rtjson_push_routes(sip_msg_t *msg)
{
	if(msg==NULL)
		return -1;

	if(rtjson_push_routes(msg)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_rtjson_next_route(sip_msg_t *msg)
{
	if(msg==NULL)
		return -1;

	if(rtjson_next_route(msg)<0)
		return -1;

	return 1;
}

/**
 *
 */
static int ki_rtjson_update_branch(sip_msg_t *msg)
{
	if(msg==NULL)
		return -1;

	if(rtjson_update_branch(msg)<0)
		return -1;

	return 1;
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_rtjson_exports[] = {
	{ str_init("rtjson"), str_init("init_routes"),
		SR_KEMIP_INT, ki_rtjson_init_routes,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("rtjson"), str_init("push_routes"),
		SR_KEMIP_INT, ki_rtjson_push_routes,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("rtjson"), str_init("next_route"),
		SR_KEMIP_INT, ki_rtjson_next_route,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("rtjson"), str_init("update_branch"),
		SR_KEMIP_INT, ki_rtjson_update_branch,
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
	sr_kemi_modules_add(sr_kemi_rtjson_exports);
	return 0;
}

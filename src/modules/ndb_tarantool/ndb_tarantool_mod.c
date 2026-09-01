/*
 * Copyright (C) 2026 Andrei Lashchinskii <koorwork+kamailio@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "tarantool_client.h"
#include "tarantool_kemi.h"

MODULE_VERSION

/* Module configuration parameters */
static int tnt_srv_param(modparam_t type, void *val);
int init_without_tarantool = 0;
int tnt_connect_timeout_param = TNT_DEFAULT_CONNECT_TIMEOUT;
int tnt_cmd_timeout_param = TNT_DEFAULT_CMD_TIMEOUT;
int tnt_disable_time_param = TNT_DEFAULT_DISABLE_TIME;
int tnt_allowed_timeouts_param = TNT_DEFAULT_ALLOWED_TIMEOUTS;

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

int mod_register(char *path, int *dlflags, void *p1, void *p2);

/* Script command wrappers */
static int w_tarantool_call(
		sip_msg_t *msg, char *proc, char *params, char *res);
static int w_tarantool_eval(
		sip_msg_t *msg, char *code, char *params, char *res);
static int w_tarantool_call_srv(
		sip_msg_t *msg, char *srv, char *proc, char *params, char *res);
static int w_tarantool_eval_srv(
		sip_msg_t *msg, char *srv, char *code, char *params, char *res);

/* clang-format off */
static cmd_export_t cmds[] = {
	{"tarantool_call", (cmd_function)w_tarantool_call, 3,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"tarantool_eval", (cmd_function)w_tarantool_eval, 3,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"tarantool_call_srv", (cmd_function)w_tarantool_call_srv, 4,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"tarantool_eval_srv", (cmd_function)w_tarantool_eval_srv, 4,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t mod_params[] = {
	{"server", PARAM_STRING | PARAM_USE_FUNC, (void *)tnt_srv_param},
	{"init_without_tarantool", PARAM_INT, &init_without_tarantool},
	{"connect_timeout", PARAM_INT, &tnt_connect_timeout_param},
	{"cmd_timeout", PARAM_INT, &tnt_cmd_timeout_param},
	{"disable_time", PARAM_INT, &tnt_disable_time_param},
	{"allowed_timeouts", PARAM_INT, &tnt_allowed_timeouts_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"ndb_tarantool",        /* module name */
	DEFAULT_DLFLAGS,        /* dlopen flags */
	cmds,                   /* exported functions */
	mod_params,             /* exported parameters */
	0,                      /* exported RPC methods */
	0,                      /* exported pseudo-variables */
	0,                      /* response function */
	mod_init,               /* module initialization function */
	child_init,             /* per child init function */
	mod_destroy             /* destroy function */
};
/* clang-format on */

/**
 * tnt_srv_param - Handler for 'server' modparam
 * @type: parameter type
 * @val: parameter value string
 */
static int tnt_srv_param(modparam_t type, void *val)
{
	if(type != PARAM_STRING || !val) {
		LM_ERR("invalid server parameter specification\n");
		return -1;
	}
	return tnt_add_server((char *)val);
}

/**
 * mod_init - Module initialization
 */
static int mod_init(void)
{
	LM_INFO("initializing ndb_tarantool module...\n");
	return 0;
}

/**
 * child_init - Per-child worker process initialization
 * @rank: process rank ID
 */
static int child_init(int rank)
{
	/* Skip management processes without SIP routing duties */
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN) {
		return 0;
	}

	LM_DBG("initializing tarantool connections for child process rank=%d\n",
			rank);
	if(tnt_child_init(rank) < 0) {
		LM_ERR("failed to initialize tarantool connections for child rank %d\n",
				rank);
		return -1;
	}
	return 0;
}

/**
 * mod_destroy - Module destroy cleanup
 */
static void mod_destroy(void)
{
	LM_INFO("destroying ndb_tarantool module resources...\n");
	tnt_destroy_all();
}

/**
 * w_tarantool_call - Script function: tarantool_call(proc, params, res)
 */
static int w_tarantool_call(sip_msg_t *msg, char *proc, char *params, char *res)
{
	str s_proc = {NULL, 0};
	str s_params = {NULL, 0};
	str s_res = {NULL, 0};

	if(!proc || fixup_get_svalue(msg, (gparam_t *)proc, &s_proc) != 0) {
		LM_ERR("failed to get procedure name\n");
		return -1;
	}
	if(params && fixup_get_svalue(msg, (gparam_t *)params, &s_params) != 0) {
		LM_ERR("failed to get procedure params\n");
		return -1;
	}
	if(res && fixup_get_svalue(msg, (gparam_t *)res, &s_res) != 0) {
		LM_ERR("failed to get result variable\n");
		return -1;
	}

	return sr_kemi_tarantool_call(msg, &s_proc, &s_params, &s_res);
}

/**
 * w_tarantool_eval - Script function: tarantool_eval(code, params, res)
 */
static int w_tarantool_eval(sip_msg_t *msg, char *code, char *params, char *res)
{
	str s_code = {NULL, 0};
	str s_params = {NULL, 0};
	str s_res = {NULL, 0};

	if(!code || fixup_get_svalue(msg, (gparam_t *)code, &s_code) != 0) {
		LM_ERR("failed to get expression argument\n");
		return -1;
	}
	if(params && fixup_get_svalue(msg, (gparam_t *)params, &s_params) != 0) {
		LM_ERR("failed to get eval params\n");
		return -1;
	}
	if(res && fixup_get_svalue(msg, (gparam_t *)res, &s_res) != 0) {
		LM_ERR("failed to get result variable\n");
		return -1;
	}

	return sr_kemi_tarantool_eval(msg, &s_code, &s_params, &s_res);
}

/**
 * w_tarantool_call_srv - Script function: tarantool_call_srv(srv, proc, params, res)
 */
static int w_tarantool_call_srv(
		sip_msg_t *msg, char *srv, char *proc, char *params, char *res)
{
	str s_srv = {NULL, 0};
	str s_proc = {NULL, 0};
	str s_params = {NULL, 0};
	str s_res = {NULL, 0};

	if(!srv || fixup_get_svalue(msg, (gparam_t *)srv, &s_srv) != 0) {
		LM_ERR("failed to get server name\n");
		return -1;
	}
	if(!proc || fixup_get_svalue(msg, (gparam_t *)proc, &s_proc) != 0) {
		LM_ERR("failed to get procedure name\n");
		return -1;
	}
	if(params && fixup_get_svalue(msg, (gparam_t *)params, &s_params) != 0) {
		LM_ERR("failed to get procedure params\n");
		return -1;
	}
	if(res && fixup_get_svalue(msg, (gparam_t *)res, &s_res) != 0) {
		LM_ERR("failed to get result variable\n");
		return -1;
	}

	return sr_kemi_tarantool_call_srv(msg, &s_srv, &s_proc, &s_params, &s_res);
}

/**
 * w_tarantool_eval_srv - Script function: tarantool_eval_srv(srv, code, params, res)
 */
static int w_tarantool_eval_srv(
		sip_msg_t *msg, char *srv, char *code, char *params, char *res)
{
	str s_srv = {NULL, 0};
	str s_code = {NULL, 0};
	str s_params = {NULL, 0};
	str s_res = {NULL, 0};

	if(!srv || fixup_get_svalue(msg, (gparam_t *)srv, &s_srv) != 0) {
		LM_ERR("failed to get server name\n");
		return -1;
	}
	if(!code || fixup_get_svalue(msg, (gparam_t *)code, &s_code) != 0) {
		LM_ERR("failed to get expression argument\n");
		return -1;
	}
	if(params && fixup_get_svalue(msg, (gparam_t *)params, &s_params) != 0) {
		LM_ERR("failed to get eval params\n");
		return -1;
	}
	if(res && fixup_get_svalue(msg, (gparam_t *)res, &s_res) != 0) {
		LM_ERR("failed to get result variable\n");
		return -1;
	}

	return sr_kemi_tarantool_eval_srv(msg, &s_srv, &s_code, &s_params, &s_res);
}

/**
 * mod_register - Dynamic registration of KEMI exports
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	(void)path;
	(void)dlflags;
	(void)p1;
	(void)p2;
	sr_kemi_ndb_tarantool_register();
	return 0;
}

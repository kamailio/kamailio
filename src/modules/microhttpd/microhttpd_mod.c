/**
 * Copyright (C) 2023 Daniel-Constantin Mierla (asipto.com)
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

#include <microhttpd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"
#include "../../core/cfg/cfg_struct.h"

MODULE_VERSION

static int _microhttpd_server_pid = -1;

static int microhttpd_server_run(void);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

int pv_get_mhttpd(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
int pv_parse_mhttpd_name(pv_spec_p sp, str *in);

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{{"mhttpd", (sizeof("mhttpd") - 1)}, PVT_OTHER, pv_get_mhttpd, 0,
			pv_parse_mhttpd_name, 0, 0, 0},

	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static cmd_export_t cmds[] = {
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{0, 0, 0}
};

struct module_exports exports = {
	"microhttpd",	 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* exported functions */
	params,			 /* exported parameters */
	0,				 /* exported rpc functions */
	mod_pvs,		 /* exported pseudo-variables */
	0,				 /* response handling function */
	mod_init,		 /* module init function */
	child_init,		 /* per child init function */
	mod_destroy		 /* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	/* add space for one extra process */
	register_procs(1);

	/* add child to update local config framework structures */
	cfg_register_child(1);

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	int pid;

	if(rank != PROC_MAIN)
		return 0;

	pid = fork_process(PROC_NOCHLDINIT, "MicroHTTPd Server Process", 1);
	if(pid < 0)
		return -1; /* error */
	if(pid == 0) {
		/* child */
		_microhttpd_server_pid = getpid();

		/* do child init to allow execution of rpc like functions */
		if(init_child(PROC_RPC) < 0) {
			LM_DBG("failed to do RPC child init for dispatcher\n");
			return -1;
		}
		/* initialize the config framework */
		if(cfg_child_init())
			return -1;
		if(microhttpd_server_run() < 0) {
			LM_ERR("failed to initialize microhttpd server process\n");
			return -1;
		}
	}

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 * parse the name of the $mhttpd(name)
 */
int pv_parse_mhttpd_name(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;
	switch(in->len) {
		case 4:
			if(strncasecmp(in->s, "path", 4)==0) {
				sp->pvp.pvn.u.isname.name.n = 0;
			} else {
				goto error;
			}
			break;
		case 6:
			if(strncasecmp(in->s, "method", 6)==0) {
				sp->pvp.pvn.u.isname.name.n = 1;
			} else {
				goto error;
			}
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("invalid variable name [%.*s]\n", in->len, in->s);
	return -1;
}

/**
 * return the value of $mhttpd(name)
 */
int pv_get_mhttpd(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	if(param == NULL)
		return -1;
	switch(param->pvn.u.isname.name.n) {
		case 1:
			return pv_get_null(msg, param, res);
		default:
			return pv_get_null(msg, param, res);
	}
}


/**
 *
 */
static int microhttpd_server_run(void)
{
	return -1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_mhttpd_exports[] = {

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */


/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_mhttpd_exports);
	return 0;
}

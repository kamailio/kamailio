/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "corex_lib.h"
#include "corex_rpc.h"
#include "corex_var.h"

MODULE_VERSION

static int w_append_branch(sip_msg_t *msg, char *su, char *sq);
static int w_send(sip_msg_t *msg, char *su, char *sq);
static int w_send_tcp(sip_msg_t *msg, char *su, char *sq);
static int w_send_data(sip_msg_t *msg, char *suri, char *sdata);

int corex_alias_subdomains_param(modparam_t type, void *val);

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static pv_export_t mod_pvs[] = {
	{ {"cfg", (sizeof("cfg")-1)}, PVT_OTHER, pv_get_cfg, 0,
		pv_parse_cfg_name, 0, 0, 0 },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"append_branch", (cmd_function)w_append_branch, 0, 0,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"append_branch", (cmd_function)w_append_branch, 1, fixup_spve_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"append_branch", (cmd_function)w_append_branch, 2, fixup_spve_spve,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send", (cmd_function)w_send, 0, 0,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send", (cmd_function)w_send, 1, fixup_spve_spve,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_tcp", (cmd_function)w_send_tcp, 0, 0,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_tcp", (cmd_function)w_send_tcp, 1, fixup_spve_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_data", (cmd_function)w_send_data, 2, fixup_spve_spve,
			0, ANY_ROUTE },


	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"alias_subdomains",  STR_PARAM|USE_FUNC_PARAM,
								(void*)corex_alias_subdomains_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"corex",
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
	child_init      /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(corex_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(corex_register_check_self()<0)
	{
		LM_ERR("failed to register check self callback\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	if (rank!=PROC_MAIN)
		return 0;

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 * config wrapper for append branch
 */
static int w_append_branch(sip_msg_t *msg, char *su, char *sq)
{
	if(corex_append_branch(msg, (gparam_t*)su, (gparam_t*)sq) < 0)
		return -1;
	return 1;
}

/**
 * config wrapper for send() and send_tcp()
 */
static int w_send(sip_msg_t *msg, char *su, char *sq)
{
	if(corex_send(msg, (gparam_t*)su, PROTO_UDP) < 0)
		return -1;
	return 1;
}
static int w_send_tcp(sip_msg_t *msg, char *su, char *sq)
{
	if(corex_send(msg, (gparam_t*)su, PROTO_TCP) < 0)
		return -1;
	return 1;
}

static int w_send_data(sip_msg_t *msg, char *suri, char *sdata)
{
	str uri;
	str data;

	if (fixup_get_svalue(msg, (gparam_t*)suri, &uri))
	{
		LM_ERR("cannot get the destination parameter\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)sdata, &data))
	{
		LM_ERR("cannot get the destination parameter\n");
		return -1;
	}
	if(corex_send_data(&uri, &data) < 0)
		return -1;
	return 1;
}

int corex_alias_subdomains_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return corex_add_alias_subdomains((char*)val);
error:
	return -1;

}


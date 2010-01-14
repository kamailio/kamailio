/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */


#include <stdlib.h>

#include "pt.h"
#include "sr_module.h"
#include "ut.h"
#include "route.h"
#include "dprint.h"
#include "mem/wrappers.h"
#include "select_buf.h"

#include "strutil.h"
#include "binds.h"
#include "appsrv.h"

MODULE_VERSION

/* rpc.c */
extern rpc_export_t mod_rpc[];
extern select_row_t sel_declaration[];

/* when SHM is available at module parameter setting time, the ASes can simply
 * be added with the 'constructor' (instead of depositing the URI in this
 * array). */
#define MAX_ASES	128
static char *app_srvs[MAX_ASES];
static size_t as_cnt = 0;


static int mod_init(void);
static int mod_child(int rank);
static void mod_destroy(void);
int dispatch_fixup(void** param, int param_no);
static int add_app_srv(modparam_t type, void * _param);

extern int onreply_rt_idx; /* rpc.c declaration */
static char *onreply_route = NULL;
#ifdef ASI_WITH_LOCDGRAM
static char *usock_uid_str = NULL;
static char *usock_gid_str = NULL;
#endif

#define ALL_ROUTES	\
	REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE | \
	ONSEND_ROUTE

static cmd_export_t mod_cmds[] = {
		{"asi_dispatch", (cmd_function)dispatch, 1, dispatch_fixup, 
				ALL_ROUTES},
		{"asi_dispatch_rebuild", (cmd_function)dispatch_rebuild, 1, 
				dispatch_fixup, ALL_ROUTES},
		{0, 0, 0, 0, 0}
};

static param_export_t mod_params[] = {
	{"app_srv",			PARAM_STRING|PARAM_USE_FUNC,(void *)add_app_srv	 },
	{"expect_reply",	PARAM_INT,					&expect_reply},
	{"onreply_route",	PARAM_STRING,				&onreply_route},
#ifdef ASI_WITH_LOCDGRAM
	{"usock_template",	PARAM_STRING,				&usock_template},
	{"usock_mode",		PARAM_INT,					&usock_mode},
	{"usock_uid",		PARAM_STRING,				&usock_uid_str},
	{"usock_gid",		PARAM_STRING,				&usock_gid_str},
#endif
	{"connect_timeout",		PARAM_INT,				&ct_timeout},
	{"transmit_timeout",	PARAM_INT,				&tx_timeout},
	{"receive_timeout",		PARAM_INT,				&rx_timeout},
	{0, 0, 0} 
};

struct module_exports exports = {
	"asi",
	mod_cmds,
	mod_rpc,
	mod_params,
	mod_init,
	NULL, /* on-reply callback */
	mod_destroy,
	NULL, /* on-cancel callback */
	mod_child,
};


static void handshake_all()
{
	int i;
	as_t *as;
	
	for (i = 0; i < as_cnt; i ++) {
		as = as4id(i);
		if (handshake_appsrv(as) < 0) {
			ERR("handshake with AS '%.*s' failed.\n", STR_FMT(&as->name));
		} else {
			INFO("handshake with AS '%.*s' succeeded.\n", 
					STR_FMT(&as->name));
		}
	}
}

static void disconnect_all()
{
	int i;
	as_t *as;
	
	for (i = 0; i < as_cnt; i ++) {
		as = as4id(i);
		disconnect_appsrv(as);
	}
}

static int mod_init(void)
{
	char *name, *uri, *param;
	int i;
	extern struct route_list onreply_rt; /* route.c declaration */

	if ((xll_bind() < 0) || (tm_bind() < 0)) {
		ERR("asi - failed to initialize due to missing dependencies.\n");
		return -1;
	}

	if (onreply_route) {
		i=route_get(&onreply_rt, onreply_route);
		if ((i < 0) || 
				/*route_get() actually adds it...*/(! onreply_rt.rlist[i])) {
			ERR("unknown route `%s'\n", onreply_route);
			return -1;
		} else {
			onreply_rt_idx = i;
		}
	}
	
	
	if (register_select_table(sel_declaration) < 0) {
		ERR("failed to register SELECTs table.\n");
		return -1;
	}

	brpc_mem_setup(w_pkg_calloc, w_pkg_malloc, w_pkg_free, w_pkg_realloc);
#if 0
	brpc_log_setup(syslog);
#endif

	/* allow user specify milis (rather than used micros) */
	ct_timeout = (ct_timeout < 0) ? DEFAULT_CT_TIMEOUT : ct_timeout * 1000;
	tx_timeout = (tx_timeout < 0) ? DEFAULT_TX_TIMEOUT : tx_timeout * 1000;
	rx_timeout = (rx_timeout < 0) ? DEFAULT_RX_TIMEOUT : rx_timeout * 1000;

	if (! as_cnt)
		WARN("ASI: empty AS list.\n");
	else {
		/* initialize AS structures */
		for (i = 0; i < as_cnt; i ++) {
			/* sems@brpcns://127.0.0.1:5111 */
			param = app_srvs[i];
			if (! (uri = strchr(param, '@'))) {
				ERR("invalid 'app_srv' parameter `%s': missingr `@' "
						"separator.\n", param);
				return -1;
			} else if (uri == param) {
				ERR("invalid 'app_srv' parameter `%s': empty AP name.\n", 
						param);
				return -1;
			} else {
				name = param;
				*uri = 0;
				uri ++;
				DEBUG("split into name `%s', uri `%s'.\n", name, uri);
			}

			if (new_appsrv(name, uri) < 0) {
				ERR("failed to add AS instance for 'app_srv' `%s'.\n", param);
				return -1;
			}
			INFO("new BINRPC URI `%s' for AS `%s' added.\n", uri, name);
		}

#ifdef ASI_WITH_LOCDGRAM
		if (expect_reply) {
			/* fix UID/GID */
			if (usock_uid_str)
				if (user2uid(&usock_uid, NULL, usock_uid_str)<0){
					ERR("bad user name/uid number %s\n", usock_uid_str);
					return -1;
				}
			if (usock_gid_str)
				if (group2gid(&usock_uid, usock_gid_str)<0){
					ERR("bad group name/gid number %s\n", usock_gid_str);
					return -1;
				}
		}
#endif
	}

	handshake_all();
	/*don't need connection in main proc*/
	disconnect_all();
	
	INFO("asi - initialized (parent).\n");
	return 0;
}



static int mod_child(int rank)
{
	brpc_id_t callid;

#ifdef ASI_WITH_RESYNC
	if (rank == PROC_INIT) {
		if (setup_bmb(get_max_procs()) < 0) {
			ERR("failed to setup resync process bit mask.\n");
			return -1;
		}
	}
#endif
	reset_static_buffer(); /* needed mostly for timer processes */
	if (rank <= 0)
		/* TODO: is the limit 0 safe? */
		/* this is no worker process */
		return 0;

	INFO("asi - initializing (worker #%d).\n", rank);
	callid = rand();
	/* make sure the distance between callid's initial value of all processes
	 * is large enough */
	callid &= 0xffffff;
	callid |= rank << 24;
	init_appsrv_proc(rank, callid);

	handshake_all();

	return 0;
}



static void mod_destroy(void)
{
	free_appsrvs();
	INFO("module ASI destroyed (boom!).\n");
}

static int add_app_srv(modparam_t type, void * _param)
{
	if ((type & PARAM_STRING) == 0) {
		BUG("invalid parameter type %d (string expected).\n", type);
		return -1;
	}

	if (MAX_ASES < as_cnt) {
		BUG("too many AS configured (%zd); update MAX_ASES definition and "
				"recompile.\n", as_cnt);
		return -1;
	}
	app_srvs[as_cnt ++] = (char *)_param;
	DEBUG("new AS mapping `%s' enqueued\n", (char *)_param);
	return 0;

}

int dispatch_fixup(void** param, int param_no)
{
	str name;
	int i;
	as_t *as;

	if (param_no != 1) {
		ERR("invalid number of arguments (%d).\n", param_no);
		return -1;
	}
	name.s = *(char **)param;
	name.len = strlen(name.s);
	DEBUG("fixing parameter `%s' [%d].\n", name.s, name.len);

	if ((name.len == 1) && (*name.s == '*')) {
		DEBUG("asi - broadcasting.\n");
		*param = NULL;
		return 0;
	}

	for (i = 0; i < as_cnt; i ++) {
		as = as4id(i);
		if (STR_EQ(name, as->name)) {
			*param = as;
			return 0;
		}
	}
	
	ERR("AS name `%s' not registered (must be specified as module "
			"parameter.\n", name.s);
	return -1;
}

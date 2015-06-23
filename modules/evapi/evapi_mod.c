/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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

#include <ev.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../pvar.h"
#include "../../mem/shm_mem.h"
#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/kcore/faked_msg.h"

#include "../../modules/tm/tm_load.h"

#include "evapi_dispatch.h"

MODULE_VERSION

static int   _evapi_workers = 1;
static char *_evapi_bind_addr = "127.0.0.1";
static int   _evapi_bind_port = 8448;
static char *_evapi_bind_param = NULL;
static int   _evapi_netstring_format_param = 1;

static tm_api_t tmb;

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_evapi_relay(sip_msg_t* msg, char* evdata, char* p2);
static int w_evapi_async_relay(sip_msg_t* msg, char* evdata, char* p2);
static int w_evapi_close(sip_msg_t* msg, char* p1, char* p2);
static int fixup_evapi_relay(void** param, int param_no);

static cmd_export_t cmds[]={
	{"evapi_relay",       (cmd_function)w_evapi_relay,       1, fixup_evapi_relay,
		0, ANY_ROUTE},
	{"evapi_async_relay", (cmd_function)w_evapi_async_relay, 1, fixup_evapi_relay,
		0, REQUEST_ROUTE},
	{"evapi_close",       (cmd_function)w_evapi_close,       0, NULL,
		0, ANY_ROUTE},
	{"evapi_close",       (cmd_function)w_evapi_close,       1, NULL,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"workers",           INT_PARAM,   &_evapi_workers},
	{"bind_addr",         PARAM_STRING,   &_evapi_bind_param},
	{"netstring_format",  INT_PARAM,   &_evapi_netstring_format_param},
	{0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{ {"evapi", (sizeof("evapi")-1)}, PVT_OTHER, pv_get_evapi,
		pv_set_evapi, pv_parse_evapi_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


struct module_exports exports = {
	"evapi",
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
	char *p;

	/* init faked sip msg */
	if(faked_msg_init()<0) {
		LM_ERR("failed to init faked sip msg\n");
		return -1;
	}

	if(load_tm_api( &tmb ) < 0) {
		LM_INFO("cannot load the TM-functions - async relay disabled\n");
		memset(&tmb, 0, sizeof(tm_api_t));
	}

	if(_evapi_bind_param!=NULL) {
		p = strchr(_evapi_bind_param, ':');
		if(p!=NULL) {
			*p++ = '\0';
			_evapi_bind_port = (short)atoi(p);
			if (_evapi_bind_port <= 0) {
				LM_ERR("invalid port: %d\n", _evapi_bind_port);
				return -1;
			}
		}
		_evapi_bind_addr = _evapi_bind_param;
	}

	/* add space for one extra process */
	register_procs(1 + _evapi_workers);

	/* add child to update local config framework structures */
	cfg_register_child(1 + _evapi_workers);

	evapi_init_environment(_evapi_netstring_format_param);

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	int pid;
	int i;

	if (rank==PROC_INIT) {
		if(evapi_init_notify_sockets()<0) {
			LM_ERR("failed to initialize notify sockets\n");
			return -1;
		}
		return 0;
	}

	if (rank!=PROC_MAIN) {
		evapi_close_notify_sockets_parent();
		return 0;
	}

	pid=fork_process(PROC_NOCHLDINIT, "EvAPI Dispatcher", 1);
	if (pid<0)
		return -1; /* error */
	if(pid==0) {
		/* child */

		/* initialize the config framework */
		if (cfg_child_init())
			return -1;
		/* main function for dispatcher */
		evapi_close_notify_sockets_child();
		if(evapi_run_dispatcher(_evapi_bind_addr, _evapi_bind_port)<0) {
			LM_ERR("failed to initialize disptacher process\n");
			return -1;
		}
	}

	for(i=0; i<_evapi_workers; i++) {
		pid=fork_process(PROC_RPC, "EvAPI Worker", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0) {
			/* child */

			/* initialize the config framework */
			if (cfg_child_init())
				return -1;
			/* main function for workers */
			if(evapi_run_worker(i+1)<0) {
				LM_ERR("failed to initialize worker process: %d\n", i);
				return -1;
			}
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
 *
 */
static int w_evapi_relay(sip_msg_t *msg, char *evdata, char *p2)
{
	str sdata;

	if(evdata==0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)evdata, &sdata)!=0) {
		LM_ERR("unable to get data\n");
		return -1;
	}
	if(sdata.s==NULL || sdata.len == 0) {
		LM_ERR("invalid data parameter\n");
		return -1;
	}
	if(evapi_relay(&sdata)<0) {
		LM_ERR("failed to relay event: %.*s\n", sdata.len, sdata.s);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_evapi_async_relay(sip_msg_t *msg, char *evdata, char *p2)
{
	str sdata;
	unsigned int tindex;
	unsigned int tlabel;
	tm_cell_t *t = 0;

	if(evdata==0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(tmb.t_suspend==NULL) {
		LM_ERR("evapi async relay is disabled - tm module not loaded\n");
		return -1;
	}

	t = tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED)
	{
		if(tmb.t_newtran(msg)<0)
		{
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = tmb.t_gett();
		if (t==NULL || t==T_UNDEFINED)
		{
			LM_ERR("cannot lookup the transaction\n");
			return -1;
		}
	}
	if(tmb.t_suspend(msg, &tindex, &tlabel)<0)
	{
		LM_ERR("failed to suppend request processing\n");
		return -1;
	}

	LM_DBG("transaction suspended [%u:%u]\n", tindex, tlabel);

	if(fixup_get_svalue(msg, (gparam_t*)evdata, &sdata)!=0) {
		LM_ERR("unable to get data\n");
		return -1;
	}
	if(sdata.s==NULL || sdata.len == 0) {
		LM_ERR("invalid data parameter\n");
		return -1;
	}

	if(evapi_relay(&sdata)<0) {
		LM_ERR("failed to relay event: %.*s\n", sdata.len, sdata.s);
		return -2;
	}
	return 1;
}

/**
 *
 */
static int fixup_evapi_relay(void** param, int param_no)
{
	return fixup_spve_null(param, param_no);
}

/**
 *
 */
static int w_evapi_close(sip_msg_t* msg, char* p1, char* p2)
{
	int ret;
	ret = evapi_cfg_close(msg);
	if(ret>=0)
		return ret+1;
	return ret;
}

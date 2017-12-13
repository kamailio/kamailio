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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pt.h"
#include "../../core/pvar.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"

#include "../../modules/tm/tm_load.h"

#include "evapi_dispatch.h"

MODULE_VERSION

static int   _evapi_workers = 1;
static char *_evapi_bind_addr = "127.0.0.1";
static int   _evapi_bind_port = 8448;
static char *_evapi_bind_param = NULL;
static int   _evapi_netstring_format_param = 1;

str _evapi_event_callback = STR_NULL;
int _evapi_dispatcher_pid = -1;
int _evapi_max_clients = 8;

static tm_api_t tmb;

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_evapi_relay(sip_msg_t* msg, char* evdata, char* p2);
static int w_evapi_async_relay(sip_msg_t* msg, char* evdata, char* p2);
static int w_evapi_multicast(sip_msg_t* msg, char* evdata, char* ptag);
static int w_evapi_async_multicast(sip_msg_t* msg, char* evdata, char* ptag);
static int w_evapi_unicast(sip_msg_t *msg, char *evdata, char *ptag);
static int w_evapi_async_unicast(sip_msg_t *msg, char *evdata, char *ptag);
static int w_evapi_close(sip_msg_t* msg, char* p1, char* p2);
static int w_evapi_set_tag(sip_msg_t* msg, char* ptag, char* p2);
static int fixup_evapi_relay(void** param, int param_no);
static int fixup_evapi_multicast(void** param, int param_no);

static cmd_export_t cmds[]={
	{"evapi_relay",			(cmd_function)w_evapi_relay,		1, fixup_evapi_relay,
		0, ANY_ROUTE},
	{"evapi_async_relay",	(cmd_function)w_evapi_async_relay, 	1, fixup_evapi_relay,
		0, REQUEST_ROUTE},
	{"evapi_multicast",		(cmd_function)w_evapi_multicast,	2, fixup_evapi_multicast,
		0, ANY_ROUTE},
	{"evapi_async_multicast", (cmd_function)w_evapi_async_multicast,	2, fixup_evapi_multicast,
		0, REQUEST_ROUTE},
	{"evapi_unicast", 		(cmd_function)w_evapi_unicast,		2, fixup_evapi_multicast,
		0, ANY_ROUTE},
	{"evapi_async_unicast", (cmd_function)w_evapi_async_unicast,2, fixup_evapi_multicast,
		0, REQUEST_ROUTE},
	{"evapi_close",       	(cmd_function)w_evapi_close,		0, NULL,
		0, ANY_ROUTE},
	{"evapi_close",       	(cmd_function)w_evapi_close,		1, NULL,
		0, ANY_ROUTE},
	{"evapi_set_tag",       (cmd_function)w_evapi_set_tag,		1, fixup_spve_null,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"workers",           INT_PARAM,   &_evapi_workers},
	{"bind_addr",         PARAM_STRING,   &_evapi_bind_param},
	{"netstring_format",  INT_PARAM,   &_evapi_netstring_format_param},
	{"event_callback",    PARAM_STR,   &_evapi_event_callback},
	{"max_clients",       PARAM_INT,   &_evapi_max_clients},
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
		LM_ERR("failed to init faked sip message\n");
		return -1;
	}

	if(load_tm_api( &tmb ) < 0) {
		LM_INFO("cannot load the TM module functions - async relay disabled\n");
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
		if(_evapi_dispatcher_pid!=getpid()) {
			evapi_close_notify_sockets_parent();
		}
		return 0;
	}

	pid=fork_process(PROC_NOCHLDINIT, "EvAPI Dispatcher", 1);
	if (pid<0)
		return -1; /* error */
	if(pid==0) {
		/* child */
		_evapi_dispatcher_pid = getpid();

		/* do child init to allow execution of rpc like functions */
		if(init_child(PROC_RPC) < 0) {
			LM_DBG("failed to do RPC child init for dispatcher\n");
			return -1;
		}
		/* initialize the config framework */
		if (cfg_child_init())
			return -1;
		/* main function for dispatcher */
		evapi_close_notify_sockets_child();
		if(evapi_run_dispatcher(_evapi_bind_addr, _evapi_bind_port)<0) {
			LM_ERR("failed to initialize evapi dispatcher process\n");
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
		LM_ERR("failed to suspend request processing\n");
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
static int w_evapi_multicast(sip_msg_t *msg, char *evdata, char *ptag)
{
	str sdata;
	str stag;

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
	if(fixup_get_svalue(msg, (gparam_t*)ptag, &stag)!=0) {
		LM_ERR("unable to get tag\n");
		return -1;
	}
	if(stag.s==NULL || stag.len == 0) {
		LM_ERR("invalid tag parameter\n");
		return -1;
	}
	if(evapi_relay_multicast(&sdata, &stag)<0) {
		LM_ERR("failed to relay event: [[%.*s]] to [%.*s] \n",
				sdata.len, sdata.s, stag.len, stag.s);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_evapi_async_multicast(sip_msg_t *msg, char *evdata, char *ptag)
{
	str sdata;
	str stag;
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
		LM_ERR("failed to suspend request processing\n");
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
	if(fixup_get_svalue(msg, (gparam_t*)ptag, &stag)!=0) {
		LM_ERR("unable to get tag\n");
		return -1;
	}
	if(stag.s==NULL || stag.len == 0) {
		LM_ERR("invalid tag parameter\n");
		return -1;
	}

	if(evapi_relay_multicast(&sdata, &stag)<0) {
		LM_ERR("failed to relay event: [[%.*s]] to [%.*s] \n",
				sdata.len, sdata.s, stag.len, stag.s);
		return -2;
	}
	return 1;
}


/**
 *
 */
static int w_evapi_unicast(sip_msg_t *msg, char *evdata, char *ptag)
{
	str sdata;
	str stag;

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
	if(fixup_get_svalue(msg, (gparam_t*)ptag, &stag)!=0) {
		LM_ERR("unable to get tag\n");
		return -1;
	}
	if(stag.s==NULL || stag.len == 0) {
		LM_ERR("invalid tag parameter\n");
		return -1;
	}
	if(evapi_relay_unicast(&sdata, &stag)<0) {
		LM_ERR("failed to relay event: [[%.*s]] to [%.*s] \n",
				sdata.len, sdata.s, stag.len, stag.s);
		return -1;
	}
	return 1;
}


static int w_evapi_async_unicast(sip_msg_t *msg, char *evdata, char *ptag)
{
	str sdata;
	str stag;
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
		LM_ERR("failed to suspend request processing\n");
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
	if(fixup_get_svalue(msg, (gparam_t*)ptag, &stag)!=0) {
		LM_ERR("unable to get tag\n");
		return -1;
	}
	if(stag.s==NULL || stag.len == 0) {
		LM_ERR("invalid tag parameter\n");
		return -1;
	}

	if(evapi_relay_unicast(&sdata, &stag)<0) {
		LM_ERR("failed to relay event: [[%.*s]] to [%.*s] \n",
				sdata.len, sdata.s, stag.len, stag.s);
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
static int fixup_evapi_multicast(void** param, int param_no)
{
	return fixup_spve_spve(param, param_no);
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

/**
 *
 */
static int w_evapi_set_tag(sip_msg_t* msg, char* ptag, char* p2)
{
	str stag;
	if(fixup_get_svalue(msg, (gparam_t*)ptag, &stag)!=0) {
		LM_ERR("no tag name\n");
		return -1;
	}
	if(evapi_set_tag(msg, &stag)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_evapi_relay(sip_msg_t *msg, str *sdata)
{
	return evapi_relay(sdata);
}

/**
 *
 */
static int ki_evapi_relay_unicast(sip_msg_t *msg, str *sdata, str *stag)
{
	return evapi_relay_unicast(sdata, stag);
}

/**
 *
 */
static int ki_evapi_relay_multicast(sip_msg_t *msg, str *sdata, str *stag)
{
	return evapi_relay_multicast(sdata, stag);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_evapi_exports[] = {
	{ str_init("evapi"), str_init("relay"),
		SR_KEMIP_INT, ki_evapi_relay,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("evapi"), str_init("relay_unicast"),
		SR_KEMIP_INT, ki_evapi_relay_unicast,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("evapi"), str_init("relay_multicast"),
		SR_KEMIP_INT, ki_evapi_relay_multicast,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("evapi"), str_init("close"),
		SR_KEMIP_INT, evapi_cfg_close,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("evapi"), str_init("set_tag"),
		SR_KEMIP_INT, evapi_set_tag,
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
	sr_kemi_modules_add(sr_kemi_evapi_exports);
	return 0;
}


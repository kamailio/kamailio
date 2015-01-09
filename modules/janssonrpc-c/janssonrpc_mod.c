/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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

#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>

#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../trim.h"
#include "../../sr_module.h"
#include "../../timer_proc.h"
#include "../tm/tm_load.h"
#include "../jansson/jansson_utils.h"

#include "janssonrpc_funcs.h"
#include "janssonrpc_request.h"
#include "janssonrpc_io.h"
#include "janssonrpc_connect.h"
#include "janssonrpc_server.h"
#include "janssonrpc_srv.h"
#include "janssonrpc.h"


MODULE_VERSION


static int mod_init(void);
static int child_init(int);
void mod_destroy(void);
int parse_server_param(modparam_t type, void* val);
int parse_retry_codes_param(modparam_t type, void* val);
int parse_min_ttl_param(modparam_t type, void* val);
static int fixup_req(void** param, int param_no);
static int fixup_req_free(void** param, int param_no);
static int fixup_notify(void** param, int param_no);
static int fixup_notify_free(void** param, int param_no);
int		  fixup_pvar_shm(void** param, int param_no);

int  pipe_fds[2] = {-1,-1};

struct tm_binds tmb;

/*
 * Exported Functions
 */
int jsonrpc_request_no_options(struct sip_msg* msg,
		char* conn,
		char* method,
		char* params) {
	return jsonrpc_request(msg, conn, method, params, NULL);
}

static cmd_export_t cmds[]={
	{"janssonrpc_request", (cmd_function)jsonrpc_request,
		4, fixup_req, fixup_req_free, ANY_ROUTE},
	{"jsansonrpc_request", (cmd_function)jsonrpc_request_no_options,
		3, fixup_req, fixup_req_free, ANY_ROUTE},
	{"janssonrpc_notification", (cmd_function)jsonrpc_notification,
		3, fixup_notify, fixup_notify_free, ANY_ROUTE},
	{"mod_janssonrpc_request", (cmd_function)mod_jsonrpc_request,
		0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Script Parameters
 */
static param_export_t mod_params[]={
	{"server",      STR_PARAM|USE_FUNC_PARAM, (void*)parse_server_param},
	{"retry_codes",  STR_PARAM|USE_FUNC_PARAM, (void*)parse_retry_codes_param},
	{"min_srv_ttl", INT_PARAM|USE_FUNC_PARAM, (void*)parse_min_ttl_param},
	{"result_pv",   STR_PARAM,                &result_pv_str.s},
	{ 0,0,0 }
};

/*
 * Exports
 */
struct module_exports exports = {
		"janssonrpc-c",       /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,            /* Exported functions */
		mod_params,      /* Exported parameters */
		0,               /* exported statistics */
		0,               /* exported MI functions */
		0,               /* exported pseudo-variables */
		0,               /* extra processes */
		mod_init,        /* module initialization function */
		0,               /* response function*/
		mod_destroy,     /* destroy function */
		child_init       /* per-child init function */
};


static int mod_init(void)
{
	/* load the tm functions  */
	if(load_tm_api(&tmb)<0) return -1;

	/* load json_to_val from json module */
	jsontoval = (jansson_to_val_f)find_export("jansson_to_val", 0, 0);
	if(jsontoval == 0) {
		ERR("ERROR:jsonrpc:mod_init: cannot import json_to_val\n");
		return -1;
	}

	/* setup result pvar */
	if (result_pv_str.s == NULL)
		result_pv_str.s = JSONRPC_RESULT_STR;
	result_pv_str.len = strlen(result_pv_str.s);

	if(pv_parse_spec(&result_pv_str, &jsonrpc_result_pv)<0) {
		ERR("cannot parse result_pv: %.*s\n", STR(result_pv_str));
		return -1;
	}

	if(!(pv_is_w(&jsonrpc_result_pv))) {
		ERR("%.*s is read only\n", STR(result_pv_str));
		return -1;
	}

	register_procs(1);
	register_basic_timers(1);

	if (pipe(pipe_fds) < 0) {
		ERR("pipe() failed\n");
		return -1;
	}

	if(jsonrpc_min_srv_ttl < ABSOLUTE_MIN_SRV_TTL) {
		jsonrpc_min_srv_ttl = JSONRPC_DEFAULT_MIN_SRV_TTL; /* 5s */
	}

	return 0;
}

static int child_init(int rank)
{
	int pid;

	if (rank>PROC_MAIN)
		cmd_pipe = pipe_fds[1];

	if (rank!=PROC_MAIN)
		return 0;

	jsonrpc_server_group_lock = lock_alloc();
	if(jsonrpc_server_group_lock == NULL) {
		ERR("cannot allocate the server_group_lock\n");
		return -1;
	}

	if(lock_init(jsonrpc_server_group_lock) == 0) {
		ERR("failed to initialized the server_group_lock\n");
		lock_dealloc(jsonrpc_server_group_lock);
		return -1;
	}

	srv_cb_params_t* params = (srv_cb_params_t*)shm_malloc(sizeof(srv_cb_params_t));
	CHECK_MALLOC(params);
	params->cmd_pipe = pipe_fds[1];
	params->srv_ttl = jsonrpc_min_srv_ttl;

	/* start timer to check SRV ttl every second */
	if(fork_basic_timer(PROC_TIMER, "jsonrpc SRV timer", 1 /*socks flag*/,
				refresh_srv_cb, (void*)params, ABSOLUTE_MIN_SRV_TTL)<0) {
		ERR("Failed to start SRV timer\n");
		return -1;
	}

	pid=fork_process(PROC_RPC, "jsonrpc io handler", 1);

	if (pid<0)
		return -1; /* error */
	if(pid==0){
		/* child */
		close(pipe_fds[1]);
		return jsonrpc_io_child_process(pipe_fds[0]);
	}

	return 0;
}

void mod_destroy(void)
{
	lock_get(jsonrpc_server_group_lock); /* blocking */
	if(jsonrpc_server_group_lock) lock_dealloc(jsonrpc_server_group_lock);

	free_server_group(global_server_group);
	CHECK_AND_FREE(global_server_group);
}

int parse_server_param(modparam_t type, void* val)
{
	if(global_server_group == NULL) {
		global_server_group = shm_malloc(sizeof(void*));
		*global_server_group = NULL;
	}
	return jsonrpc_parse_server((char*)val, global_server_group);
}

/* helper function for parse_retry_codes_param */
int s2i(char* str, int* result)
{
	char* endptr;
	errno = 0;

	long val = strtol(str, &endptr, 10);

	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
			|| (errno != 0 && val == 0)) {
		ERR("%s is not a number: %s\n", str, strerror(errno));
		return -1;
	}

	if (endptr == str) {
		ERR("failed to convert %s to integer\n", str);
		return -1;
	}

	*result = (int)val;
	return 0;
}

int parse_retry_codes_param(modparam_t type, void* val)
{
	if (val==NULL) {
		ERR("retry_codes cannot be NULL!\n");
		return -1;
	}

	if (PARAM_TYPE_MASK(type) != STR_PARAM) {
		ERR("retry_codes must be a string\n");
		return -1;
	}

	global_retry_ranges = NULL;

	char* save_comma;
	char* save_elipse;
	char* token;
	char* start_s;
	int start;
	char* end_s;
	int end;
	char* codes_s = (char*)val;

	char* tmp;
	retry_range_t** tmp_range;
	tmp_range = &global_retry_ranges;
	for (tmp = codes_s; ; tmp = NULL) {
		token = strtok_r(tmp, ",", &save_comma);
		if (token == NULL)
			break;

		start_s = strtok_r(token, ".", &save_elipse);
		if (start_s == NULL) {
			continue;
		}

		if(s2i(start_s, &start)<0) return -1;

		*tmp_range = shm_malloc(sizeof(retry_range_t));
		CHECK_MALLOC(*tmp_range);
		memset(*tmp_range, 0, sizeof(retry_range_t));

		(*tmp_range)->start = start;

		end_s = strtok_r(NULL, ".", &save_elipse);
		if (end_s == NULL) {
			end_s = start_s;
		}

		if(s2i(end_s, &end)<0) return -1;
		(*tmp_range)->end = end;

		tmp_range = &((*tmp_range)->next);
	}

	return 0;
}

int parse_min_ttl_param(modparam_t type, void* val)
{
	if (val==0) {
		ERR("min_srv_ttl cannot be NULL!\n");
		return -1;
	}

	if (PARAM_TYPE_MASK(type) != INT_PARAM) {
		ERR("min_srv_ttl must be of type %d, not %d!\n", INT_PARAM, type);
		return -1;
	}

	jsonrpc_min_srv_ttl = (int)(long)val;
	if(jsonrpc_min_srv_ttl < ABSOLUTE_MIN_SRV_TTL) {
		ERR("Cannot set min_srv_ttl lower than %d", ABSOLUTE_MIN_SRV_TTL);
		return -1;
	}

	INFO("min_srv_ttl set to %d\n", jsonrpc_min_srv_ttl);

	return 0;
}

/* Fixup Functions */

static int fixup_req(void** param, int param_no)
{
	if (param_no <= 4) {
		return fixup_spve_null(param, 1);
	}
	ERR("function takes at most 4 parameters.\n");
	return -1;
}

static int fixup_req_free(void** param, int param_no)
{
	if (param_no <= 4) {
		return fixup_free_spve_null(param, 1);
	}
	ERR("function takes at most 4 parameters.\n");
	return -1;
}

static int fixup_notify(void** param, int param_no)
{
	if (param_no <= 3) {
		return fixup_spve_null(param, 1);
	}
	ERR("function takes at most 3 parameters.\n");
	return -1;
}

static int fixup_notify_free(void** param, int param_no)
{
	if (param_no <= 3) {
		return fixup_free_spve_null(param, 1);
	}
	ERR("function takes at most 3 parameters.\n");
	return -1;
}

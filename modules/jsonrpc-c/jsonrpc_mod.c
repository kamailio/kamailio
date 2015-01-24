/*
 *
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
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

#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>

#include "../../mod_fix.h"
#include "../../trim.h"
#include "../../sr_module.h"
#include "../tm/tm_load.h"

#include "jsonrpc_request.h"
#include "jsonrpc_io.h"
#include "jsonrpc.h"


MODULE_VERSION


static int mod_init(void);
static int child_init(int);
static int fixup_request(void** param, int param_no);
static int fixup_notification(void** param, int param_no);
static int fixup_request_free(void** param, int param_no);
int        fixup_pvar_shm(void** param, int param_no);

char *servers_param;
int  pipe_fds[2] = {-1,-1};

struct tm_binds tmb;

/*
 * Exported Functions
 */
static cmd_export_t cmds[]={
	{"jsonrpc_request", (cmd_function)jsonrpc_request, 5, fixup_request, fixup_request_free, ANY_ROUTE},
	{"jsonrpc_notification", (cmd_function)jsonrpc_notification, 2, fixup_notification, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};
 

/*
 * Script Parameters
 */
static param_export_t mod_params[]={
	{"servers", PARAM_STRING, &servers_param},
	{ 0,0,0 }
};

 
/*
 * Exports
 */
struct module_exports exports = {
		"jsonrpc",           /* module name */
		DEFAULT_DLFLAGS,     /* dlopen flags */
		cmds,                /* Exported functions */
		mod_params,          /* Exported parameters */
		0,                   /* exported statistics */
		0,                   /* exported MI functions */
		0,                   /* exported pseudo-variables */
		0,                   /* extra processes */
		mod_init,            /* module initialization function */
		0,                   /* response function*/
		0,                   /* destroy function */
		child_init           /* per-child init function */
};


static int mod_init(void) {
	load_tm_f  load_tm;

	/* load the tm functions  */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0)))
	{
		LOG(L_ERR, "ERROR:jsonrpc:mod_init: cannot import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1)
		return -1;

	if (servers_param == NULL) {
		LM_ERR("servers parameter missing.\n");
		return -1;
	}

	register_procs(1);

	if (pipe(pipe_fds) < 0) {
		LM_ERR("pipe() failed\n");
		return -1;
	}
	
	return(0);
}

static int child_init(int rank) 
{
	int pid;
	
	if (rank>PROC_MAIN)
		cmd_pipe = pipe_fds[1];

	if (rank!=PROC_MAIN)
		return 0;

	pid=fork_process(PROC_NOCHLDINIT, "jsonrpc io handler", 1);
	if (pid<0)
		return -1; /* error */
	if(pid==0){
		/* child */
		close(pipe_fds[1]);
		return jsonrpc_io_child_process(pipe_fds[0], servers_param);
	}
	return 0;
}

/* Fixup Functions */

static int fixup_request(void** param, int param_no)
{
  if (param_no <= 4) {
		return fixup_spve_null(param, 1);
	} else if (param_no == 5) {
		return fixup_pvar_null(param, 1);
	}
	LM_ERR("jsonrpc_request takes exactly 5 parameters.\n");
	return -1;
}

static int fixup_notification(void** param, int param_no)
{
  if (param_no <= 2) {
		return fixup_spve_null(param, 1);
	}
	LM_ERR("jsonrpc_notification takes exactly 2 parameters.\n");
	return -1;
}

static int fixup_request_free(void** param, int param_no)
{
  if (param_no <= 4) {
		return 0;
	} else if (param_no == 5) {
		return fixup_free_pvar_null(param, 1);
	}
	LM_ERR("jsonrpc_request takes exactly 5 parameters.\n");
	return -1;
}

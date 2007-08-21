/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 */




#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <grp.h>
#include <stdlib.h>
#include "mi_xmlrpc.h"
#include "xr_writer.h"
#include "xr_parser.h"
#include "xr_server.h"
#define XMLRPC_SERVER_WANT_ABYSS_HANDLERS
#include "abyss.h"
#include <xmlrpc_abyss.h>
#include "../../sr_module.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

xmlrpc_env env;
xmlrpc_value * xr_response;
int rpl_opt = 0;

/* module functions */
static int mod_init();
static int destroy(void);
static void xmlrpc_process(int rank);

static int port = 8080;
static char *log_file = NULL; 
static int read_buf_size = MAX_READ;
static TServer srv;
MODULE_VERSION


static proc_export_t mi_procs[] = {
	{"MI XMLRPC",  0,  0, xmlrpc_process, 1 },
	{0,0,0}
};


/* module parameters */
static param_export_t mi_params[] = {
	{"port",					INT_PARAM, &port},
	{"log_file",				STR_PARAM, &log_file},
	{"reply_option",			INT_PARAM, &rpl_opt},
	{"buffer_size",				INT_PARAM, &read_buf_size},
	{0,0,0}
};

/* module exports */
struct module_exports exports = {
	"mi_xmlrpc",                        /* module name */
	DEFAULT_DLFLAGS,                    /* dlopen flags */
	0,                                  /* exported functions */
	mi_params,                          /* exported parameters */
	0,                                  /* exported statistics */
	0,                                  /* exported MI functions */
	0,                                  /* exported PV */
	mi_procs,                           /* extra processes */
	mod_init,                           /* module initialization function */
	(response_function) 0,              /* response handling function */
	(destroy_function) destroy,         /* destroy function */
	0                                   /* per-child init function */
};


static int mod_init(void)
{
	DBG("DBG: mi_xmlrpc: mod_init: Testing port number...\n");

	if ( port <= 1024 ) {
		LOG(L_WARN,"WARNING: mi_xmlrpc: mod_init: port<1024, using 8080...\n");
		port = 8080;
	}

	if (init_async_lock()!=0) {
		LOG(L_ERR, "ERROR mi_xmlrpc: mod_init: failed to init async lock\n");
		return -1;
	}

	return 0;
}


static void xmlrpc_sigchld( int sig )
{
	pid_t pid;
	int status;

	while(1) {
		pid = waitpid( (pid_t) -1, &status, WNOHANG );

		/* none left */
		if ( pid == 0 )
			break;

		if (pid<0) {
			/* because of ptrace */
			if ( errno == EINTR )
				continue;

			break;
		}
	}
}


static void xmlrpc_process(int rank)
{
	/* install handler to catch termination of child processes */
	if (signal(SIGCHLD, xmlrpc_sigchld)==SIG_ERR) {
		LOG(L_ERR,"ERROR: mi_xmlrpc: mod_child_init: failed to install "
			"signal handler for SIGCHLD\n");
		goto error;
	}

	/* Server Abyss init */
	xmlrpc_server_abyss_init_registry();
	DateInit();
	MIMETypeInit();

	if (!ServerCreate(&srv, "XmlRpcServer", port, "", log_file)) {
		LOG(L_ERR,"ERROR: mi_xmlrpc: mod_child_init: failed to create XMLRPC "
			"server\n");
		goto error;
	}

	if (!ServerAddHandler(&srv, xmlrpc_server_abyss_rpc2_handler)) {
		LOG(L_ERR,"ERROR: mi_xmlrpc: mod_child_init: failed to add handler "
			"to server\n");
		goto error;
	}

	ServerDefaultHandler(&srv, xmlrpc_server_abyss_default_handler);
	ServerInit(&srv);

	if( init_mi_child() != 0 ) {
		LOG(L_CRIT, "CRITICAL: mi_xmlrpc: mod_child_init: Failed to init "
			"the mi process\n");
		goto error;
	}

	if ( xr_writer_init(read_buf_size) != 0 ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: mod_child_init: Failed to init the "
			"reply writer\n");
		goto error;
	}

	xmlrpc_env_init(&env);

	if ( rpl_opt == 1 ) {
		xr_response = xmlrpc_build_value(&env, "()");
		if ( env.fault_occurred ){
			LOG(L_ERR, "ERROR: mi_xmlrpc: mod_child_init: Failed to create "
				"and emtpy array: %s\n", env.fault_string);
			goto cleanup;
		}
	}

	if ( set_default_method(&env) != 0 ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: mod_child_init: Failed to set up the "
			"default method!\n");
		goto cleanup;
	}

	/* Run server abyss */
	LOG(L_INFO, "INFO: mi_xmlrpc: mod_child_init: Starting xmlrpc server "
		"on (%d)\n", getpid());

	ServerRun(&srv);

	LOG(L_CRIT, "CRITICAL: mi_xmlrpc: mod_child_init: Server terminated!!!\n");

cleanup:
	xmlrpc_env_clean(&env);
	if ( xr_response ) xmlrpc_DECREF(xr_response);
error:
	exit(-1);
}


int destroy(void)
{
	DBG("DBG: mi_xmlrpc: destroy: Destroying module ...\n");

	destroy_async_lock();

	return 0;
}


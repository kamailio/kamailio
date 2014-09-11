/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 *  2007-10-05  support for libxmlrpc-c3 version 1.x.x added (dragos)
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

#ifdef XMLRPC_OLD_VERSION

#include "abyss.h"
#include <xmlrpc_abyss.h>
#include <xmlrpc.h>

#else

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#endif


#include "../../sr_module.h"
#include "../../str.h"
#include "../../pt.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../cfg/cfg_struct.h"

xmlrpc_env env;
xmlrpc_value * xr_response;
xmlrpc_registry * registryP;

int rpl_opt = 0;

/* module functions */
static int mod_init();
static void destroy(void);
static int child_init(int rank);
static void xmlrpc_process(int rank);

static int port = 8080;
static char *log_file = NULL; 
static int read_buf_size = MAX_READ;
static TServer srv;
MODULE_VERSION


static proc_export_t mi_procs[] = {
	{"MI XMLRPC",  0,  0, xmlrpc_process, 1 },
	{0,0,0,0,0}
};


/* module parameters */
static param_export_t mi_params[] = {
	{"port",					INT_PARAM, &port},
	{"log_file",				PARAM_STRING, &log_file},
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
	0,                                  /* response handling function */
	destroy,                            /* destroy function */
	child_init                          /* per-child init function */
};


static int mod_init(void)
{
	LM_DBG("testing port number...\n");

	if ( port <= 1024 ) {
		LM_WARN("port<1024, using 8080...\n");
		port = 8080;
	}

	if (init_async_lock()!=0) {
		LM_ERR("failed to init async lock\n");
		return -1;
	}

	/* add space for extra processes */
	register_procs(1);

	/* add child to update local config framework structures */
	cfg_register_child(1);

	return 0;
}

static int child_init(int rank)
{
	int pid;
	if (rank==PROC_MAIN) {
		pid=fork_process(PROC_NOCHLDINIT, "MI XMLRPC", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0){
			/* child */

			/* initialize the config framework */
			if (cfg_child_init())
				return -1;

			xmlrpc_process(1);
		}
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
		#ifndef XMLRPC_OLD_VERSION
		else 
			ServerHandleSigchld(pid);
		#endif
	}
}


static void xmlrpc_process(int rank)
{
	/* install handler to catch termination of child processes */
	if (signal(SIGCHLD, xmlrpc_sigchld)==SIG_ERR) {
		LM_ERR("failed to install signal handler for SIGCHLD\n");
		goto error;
	}

	/* Server Abyss init */

	xmlrpc_env_init(&env);

	#ifdef XMLRPC_OLD_VERSION
	xmlrpc_server_abyss_init_registry();
	registryP= xmlrpc_server_abyss_registry();
	#else
	registryP = xmlrpc_registry_new(&env);    
	#endif

	DateInit();
	MIMETypeInit();

	if (!ServerCreate(&srv, "XmlRpcServer", port, "", log_file)) {
		LM_ERR("failed to create XMLRPC server\n");
		goto error;
	}

	#ifdef XMLRPC_OLD_VERSION
	if (!ServerAddHandler(&srv, xmlrpc_server_abyss_rpc2_handler)) {
		LM_ERR("failed to add handler to server\n");
		goto error;
	}

	ServerDefaultHandler(&srv, xmlrpc_server_abyss_default_handler);

	#else

	xmlrpc_server_abyss_set_handlers2(&srv, "/RPC2", registryP);

	#endif

	ServerInit(&srv);

	if( init_mi_child(PROC_NOCHLDINIT, 1) != 0 ) {
		LM_CRIT("failed to init the mi process\n");
		goto error;
	}

	if ( xr_writer_init(read_buf_size) != 0 ) {
		LM_ERR("failed to init the reply writer\n");
		goto error;
	}
	#ifdef XMLRPC_OLD_VERSION
	xmlrpc_env_init(&env);
	#endif

	if ( rpl_opt == 1 ) {
		xr_response = xmlrpc_build_value(&env, "()");
		if ( env.fault_occurred ){
			LM_ERR("failed to create an empty array: %s\n", env.fault_string);
			goto cleanup;
		}
	}

	if ( set_default_method(&env,registryP) != 0 ) {
		LM_ERR("failed to set up the default method!\n");
		goto cleanup;
	}

	/* Run server abyss */
	LM_INFO("starting xmlrpc server\n");

	ServerRun(&srv);

	LM_CRIT("Server terminated!!!\n");

cleanup:
	xmlrpc_env_clean(&env);
	if ( xr_response ) xmlrpc_DECREF(xr_response);
error:
	exit(-1);
}


static void destroy(void)
{
	destroy_async_lock();
}

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
static int mod_child_init(int rank);
static int destroy(void);

static pid_t * mi_xmlrpc_pid = 0;
static int port = 8080;
static char *log_file = NULL; 
static int read_buf_size = MAX_READ;
static TServer srv;
MODULE_VERSION

/* module parameters */
static param_export_t mi_params[] = {
	{"port",        				INT_PARAM, &port},
	{"log_file",       				STR_PARAM, &log_file},
	{"reply_option",        		INT_PARAM, &rpl_opt},
	{"buffer_size", 				INT_PARAM, &read_buf_size},
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
	0,                                  /* extra processes */
	mod_init,                           /* module initialization function */
	(response_function) 0,              /* response handling function */
	(destroy_function) destroy,         /* destroy function */
	mod_child_init                      /* per-child init function */
};

static int mod_init(void)
{
	DBG("DBG: mi_xmlrpc: mod_init: Testing port number...\n");

	if ( port <= 1024 ) {
		LOG(L_WARN,"WARNING: mi_xmlrpc: mod_init: port<1024, using 8080...\n");
		port = 8080;
	}

	mi_xmlrpc_pid = (pid_t*) shm_malloc ( sizeof(pid_t) );
	if ( mi_xmlrpc_pid == 0 ) {
		LOG(L_ERR, "ERROR mi_xmlrpc: mod_init: failed to init shm mem for "
			"mi_xmlrpc_pid\n");
		return -1;
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

static int mod_child_init( int rank )
{
	if ( rank != 1 )
		return 0;
	
	*mi_xmlrpc_pid = fork();
	
	if ( *mi_xmlrpc_pid < 0 ){
		LOG(L_ERR, "ERROR: mi_xmlrpc: mod_child_init: The process cannot "
			"fork!\n");
		return -1;
	} else if ( *mi_xmlrpc_pid ) {
		LOG(L_INFO, "INFO: mi_xmlrpc: mod_child_init: XMLRPC listener "
			"process created (pid: %d)\n", *mi_xmlrpc_pid);
		return 0;
	}

	/* install handler to catch termination of child processes */
	if (signal(SIGCHLD, xmlrpc_sigchld)==SIG_ERR) {
		LOG(L_ERR,"ERROR: mi_xmlrpc: mod_child_init: failed to install "
			"signal handler for SIGCHLD\n");
		return -1;
	}

	/* Server Abyss init */
	xmlrpc_server_abyss_init_registry();
	DateInit();
	MIMETypeInit();

	if (!ServerCreate(&srv, "XmlRpcServer", port, "", log_file)) {
		LOG(L_ERR,"ERROR: mi_xmlrpc: mod_child_init: failed to create XMLRPC "
			"server\n");
		return -1;
	}

	if (!ServerAddHandler(&srv, xmlrpc_server_abyss_rpc2_handler)) {
		LOG(L_ERR,"ERROR: mi_xmlrpc: mod_child_init: failed to add handler "
			"to server\n");
		return -1;
	}

	ServerDefaultHandler(&srv, xmlrpc_server_abyss_default_handler);
	ServerInit(&srv);

	if( init_mi_child() != 0 ) {
		LOG(L_CRIT, "CRITICAL: mi_xmlrpc: mod_child_init: Failed to init "
			"the mi process\n");
		return -1;
	}

	if ( xr_writer_init(read_buf_size) != 0 ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: mod_child_init: Failed to init the "
			"reply writer\n");
		return -1;
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
	return -1;
}

int destroy(void)
{
	DBG("DBG: mi_xmlrpc: destroy: Destroying module ...\n");

	if ( mi_xmlrpc_pid == 0 ) {
		LOG(L_INFO, "INFO: mi_xmlrpc: destroy: Process hasn't been created "
			"-> nothing to kill\n");
	} else {
		if ( *mi_xmlrpc_pid != 0 ) {
			if ( kill(*mi_xmlrpc_pid, SIGKILL) != 0 ) {
				if ( errno == ESRCH ) {
					LOG(L_INFO, "INFO: mi_xmlrpc: destroy: seems that xmlrpc"
						" process is already dead!\n");
				} else {
					LOG(L_ERR, "ERROR: mi_xmlrpc: destroy: killing the aux. "
						"process failed! kill said: %s\n", strerror(errno));
				}
			} else {
				LOG(L_INFO, "INFO: mi_xmlrpc: destroy: xmlrpc child "
					"successfully killed!");
			}
		}
		shm_free(mi_xmlrpc_pid);
	}

	destroy_async_lock();

	return 0;
}


/*
 * $Id$
 *
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2010-03-29  initial version (mariusbucur)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../usr_avp.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../pt.h"
#include "../../lib/kmi/mi.h"
#include "../../lib/kcore/hash_func.h"
#include "dmq.h"
#include "bind_dmq.h"
#include "dmq_worker.h"
#include "../../mod_fix.h"

static int mi_child_init(void);
static int mod_init(void);
static int child_init(int);
static void destroy(void);

MODULE_VERSION

/* database connection */
int library_mode = 0;
str server_address = {0, 0};
int startup_time = 0;
int pid = 0;

/* module parameters */
int num_workers = DEFAULT_NUM_WORKERS;

/* TM bind */
struct tm_binds tmb;
/* SL API structure */
sl_api_t slb;

/** module variables */
dmq_worker_t* workers;

/** module functions */
static int mod_init(void);
static int child_init(int);
static void destroy(void);
static int fixup_dmq(void** param, int param_no);

static cmd_export_t cmds[] = {
	{"handle_dmq_message",  (cmd_function)handle_dmq_message, 0, fixup_dmq, 0, 
		REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"num_workers", INT_PARAM, &num_workers},
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{"cleanup", 0, 0, 0, mi_child_init},
	{0, 0, 0, 0, 0}
};

/** module exports */
struct module_exports exports = {
	"dmq",				/* module name */
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,				/* exported statistics */
	mi_cmds,   			/* exported MI functions */
	0,				/* exported pseudo-variables */
	0,				/* extra processes */
	mod_init,			/* module initialization function */
	0,   				/* response handling function */
	(destroy_function) destroy, 	/* destroy function */
	child_init                  	/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void) {
	int i = 0;
	
	if(register_mi_mod(exports.name, mi_cmds)!=0) {
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if(library_mode== 1) {
		LM_DBG("dmq module used for API library purpose only\n");
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1) {
		LM_ERR("Can't load tm functions. Module TM not loaded?\n");
		return -1;
	}
	
	/* fork worker processes */
	workers = shm_malloc(num_workers * sizeof(*workers));
	for(i = 0; i < num_workers; i++) {
		int newpid = fork_process(PROC_NOCHLDINIT, "DMQ WORKER", 0);
		if(newpid < 0) {
			LM_ERR("failed to form process\n");
			return -1;
		} else if(newpid == 0) {
			/* child */
			// worker loop
		} else {
			workers[i].pid = newpid;
		}
	}
	
	startup_time = (int) time(NULL);
	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank) {
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN) {
		/* do nothing for the main process */
		return 0;
	}

	pid = my_pid();
	
	if(library_mode)
		return 0;

	return 0;
}

static int mi_child_init(void) {
	return 0;
}


/*
 * destroy function
 */
static void destroy(void) {
}

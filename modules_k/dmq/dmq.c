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
#include "peer.h"
#include "bind_dmq.h"
#include "worker.h"
#include "notification_peer.h"
#include "dmqnode.h"
#include "../../mod_fix.h"

static int mod_init(void);
static int child_init(int);
static void destroy(void);

MODULE_VERSION

int startup_time = 0;
int pid = 0;

/* module parameters */
int num_workers = DEFAULT_NUM_WORKERS;
str dmq_server_address = {0, 0};

/* TM bind */
struct tm_binds tmb;
/* SL API structure */
sl_api_t slb;

/** module variables */
str dmq_request_method = {"KDMQ", 4};
dmq_worker_t* workers;
dmq_peer_list_t* peer_list;
/* the list of dmq servers */
dmq_node_list_t* node_list;
// the dmq module is a peer itself for receiving notifications regarding nodes
dmq_peer_t dmq_notification_peer;

/** module functions */
static int mod_init(void);
static int child_init(int);
static void destroy(void);
static int handle_dmq_fixup(void** param, int param_no);
static int check_dmq_server_address();

static cmd_export_t cmds[] = {
	{"handle_dmq_message",  (cmd_function)handle_dmq_message, 0, handle_dmq_fixup, 0, 
		REQUEST_ROUTE},
	{"bind_dmq",        (cmd_function)bind_dmq, 0, 0, 0,
		REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"num_workers", INT_PARAM, &num_workers},
	{"server_address", STR_PARAM, &dmq_server_address.s},
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
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
	
	if(register_mi_mod(exports.name, mi_cmds)!=0) {
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}
	
	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1) {
		LM_ERR("can't load tm functions. TM module probably not loaded\n");
		return -1;
	}
	/* load peer list - the list containing the module callbacks for dmq */
	
	peer_list = init_peer_list();
	
	/* load the dmq node list - the list containing the dmq servers */
	node_list = init_dmq_node_list();
	
	/* register worker processes - add one because of the ping process */
	register_procs(num_workers);
	/* check server_address not empty and correct */
	
	if(check_dmq_server_address() < 0) {
		LM_ERR("server address invalid\n");
		return -1;
	}
	
	/* allocate workers array */
	workers = shm_malloc(num_workers * sizeof(*workers));
	if(workers == NULL) {
		LM_ERR("error in shm_malloc\n");
		return -1;
	}
	
	/* add first dmq peer - the dmq module itself to receive peer notify messages */
	
	startup_time = (int) time(NULL);
	return 0;
}

/**
 * initialize children
 */
static int child_init(int rank) {
  	int i, newpid;
	if (rank == PROC_MAIN) {
		/* fork worker processes */
		for(i = 0; i < num_workers; i++) {
			init_worker(&workers[i]);
			LM_DBG("starting worker process %d\n", i);
			newpid = fork_process(PROC_NOCHLDINIT, "DMQ WORKER", 0);
			if(newpid < 0) {
				LM_ERR("failed to form process\n");
				return -1;
			} else if(newpid == 0) {
				// child - this will loop forever
				worker_loop(i);
			} else {
				workers[i].pid = newpid;
			}
		}
		/**
		 * add the dmq notification peer.
		 * the dmq is a peer itself so that it can receive node notifications
		 */
		add_notification_peer();
		return 0;
	}
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN) {
		/* do nothing for the main process */
		return 0;
	}

	pid = my_pid();
	return 0;
}

/*
 * destroy function
 */
static void destroy(void) {
}

static int handle_dmq_fixup(void** param, int param_no) {
 	return 0;
}

static int check_dmq_server_address() {
	if(!dmq_server_address.s) {
		return -1;
	}
	dmq_server_address.len = strlen(dmq_server_address.s);
	if(!dmq_server_address.len) {
		LM_ERR("empty server address\n");
		return -1;
	}
	if(strncmp(dmq_server_address.s, "sip:", 4)) {
		LM_ERR("server address must start with sip:\n");
		return -1;
	}
	if(!strchr(dmq_server_address.s + 4, ':')) {
		LM_ERR("server address must be of form sip:host:port\n");
		return -1;
	}
	return 0;
}
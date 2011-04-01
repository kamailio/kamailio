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

#include "../../lib/srdb1/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h" 
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../usr_avp.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../pt.h"
#include "../../lib/kmi/mi.h"
#include "../../lib/kcore/hash_func.h"
#include "../pua/hash.h"
#include "dmq.h"
#include "bind_dmq.h"
#include "../../mod_fix.h"

static int mi_child_init(void);
static int mod_init(void);
static int child_init(int);
static void destroy(void);

MODULE_VERSION

/* database connection */
db1_con_t *dmq_db = NULL;
db_func_t dmq_dbf;
int library_mode = 0;
str server_address = {0, 0};
int startup_time = 0;
int pid = 0;

/* module parameters */
str db_url;

/* TM bind */
struct tm_binds tmb;
/* SL API structure */
sl_api_t slb;

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
	{"db_url", STR_PARAM, &db_url.s},
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
	if(register_mi_mod(exports.name, mi_cmds)!=0) {
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len,db_url.s);
	if(db_url.s== NULL) {
		library_mode= 1;
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
	
	if(db_url.s== NULL) {
		LM_ERR("database url not set!\n");
		return -1;
	}

	/* binding to database module  */
	if (db_bind_mod(&db_url, &dmq_dbf)) {
		LM_ERR("database module not found\n");
		return -1;
	}
	

	if (!DB_CAPABILITY(dmq_dbf, DB_CAP_ALL)) {
		LM_ERR("database module does not implement all functions needed by dmq module\n");
		return -1;
	}

	dmq_db = dmq_dbf.init(&db_url);
	if (!dmq_db) {
		LM_ERR("connection to database failed\n");
		return -1;
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

	if (dmq_dbf.init==0) {
		LM_CRIT("child_init: database not bound\n");
		return -1;
	}
	if (dmq_db)
		return 0;
	
	dmq_db = dmq_dbf.init(&db_url);
	if (!dmq_db) {
		LM_ERR("child %d: unsuccessful connecting to database\n", rank);
		return -1;
	}
	
	LM_DBG("child %d: database connection opened successfully\n", rank);
	return 0;
}

static int mi_child_init(void) {
	if(library_mode)
		return 0;

	if (dmq_dbf.init==0) {
		LM_CRIT("database not bound\n");
		return -1;
	}
	if (dmq_db)
		return 0;
	dmq_db = dmq_dbf.init(&db_url);
	if (!dmq_db) {
		LM_ERR("connecting database\n");
		return -1;
	}
	
	LM_DBG("database connection opened successfully\n");
	return 0;
}


/*
 * destroy function
 */
static void destroy(void) {
}

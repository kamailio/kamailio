/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * SPEEDDIAL SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * SPEEDDIAL SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *
 */


#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../lib/srdb2/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../modules/sl/sl.h"
#include "sdlookup.h"
#include "speeddial.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);

static int sd_lookup_fixup(void** param, int param_no);


/* Module parameter variables */
char* db_url           = DEFAULT_RODB_URL;
char* uid_column            = "uid";
char* dial_username_column  = "dial_username";
char* dial_did_column       = "dial_did";
char* new_uri_column        = "new_uri";

db_ctx_t* db=NULL;

struct db_table_name* tables = NULL;
unsigned int tables_no = 0;


sl_api_t slb;


/* Exported functions */
static cmd_export_t cmds[] = {
	{"sd_lookup",         sd_lookup, 1, sd_lookup_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"db_url",               PARAM_STRING, &db_url               },
	{"uid_column",           PARAM_STRING, &uid_column           },
	{"dial_username_column", PARAM_STRING, &dial_username_column },
	{"dial_did_column",      PARAM_STRING, &dial_did_column      },
	{"new_uri_column",       PARAM_STRING, &new_uri_column       },
	{0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
	"speeddial",
	cmds,       /* Exported functions */
	0,          /* RPC params */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* child initialization function */
};


static int build_db_cmds(void)
{
	int i;

	db_fld_t match[] = {
		{.name = uid_column,           .type = DB_STR},
		{.name = dial_did_column,      .type = DB_STR},
		{.name = dial_username_column, .type = DB_STR},
		{.name = NULL}
	};
	
	db_fld_t cols[] = {
		{.name = new_uri_column, .type = DB_STR},
		{.name = NULL}
	};

	for(i = 0; i < tables_no; i++) {
		tables[i].lookup_num = db_cmd(DB_GET, db, tables[i].table, cols, match, NULL);
		if (tables[i].lookup_num == NULL) {
			ERR("speeddial: Error while preparing database commands\n");
			goto error;
		}
	}

	return 0;

 error:
	i--;
	while(i >= 0) {
		db_cmd_free(tables[i].lookup_num);
		tables[i].lookup_num = NULL;
		i--;
	}
	return -1;
}



/**
 *
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */

	db = db_ctx("speeddial");
	if (db == NULL) {
		ERR("Error while initializing database layer\n");
		return -1;
	}

	if (db_add_db(db, db_url) < 0) return -1;
	if (db_connect(db) < 0) return -1;

	if (build_db_cmds() < 0) {
		pkg_free(tables);
		return -1;
	}

	return 0;

}


/**
 *
 */
static int mod_init(void)
{
	DBG("speeddial module - initializing\n");

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	return 0;
}


/**
 *
 */
static void destroy(void)
{
	int i;

	if (tables) {
		for(i = 0; i < tables_no; i++) {
			if (tables[i].lookup_num) db_cmd_free(tables[i].lookup_num);
		}
		pkg_free(tables);
	}
}


static int sd_lookup_fixup(void** param, int param_no)
{
	struct db_table_name* ptr;

	if (param_no == 1) {
		ptr = pkg_realloc(tables, sizeof(struct db_table_name) * (tables_no + 1));
		if (ptr == NULL) {
			ERR("No memory left\n");
			return -1;
		}
		ptr[tables_no].table = (char*)*param;
		ptr[tables_no].lookup_num = NULL;
		*param = (void*)(long)tables_no;
		tables_no++;
		tables = ptr;
	}
	return 0;
}

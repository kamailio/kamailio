/* domain_mod.c v 0.1 2002/12/27
 *
 * Domain module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "domain_mod.h"
#include <stdio.h>
#include "../../sr_module.h"
#include "domain.h"


/*
 * Module management function prototypes
 */
static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

/*
 * Module parameter variables
 */
char* db_url = "sql://serro:47serro11@localhost/ser";
char* domain_table = "domain";        /* Name of domain table */
char* domain_domain_col = "domain";   /* Name of domain column */


/*
 * Other module variables
 */
db_con_t* db_handle;   /* Database connection handle */


struct module_exports exports = {
	"domain", 
	(char*[]) {"is_from_local", "is_uri_host_local"},
	(cmd_function[]) {is_from_local, is_uri_host_local},
	(int[]) {0, 0},
	(fixup_function[]) {0, 0},
	2, /* number of functions*/
	(char*[]){"db_url", "domain_table", "domain_domain_column"},
	(modparam_t[]){STR_PARAM, STR_PARAM, STR_PARAM},
	(void*[]){&db_url, &domain_table, &domain_domain_col},
	3,
	
	mod_init,  /* module initialization function */
	NULL,      /* response function*/
	destroy,   /* destroy function */
	NULL,      /* cancel function */
	child_init /* per-child init function */
};


static int mod_init(void)
{
	printf("Domain module - initializing\n");
	
	/* Check if database modulke has been laoded */
	if (bind_dbmod()) {
		LOG(L_ERR, "domain:mod_init(): Unable to bind database module\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	if (db_url == NULL) {
		LOG(L_ERR, "domain:init_child(): Use db_url parameter\n");
		return -1;
	}

	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "auth:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}


static void destroy(void)
{
	db_close(db_handle);
}

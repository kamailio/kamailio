/* domain_mod.c v 0.2 2003/1/19
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
#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "domain.h"
#include "fifo.h"

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
int db_mode = 0;                      /* Database usage mode: 0 = no cache, 1 = cache */
char* domain_table = "domain";        /* Name of domain table */
char* domain_domain_col = "domain";   /* Name of domain column */

/*
 * Other module variables
 */
db_con_t* db_handle = NULL;                  /* Database connection handle */
struct domain_list ***hash_table;            /* Pointer to current hash table pointer */
struct domain_list **hash_table_1;           /* Pointer to hash table 1 */
struct domain_list **hash_table_2;           /* Pointer to hash table 2 */

/*
 * Module interface
 */
struct module_exports exports = {
	"domain", 
	(char*[]) {"is_from_local", "is_uri_host_local"},
	(cmd_function[]) {is_from_local, is_uri_host_local},
	(int[]) {0, 0},
	(fixup_function[]) {0, 0},
	2, /* number of functions*/
	(char*[]){"db_url", "db_mode", "domain_table", "domain_domain_column"},
	(modparam_t[]){STR_PARAM, INT_PARAM, STR_PARAM, STR_PARAM},
	(void*[]){&db_url, &db_mode, &domain_table, &domain_domain_col},
	4,
	
	mod_init,  /* module initialization function */
	NULL,      /* response function*/
	destroy,   /* destroy function */
	NULL,      /* cancel function */
	child_init /* per-child init function */
};


static int mod_init(void)
{
	int i;

	fprintf(stderr, "domain - initializing\n");
	
	/* Check if database module has been loaded */
	if (bind_dbmod()) {
		LOG(L_ERR, "domain:mod_init(): Unable to bind database module\n");
		return -1;
	}

	/* Check if cache needs to be loaded from domain table */
	if (db_mode == 1) {
		db_handle = db_init(db_url);
		if (!db_handle) {
			LOG(L_ERR, "domain:mod_init(): Unable to connect database\n");
			return -1;
		}
		
		/* Initialize fifo interface */
		(void)init_domain_fifo();

		/* Initializing hash tables and hash table variable */
		hash_table_1 = (struct domain_list **)shm_malloc(sizeof(struct domain_list *) * HASH_SIZE);
		if (hash_table_1 == 0) {
			LOG(L_ERR, "domain: mod_init(): No memory for hash table\n");
		}

		hash_table_2 = (struct domain_list **)shm_malloc(sizeof(struct domain_list *) * HASH_SIZE);
		if (hash_table_2 == 0) {
			LOG(L_ERR, "domain: mod_init(): No memory for hash table\n");
		}
		for (i = 0; i < HASH_SIZE; i++) {
			hash_table_1[i] = hash_table_2[i] = (struct domain_list *)0;
		}

		hash_table = (struct domain_list ***)shm_malloc(sizeof(struct domain_list *));
		*hash_table = hash_table_1;

		if (reload_domain_table() == -1) {
			LOG(L_CRIT, "domain:mod_init(): Domain table reload failed\n");
			return -1;
		}
	}

	return 0;
}


static int child_init(int rank)
{
	/* Check if database is needed by child */
	if (db_mode == 0) {
		if (db_url == NULL) {
			LOG(L_ERR, "domain:child_init(): Use db_url parameter\n");
			return -1;
		}

		db_handle = db_init(db_url);
		if (!db_handle) {
			LOG(L_ERR, "domain:child_init(): Unable to connect database\n");
			return -1;
		}
	}

	return 0;

}


static void destroy(void)
{
	if (db_handle) db_close(db_handle);
}

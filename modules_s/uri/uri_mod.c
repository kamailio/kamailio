/* 
 * $Id$ 
 *
 * Various URI related functions
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "uri_mod.h"
#include "checks.h"


/*
 * Module destroy function prototype
 */
static void destroy(void);


/*
 * Module child-init function prototype
 */
static int child_init(int rank);


/*
 * Module initialization function prototype
 */
static int mod_init(void);

static int str_fixup(void** param, int param_no);


/*
 * Module parameter variables
 */
char* db_url         = "sql://serro:47serro11@localhost/ser";
char* table          = "uri";       /* Name of URI table */
char* user_column    = "user";      /* Name of user column in URI table */
char* domain_column  = "domain";    /* Name of domain column in URI table */
char* uriuser_column = "uri_user";  /* Name of uri_user column in URI table */

char* subscriber_table = "subscriber";
char* subscriber_user_column = "user";
char* subscriber_domain_column = "domain";

int use_uri_table = 0;              /* Should we use URI table ?, default no */

db_con_t* db_handle = 0;   /* Database connection handle */


/*
 * Module interface
 */
struct module_exports exports = {
	"uri", 
	(char*[]) { 
		"is_user",
		"check_to",
		"check_from",
		"does_uri_exist"
	},
	(cmd_function[]) {
		is_user,
		check_to,
		check_from,
		does_uri_exist
	},
	(int[]) {1, 0, 0, 0, 1},
	(fixup_function[]) { 
		str_fixup, 0, 0, 0
	},
	4,
	
	(char*[]) {
		"db_url",           /* Database URL */
		"table",            /* Name of URI table */
		"user_column",      /* Name of user column in URI table */
		"domain_column",    /* Name of domain column in URI table */
		"uriuser_column",   /* Name of uri_user column in URI table */
		"subscriber_table",
		"subscriber_user_column",
		"subscriber_domain_column",
		"use_uri_table"     /* Should URI table be used ? */
		
	},   /* Module parameter names */
	(modparam_t[]) {
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
	        INT_PARAM
	},   /* Module parameter types */
	(void*[]) {
		&db_url,
		&table,
		&user_column,
		&domain_column,
		&uriuser_column,
		&subscriber_table,
		&subscriber_user_column,
		&subscriber_domain_column,
		&use_uri_table
		
	},         /* Module parameter variable pointers */
	9,         /* Number of module paramers */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	0,         /* oncancel function */
	child_init /* child initialization function */
};


static int child_init(int rank)
{
	if (db_url == 0) {
		LOG(L_ERR, "uri:init_child(): Use db_url parameter\n");
		return -1;
	}
	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "uri:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}


static int mod_init(void)
{
	printf("uri module - initializing\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_init(): Unable to bind database module\n");
		return -1;
	}

	return 0;
}



static void destroy(void)
{
	if (db_handle) {
		db_close(db_handle);
	}
}


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}

/* 
 * $Id$ 
 *
 * Group membership - module interface
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
 *
 * History:
 * --------
 * 2003-02-25 - created by janakj
 * 2003-03-11 - New module interface (janakj)
 * 2003-03-16 - flags export parameter added (janakj)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "group_mod.h"
#include "group.h"


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


/* Header field fixup */
static int hf_fixup(void** param, int param_no);


/*
 * Module parameter variables
 */
char* db_url       = "sql://serro:47serro11@localhost/ser";

char* table        = "grp";    /* Table name where group definitions are stored */
char* user_col     = "user";
char* domain_col   = "domain";
char* grp_col      = "grp";
int   use_domain   = 0;

db_con_t* db_handle = 0;   /* Database connection handle */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_user_in", is_user_in, 2, hf_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",        STR_PARAM, &db_url       },
	{"table",         STR_PARAM, &table        },
	{"user_column",   STR_PARAM, &user_column  },
	{"domain_column", STR_PARAM, &domain_column},
	{"group_column",  STR_PARAM, &group_column },
	{"use_domain",    INT_PARAM, &use_domain   },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"group", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	if (db_url == 0) {
		LOG(L_ERR, "group:init_child(): Use db_url parameter\n");
		return -1;
	}

	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "group:init_child(): Unable to connect database\n");
		return -1;
	}

	return 0;
}


static int mod_init(void)
{
	printf("group module - initializing\n");
	
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
 * Convert HF description string to hdr_field pointer
 *
 * Supported strings: 
 * "Request-URI", "To", "From", "Credentials"
 */
static int hf_fixup(void** param, int param_no)
{
	void* ptr;
	str* s;

	if (param_no == 1) {
		ptr = *param;
		
		if (!strcasecmp((char*)*param, "Request-URI")) {
			*param = (void*)1;
		} else if (!strcasecmp((char*)*param, "To")) {
			*param = (void*)2;
		} else if (!strcasecmp((char*)*param, "From")) {
			*param = (void*)3;
		} else if (!strcasecmp((char*)*param, "Credentials")) {
			*param = (void*)4;
		} else {
			LOG(L_ERR, "hf_fixup(): Unsupported Header Field identifier\n");
			return E_UNSPEC;
		}

		free(ptr);
	} else if (param_no == 2) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "hf_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}


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
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
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


static void destroy(void);       /* Module destroy function */
static int child_init(int rank); /* Per-child initialization function */
static int mod_init(void);       /* Module initialization function */

static int str_fixup(void** param, int param_no);


/*
 * Module parameter variables
 */
char* db_url                = "sql://serro:47serro11@localhost/ser";
char* uri_table             = "uri";        /* Name of URI table */
char* uri_user_col          = "username";   /* Name of username column in URI table */
char* uri_domain_col        = "domain";     /* Name of domain column in URI table */
char* uri_uriuser_col       = "uri_user";   /* Name of uri_user column in URI table */
char* subscriber_table      = "subscriber"; /* Name of subscriber table */
char* subscriber_user_col   = "user";       /* Name of user column in subscriber table */
char* subscriber_domain_col = "domain";     /* Name of domain column in subscriber table */

int use_uri_table = 0;     /* Should uri table be used */
db_con_t* db_handle = 0;   /* Database connection handle */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_user",        is_user,        1, str_fixup},
	{"check_to",       check_to,       0, 0        },
	{"check_from",     check_from,     0, 0        },
	{"does_uri_exist", does_uri_exist, 0, 0        },
	{0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",                   STR_PARAM, &db_url               },
	{"uri_table",                STR_PARAM, &uri_table            },
	{"uri_user_column",          STR_PARAM, &uri_user_col         },
	{"uri_domain_column",        STR_PARAM, &uri_domain_col       },
	{"uri_uriuser_column",       STR_PARAM, &uri_uriuser_col      },
	{"subscriber_table",         STR_PARAM, &subscriber_table     },
	{"subscriber_user_column",   STR_PARAM, &subscriber_user_col  },
	{"subscriber_domain_column", STR_PARAM, &subscriber_domain_col},
	{"use_uri_table",            INT_PARAM, &use_uri_table        },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri", 
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	mod_init,  /* module initialization function */
	0,         /* response function */
	destroy,   /* destroy function */
	0,         /* oncancel function */
	child_init /* child initialization function */
};


/*
 * Module initialization function calle in each child separately
 */
static int child_init(int rank)
{
	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "uri:init_child(%d): Unable to connect database\n", rank);
		return -1;
	}
	return 0;

}


/*
 * Module initialization function that is called before the main process forks
 */
static int mod_init(void)
{
	DBG("uri - initializing\n");

	if (bind_dbmod()) {
		LOG(L_ERR, "uri:mod_init(): No database module found\n");
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

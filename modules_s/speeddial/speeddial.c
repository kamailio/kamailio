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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"

#include "sdlookup.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);


/* Module parameter variables */
char* db_url           = DEFAULT_RODB_URL;
char* user_column      = "username";
char* domain_column    = "domain";
char* sd_user_column   = "sd_username";
char* sd_domain_column = "sd_domain";
char* new_uri_column   = "new_uri";
int   use_domain       = 0;
char* domain_prefix    = NULL;

str   dstrip_s;


db_func_t db_funcs;      /* Database functions */
db_con_t* db_handle=0;   /* Database connection handle */

/*
 * sl_send_reply function pointer
 */
int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);


/* Exported functions */
static cmd_export_t cmds[] = {
	{"sd_lookup", sd_lookup, 1, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"db_url",           STR_PARAM, &db_url          },
	{"user_column",      STR_PARAM, &user_column     },
	{"domain_column",    STR_PARAM, &domain_column   },
	{"sd_user_column",   STR_PARAM, &sd_user_column     },
	{"sd_domain_column", STR_PARAM, &sd_domain_column   },
	{"new_uri_column",   STR_PARAM, &new_uri_column     },
	{"use_domain",       INT_PARAM, &use_domain      },
	{"domain_prefix",    STR_PARAM, &domain_prefix   },
	{0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
	"speeddial", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* child initialization function */
};


/**
 *
 */
static int child_init(int rank)
{
	db_handle = db_funcs.init(db_url);
	if (!db_handle)
	{
		LOG(L_ERR, "sd:init_child: Unable to connect database\n");
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

    /* Find a database module */
	if (bind_dbmod(db_url, &db_funcs))
	{
		LOG(L_ERR, "sd:mod_init: Unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(db_funcs, DB_CAP_QUERY))
	{
		LOG(L_ERR, "sd:mod_init: Database modules does not "
			"provide all functions needed by SPEEDDIAL module\n");
		return -1;
	}
	
	/**
	 * We will need sl_send_reply from stateless
	 * module for sending replies
	 */
	
	sl_reply = find_export("sl_send_reply", 2, 0);
	if (!sl_reply)
	{
		LOG(L_ERR, "sd: This module requires sl module\n");
		return -1;
	}

	if(domain_prefix==NULL || strlen(domain_prefix)==0)
	{
		dstrip_s.s   = 0;
		dstrip_s.len = 0;
	}
	else
	{
		dstrip_s.s   = domain_prefix;
		dstrip_s.len = strlen(domain_prefix);
	}

	return 0;
}


/**
 *
 */
static void destroy(void)
{
	if (db_handle)
		db_funcs.close(db_handle);
}


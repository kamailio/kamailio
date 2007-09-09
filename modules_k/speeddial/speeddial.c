/*
 * $Id$
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "../../pvar.h"

#include "sdlookup.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);

static int fixup_sd(void** param, int param_no);

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


/* Exported functions */
static cmd_export_t cmds[] = {
	{"sd_lookup", sd_lookup, 1, 0, REQUEST_ROUTE},
	{"sd_lookup", sd_lookup, 2, fixup_sd, REQUEST_ROUTE},
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
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
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
		LOG(L_ERR, "speeddial:init_child: Unable to connect database\n");
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
		LOG(L_ERR, "speeddial:mod_init: Unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(db_funcs, DB_CAP_QUERY))
	{
		LOG(L_ERR, "speeddial:mod_init: Database modules does not "
			"provide all functions needed by SPEEDDIAL module\n");
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

static int fixup_sd(void** param, int param_no)
{
	pv_elem_t *model;
	str s;

	if(param_no==1)
		return 0;

	if(*param)
	{
		s.s = (char*)(*param); s.len = strlen(s.s);
		if(pv_parse_format(&s, &model)<0)
		{
			LOG(L_ERR, "ERROR:speeddial:fixup_sd: wrong format[%s]\n",
				(char*)(*param));
			return E_UNSPEC;
		}
			
		*param = (void*)model;
		return 0;
	}
	LOG(L_ERR, "ERROR:speeddial:fixup_sd: null format\n");
	return E_UNSPEC;
}


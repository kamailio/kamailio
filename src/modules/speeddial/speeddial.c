/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */


#include <stdio.h>
#include <string.h>
#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "sdlookup.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);

/* Module parameter variables */
static str db_url    = str_init(DEFAULT_RODB_URL);
str user_column      = str_init("username");
str domain_column    = str_init("domain");
str sd_user_column   = str_init("sd_username");
str sd_domain_column = str_init("sd_domain");
str new_uri_column   = str_init("new_uri");
int use_domain       = 0;
static str domain_prefix    = {NULL, 0};

str dstrip_s = {NULL, 0};


db_func_t db_funcs;      /* Database functions */
db1_con_t* db_handle=0;   /* Database connection handle */


/* Exported functions */
static cmd_export_t cmds[] = {
	{"sd_lookup", (cmd_function)w_sd_lookup, 1, fixup_spve_null, 0,
		REQUEST_ROUTE},
	{"sd_lookup", (cmd_function)w_sd_lookup, 2, fixup_spve_spve, 0,
		REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"db_url",           PARAM_STR, &db_url             },
	{"user_column",      PARAM_STR, &user_column        },
	{"domain_column",    PARAM_STR, &domain_column      },
	{"sd_user_column",   PARAM_STR, &sd_user_column     },
	{"sd_domain_column", PARAM_STR, &sd_domain_column   },
	{"new_uri_column",   PARAM_STR, &new_uri_column     },
	{"use_domain",       INT_PARAM, &use_domain           },
	{"domain_prefix",    PARAM_STR, &domain_prefix      },
	{0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
	"speeddial",     /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported RPC functions */
	0,               /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* child initialization function */
	destroy          /* destroy function */
};


/**
 *
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	db_handle = db_funcs.init(&db_url);
	if (!db_handle)
	{
		LM_ERR("failed to connect database\n");
		return -1;
	}
	return 0;

}


/**
 *
 */
static int mod_init(void)
{
	/* Find a database module */
	if (db_bind_mod(&db_url, &db_funcs))
	{
		LM_ERR("failed to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(db_funcs, DB_CAP_QUERY))
	{
		LM_ERR("Database modules does not "
			"provide all functions needed by SPEEDDIAL module\n");
		return -1;
	}
	if (domain_prefix.s && domain_prefix.len > 0) {
		dstrip_s.s = domain_prefix.s;
		dstrip_s.len = domain_prefix.len;
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

/**
 *
 */
static int ki_sd_lookup(sip_msg_t *msg, str *stable)
{
	return sd_lookup_owner(msg, stable, NULL);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_speeddial_exports[] = {
	{ str_init("speeddial"), str_init("lookup"),
		SR_KEMIP_INT, ki_sd_lookup,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("speeddial"), str_init("lookup_owner"),
		SR_KEMIP_INT, sd_lookup_owner,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_speeddial_exports);
	return 0;
}

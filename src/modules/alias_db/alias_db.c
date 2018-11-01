/*
 * ALIAS_DB Module
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 * 2004-09-01: first version (ramona)
 */


#include <stdio.h>
#include <string.h>
#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "alookup.h"
#include "api.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);

/* Fixup function */
static int lookup_fixup(void** param, int param_no);
static int find_fixup(void** param, int param_no);

static int w_alias_db_lookup1(struct sip_msg* _msg, char* _table, char* p2);
static int w_alias_db_lookup2(struct sip_msg* _msg, char* _table, char* flags);
static int w_alias_db_find3(struct sip_msg* _msg, char* _table, char* _in,
		char* _out);
static int w_alias_db_find4(struct sip_msg* _msg, char* _table, char* _in,
		char* _out, char* flags);


/* Module parameter variables */
static str db_url       = str_init(DEFAULT_RODB_URL);
str user_column         = str_init("username");
str domain_column       = str_init("domain");
str alias_user_column   = str_init("alias_username");
str alias_domain_column = str_init("alias_domain");
str domain_prefix       = {NULL, 0};
int alias_db_use_domain = 0;
int ald_append_branches = 0;

db1_con_t* db_handle;   /* Database connection handle */
db_func_t adbf;  /* DB functions */

/* Exported functions */
static cmd_export_t cmds[] = {
	{"alias_db_lookup", (cmd_function)w_alias_db_lookup1, 1, lookup_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"alias_db_lookup", (cmd_function)w_alias_db_lookup2, 2, lookup_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"alias_db_find", (cmd_function)w_alias_db_find3, 3, find_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"alias_db_find", (cmd_function)w_alias_db_find4, 4, find_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"bind_alias_db",   (cmd_function)bind_alias_db, 1, 0, 0,
		0},
	{0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"db_url",              PARAM_STR, &db_url        },
	{"user_column",         PARAM_STR, &user_column   },
	{"domain_column",       PARAM_STR, &domain_column },
	{"alias_user_column",   PARAM_STR, &alias_user_column   },
	{"alias_domain_column", PARAM_STR, &alias_domain_column },
	{"use_domain",          INT_PARAM, &alias_db_use_domain },
	{"domain_prefix",       PARAM_STR, &domain_prefix },
	{"append_branches",     INT_PARAM, &ald_append_branches   },
	{0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
	"alias_db",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,		/* Exported functions */
	params,		/* exported params */
	0,		/*·exported·RPC·methods·*/
	0,		/* exported pseudo-variables */
	0,		/* response·function */
	mod_init,	/* initialization·module */
	child_init,	/* per-child·init·function */
	destroy		/* destroy function */
};


static int alias_flags_fixup(void** param)
{
	char *c;
	unsigned int flags;

	c = (char*)*param;
	flags = 0;

	if(alias_db_use_domain) {
		flags |= ALIAS_DOMAIN_FLAG;
	}

	while (*c) {
		switch (*c)
		{
			case 'd':
			case 'D':
				flags &= ~ALIAS_DOMAIN_FLAG;
				break;
			case 'r':
			case 'R':
				flags |= ALIAS_REVERSE_FLAG;
				break;
			case 'u':
			case 'U':
				flags |= ALIAS_DOMAIN_FLAG;
				break;
			default:
				LM_ERR("unsupported flag '%c'\n",*c);
				return -1;
		}
		c++;
	}
	pkg_free(*param);
	*param = (void*)(unsigned long)flags;
	return 0;
}


static int lookup_fixup(void** param, int param_no)
{
	if (param_no==1)
	{
		/* string or pseudo-var - table name */
		return fixup_spve_null(param, 1);
	} else if (param_no==2) {
		/* string - flags ? */
		return alias_flags_fixup(param);
	} else {
		LM_CRIT(" invalid number of params %d \n",param_no);
		return -1;
	}
}


static int find_fixup(void** param, int param_no)
{
	pv_spec_t *sp;

	if (param_no==1)
	{
		/* string or pseudo-var - table name */
		return fixup_spve_null(param, 1);
	} else if(param_no==2) {
		/* pseudo-var - source URI */
		return fixup_pvar_null(param, 1);
	} else if(param_no==3) {
		/* pvar (AVP or VAR) - destination URI */
		if (fixup_pvar_null(param, 1))
			return E_CFG;
		sp = (pv_spec_t*)*param;
		if (sp->type!=PVT_AVP && sp->type!=PVT_SCRIPTVAR)
		{
			LM_ERR("PV type %d (param 3) cannot be written\n", sp->type);
			pv_spec_free(sp);
			return E_CFG;
		}
		return 0;
	} else if (param_no==4) {
		/* string - flags  ? */
		return alias_flags_fixup(param);
	} else {
		LM_CRIT(" invalid number of params %d \n",param_no);
		return -1;
	}
}


/**
 *
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	db_handle = adbf.init(&db_url);
	if (!db_handle)
	{
		LM_ERR("unable to connect database\n");
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
	if (db_bind_mod(&db_url, &adbf))
	{
		LM_ERR("unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(adbf, DB_CAP_QUERY))
	{
		LM_CRIT("database modules does not "
			"provide all functions needed by alias_db module\n");
		return -1;
	}

	return 0;
}


/**
 *
 */
static void destroy(void)
{
	if (db_handle) {
		adbf.close(db_handle);
		db_handle = 0;
	}
}

static int w_alias_db_lookup1(struct sip_msg* _msg, char* _table, char* p2)
{
	str table_s;
	unsigned long flags;

	flags = 0;
	if(alias_db_use_domain) {
		flags |= ALIAS_DOMAIN_FLAG;
	}

	if(_table==NULL || fixup_get_svalue(_msg, (gparam_p)_table, &table_s)!=0) {
		LM_ERR("invalid table parameter\n");
		return -1;
	}

	return alias_db_lookup_ex(_msg, table_s, flags);
}

static int w_alias_db_lookup2(struct sip_msg* _msg, char* _table, char* flags)
{
	str table_s;

	if(_table==NULL || fixup_get_svalue(_msg, (gparam_p)_table, &table_s)!=0) {
		LM_ERR("invalid table parameter\n");
        return -1;
	}

	return alias_db_lookup_ex(_msg, table_s, (unsigned long)flags);
}

static int w_alias_db_find3(struct sip_msg* _msg, char* _table, char* _in,
		char* _out)
{
	str table_s;
	unsigned long flags;

	flags = 0;
	if(alias_db_use_domain) {
		flags |= ALIAS_DOMAIN_FLAG;
	}

	if(_table==NULL || fixup_get_svalue(_msg, (gparam_p)_table, &table_s)!=0) {
		LM_ERR("invalid table parameter\n");
		return -1;
	}

	return alias_db_find(_msg, table_s, _in, _out, (char*)flags);
}

static int w_alias_db_find4(struct sip_msg* _msg, char* _table, char* _in,
		char* _out, char* flags)
{
	str table_s;

	if(_table==NULL || fixup_get_svalue(_msg, (gparam_p)_table, &table_s)!=0) {
		LM_ERR("invalid table parameter\n");
		return -1;
	}

	return alias_db_find(_msg, table_s, _in, _out, flags);
}

int bind_alias_db(struct alias_db_binds *pxb)
{
	if (pxb == NULL) {
		LM_WARN("bind_alias_db: Cannot load alias_db API into a NULL pointer\n");
		return -1;
	}

	pxb->alias_db_lookup = alias_db_lookup;
	pxb->alias_db_lookup_ex = alias_db_lookup_ex;
	pxb->alias_db_find = alias_db_find;
	return 0;
}

/**
 *
 */
static int ki_alias_db_lookup(sip_msg_t* msg, str* stable)
{
	unsigned long flags;

	flags = 0;
	if(alias_db_use_domain) {
		flags |= ALIAS_DOMAIN_FLAG;
	}

	return alias_db_lookup_ex(msg, *stable, flags);
}

/**
 *
 */
static int ki_alias_db_lookup_ex(sip_msg_t* msg, str* stable, str* sflags)
{
	unsigned long flags;
	int i;

	flags = 0;
	if(alias_db_use_domain) {
		flags |= ALIAS_DOMAIN_FLAG;
	}
	for(i=0; i<sflags->len; i++) {
		switch (sflags->s[i])
		{
			case 'd':
			case 'D':
				flags &= ~ALIAS_DOMAIN_FLAG;
				break;
			case 'r':
			case 'R':
				flags |= ALIAS_REVERSE_FLAG;
				break;
			case 'u':
			case 'U':
				flags |= ALIAS_DOMAIN_FLAG;
				break;
			default:
				LM_ERR("unsupported flag '%c' - ignoring\n", sflags->s[i]);
				break;
		}
	}

	return alias_db_lookup_ex(msg, *stable, flags);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_alias_db_exports[] = {
	{ str_init("alias_db"), str_init("lookup"),
		SR_KEMIP_INT, ki_alias_db_lookup,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("alias_db"), str_init("lookup_ex"),
		SR_KEMIP_INT, ki_alias_db_lookup_ex,
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
	sr_kemi_modules_add(sr_kemi_alias_db_exports);
	return 0;
}

/* 
 * $Id: perlvdb.c 770 2007-01-22 10:16:34Z bastian $
 *
 * Perl virtual database module
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 *
 * This file is part of Kamailio, a free SIP server.
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
 */

#include "db_perlvdb.h"

MODULE_VERSION

static int mod_init(void);

SV* vdbmod;

static int db_perlvdb_bind_api(db_func_t *dbb);

/*
 * Perl virtual database module interface
 */
static cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)db_perlvdb_bind_api,    0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};



struct module_exports exports = {
	"db_perlvdb",
	RTLD_NOW | RTLD_GLOBAL, /* dlopen flags */
	cmds,
	params,      /*  module parameters */
	0,           /* exported statistics */
	0,           /* exported MI functions */
	0,           /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	0,           /* destroy function */
	0            /* per-child init function */
};


static int mod_init(void)
{
	if (!module_loaded("app_perl")) {
		LM_CRIT("app_perl module not loaded. Exiting.\n");
		return -1;
	}

	return 0;
}

static int db_perlvdb_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = perlvdb_use_table;
	dbb->init             = perlvdb_db_init;
	dbb->close            = perlvdb_db_close;
	dbb->query            = perlvdb_db_query;
	dbb->fetch_result     = 0;
	dbb->raw_query        = 0;
	dbb->free_result      = perlvdb_db_free_result;
	dbb->insert           = perlvdb_db_insert;
	dbb->delete           = perlvdb_db_delete; 
	dbb->update           = perlvdb_db_update;
	dbb->replace          = perlvdb_db_replace;

	return 0;
}

/*
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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

/*! \file
 * Berkeley DB : Module interface
 *
 * \ingroup database
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>


#include "../../str.h"
#include "../../ut.h"
#include "../../mem/mem.h"

#include "../../sr_module.h"
#include "../../lib/srdb2/db_res.h"
#include "../../lib/srdb2/db.h"

#include "bdb_lib.h"
#include "bdb_con.h"
#include "bdb_uri.h"
#include "bdb_fld.h"
#include "bdb_res.h"
#include "bdb_cmd.h"
#include "km_db_berkeley.h"

MODULE_VERSION

int auto_reload = 0;
int log_enable  = 0;
int journal_roll_interval = 0;

static int  bdb_mod_init(void);
static void bdb_mod_destroy(void);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"db_ctx",    (cmd_function)NULL,          0, 0, 0},
	{"db_con",    (cmd_function)bdb_con,       0, 0, 0},
	{"db_uri",    (cmd_function)bdb_uri,       0, 0, 0},
	{"db_cmd",    (cmd_function)bdb_cmd,       0, 0, 0},
	{"db_put",    (cmd_function)bdb_cmd_exec,  0, 0, 0},
	{"db_del",    (cmd_function)bdb_cmd_exec,  0, 0, 0},
	{"db_get",    (cmd_function)bdb_cmd_exec,  0, 0, 0},
	{"db_upd",    (cmd_function)bdb_cmd_exec,  0, 0, 0},
	{"db_sql",    (cmd_function)bdb_cmd_exec,  0, 0, 0},
	{"db_first",  (cmd_function)bdb_cmd_first, 0, 0, 0},
	{"db_next",   (cmd_function)bdb_cmd_next,  0, 0, 0},
	{"db_res",    (cmd_function)bdb_res,       0, 0, 0},
	{"db_fld",    (cmd_function)bdb_fld,       0, 0, 0},
	{"db_bind_api", (cmd_function)bdb_bind_api, 0, 0, 0},
	{0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"auto_reload",        INT_PARAM, &auto_reload },
	{"log_enable",         INT_PARAM, &log_enable  },
	{"journal_roll_interval", INT_PARAM, &journal_roll_interval  },
	{0, 0, 0}
};

struct module_exports exports = {	
	"db_berkeley",
	cmds,     /* Exported functions */
	0,        /* RPC method */
	params,   /* Exported parameters */
	bdb_mod_init,     /* module initialization function */
	0,        /* response function*/
	bdb_mod_destroy,  /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_api_init()<0)
		return -1;
	return 0;
}

static int bdb_mod_init(void)
{
	bdb_params_t p;
	
	p.auto_reload = auto_reload;
	p.log_enable = log_enable;
	p.cache_size  = (4 * 1024 * 1024); //4Mb
	p.journal_roll_interval = journal_roll_interval;
	
	if(bdblib_init(&p))
		return -1;

	return km_mod_init();
}

static void bdb_mod_destroy(void)
{
	km_destroy();
	bdblib_destroy();
}


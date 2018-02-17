/**
 * Copyright (C) 2017 plivo (plivo.com)
 * Author : Surendra Tiwari (surendratiwari3@gmail.com)
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_query.h"

#include "redisdb_dbase.h"

MODULE_VERSION

static int db_redis_bind_api(db_func_t *dbb);
static int mod_init(void);

static cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)db_redis_bind_api,    0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};


struct module_exports exports = {	
	"db_redisusrloc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,             /*  module parameters */
	0,                  /* exported statistics */
	0,                  /* exported MI functions */
	0,                  /* exported pseudo-variables */
	0,                  /* extra processes */
	mod_init,           /* module initialization function */
	0,                  /* response function*/
	0,                  /* destroy function */
	0                   /* per-child init function */
};

static int db_redis_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = db_redis_use_table;
	dbb->init             = db_redis_init;
	dbb->close            = db_redis_close;
	dbb->query            = db_redis_query;
	dbb->fetch_result     = 0; //db_redis_fetch_result;
	dbb->raw_query        = 0; //db_redis_raw_query;
	dbb->free_result      = db_redis_free_result;
	dbb->insert           = db_redis_insert;
	dbb->delete           = db_redis_delete; 
	dbb->update           = db_redis_update;
	dbb->replace          = 0; //db_redis_replace;

	return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_api_init()<0)
		return -1;
	return 0;
}
static int mod_init(void)
{
	LM_DBG("module initializing\n");
	return 0;
}


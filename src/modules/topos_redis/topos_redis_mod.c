/**
 * Copyright (C) 2017 kamailio.org
 * Copyright (C) 2017 flowroute.com
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"

#include "../ndb_redis/api.h"
#include "../topos/api.h"

#include "topos_redis_storage.h"

MODULE_VERSION

str _topos_redis_serverid = STR_NULL;

static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);

tps_storage_api_t _tps_storage_api = {0};
topos_api_t _tps_api = {0};

ndb_redis_api_t _tps_redis_api = {0};

static cmd_export_t cmds[] = {{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {
		{"serverid", PARAM_STR, &_topos_redis_serverid}, {0, 0, 0}};

struct module_exports exports = {
		"topos_redis",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported rpc functions */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module init function */
		child_init,		 /* per child init function */
		mod_destroy		 /* destroy function */
};


/**
 *
 */
static int mod_init(void)
{
	if(_topos_redis_serverid.s == NULL || _topos_redis_serverid.len <= 0) {
		LM_ERR("invalid serverid parameter\n");
		return -1;
	}
	if(topos_load_api(&_tps_api) < 0) {
		LM_ERR("failed to bind to topos module\n");
		return -1;
	}
	if(ndb_redis_load_api(&_tps_redis_api)) {
		LM_ERR("failed to bind to ndb_redis module\n");
		return -1;
	}

	_tps_storage_api.insert_dialog = tps_redis_insert_dialog;
	_tps_storage_api.clean_dialogs = tps_redis_clean_dialogs;
	_tps_storage_api.insert_branch = tps_redis_insert_branch;
	_tps_storage_api.clean_branches = tps_redis_clean_branches;
	_tps_storage_api.load_branch = tps_redis_load_branch;
	_tps_storage_api.load_dialog = tps_redis_load_dialog;
	_tps_storage_api.update_branch = tps_redis_update_branch;
	_tps_storage_api.update_dialog = tps_redis_update_dialog;
	_tps_storage_api.end_dialog = tps_redis_end_dialog;

	if(_tps_api.set_storage_api(&_tps_storage_api) < 0) {
		LM_ERR("failed to set topos storage api\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	/* skip child init for non-worker process ranks */
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0;

	return 0;
}

/**
 *
 */
static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");
}

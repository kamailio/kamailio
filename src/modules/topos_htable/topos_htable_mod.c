/**
 * Copyright (C) 2024 kamailio.org
 * Copyright (C) 2024 net2phone.com
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
#include "../topos/api.h"
#include "../htable/ht_api.h"
#include "../htable/api.h"

#include "topos_htable_storage.h"

MODULE_VERSION

static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);

tps_storage_api_t _tps_storage_api = {0};
topos_api_t _tps_api = {0};
htable_api_t _tps_htable_api = {0};

str _tps_htable_dialog = {
		"topos_dialog=>size=4;autoexpire=7200;dmqreplicate=1", 52};
str _tps_htable_transaction = {"topos_transaction=>size=4;autoexpire=7200", 42};
int _tps_base64 = 0;

static cmd_export_t cmds[] = {{0, 0, 0, 0, 0, 0}};
static param_export_t params[] = {
		{"topos_htable_dialog", PARAM_STR, &_tps_htable_dialog},
		{"topos_htable_transaction", PARAM_STR, &_tps_htable_transaction},
		{"topos_htable_base64", PARAM_INT, &_tps_base64}, {0, 0, 0}};
struct module_exports exports = {
		"topos_htable",	 /* module name */
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
	char *ptr = NULL;

	if(topos_load_api(&_tps_api) < 0) {
		LM_ERR("failed to bind to topos module\n");
		return -1;
	}

	if(htable_load_api(&_tps_htable_api) < 0) {
		LM_ERR("failed to bind to htable module\n");
		return -1;
	}

	_tps_storage_api.insert_branch = tps_htable_insert_branch;
	_tps_storage_api.load_branch = tps_htable_load_branch;
	_tps_storage_api.update_branch = tps_htable_update_branch;
	_tps_storage_api.clean_branches = tps_htable_clean_branches;

	_tps_storage_api.insert_dialog = tps_htable_insert_dialog;
	_tps_storage_api.load_dialog = tps_htable_load_dialog;
	_tps_storage_api.update_dialog = tps_htable_update_dialog;
	_tps_storage_api.clean_dialogs = tps_htable_clean_dialogs;
	_tps_storage_api.end_dialog = tps_htable_end_dialog;

	if(_tps_api.set_storage_api(&_tps_storage_api) < 0) {
		LM_ERR("failed to set topos storage api\n");
		return -1;
	}

	/* create topos_dialog and topos_transaction hash tables using htable module api */
	if(_tps_htable_api.table_spec(_tps_htable_dialog.s) < 0) {
		LM_ERR("failed table spec for topos_dialog hash table\n");
		return -1;
	}
	if(_tps_htable_api.table_spec(_tps_htable_transaction.s) < 0) {
		LM_ERR("failed table spec for topos_transaction hash table\n");
		return -1;
	}
	if(_tps_htable_api.init_tables() < 0) {
		LM_ERR("failed init topos htable tables\n");
		return -1;
	}

	/* get htable dialog/transaction names needed from now on */
	ptr = strchr(_tps_htable_dialog.s, '=');
	if(ptr != NULL) {
		_tps_htable_dialog.len = ptr - _tps_htable_dialog.s;
	}

	ptr = strchr(_tps_htable_transaction.s, '=');
	if(ptr != NULL) {
		_tps_htable_transaction.len = ptr - _tps_htable_transaction.s;
	}

	LM_DBG("modinit success topos_htable_dialog=%.*s, "
		   "topos_htable_transaction=%.*s\n",
			_tps_htable_dialog.len, _tps_htable_dialog.s,
			_tps_htable_transaction.len, _tps_htable_transaction.s);

	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	return 0;
}

/**
 *
 */
static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");
}

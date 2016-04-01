/* 
 * $Id$ 
 *
 * Flatstore module interface
 *
 * Copyright (C) 2004 FhG Fokus
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
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../rpc_lookup.h"
#include "km_flatstore.h"
#include "km_flatstore_mod.h"
#include "flatstore_mod.h"

/*MODULE_VERSION*/

int db_flat_bind_api(db_func_t *dbb);

/*
 * Process number used in filenames
 */
int km_flat_pid;


/*
 * Delimiter delimiting columns
 */
char* km_flat_delimiter = "|";


/*
 * Timestamp of the last log rotation request from
 * the FIFO interface
 */
time_t* km_flat_rotate;

time_t km_local_timestamp;

/*
 * Flatstore database module interface
 */
static kam_cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)db_flat_bind_api,      0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};

static rpc_export_t k_rpc_methods[];

struct kam_module_exports km_exports = {
	"db_flatstore",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,      /*  module parameters */
	0,           /* exported statistics */
	NULL,       /* exported MI functions */
	0,           /* exported pseudo-variables */
	0,           /* extra processes */
	km_mod_init,    /* module initialization function */
	0,           /* response function*/
	km_mod_destroy, /* destroy function */
	km_child_init   /* per-child init function */
};


int km_mod_init(void)
{
	if (rpc_register_array(k_rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (strlen(km_flat_delimiter) != 1) {
		LM_ERR("delimiter has to be exactly one character\n");
		return -1;
	}

	km_flat_rotate = (time_t*)shm_malloc(sizeof(time_t));
	if (!km_flat_rotate) {
		LM_ERR("no shared memory left\n");
		return -1;
	}

	*km_flat_rotate = time(0);
	km_local_timestamp = *km_flat_rotate;

	return 0;
}


void km_mod_destroy(void)
{
	if (km_flat_rotate) shm_free(km_flat_rotate);
}


int km_child_init(int rank)
{
	if (rank <= 0) {
		km_flat_pid = - rank;
	} else {
		km_flat_pid = rank - PROC_MIN;
	}
	return 0;
}

int db_flat_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = flat_use_table;
	dbb->init             = flat_db_init;
	dbb->close            = flat_db_close;
	dbb->insert           = flat_db_insert;

	return 0;
}

/* rpc function documentation */
static const char *rpc_k_rotate_doc[2] = {
	"Close and reopen flatrotate files during log rotation.", 0
};

/* rpc function implementations */
static void rpc_k_rotate(rpc_t *rpc, void *c)
{
	*km_flat_rotate = time(0);
	return;
}

static rpc_export_t k_rpc_methods[] = {
	{"flatstore.k_rotate", rpc_k_rotate, rpc_k_rotate_doc, 0},
	{0, 0, 0, 0},
};


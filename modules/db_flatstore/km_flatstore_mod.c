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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "km_flatstore.h"
#include "km_flat_mi.h"
#include "km_flatstore_mod.h"

MODULE_VERSION

static int child_init(int rank);

static int mod_init(void);

static void mod_destroy(void);

int db_flat_bind_api(db_func_t *dbb);

/*
 * Process number used in filenames
 */
int flat_pid;

/*
 * Should we flush after each write to the database ?
 */
int flat_flush = 1;


/*
 * Delimiter delimiting columns
 */
char* flat_delimiter = "|";


/*
 * Timestamp of the last log rotation request from
 * the FIFO interface
 */
time_t* flat_rotate;

time_t local_timestamp;

/*
 * Flatstore database module interface
 */
static cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)db_flat_bind_api,      0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"flush", INT_PARAM, &flat_flush},
	{0, 0, 0}
};


/*
 * Exported parameters
 */
static mi_export_t mi_cmds[] = {
	{ MI_FLAT_ROTATE, mi_flat_rotate_cmd,   MI_NO_INPUT_FLAG,  0,  0 },
	{ 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"db_flatstore",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,      /*  module parameters */
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	0,           /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	child_init   /* per-child init function */
};


static int mod_init(void)
{
	if (strlen(flat_delimiter) != 1) {
		LM_ERR("delimiter has to be exactly one character\n");
		return -1;
	}

	flat_rotate = (time_t*)shm_malloc(sizeof(time_t));
	if (!flat_rotate) {
		LM_ERR("no shared memory left\n");
		return -1;
	}

	*flat_rotate = time(0);
	local_timestamp = *flat_rotate;

	return 0;
}


static void mod_destroy(void)
{
	if (flat_rotate) shm_free(flat_rotate);
}


static int child_init(int rank)
{
	if (rank <= 0) {
		flat_pid = - rank;
	} else {
		flat_pid = rank - PROC_TCP_MAIN;
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


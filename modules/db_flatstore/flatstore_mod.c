/* 
 * $Id$ 
 *
 * Flatstore module interface
 *
 * Copyright (C) 2004 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "flatstore.h"
#include "flat_fifo.h"
#include "flatstore_mod.h"

MODULE_VERSION

static int child_init(int rank);

static int mod_init(void);

static void mod_destroy(void);


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
	{"db_use_table",   (cmd_function)flat_use_table, 2, 0, 0},
	{"db_init",        (cmd_function)flat_db_init,   1, 0, 0},
	{"db_close",       (cmd_function)flat_db_close,  2, 0, 0},
	{"db_insert",      (cmd_function)flat_db_insert, 2, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"flush", INT_PARAM, &flat_flush},
	{0, 0, 0}
};


struct module_exports exports = {	
	"flatstore",
	cmds,
	params,      /*  module parameters */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};


static int mod_init(void)
{
	if (strlen(flat_delimiter) != 1) {
		LOG(L_ERR, "flatstore:mod_init: Delimiter has to be exactly one character\n");
		return -1;
	}

	     /* Initialize fifo interface */
	if (init_flat_fifo() < 0) {
		LOG(L_ERR, "usrloc/fifo initialization failed\n");
		return -1;
	}

	flat_rotate = (time_t*)shm_malloc(sizeof(time_t));
	if (!flat_rotate) {
		LOG(L_ERR, "flatstore: No shared memory left\n");
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
		flat_pid = rank - PROC_UNIXSOCK;
	}
	return 0;
}

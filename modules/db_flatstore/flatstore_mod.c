/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * Flatstore module interface.
 */
#include "flatstore_mod.h"
#include "km_flatstore_mod.h"
#include "flat_con.h"
#include "flat_cmd.h"
#include "flat_rpc.h"
#include "flat_uri.h"

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"

#include <stdlib.h>
#include <string.h>

MODULE_VERSION

static int child_init(int rank);

static int mod_init(void);

static void mod_destroy(void);


/** PID to be used in file names.  
 * The flatstore module generates one file per SER process to ensure that
 * every SER process has its own file and no locking/synchronization is
 * necessary.  This variable contains a unique id of the SER process which
 * will be added to the file name.
 */
str flat_pid = STR_NULL;


/** Enable/disable flushing after eaach write. */
int flat_flush = 1;


/** Row delimiter.
 * The character in this variable will be used to delimit rows.
 */
str flat_record_delimiter = STR_STATIC_INIT("\n");


/** Field delimiter.
 * The character in this variable will be used to delimit fields.
 */
str flat_delimiter = STR_STATIC_INIT("|");


/** Escape character.
 * The character in this variable will be used to escape specia characters,
 * such as row and field delimiters, if they appear in the data being written
 * in the files.
 */
str flat_escape = STR_STATIC_INIT("\\");


/** Filename suffix.
 * This is the suffix of newly created files.
 */
str flat_suffix = STR_STATIC_INIT(".log");


/** Timestamp of last file rotation request.
 * This variable holds the timestamp of the last file rotation request
 * received through the management interface.
 */
time_t* flat_rotate;


/** Timestamp of last file rotation.
 * This variable contains the time of the last rotation of files.
 */
time_t flat_local_timestamp;


/* Flatstore database module interface */
static cmd_export_t cmds[] = {
	{"db_uri", (cmd_function)flat_uri, 0, 0, 0},
	{"db_con", (cmd_function)flat_con, 0, 0, 0},
	{"db_cmd", (cmd_function)flat_cmd, 0, 0, 0},
	{"db_put", (cmd_function)flat_put, 0, 0, 0},
	{"db_bind_api", (cmd_function)db_flat_bind_api,      0, 0, 0},
	{0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"flush",            PARAM_INT, &flat_flush},
	{"field_delimiter",  PARAM_STR, &flat_delimiter},
	{"record_delimiter", PARAM_STR, &flat_record_delimiter},
	{"escape_char",      PARAM_STR, &flat_escape},
	{"file_suffix",      PARAM_STR, &flat_suffix},
	{0, 0, 0}
};


struct module_exports exports = {
	"db_flatstore",
	cmds,
	flat_rpc,    /* RPC methods */
	params,      /*  module parameters */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};


int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_api_init()<0)
		return -1;
	return 0;
}

static int mod_init(void)
{
	if (flat_delimiter.len != 1) {
		ERR("flatstore: Parameter 'field_delimiter' "
			"must be exactly one character long.\n");
		return -1;
	}

	if (flat_record_delimiter.len != 1) {
		ERR("flatstore: Parameter 'record_delimiter' "
			"must be exactly one character long.\n");
		return -1;
	}

	if (flat_escape.len != 1) {
		ERR("flatstore: Parameter 'escape_char' "
			"must be exaactly one character long.\n");
		return -1;
	}

	flat_rotate = (time_t*)shm_malloc(sizeof(time_t));
	if (!flat_rotate) {
		ERR("flatstore: Not enough shared memory left\n");
		return -1;
	}

	*flat_rotate = time(0);
	flat_local_timestamp = *flat_rotate;

	return km_mod_init();
}


static void mod_destroy(void)
{
	km_mod_destroy();
	if (flat_pid.s) free(flat_pid.s);
	if (flat_rotate) shm_free(flat_rotate);
}


/*
 * FIXME: We should check whether just calling km_child_init would really work
 * here. This function comes from kamailio and since the core of sip-router is
 * based on SER 2.0, the way how child_init is called and values of the rank
 * variable could be incompatible with km_child_init function. A solution here
 * would be to rewrite km_child_init with ser 2.0 init stuff in mind.
 */
static int child_init(int rank)
{
	char* tmp;
	unsigned int v;

	if(rank==PROC_INIT)
		return 0;

	km_child_init(rank);

	if (rank <= 0) {
		v = -rank;
	} else {
		v = rank - PROC_MIN;
	}

    if ((tmp = int2str(v, &flat_pid.len)) == NULL) {
		BUG("flatstore: Error while converting process id to number\n");
		return -1;
	}

	if ((flat_pid.s = strdup(tmp)) == NULL) {
		ERR("flatstore: No memory left\n");
		return -1;
	}

	return 0;
}

/** @} */


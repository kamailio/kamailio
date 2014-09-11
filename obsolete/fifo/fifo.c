/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "../../str.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../pt.h"
#include "../../cfg/cfg_struct.h"
#include "fifo_server.h"
#include "fifo.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"fifo_file",     PARAM_STRING, &fifo               },
	{"fifo_dir",      PARAM_STRING, &fifo_dir           },
	{"user",          PARAM_STRING, &fifo_user          },
	{"mode",          PARAM_INT,    &fifo_mode          },
	{"group",         PARAM_STRING, &fifo_group         },
	{"reply_retries", PARAM_INT,    &fifo_reply_retries },
	{"reply_wait",    PARAM_INT,    &fifo_reply_wait    },
	{0, 0, 0}
};

struct module_exports exports = {
	"fifo",
	cmds,           /* Exported commands */
	0,              /* Exported RPC methods */
	params,         /* Exported parameters */
	mod_init,       /* module initialization function */
	0,              /* response function*/
	mod_destroy,    /* destroy function */
	0,              /* oncancel function */
	child_init      /* per-child init function */
};


static int mod_init(void)
{
	if (init_fifo_server() < 0) return -1;
	     /* Signal to the core that we will be creating one
	      * additional process
	      */
	if (fifo) {
		register_procs(1);
		/* The child process will keep updating its
		 * local configuration */
		cfg_register_child(1);
	}
	return 0;
}


static int child_init(int rank)
{
	     /* Note: We call init_fifo_server from mod_init, this
	      * will call the function at an early init stage -- before
	      * do_suid, thus the function would have sufficient permissions
	      * to change the user and group for FIFO
	      *
	      * start_fifo_server gets called from PROC_MAIN child init, this
	      * ensures that the function gets called at the end of the init
	      * process, when all the sockets are properly initialized.
	      */
	if (rank == PROC_MAIN) { /* FIXME: nofork rank==1 */
		if (start_fifo_server() < 0) return -1;
	}
	return 0;
}

static void mod_destroy(void)
{
	destroy_fifo();
}

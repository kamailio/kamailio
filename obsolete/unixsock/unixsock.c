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
#include "unixsock_server.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int rank);

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
	{"socket",       PARAM_STRING, &unixsock_name      },
	{"children",     PARAM_INT,    &unixsock_children  },
	{"send_timeout", PARAM_INT,    &unixsock_tx_timeout},
	{"socket_user",  PARAM_STRING, &unixsock_user      },
	{"socket_mode",  PARAM_STRING, &unixsock_mode      },
	{"socket_group", PARAM_STRING, &unixsock_group     },
	{0, 0, 0}
};


struct module_exports exports = {
	"unixsock",
	cmds,           /* Exported commands */
	0,              /* Exported RPC methods */
	params,         /* Exported parameters */
	mod_init,       /* module initialization function */
	0,              /* response function*/
	0,              /* destroy function */
	0,              /* oncancel function */
	child_init      /* per-child init function */
};


static int mod_init(void)
{
	if (init_unixsock_socket() < 0) return -1;
	     /* Signal to the core that we will be
	      * creating additional processes
	      */
	if (unixsock_name) register_procs(unixsock_children);
	return 0;
}


static int child_init(int rank)
{
	if (rank == PROC_MAIN) {
		if (init_unixsock_children() < 0) return -1;
	}
	return 0;
}

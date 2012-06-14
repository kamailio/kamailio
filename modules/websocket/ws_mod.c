/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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
 */

#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../parser/msg_parser.h"
#include "ws_handshake.h"
#include "ws_mod.h"

MODULE_VERSION

static int mod_init(void);

sl_api_t ws_slb;
int ws_ping_interval = 25;	/* time (in seconds) after which a Ping will be
				   sent on an idle connection */
int ws_ping_timeout = 1;	/* time (in seconds) to wait for a Pong in
				   response to a Ping before closing a
				   connection */

static param_export_t params[]=
{
	{"ping_interval",	INT_PARAM, &ws_ping_interval},
	{"ping_timeout",	INT_PARAM, &ws_ping_timeout},
};

static cmd_export_t cmds[]= 
{
    {"ws_handle_handshake", (cmd_function)ws_handle_handshake, 0,
	0, 0,
	ANY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

struct module_exports exports= 
{
	"websocket",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	0,			/* destroy function */
	0			/* per-child initialization function */
};

static int mod_init(void)
{
	if (sl_load_api(&ws_slb) != 0)
	{
		LM_ERR("cannot bind to SL\n");
		return -1;
	}

	/* TODO: register module with core to receive WS/WSS messages */

	return 0;
}

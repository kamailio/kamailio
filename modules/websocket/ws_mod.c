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
#include "../../events.h"
#include "../../ip_addr.h"
#include "../../locking.h"
#include "../../sr_module.h"
#include "../../tcp_conn.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "ws_conn.h"
#include "ws_handshake.h"
#include "ws_frame.h"
#include "ws_mod.h"

MODULE_VERSION

/* Maximum number of connections to display when using the ws.dump MI command */
#define MAX_WS_CONNS_DUMP	50

static int mod_init(void);
static void destroy(void);

sl_api_t ws_slb;
int *ws_enabled;

int ws_ping_interval = 180;	/* time (in seconds) between sending Pings */

static cmd_export_t cmds[]= 
{
    { "ws_handle_handshake", (cmd_function)ws_handle_handshake,
	0, 0, 0,
	ANY_ROUTE },
    { 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]=
{
	{ "ping_interval",	INT_PARAM, &ws_ping_interval },
	{ 0, 0, 0 }
};

static stat_export_t stats[] =
{
	/* ws_conn.c */
	{ "ws_current_connections",       0, &ws_current_connections },
	{ "ws_max_concurrent_connections",0, &ws_max_concurrent_connections },

	/* ws_handshake.c */
	{ "ws_failed_handshakes",         0, &ws_failed_handshakes },
	{ "ws_successful_handshakes",     0, &ws_successful_handshakes },

	/* ws_frame.c */
	{ "ws_failed_connections",        0, &ws_failed_connections },
	{ "ws_local_closed_connections",  0, &ws_local_closed_connections },
	{ "ws_received_frames",           0, &ws_received_frames },
	{ "ws_remote_closed_connections", 0, &ws_remote_closed_connections },
	{ "ws_transmitted_frames",        0, &ws_transmitted_frames },
	{ 0, 0, 0 }
};

static mi_export_t mi_cmds[] =
{
	{ "ws.close",   ws_mi_close,   0, 0, 0 },
	{ "ws.disable", ws_mi_disable, 0, 0, 0 },
	{ "ws.dump",	ws_mi_dump,    0, 0, 0 },
	{ "ws.enable",	ws_mi_enable,  0, 0, 0 },
	{ "ws.ping",    ws_mi_ping,    0, 0, 0 },
	{ "ws.pong",	ws_mi_pong,    0, 0, 0 },
	{ 0, 0, 0, 0, 0 }
};

struct module_exports exports= 
{
	"websocket",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	stats,			/* exported statistics */
	mi_cmds,		/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	destroy,		/* destroy function */
	0			/* per-child initialization function */
};

static int mod_init(void)
{
	if (sl_load_api(&ws_slb) != 0)
	{
		LM_ERR("binding to SL\n");
		goto error;
	}

	if (sr_event_register_cb(SREV_TCP_WS_FRAME, ws_frame_received) != 0)
	{
		LM_ERR("registering WebSocket call-back\n");
		goto error;
	}

	if (register_module_stats(exports.name, stats) != 0)
	{
		LM_ERR("registering core statistics\n");
		goto error;
	}

	if (register_mi_mod(exports.name, mi_cmds) != 0)
	{
		LM_ERR("registering MI commands\n");
		goto error;
	}

	if (wsconn_init() < 0)
	{
		LM_ERR("initialising WebSocket connections table\n");
		goto error;
	}

	if ((ws_enabled = (int *) shm_malloc(sizeof(int))) == NULL)
	{
		LM_ERR("allocating shared memory\n");
		goto error;
	}
	*ws_enabled = 1;

	return 0;

error:
	wsconn_destroy();
	shm_free(ws_enabled);

	return -1;
}

static void destroy(void)
{
	wsconn_destroy();
	shm_free(ws_enabled);
}

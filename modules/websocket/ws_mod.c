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
#include "../../locking.h"
#include "../../sr_module.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"
#include "../../lib/kmi/tree.h"
#include "../../parser/msg_parser.h"
#include "ws_handshake.h"
#include "ws_frame.h"
#include "ws_mod.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

sl_api_t ws_slb;
int *ws_enabled;
gen_lock_t *ws_stats_lock;

int ws_ping_interval = 30;	/* time (in seconds) between sending Pings */

stat_var *ws_current_connections;
stat_var *ws_failed_connections;
stat_var *ws_failed_handshakes;
stat_var *ws_local_closed_connections;
stat_var *ws_max_concurrent_connections;
stat_var *ws_received_frames;
stat_var *ws_remote_closed_connections;
stat_var *ws_successful_handshakes;
stat_var *ws_transmitted_frames;

static struct mi_root *mi_dump(struct mi_root *cmd, void *param);

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
	{ 0, 0 }
};

static stat_export_t stats[] =
{
	{ "ws_current_connections",       0, &ws_current_connections },
	{ "ws_failed_connections",        0, &ws_failed_connections },
	{ "ws_failed_handshakes",         0, &ws_failed_handshakes },
	{ "ws_local_closed_connections",  0, &ws_local_closed_connections },
	{ "ws_max_concurrent_connections",0, &ws_max_concurrent_connections },
	{ "ws_received_frames",           0, &ws_received_frames },
	{ "ws_remote_closed_connections", 0, &ws_remote_closed_connections },
	{ "ws_successful_handshakes",     0, &ws_successful_handshakes },
	{ "ws_transmitted_frames",        0, &ws_transmitted_frames },
	{ 0, 0, 0 }
};

static mi_export_t mi_cmds[] =
{
	{ "ws_close",   ws_mi_close,   0, 0, 0 },
	{ "ws_disable", ws_mi_disable, 0, 0, 0 },
	{ "ws_dump",	mi_dump,       0, 0, 0 },
	{ "ws_enable",	ws_mi_enable,  0, 0, 0 },
	{ "ws_ping",    ws_mi_ping,    0, 0, 0 },
	{ "ws_pong",	ws_mi_pong,    0, 0, 0 },
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
		return -1;
	}

	if (sr_event_register_cb(SREV_TCP_WS_FRAME, ws_frame_received) != 0)
	{
		LM_ERR("registering WebSocket call-back\n");
		return -1;
	}

	if (register_module_stats(exports.name, stats) != 0)
	{
		LM_ERR("registering core statistics\n");
		return -1;
	}

	if (register_mi_mod(exports.name, mi_cmds) != 0)
	{
		LM_ERR("registering MI commands\n");
		return -1;
	}

	if ((ws_enabled = (int *) shm_malloc(sizeof(int))) == NULL)
	{
		LM_ERR("allocating shared memory\n");
		return -1;
	}
	*ws_enabled = 1;

	if ((ws_stats_lock = lock_alloc()) == NULL)
	{
		LM_ERR("allocating lock\n");
		return -1;
	}
	if (lock_init(ws_stats_lock) == NULL)
	{
		LM_ERR("initialising lock\n");
		lock_dealloc(ws_stats_lock);
		return -1;
	}

	/* TODO: register module with core to receive WS/WSS messages */

	return 0;
}

static void destroy(void)
{
	shm_free(ws_enabled);
	lock_destroy(ws_stats_lock);
	lock_dealloc(ws_stats_lock);

	/* TODO: close all connections */
}

static struct mi_root *mi_dump(struct mi_root *cmd, void *param)
{
	/* TODO: output all open websocket connections */
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

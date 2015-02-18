/*
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#include "../../dprint.h"
#include "../../events.h"
#include "../../ip_addr.h"
#include "../../locking.h"
#include "../../sr_module.h"
#include "../../tcp_conn.h"
#include "../../timer_proc.h"
#include "../../cfg/cfg.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../../parser/msg_parser.h"
#include "ws_conn.h"
#include "ws_handshake.h"
#include "ws_frame.h"
#include "ws_mod.h"
#include "config.h"

MODULE_VERSION

/* Maximum number of connections to display when using the ws.dump MI command */
#define MAX_WS_CONNS_DUMP		50

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int ws_close_fixup(void** param, int param_no);

sl_api_t ws_slb;

#define DEFAULT_KEEPALIVE_INTERVAL	1
static int ws_keepalive_interval = DEFAULT_KEEPALIVE_INTERVAL;

static int ws_keepalive_timeout = DEFAULT_KEEPALIVE_TIMEOUT;

#define DEFAULT_KEEPALIVE_PROCESSES	1
static int ws_keepalive_processes = DEFAULT_KEEPALIVE_PROCESSES;

static cmd_export_t cmds[]= 
{
	/* ws_frame.c */
	{ "ws_close", (cmd_function) ws_close,
	  0, 0, 0,
	  ANY_ROUTE },
	{ "ws_close", (cmd_function) ws_close2,
	  2, ws_close_fixup, 0,
	  ANY_ROUTE },
	{ "ws_close", (cmd_function) ws_close3,
	  3, ws_close_fixup, 0,
	  ANY_ROUTE },

	/* ws_handshake.c */
	{ "ws_handle_handshake", (cmd_function) ws_handle_handshake,
	  0, 0, 0,
	  ANY_ROUTE },

	{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]=
{
	/* ws_frame.c */
	{ "keepalive_mechanism",	INT_PARAM, &ws_keepalive_mechanism },
	{ "keepalive_timeout",		INT_PARAM, &ws_keepalive_timeout },
	{ "ping_application_data",	PARAM_STR, &ws_ping_application_data },

	/* ws_handshake.c */
	{ "sub_protocols",		INT_PARAM, &ws_sub_protocols },
	{ "cors_mode",			INT_PARAM, &ws_cors_mode },

	/* ws_mod.c */
	{ "keepalive_interval",		INT_PARAM, &ws_keepalive_interval },
	{ "keepalive_processes",	INT_PARAM, &ws_keepalive_processes },

	{ 0, 0, 0 }
};

static stat_export_t stats[] =
{
	/* ws_conn.c */
	{ "ws_current_connections",            0, &ws_current_connections },
	{ "ws_max_concurrent_connections",     0, &ws_max_concurrent_connections },
	{ "ws_sip_current_connections",        0, &ws_sip_current_connections },
        { "ws_sip_max_concurrent_connectons",  0, &ws_sip_max_concurrent_connections },
        { "ws_msrp_current_connections",       0, &ws_msrp_current_connections },
        { "ws_msrp_max_concurrent_connectons", 0, &ws_msrp_max_concurrent_connections },

	/* ws_frame.c */
	{ "ws_failed_connections",             0, &ws_failed_connections },
	{ "ws_local_closed_connections",       0, &ws_local_closed_connections },
	{ "ws_received_frames",                0, &ws_received_frames },
	{ "ws_remote_closed_connections",      0, &ws_remote_closed_connections },
	{ "ws_transmitted_frames",             0, &ws_transmitted_frames },
	{ "ws_sip_failed_connections",         0, &ws_sip_failed_connections },
	{ "ws_sip_local_closed_connections",   0, &ws_sip_local_closed_connections },
	{ "ws_sip_received_frames",            0, &ws_sip_received_frames },
	{ "ws_sip_remote_closed_connections",  0, &ws_sip_remote_closed_connections },
	{ "ws_sip_transmitted_frames",         0, &ws_sip_transmitted_frames },
	{ "ws_msrp_failed_connections",        0, &ws_msrp_failed_connections },
	{ "ws_msrp_local_closed_connections",  0, &ws_msrp_local_closed_connections },
	{ "ws_msrp_received_frames",           0, &ws_msrp_received_frames },
	{ "ws_msrp_remote_closed_connections", 0, &ws_msrp_remote_closed_connections },
	{ "ws_msrp_transmitted_frames",        0, &ws_msrp_transmitted_frames },

	/* ws_handshake.c */
	{ "ws_failed_handshakes",              0, &ws_failed_handshakes },
	{ "ws_successful_handshakes",          0, &ws_successful_handshakes },
	{ "ws_sip_successful_handshakes",      0, &ws_sip_successful_handshakes },
	{ "ws_msrp_successful_handshakes",     0, &ws_msrp_successful_handshakes },

	{ 0, 0, 0 }
};

static mi_export_t mi_cmds[] =
{
	/* ws_conn.c */
	{ "ws.dump",	ws_mi_dump,    0, 0, 0 },

	/* ws_frame.c */
	{ "ws.close",   ws_mi_close,   0, 0, 0 },
	{ "ws.ping",    ws_mi_ping,    0, 0, 0 },
	{ "ws.pong",	ws_mi_pong,    0, 0, 0 },

	/* ws_handshake.c */
	{ "ws.disable", ws_mi_disable, 0, 0, 0 },
	{ "ws.enable",	ws_mi_enable,  0, 0, 0 },

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
	child_init		/* per-child initialization function */
};

static int mod_init(void)
{
	if (sl_load_api(&ws_slb) != 0)
	{
		LM_ERR("binding to SL\n");
		goto error;
	}

	if (sr_event_register_cb(SREV_TCP_WS_FRAME_IN, ws_frame_receive) != 0)
	{
		LM_ERR("registering WebSocket receive call-back\n");
		goto error;
	}

	if (sr_event_register_cb(SREV_TCP_WS_FRAME_OUT, ws_frame_transmit) != 0)
	{
		LM_ERR("registering WebSocket transmit call-back\n");
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

	if (ws_ping_application_data.len < 1
		|| ws_ping_application_data.len > 125)
	{
		ws_ping_application_data.s = DEFAULT_PING_APPLICATION_DATA + 8;
		ws_ping_application_data.len =
					DEFAULT_PING_APPLICATION_DATA_LEN - 8;
	}

	if (ws_keepalive_mechanism != KEEPALIVE_MECHANISM_NONE)
	{
		if (ws_keepalive_timeout < 1 || ws_keepalive_timeout > 3600)
			ws_keepalive_timeout = DEFAULT_KEEPALIVE_TIMEOUT;

		switch(ws_keepalive_mechanism)
		{
		case KEEPALIVE_MECHANISM_PING:
		case KEEPALIVE_MECHANISM_PONG:
			break;
		default:
			ws_keepalive_mechanism = DEFAULT_KEEPALIVE_MECHANISM;
			break;
		}

		if (ws_keepalive_interval < 1 || ws_keepalive_interval > 60)
			ws_keepalive_interval = DEFAULT_KEEPALIVE_INTERVAL;

		if (ws_keepalive_processes < 1 || ws_keepalive_processes > 16)
			ws_keepalive_processes = DEFAULT_KEEPALIVE_PROCESSES;

		/* Add extra process/timer for the keepalive process */
		register_sync_timers(ws_keepalive_processes);
	}

	if (ws_sub_protocols & SUB_PROTOCOL_MSRP
		&& !sr_event_enabled(SREV_TCP_MSRP_FRAME))
		ws_sub_protocols &= ~SUB_PROTOCOL_MSRP;

	if ((ws_sub_protocols & SUB_PROTOCOL_ALL) == 0)
	{
		LM_ERR("no sub-protocols enabled\n");
		goto error;
	}

	if ((ws_sub_protocols | SUB_PROTOCOL_ALL) != SUB_PROTOCOL_ALL)
	{
		LM_ERR("unrecognised sub-protocols enabled\n");
		goto error;
	}

	if (ws_cors_mode < 0 || ws_cors_mode > 2)
	{
		LM_ERR("bad value for cors_mode\n");
		goto error;
	}

	if (cfg_declare("websocket", ws_cfg_def, &default_ws_cfg,
			cfg_sizeof(websocket), &ws_cfg))
	{
		LM_ERR("declaring configuration\n");
		return -1;
	}
	cfg_get(websocket, ws_cfg, keepalive_timeout) = ws_keepalive_timeout;

	if (!module_loaded("xhttp"))
	{
		LM_ERR("\"xhttp\" must be loaded to use WebSocket.\n");
		return -1;
	}

	if (((ws_sub_protocols & SUB_PROTOCOL_SIP) == SUB_PROTOCOL_SIP)
			&& !module_loaded("nathelper")
			&& !module_loaded("outbound"))
	{
		LM_WARN("neither \"nathelper\" nor \"outbound\" modules are"
			" loaded. At least one of these is required for correct"
			" routing of SIP over WebSocket.\n");
	}

	return 0;

error:
	wsconn_destroy();
	return -1;
}

static int child_init(int rank)
{
	int i;

	if (rank == PROC_INIT || rank == PROC_TCP_MAIN)
		return 0;

	if (rank == PROC_MAIN
		&& ws_keepalive_mechanism != KEEPALIVE_MECHANISM_NONE)
	{
		for (i = 0; i < ws_keepalive_processes; i++)
		{
			if (fork_sync_timer(PROC_TIMER, "WEBSOCKET KEEPALIVE",
						1, ws_keepalive, NULL,
						ws_keepalive_interval) < 0)
			{
				LM_ERR("starting keepalive process\n");
				return -1;
			}
		}

	}

	return 0;
}

static void destroy(void)
{
	wsconn_destroy();
}

static int ws_close_fixup(void** param, int param_no)
{
	switch(param_no) {
	case 1:
	case 3:
		return fixup_var_int_1(param, 1);
	case 2:
		return fixup_spve_null(param, 1);
	default:
		return 0;
	}
}

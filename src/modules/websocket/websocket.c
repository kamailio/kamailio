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

#include "../../core/dprint.h"
#include "../../core/events.h"
#include "../../core/ip_addr.h"
#include "../../core/locking.h"
#include "../../core/sr_module.h"
#include "../../core/tcp_conn.h"
#include "../../core/timer_proc.h"
#include "../../core/cfg/cfg.h"
#include "../../core/counters.h"
#include "../../core/mem/mem.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "ws_conn.h"
#include "ws_handshake.h"
#include "ws_frame.h"
#include "websocket.h"
#include "config.h"

MODULE_VERSION

/* Maximum number of connections to display when using the ws.dump command */
#define MAX_WS_CONNS_DUMP 50

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int ws_close_fixup(void **param, int param_no);
static int pv_get_ws_conid_f(struct sip_msg *, pv_param_t *, pv_value_t *);

static int ws_init_rpc(void);

sl_api_t ws_slb;

#define WS_DEFAULT_RM_DELAY_INTERVAL 5
int ws_rm_delay_interval = WS_DEFAULT_RM_DELAY_INTERVAL;

#define DEFAULT_TIMER_INTERVAL 1
static int ws_timer_interval = DEFAULT_TIMER_INTERVAL;

#define DEFAULT_KEEPALIVE_INTERVAL 1
static int ws_keepalive_interval = DEFAULT_KEEPALIVE_INTERVAL;

static int ws_keepalive_timeout = DEFAULT_KEEPALIVE_TIMEOUT;

#define DEFAULT_KEEPALIVE_PROCESSES 1
int ws_keepalive_processes = DEFAULT_KEEPALIVE_PROCESSES;

int ws_verbose_list = 0;

str ws_event_callback = STR_NULL;

/* clang-format off */
static cmd_export_t cmds[] = {
	/* ws_frame.c */
	{ "ws_close", (cmd_function)w_ws_close0,
	  0, 0, 0,
	  ANY_ROUTE },
	{ "ws_close", (cmd_function)w_ws_close2,
	  2, ws_close_fixup, 0,
	  ANY_ROUTE },
	{ "ws_close", (cmd_function)w_ws_close3,
	  3, ws_close_fixup, 0,
	  ANY_ROUTE },

	/* ws_handshake.c */
	{ "ws_handle_handshake", (cmd_function)w_ws_handle_handshake,
	  0, 0, 0,
	  ANY_ROUTE },

	{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
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

	{ "timer_interval",		INT_PARAM, &ws_timer_interval },
	{ "rm_delay_interval",	INT_PARAM, &ws_rm_delay_interval },

	{ "verbose_list",		PARAM_INT, &ws_verbose_list },
	{ "event_callback",		PARAM_STR, &ws_event_callback},

	{ 0, 0, 0 }
};

static stat_export_t stats[] = {
	/* ws_conn.c */
	{ "ws_current_connections",            0, &ws_current_connections },
	{ "ws_max_concurrent_connections",     0, &ws_max_concurrent_connections },
	{ "ws_sip_current_connections",        0, &ws_sip_current_connections },
	{ "ws_sip_max_concurrent_connections", 0, &ws_sip_max_concurrent_connections },
	{ "ws_msrp_current_connections",       0, &ws_msrp_current_connections },
	{ "ws_msrp_max_concurrent_connections", 0, &ws_msrp_max_concurrent_connections },

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

static pv_export_t mod_pvs[] = {
    {{"ws_conid", (sizeof("ws_conid")-1)}, PVT_CONTEXT,
     pv_get_ws_conid_f, 0, 0, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"websocket",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* exported rpc functions */
	mod_pvs,			/* exported pseudo-variables */
	0,					/* response handlin function */
	mod_init,			/* module init function */
	child_init,			/* per-child init function */
	destroy				/* destroy function */
};
/* clang-format on */

static int mod_init(void)
{
	if(sl_load_api(&ws_slb) != 0) {
		LM_ERR("binding to SL\n");
		goto error;
	}

	if(sr_event_register_cb(SREV_TCP_WS_FRAME_IN, ws_frame_receive) != 0) {
		LM_ERR("registering WebSocket receive call-back\n");
		goto error;
	}

	if(sr_event_register_cb(SREV_TCP_WS_FRAME_OUT, ws_frame_transmit) != 0) {
		LM_ERR("registering WebSocket transmit call-back\n");
		goto error;
	}

	if(register_module_stats(exports.name, stats) != 0) {
		LM_ERR("registering core statistics\n");
		goto error;
	}

	if(ws_init_rpc() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(wsconn_init() < 0) {
		LM_ERR("initialising WebSocket connections table\n");
		goto error;
	}

	if(ws_ping_application_data.len < 1 || ws_ping_application_data.len > 125) {
		ws_ping_application_data.s = DEFAULT_PING_APPLICATION_DATA + 8;
		ws_ping_application_data.len = DEFAULT_PING_APPLICATION_DATA_LEN - 8;
	}

	if(ws_keepalive_mechanism != KEEPALIVE_MECHANISM_NONE) {
		if(ws_keepalive_timeout < 1 || ws_keepalive_timeout > 3600)
			ws_keepalive_timeout = DEFAULT_KEEPALIVE_TIMEOUT;

		switch(ws_keepalive_mechanism) {
			case KEEPALIVE_MECHANISM_PING:
			case KEEPALIVE_MECHANISM_PONG:
				break;
			default:
				ws_keepalive_mechanism = DEFAULT_KEEPALIVE_MECHANISM;
				break;
		}

		if(ws_keepalive_interval < 1 || ws_keepalive_interval > 60)
			ws_keepalive_interval = DEFAULT_KEEPALIVE_INTERVAL;

		if(ws_keepalive_processes < 1 || ws_keepalive_processes > 16)
			ws_keepalive_processes = DEFAULT_KEEPALIVE_PROCESSES;

		/* Add extra process/timer for the keepalive process */
		register_sync_timers(ws_keepalive_processes);
	}
	if(ws_timer_interval < 1 || ws_timer_interval > 60)
		ws_timer_interval = DEFAULT_TIMER_INTERVAL;
	/* timer routing to clean up inactive connections */
	register_sync_timers(1);

	if(ws_rm_delay_interval < 1 || ws_rm_delay_interval > 60)
		ws_rm_delay_interval = WS_DEFAULT_RM_DELAY_INTERVAL;

	if(ws_sub_protocols & SUB_PROTOCOL_MSRP
			&& !sr_event_enabled(SREV_TCP_MSRP_FRAME))
		ws_sub_protocols &= ~SUB_PROTOCOL_MSRP;

	if((ws_sub_protocols & SUB_PROTOCOL_ALL) == 0) {
		LM_ERR("no sub-protocols enabled\n");
		goto error;
	}

	if((ws_sub_protocols | SUB_PROTOCOL_ALL) != SUB_PROTOCOL_ALL) {
		LM_ERR("unrecognised sub-protocols enabled\n");
		goto error;
	}

	if(ws_cors_mode < 0 || ws_cors_mode > 2) {
		LM_ERR("bad value for cors_mode\n");
		goto error;
	}

	if(cfg_declare("websocket", ws_cfg_def, &default_ws_cfg,
			   cfg_sizeof(websocket), &ws_cfg)) {
		LM_ERR("declaring configuration\n");
		return -1;
	}
	cfg_get(websocket, ws_cfg, keepalive_timeout) = ws_keepalive_timeout;

	if(!module_loaded("xhttp")) {
		LM_ERR("\"xhttp\" must be loaded to use WebSocket.\n");
		return -1;
	}

	if(((ws_sub_protocols & SUB_PROTOCOL_SIP) == SUB_PROTOCOL_SIP)
			&& !module_loaded("nathelper") && !module_loaded("outbound")) {
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

	if(rank == PROC_INIT || rank == PROC_TCP_MAIN)
		return 0;

	if(rank == PROC_MAIN) {
		if(ws_keepalive_mechanism != KEEPALIVE_MECHANISM_NONE) {
			for(i = 0; i < ws_keepalive_processes; i++) {
				if(fork_sync_timer(PROC_TIMER, "WEBSOCKET KEEPALIVE", 1,
						   ws_keepalive, (void*)(long)i, ws_keepalive_interval)
						< 0) {
					LM_ERR("starting keepalive process\n");
					return -1;
				}
			}
		}
		if(fork_sync_timer(PROC_TIMER, "WEBSOCKET TIMER", 1,
			   ws_timer, NULL, ws_timer_interval)
					< 0) {
				LM_ERR("starting timer process\n");
				return -1;
		}

	}

	return 0;
}

static void destroy(void)
{
	wsconn_destroy();
}

static int ws_close_fixup(void **param, int param_no)
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

static int pv_get_ws_conid_f(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(msg == NULL)
		return -1;

	return pv_get_sintval(msg, param, res, msg->rcv.proto_reserved1);
}

/* clang-format off */
static const char* ws_rpc_dump_doc[2] = {
	"List websocket connections",
	0
};

static const char* ws_rpc_close_doc[2] = {
	"Close a websocket connection by id",
	0
};

static const char* ws_rpc_ping_doc[2] = {
	"Send ping on a websocket connection by id",
	0
};

static const char* ws_rpc_pong_doc[2] = {
	"Send pong on a websocket connection by id",
	0
};

static const char* ws_rpc_enable_doc[2] = {
	"Enable websocket connection handling",
	0
};

static const char* ws_rpc_disable_doc[2] = {
	"Disable websocket connection handling",
	0
};

rpc_export_t ws_rpc_cmds[] = {
	{"ws.dump", ws_rpc_dump, ws_rpc_dump_doc, 0},
	{"ws.close", ws_rpc_close, ws_rpc_close_doc, 0},
	{"ws.ping", ws_rpc_ping, ws_rpc_ping_doc, 0},
	{"ws.pong", ws_rpc_pong, ws_rpc_pong_doc, 0},
	{"ws.enable", ws_rpc_enable, ws_rpc_enable_doc, 0},
	{"ws.disable", ws_rpc_disable, ws_rpc_disable_doc, 0},
	{0, 0, 0, 0}
};
/* clang-format on */

/**
 * register RPC commands
 */
static int ws_init_rpc(void)
{
	if(rpc_register_array(ws_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_websocket_exports[] = {
	{ str_init("websocket"), str_init("handle_handshake"),
		SR_KEMIP_INT, ws_handle_handshake,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("websocket"), str_init("close"),
		SR_KEMIP_INT, ws_close,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("websocket"), str_init("close_reason"),
		SR_KEMIP_INT, ws_close2,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("websocket"), str_init("close_conid"),
		SR_KEMIP_INT, ws_close3,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_websocket_exports);
	return 0;
}

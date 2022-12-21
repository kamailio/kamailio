/*
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "../../core/ut.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/usr_avp.h"
#include "../../core/pt.h"
#include "../../core/hashes.h"
#include "../../core/mod_fix.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "dmq.h"
#include "dmq_funcs.h"
#include "bind_dmq.h"
#include "message.h"
#include "notification_peer.h"
#include "dmqnode.h"

MODULE_VERSION

int dmq_startup_time = 0;
int dmq_pid = 0;

/* module parameters */
int dmq_num_workers = DEFAULT_NUM_WORKERS;
int dmq_worker_usleep = 0;
str dmq_server_address = {0, 0};
str dmq_server_socket = {0, 0};
sip_uri_t dmq_server_uri = {0};

str_list_t *dmq_notification_address_list = NULL;
static str_list_t *dmq_tmp_list = NULL;
str dmq_notification_channel = str_init("notification_peer");
int dmq_multi_notify = 0;
static sip_uri_t dmq_notification_uri = {0};
int dmq_ping_interval = 60;

/* TM bind */
struct tm_binds tmb = {0};
/* SL API structure */
sl_api_t slb = {0};

/** module variables */
str dmq_request_method = str_init("KDMQ");
dmq_worker_t *dmq_workers = NULL;
dmq_peer_list_t *dmq_peer_list = 0;
/* the list of dmq servers */
dmq_node_list_t *dmq_node_list = NULL;
/* dmq module is a peer itself for receiving notifications regarding nodes */
dmq_peer_t *dmq_notification_peer = NULL;
/* add notification servers */
static int dmq_add_notification_address(modparam_t type, void * val);


/** module functions */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* clang-format off */
static cmd_export_t cmds[] = {
	{"dmq_handle_message", (cmd_function)dmq_handle_message, 0,
		0, 0, REQUEST_ROUTE},
	{"dmq_handle_message", (cmd_function)w_dmq_handle_message, 1,
		fixup_int_1, 0, REQUEST_ROUTE},
	{"dmq_process_message", (cmd_function)dmq_process_message, 0,
		0, 0, REQUEST_ROUTE},
	{"dmq_process_message", (cmd_function)w_dmq_process_message, 1,
		fixup_int_1, 0, REQUEST_ROUTE},
	{"dmq_send_message", (cmd_function)cfg_dmq_send_message, 4,
		fixup_spve_all, 0, ANY_ROUTE},
	{"dmq_bcast_message", (cmd_function)cfg_dmq_bcast_message, 3,
		fixup_spve_all, 0, ANY_ROUTE},
	{"dmq_t_replicate", (cmd_function)cfg_dmq_t_replicate, 0,
		0, 0, REQUEST_ROUTE},
	{"dmq_t_replicate", (cmd_function)cfg_dmq_t_replicate, 1,
		fixup_spve_null, 0, REQUEST_ROUTE},
	{"dmq_is_from_node", (cmd_function)cfg_dmq_is_from_node, 0,
		0, 0, REQUEST_ROUTE},
	{"bind_dmq", (cmd_function)bind_dmq, 0,
		0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"num_workers", INT_PARAM, &dmq_num_workers},
	{"ping_interval", INT_PARAM, &dmq_ping_interval},
	{"server_address", PARAM_STR, &dmq_server_address},
	{"server_socket", PARAM_STR, &dmq_server_socket},
	{"notification_address", PARAM_STR|USE_FUNC_PARAM, dmq_add_notification_address},
	{"notification_channel", PARAM_STR, &dmq_notification_channel},
	{"multi_notify", INT_PARAM, &dmq_multi_notify},
	{"worker_usleep", INT_PARAM, &dmq_worker_usleep},
	{0, 0, 0}
};

static rpc_export_t rpc_methods[];

/** module exports */
struct module_exports exports = {
	"dmq",				/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,			/* per-child init function */
	destroy				/* module destroy function */
};
/* clang-format on */


static int make_socket_str_from_uri(struct sip_uri *uri, str *socket)
{
	str sproto = STR_NULL;

	if(!uri->host.s || !uri->host.len) {
		LM_ERR("no host in uri\n");
		return -1;
	}

	socket->len = uri->host.len + uri->port.len + 7 /*sctp + : + : \0*/;
	socket->s = pkg_malloc(socket->len);
	if(socket->s == NULL) {
		LM_ERR("no more pkg\n");
		return -1;
	}

	if(get_valid_proto_string(uri->proto, 0, 0, &sproto)<0) {
		LM_INFO("unknown transport protocol - fall back to udp\n");
		sproto.s = "udp";
		sproto.len = 3;
	}

	memcpy(socket->s, sproto.s, sproto.len);
	socket->s[sproto.len] = ':';
	socket->len = sproto.len + 1;

	memcpy(socket->s + socket->len, uri->host.s, uri->host.len);
	socket->len += uri->host.len;

	if(uri->port.s && uri->port.len) {
		socket->s[socket->len++] = ':';
		memcpy(socket->s + socket->len, uri->port.s, uri->port.len);
		socket->len += uri->port.len;
	}
	socket->s[socket->len] = '\0';

	return 0;
}


/**
 * init module function
 */
static int mod_init(void)
{
	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb) == -1) {
		LM_ERR("can't load tm functions. TM module probably not loaded\n");
		return -1;
	}

	/* load peer list - the list containing the module callbacks for dmq */
	dmq_peer_list = init_peer_list();
	if(dmq_peer_list == NULL) {
		LM_ERR("cannot initialize peer list\n");
		return -1;
	}

	/* load the dmq node list - the list containing the dmq servers */
	dmq_node_list = init_dmq_node_list();
	if(dmq_node_list == NULL) {
		LM_ERR("cannot initialize node list\n");
		return -1;
	}

	if(rpc_register_array(rpc_methods) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* register worker processes - add one because of the ping process */
	register_procs(dmq_num_workers);

	/* check server_address and notification_address are not empty and correct */
	if(parse_uri(dmq_server_address.s, dmq_server_address.len, &dmq_server_uri)
			< 0) {
		LM_ERR("server address invalid\n");
		return -1;
	}

	if(dmq_server_socket.s==NULL || dmq_server_socket.len<=0) {
		/* create socket string out of the server_uri */
		if(make_socket_str_from_uri(&dmq_server_uri, &dmq_server_socket) < 0) {
			LM_ERR("failed to create socket out of server_uri\n");
			return -1;
		}
	}
	if(lookup_local_socket(&dmq_server_socket) == NULL) {
		LM_ERR("server_uri is not a socket the proxy is listening on\n");
		return -1;
	}

	/* allocate workers array */
	dmq_workers = shm_malloc(dmq_num_workers * sizeof(dmq_worker_t));
	if(dmq_workers == NULL) {
		LM_ERR("error in shm_malloc\n");
		return -1;
	}
	memset(dmq_workers, 0, dmq_num_workers * sizeof(dmq_worker_t));

	dmq_init_callback_done = shm_malloc(sizeof(int));
	if(!dmq_init_callback_done) {
		LM_ERR("no more shm\n");
		return -1;
	}
	*dmq_init_callback_done = 0;

	/**
	 * add the dmq notification peer.
	 * the dmq is a peer itself so that it can receive node notifications
	 */
	if(add_notification_peer() < 0) {
		LM_ERR("cannot add notification peer\n");
		return -1;
	}

	dmq_startup_time = (int)time(NULL);

	/**
	 * add the ping timer
	 * it pings the servers once in a while so that we know which failed
	 */
	if(dmq_ping_interval < MIN_PING_INTERVAL) {
		dmq_ping_interval = MIN_PING_INTERVAL;
	}
	if(register_timer(ping_servers, 0, dmq_ping_interval) < 0) {
		LM_ERR("cannot register timer callback\n");
		return -1;
	}

	return 0;
}

/**
 * initialize children
 */
static int child_init(int rank)
{
	int i, newpid;

	if(rank == PROC_TCP_MAIN) {
		/* do nothing for the tcp main process */
		return 0;
	}

	if(rank == PROC_INIT) {
		for(i = 0; i < dmq_num_workers; i++) {
			if (init_worker(&dmq_workers[i]) < 0) {
				LM_ERR("failed to init struct for worker[%d]\n", i);
				return -1;
			}
		}
		return 0;
	}

	if(rank == PROC_MAIN) {
		/* fork worker processes */
		for(i = 0; i < dmq_num_workers; i++) {
			LM_DBG("starting worker process %d\n", i);
			newpid = fork_process(PROC_RPC, "DMQ WORKER", 1);
			if(newpid < 0) {
				LM_ERR("failed to fork worker process %d\n", i);
				return -1;
			} else if(newpid == 0) {
				if (cfg_child_init()) return -1;
				/* child - this will loop forever */
				worker_loop(i);
			} else {
				dmq_workers[i].pid = newpid;
			}
		}
		return 0;
	}

	if(rank == PROC_SIPINIT) {
		/* notification_node - the node from which the Kamailio instance
		 * gets the server list on startup.
		 * the address is given as a module parameter in dmq_notification_address
		 * the module MUST have this parameter if the Kamailio instance is not
		 * a master in this architecture
		 */
		if(dmq_notification_address_list != NULL) {
			dmq_notification_node =
					add_server_and_notify(dmq_notification_address_list);
			if(!dmq_notification_node) {
				LM_WARN("cannot retrieve initial nodelist, first list entry %.*s\n",
						STR_FMT(&dmq_notification_address_list->s));

			}
		}
	}

	dmq_pid = my_pid();
	return 0;
}

/*
 * destroy function
 */
static void destroy(void)
{
	/* TODO unregister dmq node, free resources */
	if(dmq_notification_address_list && dmq_notification_node && dmq_self_node) {
		LM_DBG("unregistering node %.*s\n", STR_FMT(&dmq_self_node->orig_uri));
		dmq_self_node->status = DMQ_NODE_DISABLED;
		request_nodelist(dmq_notification_node, 1);
	}
	if(dmq_init_callback_done) {
		shm_free(dmq_init_callback_done);
	}
}

static int dmq_add_notification_address(modparam_t type, void * val)
{
	str tmp_str;
	int total_list = 0; /* not used */

	if(val==NULL) {
		LM_ERR("invalid notification address parameter value\n");
		return -1;
	}
	tmp_str.s = ((str*) val)->s;
	tmp_str.len = ((str*) val)->len;
	if(parse_uri(tmp_str.s,  tmp_str.len, &dmq_notification_uri) < 0) {
		LM_ERR("could not parse notification address\n");
		return -1;
	}

	/* initial allocation */
	if (dmq_notification_address_list == NULL) {
		dmq_notification_address_list = pkg_malloc(sizeof(str_list_t));
		if (dmq_notification_address_list == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		dmq_tmp_list = dmq_notification_address_list;
		dmq_tmp_list->s = tmp_str;
		dmq_tmp_list->next = NULL;
	} else {
		dmq_tmp_list = append_str_list(tmp_str.s, tmp_str.len, &dmq_tmp_list, &total_list);
		if (dmq_tmp_list == NULL) {
			LM_ERR("could not append to list\n");
			return -1;
		}
		LM_DBG("added new notification address to the list %.*s\n",
			dmq_tmp_list->s.len, dmq_tmp_list->s.s);
	}
	return 0;
}


static void dmq_rpc_list_nodes(rpc_t *rpc, void *c)
{
	void *h;
	dmq_node_t *cur = dmq_node_list->nodes;
	char ip[IP6_MAX_STR_SIZE + 1];

	while(cur) {
		memset(ip, 0, IP6_MAX_STR_SIZE + 1);
		ip_addr2sbuf(&cur->ip_address, ip, IP6_MAX_STR_SIZE);
		if(rpc->add(c, "{", &h) < 0)
			goto error;
		if(rpc->struct_add(h, "SSssSdd", "host", &cur->uri.host, "port",
				   &cur->uri.port, "proto", get_proto_name(cur->uri.proto),
				   "resolved_ip", ip, "status", dmq_get_status_str(cur->status),
				   "last_notification", cur->last_notification,
				   "local", cur->local)
				< 0)
			goto error;
		cur = cur->next;
	}
	return;
error:
	LM_ERR("Failed to add item to RPC response\n");
	rpc->fault(c, 500, "Server failure");
	return;
}

static const char *dmq_rpc_list_nodes_doc[2] = {"Print all nodes", 0};

void rpc_dmq_remove(rpc_t* rpc, void* ctx)
{
	str taddr = STR_NULL;

	if (rpc->scan(ctx, ".S", &taddr) < 1) {
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(dmq_node_del_by_uri(dmq_node_list, &taddr)<0) {
		rpc->fault(ctx, 500, "Failure");
		return;
	}
	rpc->rpl_printf(ctx, "Ok. DMQ node removed.");
}

static const char* rpc_dmq_remove_doc[3] = {
	"Remove a DMQ node",
	"address - the DMQ node address",
	0
};

static rpc_export_t rpc_methods[] = {
	{"dmq.list_nodes", dmq_rpc_list_nodes, dmq_rpc_list_nodes_doc, RET_ARRAY},
	{"dmq.remove",     rpc_dmq_remove,     rpc_dmq_remove_doc, 0},
	{0, 0, 0, 0}
};

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_dmq_exports[] = {
	{ str_init("dmq"), str_init("handle_message"),
		SR_KEMIP_INT, ki_dmq_handle_message,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("handle_message_rc"),
		SR_KEMIP_INT, ki_dmq_handle_message_rc,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("process_message"),
		SR_KEMIP_INT, ki_dmq_process_message,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("process_message_rc"),
		SR_KEMIP_INT, ki_dmq_process_message_rc,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("is_from_node"),
		SR_KEMIP_INT, is_from_remote_node,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("t_replicate"),
		SR_KEMIP_INT, ki_dmq_t_replicate,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("t_replicate_mode"),
		SR_KEMIP_INT, ki_dmq_t_replicate_mode,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("send_message"),
		SR_KEMIP_INT, ki_dmq_send_message,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dmq"), str_init("bcast_message"),
		SR_KEMIP_INT, ki_dmq_bcast_message,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_dmq_exports);
	return 0;
}

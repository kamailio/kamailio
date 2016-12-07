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

#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../usr_avp.h"
#include "../../pt.h"
#include "../../lib/kmi/mi.h"
#include "../../hashes.h"
#include "../../mod_fix.h"
#include "../../rpc_lookup.h"

#include "dmq.h"
#include "dmq_funcs.h"
#include "bind_dmq.h"
#include "message.h"
#include "notification_peer.h"
#include "dmqnode.h"

static int mod_init(void);
static int child_init(int);
static void destroy(void);

MODULE_VERSION

int startup_time = 0;
int pid = 0;

/* module parameters */
int num_workers = DEFAULT_NUM_WORKERS;
str dmq_server_address = {0, 0};
str dmq_server_socket = {0, 0};
struct sip_uri dmq_server_uri;

str dmq_notification_address = {0, 0};
int multi_notify = 0;
struct sip_uri dmq_notification_uri;
int ping_interval = 60;

/* TM bind */
struct tm_binds tmb;
/* SL API structure */
sl_api_t slb;

/** module variables */
str dmq_request_method = str_init("KDMQ");
dmq_worker_t* workers = NULL;
dmq_peer_list_t* peer_list = 0;
/* the list of dmq servers */
dmq_node_list_t* node_list = NULL;
// the dmq module is a peer itself for receiving notifications regarding nodes
dmq_peer_t* dmq_notification_peer = NULL;

/** module functions */
static int mod_init(void);
static int child_init(int);
static void destroy(void);
static int handle_dmq_fixup(void** param, int param_no);
static int send_dmq_fixup(void** param, int param_no);
static int bcast_dmq_fixup(void** param, int param_no);

static cmd_export_t cmds[] = {
	{"dmq_handle_message",  (cmd_function)dmq_handle_message, 0, handle_dmq_fixup, 0, 
		REQUEST_ROUTE},
	{"dmq_send_message", (cmd_function)cfg_dmq_send_message, 4, send_dmq_fixup, 0,
		ANY_ROUTE},
        {"dmq_bcast_message", (cmd_function)cfg_dmq_bcast_message, 3, bcast_dmq_fixup, 0,
                ANY_ROUTE},
	{"dmq_t_replicate",  (cmd_function)cfg_dmq_t_replicate, 0, 0, 0,
		REQUEST_ROUTE},
        {"dmq_t_replicate",  (cmd_function)cfg_dmq_t_replicate, 1, fixup_spve_null, 0,
                REQUEST_ROUTE},
        {"dmq_is_from_node",  (cmd_function)cfg_dmq_is_from_node, 0, 0, 0,
                REQUEST_ROUTE},
        {"bind_dmq",        (cmd_function)bind_dmq,       0, 0,              0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"num_workers", INT_PARAM, &num_workers},
	{"ping_interval", INT_PARAM, &ping_interval},
	{"server_address", PARAM_STR, &dmq_server_address},
	{"notification_address", PARAM_STR, &dmq_notification_address},
	{"multi_notify", INT_PARAM, &multi_notify},
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{0, 0, 0, 0, 0}
};

static rpc_export_t rpc_methods[];

/** module exports */
struct module_exports exports = {
	"dmq",				/* module name */
	DEFAULT_DLFLAGS,		/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,				/* exported statistics */
	mi_cmds,   			/* exported MI functions */
	0,				/* exported pseudo-variables */
	0,				/* extra processes */
	mod_init,			/* module initialization function */
	0,   				/* response handling function */
	(destroy_function) destroy, 	/* destroy function */
	child_init                  	/* per-child init function */
};


static int make_socket_str_from_uri(struct sip_uri *uri, str *socket) {
	if(!uri->host.s || !uri->host.len) {
		LM_ERR("no host in uri\n");
		return -1;
	}

	socket->len = uri->host.len + uri->port.len + 6;
	socket->s = pkg_malloc(socket->len);
	if(socket->s==NULL) {
		LM_ERR("no more pkg\n");
		return -1;
	}
	memcpy(socket->s, "udp:", 4);
	socket->len = 4;

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
	
	if(register_mi_mod(exports.name, mi_cmds)!=0) {
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* load all TM stuff */
	if(load_tm_api(&tmb)==-1) {
		LM_ERR("can't load tm functions. TM module probably not loaded\n");
		return -1;
	}

	/* load peer list - the list containing the module callbacks for dmq */
	peer_list = init_peer_list();
	if(peer_list==NULL) {
		LM_ERR("cannot initialize peer list\n");
		return -1;
	}

	/* load the dmq node list - the list containing the dmq servers */
	node_list = init_dmq_node_list();
	if(node_list==NULL) {
		LM_ERR("cannot initialize node list\n");
		return -1;
	}

	if (rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* register worker processes - add one because of the ping process */
	register_procs(num_workers);
	
	/* check server_address and notification_address are not empty and correct */
	if(parse_uri(dmq_server_address.s, dmq_server_address.len, &dmq_server_uri) < 0) {
		LM_ERR("server address invalid\n");
		return -1;
	}

	if(parse_uri(dmq_notification_address.s, dmq_notification_address.len, &dmq_notification_uri) < 0) {
		LM_ERR("notification address invalid\n");
		return -1;
	}

	/* create socket string out of the server_uri */
	if(make_socket_str_from_uri(&dmq_server_uri, &dmq_server_socket) < 0) {
		LM_ERR("failed to create socket out of server_uri\n");
		return -1;
	}
	if (lookup_local_socket(&dmq_server_socket) == NULL) {
		LM_ERR("server_uri is not a socket the proxy is listening on\n");
		return -1;
	}

	/* allocate workers array */
	workers = shm_malloc(num_workers * sizeof(*workers));
	if(workers == NULL) {
		LM_ERR("error in shm_malloc\n");
		return -1;
	}

	dmq_init_callback_done = shm_malloc(sizeof(int));
	if (!dmq_init_callback_done) {
		LM_ERR("no more shm\n");
		return -1;
	}
	*dmq_init_callback_done = 0;

	/**
	 * add the dmq notification peer.
	 * the dmq is a peer itself so that it can receive node notifications
	 */
	if(add_notification_peer()<0) {
		LM_ERR("cannot add notification peer\n");
		return -1;
	}

	startup_time = (int) time(NULL);

	/**
	 * add the ping timer
	 * it pings the servers once in a while so that we know which failed
	 */
	if(ping_interval < MIN_PING_INTERVAL) {
		ping_interval = MIN_PING_INTERVAL;
	}
	if(register_timer(ping_servers, 0, ping_interval)<0) {
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
	if (rank == PROC_MAIN) {
		/* fork worker processes */
		for(i = 0; i < num_workers; i++) {
			init_worker(&workers[i]);
			LM_DBG("starting worker process %d\n", i);
			newpid = fork_process(PROC_NOCHLDINIT, "DMQ WORKER", 0);
			if(newpid < 0) {
				LM_ERR("failed to form process\n");
				return -1;
			} else if(newpid == 0) {
				/* child - this will loop forever */
				worker_loop(i);
			} else {
				workers[i].pid = newpid;
			}
		}
		/* notification_node - the node from which the Kamailio instance
		 * gets the server list on startup.
		 * the address is given as a module parameter in dmq_notification_address
		 * the module MUST have this parameter if the Kamailio instance is not
		 * a master in this architecture
		 */
		if(dmq_notification_address.s) {
			notification_node = add_server_and_notify(&dmq_notification_address);
			if(!notification_node) {
				LM_ERR("cannot retrieve initial nodelist from %.*s\n",
				       STR_FMT(&dmq_notification_address));
				return -1;
			}
		}
		return 0;
	}
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN) {
		/* do nothing for the main process */
		return 0;
	}

	pid = my_pid();
	return 0;
}

/*
 * destroy function
 */
static void destroy(void) {
	/* TODO unregister dmq node, free resources */
	if(dmq_notification_address.s && notification_node && self_node) {
		LM_DBG("unregistering node %.*s\n", STR_FMT(&self_node->orig_uri));
		self_node->status = DMQ_NODE_DISABLED;
		request_nodelist(notification_node, 1);
	}
	if (dmq_server_socket.s) {
		pkg_free(dmq_server_socket.s);
	}
	if (dmq_init_callback_done) {
		shm_free(dmq_init_callback_done);
	}
}

static int handle_dmq_fixup(void** param, int param_no)
{
 	return 0;
}

static int send_dmq_fixup(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}

static int bcast_dmq_fixup(void** param, int param_no)
{
        return fixup_spve_null(param, 1);
}

static void dmq_rpc_list_nodes(rpc_t *rpc, void *c)
{
	void *h;
	dmq_node_t* cur = node_list->nodes;
	char ip[IP6_MAX_STR_SIZE + 1];

	while(cur) {
		memset(ip, 0, IP6_MAX_STR_SIZE + 1);
		ip_addr2sbuf(&cur->ip_address, ip, IP6_MAX_STR_SIZE);
		if (rpc->add(c, "{", &h) < 0) goto error;
		if (rpc->struct_add(h, "SSsddd",
			"host", &cur->uri.host,
			"port", &cur->uri.port,
			"resolved_ip", ip,
			"status", cur->status,
			"last_notification", cur->last_notification,
			"local", cur->local) < 0) goto error;
		cur = cur->next;
	}
	return;
error:
	LM_ERR("Failed to add item to RPC response\n");
	return;

}

static const char *dmq_rpc_list_nodes_doc[2] = {
	"Print all nodes", 0
};

static rpc_export_t rpc_methods[] = {
	{"dmq.list_nodes", dmq_rpc_list_nodes, dmq_rpc_list_nodes_doc, RET_ARRAY},
	{0, 0, 0, 0}
};

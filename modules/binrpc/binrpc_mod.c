/*
 * $Id: ctl.c,v 1.3 2007/01/18 20:35:01 andrei Exp $
 *
 * Copyright (C) 2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* History:
 * --------
 *  2006-02-08  created by andrei
 *  2007-01-18  use PROC_RPC rank when forking (andrei)
 *  2007        ported to libbinrpc (bpintea)
 */



#include "../../sr_module.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../pt.h"
#include "ctrl_socks.h"
#include "io_listener.h"

#include <sys/types.h>
#include <sys/socket.h> /* socketpair */
#include <unistd.h> /* close, fork, getpid */
#include <stdio.h> /* snprintf */
#include <string.h> /* strerror */
#include <signal.h> /* kill */
#include <errno.h>

#include <binrpc.h>
#include "../../mem/wrappers.h"

MODULE_VERSION

#include "modbrpc_defaults.h"
#include "libbinrpc_wrapper.h"
#include "proctab.h"
#ifdef USE_FIFO
#include "fifo_server.h"
#endif

static int mod_init(void);
static int mod_child(int rank);
static void mod_destroy(void);

static cmd_export_t cmds[]={
		{0,0,0,0,0}
};


static int usock_mode=0600; /* permissions, default rw-------*/
static int usock_uid=-1; /* username and group for the unix sockets*/
static int usock_gid=-1;
static int workers = 1; /* how many processes to start */

static int append_binrpc_listener(modparam_t type, void * val);
#ifdef USE_FIFO
static int append_fifo_socket(modparam_t type, void * val);
#endif
static int fix_user(modparam_t type, void * val);
static int fix_group(modparam_t type, void * val);


void io_listen_who_rpc(rpc_t* rpc, void* ctx);
void io_listen_conn_rpc(rpc_t* rpc, void* ctx);
#ifdef EXTRA_DEBUG
static void ctrl_listen_pid(rpc_t *rpc, void *ctx);
#endif


static char* io_listen_who_doc[]={ "list open connections", 0 };
static char* io_listen_conn_doc[]={ "returns number of open connections", 0 };
#ifdef EXTRA_DEBUG
static char *ctrl_listen_pid_doc[] = {"list serving's process PID", NULL};
#endif

static rpc_export_t ctl_rpc[]={
	{"binrpc.who",         io_listen_who_rpc, (const char**)io_listen_who_doc,
			0},
	{"binrpc.connections", io_listen_conn_rpc,(const char**)io_listen_conn_doc,
			0},
	{"binrpc.listen",      ctrl_listen_ls_rpc,(const char**)ctl_listen_ls_doc,
			0},
#ifdef EXTRA_DEBUG
	{"binrpc.pid", ctrl_listen_pid, (const char **)ctrl_listen_pid_doc, 0},
#endif
	{ 0, 0, 0, 0}
};

static param_export_t params[]={
#ifdef USE_FIFO
	{"fifo",	PARAM_STRING|PARAM_USE_FUNC,	(void*)append_fifo_socket},
#endif
	{"listen",	PARAM_STRING|PARAM_USE_FUNC,	(void*)append_binrpc_listener},
	{"mode",	PARAM_INT,						&usock_mode				 },
	{"user",	PARAM_STRING|PARAM_USE_FUNC,	fix_user				 },
	{"group",	PARAM_STRING|PARAM_USE_FUNC,	fix_group				 },
	{"workers",	PARAM_INT,						&workers},
	{"force_reply", PARAM_INT,					&force_reply},
	{0,0,0} 
}; /* no params */

struct module_exports exports= {
	"binrpc",
	cmds,
	ctl_rpc,        /* RPC methods */
	params,
	mod_init, /* module initialization function */
	0, /* response function */
	mod_destroy,  /* destroy function */
	0, /* on_cancel function */
	mod_child, /* per-child init function */
};

static char **sock_paths = NULL;


static int append_binrpc_listener(modparam_t type, void * val)
{
	char *s;
	
	if ((type & PARAM_STRING)==0){
		BUG("binrpc: bad parameter type %d (expected string).\n", type);
		return -1;
	}
	s=(char*)val;
	return add_binrpc_listener(s);
}


#ifdef USE_FIFO
static int append_fifo_socket(modparam_t type, void * val)
{
	char *s;
	
	if ((type & PARAM_STRING)==0){
		BUG("add_fifo: bad parameter type %d (expected string).\n", type);
		return -1;
	}
	s=(char*)val;
	return add_fifo_socket(s);
}
#endif /* USE_FIFO */



static int fix_user(modparam_t type, void * val)
{
	char* s;
	
	if ((type & PARAM_STRING)==0){
		BUG("fix_user: bad parameter type %d\n", type);
		goto error;
	}
	s=(char*)val;
	if (user2uid(&usock_uid, 0, s)<0){
		ERR("bad user name/uid number %s\n", s);
		goto error;
	}
	return 0;
error:
	return -1;
}



static int fix_group(modparam_t type, void * val)
{
	char* s;
	
	if ((type & PARAM_STRING)==0){
		BUG("fix_group: bad parameter type %d\n", type);
		goto error;
	}
	s=(char*)val;
	if (group2gid(&usock_gid, s)<0){
		ERR("bad group name/gid number %s\n", s);
		goto error;
	}
	return 0;
error:
	return -1;
}



static int mod_init(void)
{
	struct ctrl_socket *csock;
	char *sock_proto_name;
	int have_stream = 0;
	
	brpc_mem_setup(w_pkg_calloc, w_pkg_malloc, w_pkg_free, w_pkg_realloc);

	if (ctrl_sock_lst==0) {
		add_binrpc_listener(DEFAULT_MODBRPC_SOCKET);
	}
	DEBUG("BINRPC listening on:\n");
	for (csock = ctrl_sock_lst; csock; csock = csock->next) {
		sock_proto_name = socket_proto_name(&csock->addr);
		DEBUG("        [%s:%s]  %s\n", 
#ifdef USE_FIFO
				payload_proto_name(csock->p_proto),
#else
				payload_proto_name(P_BINRPC),
#endif
				sock_proto_name, csock->name);

		/* initialize now all sockets. UDP receivers will be shut down later,
		 * if using TCP receiver */
		if (init_ctrl_socket(csock, usock_mode, usock_uid, usock_gid) < 0) {
			ERR("socket initialization for %s failed (%s).\n", 
					csock->name, strerror(errno));
			return -1;
		}
		if (BRPC_ADDR_TYPE(&csock->addr) == SOCK_STREAM)
			have_stream ++;
	}
	
	if (ctrl_sock_lst){
		/* will we be creating an extra process: conn. listener? */
		have_connection_listener = (1 < workers) && (0 < have_stream);
		/* we will fork */
		register_procs(workers + 
				((have_connection_listener) ? /*listener*/1 : 0));

		if (! (sock_paths = ctrl_sock_paths()))
			/* at least NULL maker must be present */
			return -1;
	}

#ifdef USE_FIFO
	fifo_rpc_init();
#endif
	return 0;
}

void release_sock_paths(int do_unlink)
{
	char **path;
	if (! sock_paths)
		/* not MAIN */
		return;
	for (path = &sock_paths[0]; *path; path ++) {
		if (do_unlink && (unlink(*path) < 0))
			WARN("failed to delete unix socket `%s': %s [%d].\n",
					*path, strerror(errno), errno);
		free(*path);
	}
	free(sock_paths);
	sock_paths = NULL;
}


static int fork_workers(void)
{
	int i;
	pid_t pid;
	int sockpair[2];

	if (have_connection_listener) {
		if (pt_init(workers) < 0)
			return -1;
	}
	for (i = 0; i < workers; i ++) {
		if (have_connection_listener) {
			if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockpair) < 0) {
				ERR("failed to create socket pair: %s [%d].\n", 
						strerror(errno), errno);
				return -1;
			}
		}
		switch ((pid = fork_process(PROC_RPC, "binrpc worker", 
					/* support for SIP TCP */1))) {
			case 0:  /* child */
				if (have_connection_listener) {
					/* close only stream listeners; they must not be removed as
					 * will be ref'ed with connection file descriptors passed
					 * by connection listener  */
					close_binrpc_listeners(SOCKLIST_DGRAM);
					/* don't need proc. table in workers */
					pt_free(/* don't have CSs initialized */0);
					close(sockpair[0]); /* parent's end */
					if (! add_fdpass_socket(sockpair[1], pid))
						return -1;
				}
				io_listen_loop(ctrl_sock_lst);
				return -1;
			case -1: /*err*/
				ERR("failed to spawn BINRPC worker: %s [%d].\n", 
						strerror(errno), errno);
				return -1;
			default: /*parent*/
				DEBUG("new BINRPC worker spawned; pid: %d.\n", pid);
				if (have_connection_listener) {
					close(sockpair[1]); /* child's end */
					if (pt_ins(pid, sockpair[0]) < 0)
						return -1;
				}
		}
	}
	if (have_connection_listener) {
		switch ((pid = fork_process(PROC_RPC, "binrpc connection listener", 
				/* won't use SIP TCP */0))) {
			case 0: /*child*/
				/* remove datagram listeners */
				DEBUG("removing dgram listeners from conn listener worker.\n");
				del_binrpc_listeners(SOCKLIST_STREAM|SOCKLIST_FDPASS);
				if (pt_fd2cs() < 0)
					return -1;
				io_listen_loop(ctrl_sock_lst);
				return 0; /* formal */
			case -1: /* err */
				ERR("failed to spawn binrpc connection listener: %s [%d].\n",
						strerror(errno), errno);
				return -1;
			default: /* parent */
				DEBUG("BINRPC stream receiver spawned; pid: %d.\n", pid);
		}
	}
	return 0;
}

static int mod_child(int rank)
{
	/* do nothing from PROC_INIT, is the same as PROC_MAIN */
	if (rank==PROC_INIT)
		return 0;
	/* we want to fork(), but only from one process */
	if ((rank == PROC_MAIN ) && (ctrl_sock_lst)){
		DEBUG("mod_child(%d), ctrl_sock_lst=%p\n", rank, ctrl_sock_lst);
		if (fork_workers() < 0)
			goto error;
	}

	if (rank!=PROC_RPC){
		/* close all the opened fds, we don't need them here */
		DEBUG("removing BINRPC listeners from non RPC worker.\n");
		del_binrpc_listeners(SOCKLIST_NONE);
	}
	return 0;
error:
	return -1;
}


static void mod_destroy(void)
{
	DEBUG("destroying BINRPC listeners.\n");
	del_binrpc_listeners(SOCKLIST_NONE);
	release_sock_paths(is_main);
}


#ifdef EXTRA_DEBUG
static void ctrl_listen_pid(rpc_t *rpc, void *ctx)
{
	rpc->add(ctx, "d", my_pid());
}
#endif

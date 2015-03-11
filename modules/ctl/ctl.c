/*
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 */

#include "../../sr_module.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../pt.h"
#include "../../cfg/cfg_struct.h"
#include "ctrl_socks.h"
#include "io_listener.h"

#include <sys/types.h>
#include <sys/socket.h> /* socketpair */
#include <unistd.h> /* close, fork, getpid */
#include <stdio.h> /* snprintf */
#include <string.h> /* strerror */
#include <errno.h>

MODULE_VERSION

#include "ctl_defaults.h"
#include "binrpc_run.h"
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

/* if set try to automatically convert values to the requested type in
   rpc->scan (default: not set) */
extern int autoconvert; 
extern int binrpc_max_body_size;
extern int binrpc_struct_max_body_size;

static int add_binrpc_socket(modparam_t type, void * val);
#ifdef USE_FIFO
static int add_fifo_socket(modparam_t type, void * val);
#endif
static int fix_user(modparam_t type, void * val);
static int fix_group(modparam_t type, void * val);


static void  ctrl_listen_ls_rpc(rpc_t* rpc, void* ctx);
void io_listen_who_rpc(rpc_t* rpc, void* ctx);
void io_listen_conn_rpc(rpc_t* rpc, void* ctx);


static char* io_listen_who_doc[]={ "list open connections", 0 };
static char* io_listen_conn_doc[]={ "returns number of open connections", 0 };
static char* ctl_listen_ls_doc[]={ "list ctl listen sockets", 0 };

static rpc_export_t ctl_rpc[]={
	{"ctl.who",         io_listen_who_rpc, (const char**)io_listen_who_doc, 0},
	{"ctl.connections", io_listen_conn_rpc,(const char**)io_listen_conn_doc,0},
	{"ctl.listen",      ctrl_listen_ls_rpc,(const char**)ctl_listen_ls_doc, 0},
	{ 0, 0, 0, 0}
};

static param_export_t params[]={
#ifdef USE_FIFO
	{"fifo",		PARAM_STRING|PARAM_USE_FUNC,	(void*) add_fifo_socket	 },
#endif
	{"binrpc",		PARAM_STRING|PARAM_USE_FUNC,	(void*) add_binrpc_socket},
	{"mode",		PARAM_INT,						&usock_mode				 },
	{"user",		PARAM_STRING|PARAM_USE_FUNC,	fix_user				 },
	{"group",		PARAM_STRING|PARAM_USE_FUNC,	fix_group				 },
	{"autoconversion",	PARAM_INT,					&autoconvert			 },
	{"binrpc_max_body_size",        PARAM_INT, &binrpc_max_body_size         },
	{"binrpc_struct_max_body_size", PARAM_INT, &binrpc_struct_max_body_size  },
	{0,0,0} 
}; /* no params */

struct module_exports exports= {
	"ctl",
	cmds,
	ctl_rpc,        /* RPC methods */
	params,
	mod_init, /* module initialization function */
	0, /* response function */
	mod_destroy,  /* destroy function */
	0, /* on_cancel function */
	mod_child, /* per-child init function */
};


static struct id_list* listen_lst=0; /* binrpc sockets name list */
static struct ctrl_socket* ctrl_sock_lst=0;
static int fd_no=0; /* number of fd used */


static int add_binrpc_socket(modparam_t type, void * val)
{
	char *s;
	struct id_list* id;
	
	if ((type & PARAM_STRING)==0){
		LOG(L_CRIT, "BUG: ctl: add_binrpc_socket: bad parameter type %d\n",
					type);
		goto error;
	}
	s=(char*)val;
	id=parse_listen_id(s, strlen(s), UDP_SOCK); /* default udp proto */
	if (id==0){
		LOG(L_ERR, "ERROR: ctl: bad listen socket: \"%s\"\n", s);
		goto error;
	}
	id->data_proto=P_BINRPC;
	id->next=listen_lst;
	listen_lst=id;
	return 0;
error:
	return -1;
}



#ifdef USE_FIFO
static int add_fifo_socket(modparam_t type, void * val)
{
	char *s;
	struct id_list* id;
	
	if ((type & PARAM_STRING)==0){
		LOG(L_CRIT, "BUG: ctl: add_fifo: bad parameter type %d\n",
					type);
		goto error;
	}
	s=(char*)val;
	id=parse_listen_id(s, strlen(s), FIFO_SOCK); /* default udp proto */
	if (id==0){
		LOG(L_ERR, "ERROR: ctl: bad fifo: \"%s\"\n", s);
		goto error;
	}
	id->data_proto=P_FIFO;
	id->next=listen_lst;
	listen_lst=id;
	return 0;
error:
	return -1;
}
#endif



static int fix_user(modparam_t type, void * val)
{
	char* s;
	
	if ((type & PARAM_STRING)==0){
		LOG(L_CRIT, "BUG: ctl: fix_user: bad parameter type %d\n",
					type);
		goto error;
	}
	s=(char*)val;
	if (user2uid(&usock_uid, 0, s)<0){
		LOG(L_ERR, "ERROR: ctl: bad user name/uid number %s\n", s);
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
		LOG(L_CRIT, "BUG: ctl: fix_group: bad parameter type %d\n",
					type);
		goto error;
	}
	s=(char*)val;
	if (group2gid(&usock_gid, s)<0){
		LOG(L_ERR, "ERROR: ctl: bad group name/gid number %s\n", s);
		goto error;
	}
	return 0;
error:
	return -1;
}



static int mod_init(void)
{
	struct id_list* l;

	binrpc_callbacks_init();

	if(binrpc_max_body_size<=0)
		binrpc_max_body_size = 4;
	if(binrpc_struct_max_body_size<=0)
		binrpc_struct_max_body_size = 1;
	binrpc_max_body_size *= 1024;
	binrpc_struct_max_body_size *= 1024;

	if (listen_lst==0) {
		add_binrpc_socket(PARAM_STRING, DEFAULT_CTL_SOCKET);
	}
	DBG("listening on:\n");
	for (l=listen_lst; l; l=l->next){
		fd_no++;
		switch(l->proto){
			case UNIXD_SOCK:
				DBG("        [%s:unix dgram]  %s\n",
						payload_proto_name(l->data_proto), l->name);
				break;
			case UNIXS_SOCK:
				DBG("        [%s:unix stream] %s\n",
						payload_proto_name(l->data_proto), l->name);
				break;
			case UDP_SOCK:
				DBG("        [%s:udp]         %s:%d\n", 
						payload_proto_name(l->data_proto), l->name,
						l->port?l->port:DEFAULT_CTL_PORT);
				break;
			case TCP_SOCK:
				DBG("        [%s:tcp]         %s:%d\n",
						payload_proto_name(l->data_proto), l->name,
						l->port?l->port:DEFAULT_CTL_PORT);
				break;
#ifdef USE_FIFO
			case FIFO_SOCK:
				DBG("        [%s:fifo]         %s\n",
						payload_proto_name(l->data_proto), l->name);
				fd_no++; /* fifos use 2 fds */
				break;
#endif
			default:
				LOG(L_CRIT, "BUG: ctrl: listen protocol %d not supported\n",
						l->proto);
				goto error;
			}
	}
	/* get the uid/gid from core if not set for the module */
	if(usock_uid==-1 && sock_uid!=-1) usock_uid = sock_uid;
	if(usock_gid==-1 && sock_gid!=-1) usock_gid = sock_gid;
	/* open socket now, before suid */
	if (init_ctrl_sockets(&ctrl_sock_lst, listen_lst, DEFAULT_CTL_PORT,
			usock_mode, usock_uid, usock_gid)<0){
		LOG(L_ERR, "ERROR: ctl: mod_init: init ctrl. sockets failed\n");
		goto error;
	}
	if (ctrl_sock_lst){
		/* we will fork */
		register_procs(1); /* we will be creating an extra process */
		register_fds(fd_no);
		/* The child process will keep updating its local configuration */
		cfg_register_child(1);
	}
#ifdef USE_FIFO
	fifo_rpc_init();
#endif
	return 0;
error:
	return -1;
}



static int mod_child(int rank)
{
	int pid;
	struct ctrl_socket* cs;
	static int rpc_handler=0;
	
	/* do nothing from PROC_INIT, is the same as PROC_MAIN */
	if (rank==PROC_INIT)
		return 0;
	/* we want to fork(), but only from one process */
	if ((rank == PROC_MAIN ) && (ctrl_sock_lst)){ /* FIXME: no fork ?? */
		DBG("ctl: mod_child(%d), ctrl_sock_lst=%p\n", rank, ctrl_sock_lst);
		/* fork, but make sure we know not to close our own sockets when
		 * ctl child_init will be called for the new child */
		rpc_handler=1;
		/* child should start with a correct estimated used fds number*/
		register_fds(MAX_IO_READ_CONNECTIONS);
		pid=fork_process(PROC_RPC, "ctl handler", 1);
		DBG("ctl: mod_child(%d), fork_process=%d, csl=%p\n",
				rank, pid, ctrl_sock_lst);
		if (pid<0){			
			goto error;
		}
		if (pid == 0){ /* child */
			is_main=0;
			DBG("ctl: %d io_listen_loop(%d, %p)\n",
					rank, fd_no, ctrl_sock_lst);
			io_listen_loop(fd_no, ctrl_sock_lst);
		}else{ /* parent */
			/* not used in parent */
			register_fds(-MAX_IO_READ_CONNECTIONS);
			rpc_handler=0;
		}
	}
	if (rank!=PROC_RPC || !rpc_handler){
		/* close all the opened fds, we don't need them here */
		for (cs=ctrl_sock_lst; cs; cs=cs->next){
			close(cs->fd);
			cs->fd=-1;
			if (cs->write_fd!=-1){
				close(cs->write_fd);
				cs->write_fd=-1;
			}
		}
		if (rank!=PROC_MAIN){ /* we need the lists in main for on_exit cleanup,
								 see mod_destroy */
			/* free memory, we don't need the lists anymore */
			free_ctrl_socket_list(ctrl_sock_lst);
			ctrl_sock_lst=0;
			free_id_list(listen_lst);
			listen_lst=0;
		}
	}
	return 0;
error:
	return -1;
}



static void mod_destroy(void)
{
	struct ctrl_socket* cs;
	
	/* close all the opened fds & unlink the files */
	for (cs=ctrl_sock_lst; cs; cs=cs->next){
		switch(cs->transport){
			case UNIXS_SOCK:
			case UNIXD_SOCK:
				close(cs->fd);
				cs->fd=-1;
				if (cs->write_fd!=-1){
					close(cs->write_fd);
					cs->write_fd=-1;
				}
				if (cs->name){
					if (unlink(cs->name)<0){
						LOG(L_ERR, "ERROR: ctl: could not delete unix"
									" socket %s: %s (%d)\n",
									cs->name, strerror(errno), errno);
					}
				}
				break;
#ifdef USE_FIFO
			case FIFO_SOCK:
				destroy_fifo(cs->fd, cs->write_fd, cs->name);
				break;
#endif
			default:
				close(cs->fd);
				cs->fd=-1;
				if (cs->write_fd!=-1){
					close(cs->write_fd);
					cs->write_fd=-1;
				}
		}
	}
	if (listen_lst){
		free_id_list(listen_lst);
		listen_lst=0;
	}
	if (ctrl_sock_lst){
		free_ctrl_socket_list(ctrl_sock_lst);
		ctrl_sock_lst=0;
	}
}


static void  ctrl_listen_ls_rpc(rpc_t* rpc, void* ctx)
{
	struct ctrl_socket* cs;
	
	for (cs=ctrl_sock_lst; cs; cs=cs->next){
		rpc->add(ctx, "ssss", payload_proto_name(cs->p_proto),
						socket_proto_name(cs->transport),
						cs->name, (cs->port)?int2str(cs->port, 0):"");
	}
}

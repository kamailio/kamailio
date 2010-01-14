/*
 * $Id: ctrl_socks.c,v 1.2 2006/02/23 21:14:02 andrei Exp $
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* History:
 * --------
 *  2006-02-14  created by andrei
 *  2007        ported to libbinrpc (bpintea)
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "ctrl_socks.h"
#include "init_socks.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"

#include "proctab.h"
#ifdef USE_FIFO
#include "fifo_server.h"
#endif

struct ctrl_socket* ctrl_sock_lst=0;

int init_ctrl_socket(struct ctrl_socket* ctrl_sock, int perm, int uid, int gid)
{
	int s;
#ifdef USE_FIFO
	int extra_fd;
#endif

	switch (ctrl_sock->addr.domain) {
		case PF_LOCAL:
			s = init_unix_sock(&ctrl_sock->addr, perm, uid, gid);
			break;
		case PF_INET:
		case PF_INET6:
			s = init_tcpudp_sock(&ctrl_sock->addr);
			break;
#ifdef USE_FIFO
		case PF_FIFO:
			s=init_fifo_fd(ctrl_sock->name, perm, uid, gid, &extra_fd);
			ctrl_sock->write_fd = extra_fd;
			break;
#endif
		default:
			BUG("unknown domain %d.\n", ctrl_sock->addr.domain);
			return -1;
	}
	
	if (s < 0) {
		ERR("failed to setup socket for URI `%s'.\n", CTRLSOCK_URI(ctrl_sock));
		return -1;
	}
	ctrl_sock->fd = s;
	return 0;
}


int add_binrpc_listener(char *s)
{
	brpc_addr_t *addr;
	struct ctrl_socket *listener;
	
	DEBUG("adding BINRPC URI `%s' to listeners list.\n", s);
	addr = brpc_parse_uri(s);
	if (! addr) {
		ERR("invalid BINRPC URI `%s' (%s).\n", s, brpc_strerror());
		return -1;
	}
	listener = (struct ctrl_socket *)pkg_malloc(sizeof(struct ctrl_socket));
	if (! listener) {
		ERR("out of pkg memory.\n");
		return -1;
	}
	memset((char *)listener, 0, sizeof(struct ctrl_socket));
	listener->addr = *addr;
	listener->name = s + (sizeof(BRPC_URI_PREFIX) - /*0-term*/1);
	listener->p_proto = P_BINRPC;
	listener->next = ctrl_sock_lst;
	ctrl_sock_lst = listener;
	return 0;
}

struct ctrl_socket *add_fdpass_socket(int sockfd, pid_t pid)
{
	struct ctrl_socket *cs;
	DEBUG("adding worker socket for child PID#%d.\n", pid);

	if (! (cs = pkg_malloc(sizeof(struct ctrl_socket)))) {
		ERR("out of pkg memory.\n");
		return NULL;
	}
	memset(cs, 0, sizeof(struct ctrl_socket));
	BRPC_ADDR_DOMAIN(&cs->addr) = PF_LOCAL;
	BRPC_ADDR_TYPE(&cs->addr) = SOCK_STREAM;
	cs->fd = sockfd;
	cs->p_proto = P_FDPASS;
	cs->child = pid;
	cs->next = ctrl_sock_lst;
	ctrl_sock_lst = cs;
	return cs;
}

#ifdef USE_FIFO
/*
 *
 * TODO: this could be complete baloney!!! 
 * (should probably go to fifo_server*)
 *
 */
static brpc_addr_t *get_fifo_addr(char *path)
{
	static brpc_addr_t addr;
	size_t pathlen;

	memset((char *)&addr, 0, sizeof(brpc_addr_t));
	addr.domain = PF_FIFO;
	pathlen = strlen(path) + /*0-term*/1;
	if (sizeof(addr.sockaddr.un.sun_path) < pathlen) {
		ERR("FIFO path `%s' too long (maximum supported: %zd).\n", path, 
				sizeof(addr.sockaddr.un.sun_path));
		return NULL;
	}
	memcpy(addr.sockaddr.un.sun_path, path, pathlen);
	return &addr;
}

int add_fifo_socket(char *s)
{
	brpc_addr_t *addr;
	struct ctrl_socket *listener;
	
	addr = get_fifo_addr(s);
	if (! addr)
		return -1;

	listener = (struct ctrl_socket *)pkg_malloc(sizeof(struct ctrl_socket));
	if (! listener) {
		ERR("out of pkg memory.\n");
		return -1;
	}
	memset(listener, 0, sizeof(struct ctrl_socket));
	memcpy((char *)&listener->addr, addr, sizeof(brpc_addr_t));
	listener->name = s;
	listener->p_proto = P_FIFO;

	listener->next = ctrl_sock_lst;
	ctrl_sock_lst = listener;
	return 0;
}
#endif /* USE_FIFO */



static void close_binrpc_listener(struct ctrl_socket *cs)
{
	char *p_type, *p_addr;

	if (cs->fd < 0)
		/* already closed */
		return;

	switch (cs->p_proto) {
		case P_BINRPC: 
			p_type = "BINRPC"; 
			p_addr = brpc_print_addr(&cs->addr);
			break;
#ifdef USE_FIFO
		case P_FIFO:
			/* TODO */
			break;
#endif
		case P_FDPASS:
			p_type = "FDPASS";
			p_addr = "[sockpair]";
			break;

		default: 
			BUG("unexpected protocol type: %d.\n", cs->p_proto);
#ifdef EXTRA_DEBUG
			abort();
#endif
			return;
	}
	DEBUG("closing %s listener: %s.\n", p_type, p_addr);

	/* close all the opened fds & unlink the files */
	switch(cs->addr.domain){
		case PF_LOCAL:
			close(cs->fd);
			cs->fd=-1;
#if USE_FIFO
			/* TODO: does this make sense? "write_fd - only used by fifo"*/
			if (cs->write_fd!=-1){
				close(cs->write_fd);
				cs->write_fd=-1;
			}
#endif
			break;
#ifdef USE_FIFO
		case PF_FIFO:
			destroy_fifo(cs->fd, cs->write_fd, cs->name);
			break;
#endif
		default:
			close(cs->fd);
			cs->fd=-1;
#if USE_FIFO
			/* TODO: does this make sense? "write_fd - only used by fifo"*/
			if (cs->write_fd!=-1){
				close(cs->write_fd);
				cs->write_fd=-1;
			}
#endif
	}
}

static void del_binrpc_listener(struct ctrl_socket *cs)
{
	close_binrpc_listener(cs);
	pkg_free(cs);
}


static inline enum SOCK_LIST_TYPES get_socklist_type(struct ctrl_socket *cs)
{
	if (cs->p_proto == P_FDPASS)
		return SOCKLIST_FDPASS;
	switch (BRPC_ADDR_TYPE(&cs->addr)) {
		case SOCK_STREAM: return SOCKLIST_STREAM;
		case SOCK_DGRAM: return SOCKLIST_DGRAM;
		default:
			BUG("invalid address type %d.\n", BRPC_ADDR_TYPE(&cs->addr));
#ifdef EXTRA_DEBUG
			abort();
#endif
			return SOCKLIST_NONE;
	}
}

/**
 * @param keep Enum SOCK_LIST_TYPES bit mask
 */
void del_binrpc_listeners(int keep)
{
	struct ctrl_socket *next, *cs, *prev;

	cs = ctrl_sock_lst;
	prev = NULL;
	while (cs) {
		if ((get_socklist_type(cs) & keep) == 0) {
			/* TODO: FIFO */
			next = cs->next;
			del_binrpc_listener(cs);
			cs = next;
			
			if (prev)
				prev->next = cs;
			else
				ctrl_sock_lst = cs;
		} else {
			prev = cs;
			cs = cs->next;
		}
	}
}

/**
 * @param keep Enum SOCK_LIST_TYPES bit mask
 */
void close_binrpc_listeners(int keep)
{
	struct ctrl_socket *cs;

	for (cs = ctrl_sock_lst; cs; cs = cs->next)
		if ((get_socklist_type(cs) & keep) == 0)
			close_binrpc_listener(cs);
}

char **ctrl_sock_paths()
{
	char **paths, **p;
	size_t cnt, i;
	struct ctrl_socket *cs;
			
	for (cs = ctrl_sock_lst, cnt = 0, paths = NULL; cs; cs = cs->next) {
		if (BRPC_ADDR_DOMAIN(&cs->addr) != PF_LOCAL)
			continue;
		if (! (p = (char **)realloc(paths, (cnt + 1) * sizeof(char *)))) {
			ERR("out of system memory!\n");
			goto err;
		}
		paths = p;
		if (! (paths[cnt] = strdup(cs->name))) {
			ERR("out of system memory!\n");
			goto err;
		}
		cnt ++;
	}
	/* add NULL as end marker */
	if (! (p = realloc(paths, (cnt + 1) * sizeof(char *)))) {
		ERR("out of system memory!\n");
		goto err;
	}
	(paths = p)[cnt] = NULL;


	return paths;
err:
	for (i = 0; i < cnt; i ++)
		free(paths[i]);
	free(paths);
	return NULL;
}

char* payload_proto_name(enum payload_proto p)
{
	switch(p){
		case P_BINRPC:
			return "binrpc";
#ifdef USE_FIFO
		case P_FIFO:
			return "fifo";
#endif
		default:
			;
	}
	return "<unknown>";
}


const char* ctl_listen_ls_doc[]={ "list ctl listen sockets", 0 };

void  ctrl_listen_ls_rpc(rpc_t* rpc, void* ctx)
{
	struct ctrl_socket* cs;
	
	for (cs=ctrl_sock_lst; cs; cs=cs->next){
		if (cs->p_proto == P_FDPASS)
			continue;
		rpc->add(ctx, "ssss", payload_proto_name(cs->p_proto),
						socket_proto_name(&cs->addr), cs->name, "");
	}
}

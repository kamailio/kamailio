/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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

#ifndef CNODE_H_
#define CNODE_H_

#include <ei.h>

/**
 * Listener for handling requests from connected nodes.
 */
typedef struct cnode_handler_s
{
	/* d-linked list  */
	struct handler_common_s *prev;
	struct handler_common_s *next;

	/* if need to add new in i/o handler */
	struct handler_common_s *new;

	/*
	 * this handler is erlang C node server
	 * handles requests from remote erlang node
	 */
	int (*handle_f)(handler_common_t *phandler_t);
	int (*wait_tmo_f)(handler_common_t *phandler_t);
	int (*destroy_f)(handler_common_t *phandler_t);
	int sockfd; /* connection socket to remote erlang node */
	ei_cnode ec; /* erlang C node (actually it's kamailio node) */

	/*
	 * connection descriptor
	 * remote node info
	 */
	ErlConnect conn;

	/* request/response buffers */
	ei_x_buff request;
	ei_x_buff response;

	/*
	 * ERL_TICK time out
	 */
	int tick_tmo;
} cnode_handler_t;

/*
 * active connection to Erlang node
 */
extern cnode_handler_t *enode;

/* where to reply - reset to NULL after use */
extern erlang_pid *cnode_reply_to_pid;

typedef struct csockfd_handler_s
{
	/* d-linked list  */
	struct handler_common_s *prev;
	struct handler_common_s *next;
	struct handler_common_s *new;

	/* handle receive fd over csockfd */
	int (*handle_f)(handler_common_t *phandler_t);
	int (*wait_tmo_f)(handler_common_t *phandler_t);
	int (*destroy_f)(handler_common_t *phandler_t);
	int sockfd; /* its csockfd */
	ei_cnode ec; /* erlang C node (actually it's kamailio node) */
} csockfd_handler_t;

extern csockfd_handler_t *csocket_handler;

int init_cnode_sockets(int idx);
void cnode_main_loop(int idx);

int handle_csockfd(handler_common_t *phandler_t);
int csockfd_init(csockfd_handler_t *phandler_t, const ei_cnode *ei);

int handle_cnode(handler_common_t *phandler_t);
int wait_cnode_tmo(handler_common_t *phandler_t);
int destroy_cnode(handler_common_t *phandler_t);

int cnode_connect_to(cnode_handler_t *phandler, ei_cnode *ec, const str *nodename );
int enode_connect();

enum erl_handle_type {
	ERL_EPMD_H = 1,
	ERL_CNODE_H,
	ERL_WORKER_H,
	ERL_CSOCKFD_H
};

#endif /* CNODE_H_ */

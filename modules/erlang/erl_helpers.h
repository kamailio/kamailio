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

#ifndef ERL_HELPERS_H_
#define ERL_HELPERS_H_

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ei.h>

#include "../../str.h"
#include "../../dprint.h"

#define CONNECT_TIMEOUT	500 /* ms */
#define sockaddr_port(addr)	((addr)->sa_family == AF_INET ? ntohs(((struct sockaddr_in *)(addr))->sin_port) : ntohs(((struct sockaddr_in6 *)(addr))->sin6_port))

#define EI_X_BUFF_NULL	{0, 0, 0}

extern char thishostname[512];

/* common members of listener */
typedef struct handler_common_s
{
	/* d-linked list  */
	struct handler_common_s *prev;
	struct handler_common_s *next;

	/* if need to add new in i/o handler */
	struct handler_common_s *new;

	/* listener function */
	int (*handle_f)(struct handler_common_s *phandler_t);
	int (*wait_tmo_f)(struct handler_common_s *phandler_t);
	int (*destroy_f)(struct handler_common_s *phandler_t);
	int sockfd;
	ei_cnode ec; /* erlang C node descriptor */

} handler_common_t;

extern handler_common_t* io_handlers;

typedef struct erlang_ref_ex_s {
	erlang_ref ref;
	char nodename[MAXATOMLEN];
	int with_node;
} erlang_ref_ex_t;

void erl_init_common();
void erl_free_common();

int erl_set_nonblock(int sockfd);
void erl_close_socket(int sockfd);

int erl_passive_socket(const char *hostname, int qlen, struct addrinfo **ai_ret);
int erl_active_socket(const char *hostname, int qlen, struct addrinfo **ai_ret);

int erl_init_ec(ei_cnode *ec, const str *alivename, const str *hostname, const str *cookie);
int erl_init_node(ei_cnode *ec, const str *alivename, const str *hostname, const str *cookie);

void io_handler_ins(handler_common_t* phandler);
void io_handler_del(handler_common_t* phandler);
void io_handlers_delete();

#define PRINT_DBG_REG_SEND(node,from,cnode,to,msg) \
		do { \
			char *mbuf = NULL; \
			char *pbuf = NULL; \
			ei_x_buff xpid = {0,0,0}; \
			int i=0,v=0; \
			ei_decode_version((msg)->buff,&i,&v);\
			i=v?i:0; \
			ei_s_print_term(&mbuf, (msg)->buff, &i); \
			ei_x_encode_pid(&xpid,&from); \
			i = 0; \
			ei_s_print_term(&pbuf,xpid.buff,&i); \
			LM_DBG("ERL_REG_SEND: {%s,'%s'} <- %s from {%s,'%s'}\n", to, cnode, mbuf, pbuf, node); \
			free(mbuf), free(pbuf), ei_x_free(&xpid); \
		} while(0)

#define PRINT_DBG_SEND(node,to,msg) \
		do { \
			char *mbuf = NULL; \
			char *pbuf = NULL; \
			ei_x_buff xpid = {0,0,0}; \
			int i=0,v=0; \
			ei_decode_version((msg)->buff,&i,&v);\
			i=v?i:0; \
			ei_s_print_term(&mbuf, (msg)->buff, &i); \
			ei_x_encode_pid(&xpid,&to); \
			i = 0; \
			ei_s_print_term(&pbuf,xpid.buff,&i); \
			LM_DBG("ERL_SEND: %s <- %s from '%s'\n", pbuf, mbuf, node); \
			free(mbuf), free(pbuf), ei_x_free(&xpid); \
		} while(0)

#define ei_x_print_reg_msg(buf, dest, send) \
do{ \
	char *mbuf = NULL; \
	int i = 1; \
	ei_s_print_term(&mbuf, (buf)->buff, &i); \
	if (send) LM_DBG("sending %s to %s\n", mbuf, dest); \
	else LM_DBG("received %s from/for %s\n", mbuf, dest); \
	free(mbuf); \
} while(0)

#define ei_x_print_msg(buf, pid, send) \
do { \
	char *pbuf = NULL; \
	int i = 0; \
	ei_x_buff pidbuf; \
	ei_x_new(&pidbuf); \
	ei_x_encode_pid(&pidbuf, pid); \
	ei_s_print_term(&pbuf, pidbuf.buff, &i); \
	ei_x_print_reg_msg(buf, pbuf, send); \
	free(pbuf); \
} while(0)

int ei_decode_strorbin(char *buf, int *index, int maxlen, char *dst);

typedef enum {
	API_RPC_CALL,
	API_REG_SEND,
	API_SEND
} eapi_t;

#endif /* ERL_HELPERS_H_ */

/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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



#ifndef _tcp_conn_h
#define _tcp_conn_h

#include "ip_addr.h"


#define TCP_BUF_SIZE 65535
#define TCP_CON_TIMEOUT 60 /* in  seconds */
#define TCP_CHILD_TIMEOUT 5 /* after 5 seconds, the child "returns" 
							 the connection to the tcp master process */
#define TCP_MAIN_SELECT_TIMEOUT 5 /* how often "tcp main" checks for timeout*/
#define TCP_CHILD_SELECT_TIMEOUT 2 /* the same as above but for children */


enum tcp_req_errors {	TCP_REQ_INIT, TCP_REQ_OK, TCP_READ_ERROR,
						TCP_REQ_OVERRUN, TCP_REQ_BAD_LEN };
enum tcp_req_states {	H_SKIP_EMPTY, H_SKIP, H_LF, H_LFCR,  H_BODY, H_STARTWS,
		H_CONT_LEN1, H_CONT_LEN2, H_CONT_LEN3, H_CONT_LEN4, H_CONT_LEN5,
		H_CONT_LEN6, H_CONT_LEN7, H_CONT_LEN8, H_CONT_LEN9, H_CONT_LEN10,
		H_CONT_LEN11, H_CONT_LEN12, H_CONT_LEN13, H_L_COLON, 
		H_CONT_LEN_BODY, H_CONT_LEN_BODY_PARSE 
	};

/* fd communication commands */
enum conn_cmds { CONN_DESTROY=-3, CONN_ERROR=-2, CONN_EOF=-1, CONN_RELEASE, 
					CONN_GET_FD, CONN_NEW };

struct tcp_req{
	struct tcp_req* next;
	/* sockaddr ? */
	char buf[TCP_BUF_SIZE]; /* bytes read so far*/
	char* start; /* where the message starts, after alll the empty lines are
					skipped*/
	char* pos; /* current position in buf */
	char* parsed; /* last parsed position */
	char* body; /* body position */
	int content_len;
	int has_content_len; /* 1 if content_length was parsed ok*/
	int complete; /* 1 if one req has been fully read, 0 otherwise*/
	int bytes_to_go; /* how many bytes we have still to read from the body*/
	enum tcp_req_errors error;
	enum tcp_req_states state;
};




struct tcp_connection{
	int s; /*socket, used by "tcp main" */
	int fd; /* used only by "children" */
	int id; /* id (unique!) used to retrieve a specific connection when
	           reply-ing*/
	struct receive_info rcv; /* src & dst ip, ports, proto a.s.o*/
#if 0
	struct ip_addr ip; /* peer ip */
	int port; /* peer port */
	int sock_idx; /* receiving socket index in the tcp_info array */
	union sockaddr_union su;
#endif
	struct tcp_req req; /* request data */
	int refcnt;
	int timeout; /* connection timeout, after this it will be removed*/
	unsigned addr_hash; /* hash indexes in thge 2 tables */
	unsigned id_hash;
	struct tcp_connection* next; /* next, prev in hash table, used by "main" */
	struct tcp_connection* prev;
	struct tcp_connection* id_next; /* next, prev in id hash table */
	struct tcp_connection* id_prev;
	struct tcp_connection* c_next; /* child next prev (use locally) */
	struct tcp_connection* c_prev;
};




#define init_tcp_req( r) \
	do{ \
		memset( (r), 0, sizeof(struct tcp_req)); \
		(r)->parsed=(r)->pos=(r)->start=(r)->buf; \
		(r)->error=TCP_REQ_OK;\
		(r)->state=H_SKIP_EMPTY; \
	}while(0)


/* add a tcpconn to a list*/
/* list head, new element, next member, prev member */
#define tcpconn_listadd(head, c, next, prev) \
	do{ \
		/* add it at the begining of the list*/ \
		(c)->next=(head); \
		(c)->prev=0; \
		if ((head)) (head)->prev=(c); \
		(head)=(c); \
	} while(0)


/* remove a tcpconn from a list*/
#define tcpconn_listrm(head, c, next, prev) \
	do{ \
		if ((head)==(c)) (head)=(c)->next; \
		if ((c)->next) (c)->next->prev=(c)->prev; \
		if ((c)->prev) (c)->prev->next=(c)->next; \
	}while(0)


#define TCPCONN_LOCK lock_get(tcpconn_lock);
#define TCPCONN_UNLOCK lock_release(tcpconn_lock);

#define TCP_ADDR_HASH_SIZE 1024
#define TCP_ID_HASH_SIZE 1024

static inline unsigned tcp_addr_hash(struct ip_addr* ip, unsigned short port)
{
	if(ip->len==4) return (ip->u.addr32[0]^port)&(TCP_ADDR_HASH_SIZE-1);
	else if (ip->len==16) 
			return (ip->u.addr32[0]^ip->u.addr32[1]^ip->u.addr32[2]^
					ip->u.addr32[3]^port) & (TCP_ADDR_HASH_SIZE-1);
	else{
		LOG(L_CRIT, "tcp_addr_hash: BUG: bad len %d for an ip address\n",
				ip->len);
		return 0;
	}
}

#define tcp_id_hash(id) (id&(TCP_ID_HASH_SIZE-1))


#endif



/**
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
 */


#ifndef _DMQNODE_H_
#define _DMQNODE_H_

#include <string.h>
#include <stdlib.h>
#include "../../lock_ops.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_param.h"

#define NBODY_LEN		1024
#define DMQ_NODE_ACTIVE		1 << 1
#define DMQ_NODE_TIMEOUT	1 << 2
#define DMQ_NODE_DISABLED	1 << 3

typedef struct dmq_node {
	int local; /* local type set means the dmq dmqnode == self */
	str orig_uri; /* original uri string - e.g. sip:127.0.0.1:5060;passive=true */
	struct sip_uri uri; /* parsed uri string */
	struct ip_addr ip_address; /* resolved IP address */
	param_t* params; /* uri parameters */
	int status; /* reserved - maybe something like active,timeout,disabled */
	int last_notification; /* last notificatino receied from the node */
	struct dmq_node* next; /* pointer to the next struct dmq_node */
} dmq_node_t;

typedef struct dmq_node_list {
	gen_lock_t lock; /* lock for the list - must acquire before manipulating it */
	struct dmq_node* nodes; /* the nodes in the list */
	int count; /* the number of nodes in the list */
} dmq_node_list_t;

extern str dmq_node_status_str;
extern dmq_node_list_t* node_list;

dmq_node_list_t* init_dmq_node_list();
dmq_node_t* build_dmq_node(str* uri, int shm);
int update_node_list(dmq_node_list_t* remote_list);
dmq_node_t* add_dmq_node(dmq_node_list_t* list, str* uri);
dmq_node_t* find_dmq_node(dmq_node_list_t* list, dmq_node_t* node);
dmq_node_t* find_dmq_node_uri(dmq_node_list_t* list, str* uri);
dmq_node_t* find_dmq_node_uri2(str* uri);
int del_dmq_node(dmq_node_list_t* list, dmq_node_t* node);
int cmp_dmq_node(dmq_node_t* node, dmq_node_t* cmpnode);
dmq_node_t* shm_dup_node(dmq_node_t* node);
void destroy_dmq_node(dmq_node_t* node, int shm);
void shm_free_node(dmq_node_t* node);
void pkg_free_node(dmq_node_t* node);
int set_dmq_node_params(dmq_node_t* node, param_t* params);

str* get_status_str(int status);
int build_node_str(dmq_node_t* node, char* buf, int buflen);

extern dmq_node_t* self_node;
extern dmq_node_t* notification_node;	

#endif


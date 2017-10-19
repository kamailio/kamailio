/**
 *
 * Copyright (C) 2015 Victor Seva (sipwise.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */
#ifndef _CFGT_INT_H_
#define _CFGT_INT_H_

#include "../../core/locking.h"
#include "../../core/route_struct.h"
#include "../../core/str_hash.h"
#include "../../lib/srutils/srjson.h"

#define CFGT_HASH_SIZE 32

enum _cfgt_action_type
{
	CFGT_ROUTE = 1,
	CFGT_DROP_E,
	CFGT_DROP_D,
	CFGT_DROP_R
};

typedef struct _cfgt_hash
{
	gen_lock_t lock;
	struct str_hash_table hash;
	str save_uuid; /* uuid to be save */
} cfgt_hash_t, *cfgt_hash_p;

typedef struct _cfgt_str_list
{
	str s;
	enum _cfgt_action_type type;
	struct _cfgt_str_list *next, *prev;
} cfgt_str_list_t, *cfgt_str_list_p;

typedef struct _cfgt_node
{
	srjson_doc_t jdoc;
	str uuid;
	int msgid;
	cfgt_str_list_p flow_head;
	cfgt_str_list_p route;
	srjson_t *in, *out, *flow;
	struct _cfgt_node *next, *prev;
} cfgt_node_t, *cfgt_node_p;

int cfgt_init(void);
cfgt_node_p cfgt_create_node(struct sip_msg *msg);
int cfgt_process_route(struct sip_msg *msg, struct action *a);
int cfgt_pre(struct sip_msg *msg, unsigned int flags, void *bar);
int cfgt_post(struct sip_msg *msg, unsigned int flags, void *bar);
#endif

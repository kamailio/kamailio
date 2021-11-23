/*
 * NATS module interface
 *
 * Copyright (C) 2021 Voxcom Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#ifndef __NATS_MOD_H_
#define __NATS_MOD_H_

#include <stdio.h>
#include <nats/nats.h>
#include <nats/adapters/libuv.h>
#include "../json/api.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/fmsg.h"

#define NATS_DEFAULT_URL "nats://localhost:4222"
#define NATS_MAX_SERVERS 10
#define NATS_URL_MAX_SIZE 256

typedef struct _nats_evroutes
{
	int connected;
	int disconnected;
} nats_evroutes_t;
static nats_evroutes_t _nats_rts;

typedef struct _init_nats_sub
{
	char *sub;
	char *queue_group;
	struct _init_nats_sub *next;
} init_nats_sub, *init_nats_sub_ptr;

typedef struct _init_nats_server
{
	char *url;
	struct _init_nats_server *next;
} init_nats_server, *init_nats_server_ptr;

typedef struct _nats_on_message
{
	int rt;
} nats_on_message, *nats_on_message_ptr;

typedef struct _nats_connection
{
	natsOptions *opts;
	char *servers[NATS_MAX_SERVERS];
} nats_connection, *nats_connection_ptr;

struct nats_consumer_worker
{
	char *subject;
	char *queue_group;
	int pid;
	natsConnection *conn;
	natsSubscription *subscription;
	uv_loop_t *uvLoop;
	nats_on_message_ptr on_message;
};
typedef struct nats_consumer_worker nats_consumer_worker_t;

static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);

int nats_run_cfg_route(int rt);
void nats_init_environment();

int _init_nats_server_url_add(modparam_t type, void *val);
init_nats_server_ptr _init_nats_server_list_new(char *url);
int init_nats_server_url_add(char *url);
int nats_cleanup_init_servers();
int nats_init_connection(nats_connection_ptr c);
int nats_cleanup_connection(nats_connection_ptr c);

int _init_nats_sub_add(modparam_t type, void *val);
nats_connection_ptr _init_nats_connection();
init_nats_sub_ptr _init_nats_sub_new(char *sub, char *queue_group);
int init_nats_sub_add(char *sub);
int nats_cleanup_init_sub();

void nats_consumer_worker_proc(
		nats_consumer_worker_t *worker, nats_connection_ptr c);
int nats_pv_get_event_payload(struct sip_msg *, pv_param_t *, pv_value_t *);

#endif

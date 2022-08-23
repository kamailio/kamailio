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

#ifndef __NATS_DEFS_H_
#define __NATS_DEFS_H_

#include <nats/nats.h>
#include <uv.h>
#include "../../core/str.h"

#define NATS_DEFAULT_URL "nats://localhost:4222"
#define NATS_MAX_SERVERS 10
#define NATS_URL_MAX_SIZE 256
#define DEFAULT_NUM_PUB_WORKERS 2

typedef struct _nats_connection
{
	natsConnection *conn;
	natsOptions *opts;
	char *servers[NATS_MAX_SERVERS];
} nats_connection, *nats_connection_ptr;


typedef struct _nats_evroutes
{
	int connected;
	int disconnected;
} nats_evroutes_t;

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
	char *_evname;
	str evname;
} nats_on_message, *nats_on_message_ptr;

struct nats_consumer_worker
{
	char *subject;
	char *queue_group;
	int pid;
	natsSubscription *subscription;
	uv_loop_t *uvLoop;
	nats_connection_ptr nc;
	nats_on_message_ptr on_message;
};
typedef struct nats_consumer_worker nats_consumer_worker_t;

struct nats_pub_worker
{
	int pid;
	int fd;
	uv_loop_t *uvLoop;
	uv_pipe_t pipe;
	uv_poll_t poll;
	nats_connection_ptr nc;
};
typedef struct nats_pub_worker nats_pub_worker_t;

#endif

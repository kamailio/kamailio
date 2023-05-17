/**
 * Copyright (C) 2018 Andreas Granig (sipwise.com)
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


#ifndef _REDIS_CONNECTION_H_
#define _REDIS_CONNECTION_H_

#ifdef WITH_HIREDIS_CLUSTER
#include <hircluster.h>
#else
#ifdef WITH_HIREDIS_PATH
#include <hiredis/hiredis.h>
#else
#include <hiredis.h>
#endif
#endif

#include "db_redis_mod.h"

#ifndef WITH_REDIS_CLUSTER
#define db_redis_check_reply(con, reply, err)                                \
	do {                                                                     \
		if(!(reply) && !(con)->con) {                                        \
			LM_ERR("Failed to fetch type entry: no connection to server\n"); \
			goto err;                                                        \
		}                                                                    \
		if(!(reply)) {                                                       \
			LM_ERR("Failed to fetch type entry: %s\n", (con)->con->errstr);  \
			redisFree((con)->con);                                           \
			(con)->con = NULL;                                               \
			goto err;                                                        \
		}                                                                    \
		if((reply)->type == REDIS_REPLY_ERROR) {                             \
			LM_ERR("Failed to fetch type entry: %s\n", (reply)->str);        \
			goto err;                                                        \
		}                                                                    \
	} while(0);
#else
#define db_redis_check_reply(con, reply, err)                                \
	do {                                                                     \
		if(!(reply) && !(con)->con) {                                        \
			LM_ERR("Failed to fetch type entry: no connection to server\n"); \
			goto err;                                                        \
		}                                                                    \
		if(!(reply)) {                                                       \
			LM_ERR("Failed to fetch type entry: %s\n", (con)->con->errstr);  \
			redisClusterFree((con)->con);                                    \
			(con)->con = NULL;                                               \
			goto err;                                                        \
		}                                                                    \
		if((reply)->type == REDIS_REPLY_ERROR) {                             \
			LM_ERR("Failed to fetch type entry: %s\n", (reply)->str);        \
			goto err;                                                        \
		}                                                                    \
	} while(0);
#endif

typedef struct redis_key redis_key_t;

typedef struct redis_command
{
	redis_key_t *query;
	struct redis_command *next;
} redis_command_t;

typedef struct km_redis_con
{
	struct db_id *id;
	unsigned int ref;
	struct pool_con *next;
#ifdef WITH_HIREDIS_CLUSTER
	redisClusterContext *con;
#else
	redisContext *con;
#endif
	redis_command_t *command_queue;
	unsigned int append_counter;
	struct str_hash_table tables;
	char srem_key_lua[41]; // sha-1 hex string
} km_redis_con_t;


struct redis_key;
typedef struct redis_key redis_key_t;

#define REDIS_CON(db_con) ((km_redis_con_t *)((db_con)->tail))

km_redis_con_t *db_redis_new_connection(const struct db_id *id);
void db_redis_free_connection(struct pool_con *con);

int db_redis_connect(km_redis_con_t *con);
void *db_redis_command_argv(km_redis_con_t *con, redis_key_t *query);
int db_redis_append_command_argv(
		km_redis_con_t *con, redis_key_t *query, int queue);
int db_redis_get_reply(km_redis_con_t *con, void **reply);
void db_redis_consume_replies(km_redis_con_t *con);
void db_redis_free_reply(redisReply **reply);
const char *db_redis_get_error(km_redis_con_t *con);

#ifdef WITH_HIREDIS_CLUSTER
void *db_redis_command_argv_to_node(
		km_redis_con_t *con, redis_key_t *query, cluster_node *node);
#endif

#endif /* _REDIS_CONNECTION_H_ */

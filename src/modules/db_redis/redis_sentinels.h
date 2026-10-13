/*
 * Copyright (C) 2025 1&1 AG (www.1und1.de)
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


#ifndef _REDIS_SENTINELS_H_
#define _REDIS_SENTINELS_H_

#ifdef WITH_SENTINELS

#include <hiredis/hiredis.h>
#include "db_redis_mod.h"
#include "redis_connection.h"
#include "../../core/parser/parse_param.h"

// Reply list structures
struct reply_node
{
	redisReply *reply;
	struct reply_node *next;
};

struct reply_list
{
	struct reply_node *head;
	struct reply_node *tail;
	size_t count;
};

// Redis role enum
typedef enum
{
	DB_REDIS_ROLE_MASTER,
	DB_REDIS_ROLE_REPLICA
} redis_role_t;

typedef struct redis_sentinel
{
	char *host;
	unsigned int port;
	struct redis_sentinel *next;
} redis_sentinel_t;

typedef struct
{
	char *user;
	char *password;
	param_t *attrs;
	char *spec;
	redis_sentinel_t *sentinel_list;
	redis_sentinel_t *sentinel_list_tail;
} sentinel_config_t;

extern sentinel_config_t db_redis_sc;
extern struct reply_list replica_list;
extern int db_redis_with_sentinels;
extern int use_replicas;
extern char *db_redis_master_name;
extern time_t last_seen_time;
extern time_t *db_redis_shared_time;
extern int using_master_read_only;
extern int recheck_replicas_interval;
extern int min_recheck_interval;

// Utility functions
int replica_list_free(struct reply_list *list);

// Sentinel selection and validation

/*
 * Parse sentinels_config in case of sentinel mode
 */
int parse_sentinel_config(char *spec);

/* module param callback for sentinels_config */
int sentinels_param(modparam_t type, void *val);

/* timer callback used for replica recheck */
void db_redis_timer(unsigned int ticks, void *param);

/* initialize sentinel runtime state during module init */
int db_redis_sentinels_init_runtime(void);

/* decide when to refresh replica usage while on master */
int should_recheck_replicas(time_t crt_time);

/* decide if connection should reconnect to refresh replica usage */
int db_redis_should_reconnect_for_replica_recheck(void);

/*
 * Add sentinels in case of sentinel mode
 */
int db_redis_add_sentinels(char *spec);

/*
 * Select a master server using the Sentinel
 */
int db_redis_select_master(redisContext *sentinel_ctx, km_redis_con_t *con);

/*
 * Select a replica server using the Sentinel
 */
int db_redis_select_replica(redisContext *sentinel_ctx, km_redis_con_t *con);

/*
 * Resolve redis host/port via configured sentinels.
 */
int db_redis_resolve_server_via_sentinels(km_redis_con_t *con);

#endif

#endif /* _REDIS_SENTINELS_H_ */

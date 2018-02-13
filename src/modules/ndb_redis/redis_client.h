/**
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * Copyright (C) 2012 Vicente Hernando Ara (System One: www.systemonenoc.com)
 *     - for: redis array reply support
 *
 * Copyright (C) 2017 Carsten Bock (ng-voice GmbH)
 *     - for: Cluster support
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _REDIS_CLIENT_H_
#define _REDIS_CLIENT_H_

#include <hiredis/hiredis.h>

#include "../../core/str.h"
#include "../../core/parser/parse_param.h"
#include "../../core/mod_fix.h"

#define MAXIMUM_PIPELINED_COMMANDS 1000
#define MAXIMUM_NESTED_KEYS 10
#define LM_DBG_redis_reply(rpl) print_redis_reply(L_DBG,(rpl),0)

int redisc_init(void);
int redisc_destroy(void);
int redisc_add_server(char *spec);

typedef struct redisc_reply {
	str rname;
	unsigned int hname;
	redisReply *rplRedis;
	struct redisc_reply *next;
} redisc_reply_t;

typedef struct redisc_piped_cmds {
	str commands[MAXIMUM_PIPELINED_COMMANDS];
	redisc_reply_t *replies[MAXIMUM_PIPELINED_COMMANDS];
	int pending_commands;
} redisc_piped_cmds_t;

typedef struct redisc_srv_disable {
	int disabled;
	int consecutive_errors;
	time_t restore_tick;
} redisc_srv_disable_t;

typedef struct redisc_server {
	str *sname;
	unsigned int hname;
	param_t *attrs;
	char *spec;
	redisContext *ctxRedis;
	struct redisc_server *next;
	redisc_piped_cmds_t piped;
	redisc_srv_disable_t disable;
} redisc_server_t;

typedef struct redisc_pv {
	str rname;
	redisc_reply_t *reply;
	str rkey;
	int rkeyid;
	gparam_t pos[MAXIMUM_NESTED_KEYS];  /* Array element position. */
	int rkeynum;
} redisc_pv_t;

/* Server related functions */
redisc_server_t* redisc_get_server(str *name);
int redisc_reconnect_server(redisc_server_t *rsrv);

/* Command related functions */
int redisc_exec(str *srv, str *res, str *cmd, ...);
int redisc_append_cmd(str *srv, str *res, str *cmd, ...);
int redisc_exec_pipelined_cmd(str *srv);
int redisc_exec_pipelined(redisc_server_t *rsrv);
int redisc_create_pipelined_message(redisc_server_t *rsrv);
void redisc_free_pipelined_cmds(redisc_server_t *rsrv);
redisReply* redisc_exec_argv(redisc_server_t *rsrv, int argc, const char **argv,
		const size_t *argvlen);
redisc_reply_t *redisc_get_reply(str *name);
int redisc_free_reply(str *name);
int redisc_check_auth(redisc_server_t *rsrv, char *pass);
int redis_check_server(redisc_server_t *rsrv);
int redis_count_err_and_disable(redisc_server_t *rsrv);
void print_redis_reply(int log_level, redisReply *rpl,int offset);
#endif

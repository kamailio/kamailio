/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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

#include "../../str.h"
#include "../../parser/parse_param.h"
#include "../../mod_fix.h"

int redisc_init(void);
int redisc_destroy(void);
int redisc_add_server(char *spec);
int redisc_exec(str *srv, str *res, str *cmd, ...);

typedef struct redisc_server {
	str *sname;
	unsigned int hname;
	param_t *attrs;
	redisContext *ctxRedis;
	struct redisc_server *next;
} redisc_server_t;

typedef struct redisc_reply {
	str rname;
	unsigned int hname;
	redisReply *rplRedis;
	struct redisc_reply *next;
} redisc_reply_t;

typedef struct redisc_pv {
	str rname;
	redisc_reply_t *reply;
	str rkey;
	int rkeyid;
	gparam_t pos;  /* Array element position. */
} redisc_pv_t;

/* Server related functions */
redisc_server_t* redisc_get_server(str *name);
int redisc_reconnect_server(redisc_server_t *rsrv);

/* Command related functions */
int redisc_exec(str *srv, str *res, str *cmd, ...);
void* redisc_exec_argv(redisc_server_t *rsrv, int argc, const char **argv, const size_t *argvlen);
redisc_reply_t *redisc_get_reply(str *name);
int redisc_free_reply(str *name);
int redisc_check_auth(redisc_server_t *rsrv, char *pass);
#endif

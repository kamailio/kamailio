/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _NDB_REDIS_API_H_
#define _NDB_REDIS_API_H_

#include "redis_client.h"

typedef redisc_server_t* (*redisc_get_server_f)(str *name);
typedef int (*redisc_exec_f)(str *srv, str *res, str *cmd, ...);
typedef redisReply* (*redisc_exec_argv_f)(redisc_server_t *rsrv, int argc,
		const char **argv, const size_t *argvlen);
typedef redisc_reply_t* (*redisc_get_reply_f)(str *name);
typedef int (*redisc_free_reply_f)(str *name);


/**
 * @brief NDB_REDIS API structure
 */
typedef struct ndb_redis_api {
	redisc_get_server_f get_server;
	redisc_exec_f exec;
	redisc_exec_argv_f exec_argv;
	redisc_get_reply_f get_reply;
	redisc_free_reply_f free_reply;
} ndb_redis_api_t;

typedef int (*bind_ndb_redis_f)(ndb_redis_api_t* api);

/**
 * @brief Load the NDB_REDIS API
 */
static inline int ndb_redis_load_api(ndb_redis_api_t *api)
{
	bind_ndb_redis_f bindndbredis;

	bindndbredis = (bind_ndb_redis_f)find_export("bind_ndb_redis", 0, 0);
	if(bindndbredis == 0) {
		LM_ERR("cannot find bind_ndb_redis\n");
		return -1;
	}
	if (bindndbredis(api)==-1) {
		LM_ERR("cannot bind ndb_redis api\n");
		return -1;
	}
	return 0;
}

#endif

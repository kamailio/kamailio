/**
 * ndb_mongodb module
 * 
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _NDB_MONGODB_API_H_
#define _NDB_MONGODB_API_H_

#include "../../sr_module.h"

typedef int (*mongodbc_exec_simple_f)(str *srv, str *dname, str *cname, str *cmd, str *res);
typedef int (*mongodbc_exec_f)(str *srv, str *dname, str *cname, str *cmd, str *res);
typedef int (*mongodbc_find_f)(str *srv, str *dname, str *cname, str *cmd, str *res);
typedef int (*mongodbc_find_one_f)(str *srv, str *dname, str *cname, str *cmd, str *res);
typedef int (*mongodbc_next_reply_f)(str *name);
typedef int (*mongodbc_free_reply_f)(str *name);


typedef struct ndb_mongodb_api {
	mongodbc_exec_simple_f cmd_simple;
	mongodbc_exec_f	cmd;
	mongodbc_find_f	find;
	mongodbc_find_one_f	find_one;
	mongodbc_next_reply_f next_reply;
	mongodbc_next_reply_f free_reply;
} ndb_mongodb_api_t;

typedef int (*bind_ndb_mongodb_f)(ndb_mongodb_api_t* api);
int bind_ndb_mongodb(ndb_mongodb_api_t* api);

/**
 * @brief Load the dispatcher API
 */
static inline int ndb_mongodb_load_api(ndb_mongodb_api_t *api)
{
	bind_ndb_mongodb_f bindndbmongodb;

	bindndbmongodb = (bind_ndb_mongodb_f)find_export("bind_ndb_mongodb", 0, 0);
	if(bindndbmongodb == 0) {
		LM_ERR("cannot find bind_ndb_mongodb\n");
		return -1;
	}
	if(bindndbmongodb(api)<0)
	{
		LM_ERR("cannot bind ndb mongodb api\n");
		return -1;
	}
	return 0;
}

#endif

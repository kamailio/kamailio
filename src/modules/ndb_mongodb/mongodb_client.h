/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _MONGODB_CLIENT_H_
#define _MONGODB_CLIENT_H_

#include <mongoc.h>
#include <bson.h>

#include "../../str.h"
#include "../../parser/parse_param.h"
#include "../../mod_fix.h"

int mongodbc_init(void);
int mongodbc_destroy(void);
int mongodbc_add_server(char *spec);
int mongodbc_exec_simple(str *srv, str *dname, str *cname, str *cmd, str *res);
int mongodbc_exec(str *srv, str *dname, str *cname, str *cmd, str *res);
int mongodbc_find(str *srv, str *dname, str *cname, str *cmd, str *res);
int mongodbc_find_one(str *srv, str *dname, str *cname, str *cmd, str *res);

typedef struct mongodbc_server {
	str *sname;
	str *uri;
	unsigned int hname;
	param_t *attrs;
	mongoc_client_t *client;
	struct mongodbc_server *next;
} mongodbc_server_t;

typedef struct mongodbc_reply {
	str rname;
	unsigned int hname;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	str jsonrpl;
	struct mongodbc_reply *next;
} mongodbc_reply_t;

typedef struct mongodbc_pv {
	str rname;
	mongodbc_reply_t *reply;
	str rkey;
	int rkeyid;
} mongodbc_pv_t;

/* Server related functions */
mongodbc_server_t* mongodbc_get_server(str *name);
int mongodbc_reconnect_server(mongodbc_server_t *rsrv);

/* Command related functions */
mongodbc_reply_t *mongodbc_get_reply(str *name);
int mongodbc_free_reply(str *name);
int mongodbc_next_reply(str *name);
#endif

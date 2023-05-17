/* 
 * Copyright (C) 2001-2004 iptel.org
 * Copyright (C) 2008 1&1 Internet AG
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
 */

/*! \file
 *  \brief DB_POSTGRES :: Core
 *  \ingroup db_postgres
 *  Module: \ref db_postgres
 */

#include "km_pg_con.h"
#include "pg_mod.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/tls_hooks_init.h"
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


extern int pg_bytea_output_escape;
extern pg_con_param_t *pg_con_param_list;

/*!
 * \brief Create a new connection
 *
 * Create a new connection structure in private memory, open the PostgreSQL
 * connection and set reference count to 1
 * \param id database id
 * \return postgres connection structure, 0 on error
 */
struct pg_con *db_postgres_new_connection(struct db_id *id)
{
	struct pg_con *ptr;
	char *ports;
	int i = 0;
	const char *keywords[32], *values[32];
	PGresult *res = NULL;
	pg_con_param_t *pg_con_param;

	LM_DBG("db_id = %p\n", id);

	if(!id) {
		LM_ERR("invalid db_id parameter value\n");
		return 0;
	}

	ptr = (struct pg_con *)pkg_malloc(sizeof(struct pg_con));
	if(!ptr) {
		PKG_MEM_ERROR_FMT("%lu bytes for connection structure",
				(unsigned long)sizeof(struct pg_con));
		return 0;
	}
	LM_DBG("%p=pkg_malloc(%lu)\n", ptr, (unsigned long)sizeof(struct pg_con));

	memset(ptr, 0, sizeof(struct pg_con));
	ptr->ref = 1;

	memset(keywords, 0, (sizeof(char *) * 10));
	memset(values, 0, (sizeof(char *) * 10));

	if(id->port) {
		ports = int2str(id->port, 0);
		keywords[i] = "port";
		values[i++] = ports;
		LM_DBG("opening connection: postgres://xxxx:xxxx@%s:%d/%s\n",
				ZSW(id->host), id->port, ZSW(id->database));
	} else {
		ports = NULL;
		LM_DBG("opening connection: postgres://xxxx:xxxx@%s/%s\n",
				ZSW(id->host), ZSW(id->database));
	}

	keywords[i] = "host";
	values[i++] = id->host;
	keywords[i] = "dbname";
	values[i++] = id->database;
	keywords[i] = "user";
	values[i++] = id->username;
	keywords[i] = "password";
	values[i++] = id->password;

	/* add other connection parameters */
	pg_con_param = pg_con_param_list;
	while(pg_con_param) {
		keywords[i] = pg_con_param->name;
		values[i++] = pg_con_param->value;
		pg_con_param = pg_con_param->next;
	}

	keywords[i] = values[i] = NULL;

	/* don't attempt to re-init openssl if done already */
	if(tls_loaded())
		PQinitSSL(0);

	ptr->con = PQconnectdbParams(keywords, values, 1);
	LM_DBG("PQconnectdbParams(%p)\n", ptr->con);

	if((ptr->con == 0) || (PQstatus(ptr->con) != CONNECTION_OK)) {
		LM_ERR("%s\n", PQerrorMessage(ptr->con));
		PQfinish(ptr->con);
		goto err;
	}

	if(PQserverVersion(ptr->con) < 90500) {
		LM_WARN("server version < 9.5 does not support insert_update\n");
	}

	ptr->connected = 1;
	ptr->timestamp = time(0);
	ptr->id = id;

#if defined(SO_KEEPALIVE) && defined(TCP_KEEPIDLE)
	if(pg_keepalive) {
		i = 1;
		if(setsockopt(
				   PQsocket(ptr->con), SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i))
				< 0) {
			LM_WARN("setsockopt for keepalive failed\n");
		}
		if(setsockopt(PQsocket(ptr->con), IPPROTO_TCP, TCP_KEEPIDLE,
				   &pg_keepalive, sizeof(pg_keepalive))
				< 0) {
			LM_WARN("setsockopt for keepidle failed\n");
		}
	}
#endif

	if(pg_bytea_output_escape != 0) {
		res = PQexec(ptr->con, "SET bytea_output=escape");
		if(PQresultStatus(res) != PGRES_COMMAND_OK) {
			LM_ERR("cannot set blob output escaping format\n");
			PQclear(res);
			goto err;
		}
		PQclear(res);
	}
	return ptr;

err:
	if(ptr) {
		LM_DBG("cleaning up %p=pkg_free()\n", ptr);
		pkg_free(ptr);
	}
	return 0;
}


/*!
 * \brief Close the connection and release memory
 * \param con connection
 */
void db_postgres_free_connection(struct pool_con *con)
{

	struct pg_con *_c;

	if(!con)
		return;

	_c = (struct pg_con *)con;

	if(_c->res) {
		LM_DBG("PQclear(%p)\n", _c->res);
		PQclear(_c->res);
		_c->res = 0;
	}
	if(_c->id)
		free_db_id(_c->id);
	if(_c->con) {
		LM_DBG("PQfinish(%p)\n", _c->con);
		PQfinish(_c->con);
		_c->con = 0;
	}
	LM_DBG("pkg_free(%p)\n", _c);
	pkg_free(_c);
}

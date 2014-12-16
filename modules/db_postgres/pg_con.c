/* 
 * $Id$ 
 *
 * PostgreSQL Database Driver for SER
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the ser software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** \addtogroup postgres
 * @{ 
 */

/** \file 
 * Functions related to connections to PostgreSQL servers.
 */

#include "pg_con.h"
#include "pg_uri.h"
#include "pg_sql.h"
#include "pg_mod.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>


/* Override the default notice processor to output the messages 
 * using SER's output subsystem.
 */
static void notice_processor(void* arg, const char* message)
{
	LOG(L_NOTICE, "postgres: %s\n", message);
}


/** Determine the format of timestamps used by the server.  
 * A PostgresSQL server can be configured to store timestamps either as 8-byte
 * integers or floating point numbers with double precision. This functions
 * sends a simple SQL query to the server and tries to determine the format of
 * timestamps from the reply. This function is executed once after connecting
 * to a PostgreSQL server and the result of the detection is then stored in
 * form of a flag in pg_con connection structure.
 * @param con A PostgreSQL connection handle
 * @retval 0 If the server stores timestamps as floating point numbers.
 * @retval 1 If the server stores timestamps as 8-byte integers.
 * @retval A negative number on error.
 */
static int timestamp_format(PGconn* con)
{
	unsigned long long offset;
	PGresult* res = 0;
	char* val;
	str sql;

	if (build_timestamp_format_sql(&sql) != 0) {
		ERR("postgres: Error while building SQL query to obtain timestamp format\n");
		return -1;
	}
	res = PQexecParams(con, sql.s, 0, 0, 0, 0, 0, 1);
	pkg_free(sql.s);

	if (PQfformat(res, 0) != 1) {
		ERR("postgres: Binary format expected but server sent text\n");
		goto error;
	}

	if (PQntuples(res) != 1) {
		ERR("postgres: Only one column expected, %d received\n", PQntuples(res));
		goto error;
	}

	if (PQnfields(res) != 1) {
		ERR("postgres: Only one row expected, %d received\n", PQnfields(res));
		goto error;
	}

	val = PQgetvalue(res, 0, 0);
	offset = ((unsigned long long)ntohl(((unsigned int*)val)[0]) << 32) 
		+ ntohl(((unsigned int*)val)[1]);
	
	PQclear(res);

	/* Server using int8 timestamps would return 1000000, because it stores
	 * timestamps in microsecond resolution across the whole range. Server
	 * using double timestamps would return 1 (encoded as double) here because
	 * subsection fraction is stored as fractional part in the IEEE
	 * representation.  1 stored as double would result in 4607182418800017408
	 * when the memory location occupied by the variable is read as unsigned
	 * long long.
	 */
	if (offset == 1000000) {
	        DBG("postgres: Server uses int8 format for timestamps.\n");
		return 1;
	} else {
		DBG("postgres: Server uses double format for timestamps.\n");
		return 0;
	}
	
 error:
	PQclear(res);
	return -1;
}


/** Retrieves a list of all supported field types from the server.
 * This function retrieves a list of all supported field types and their Oids
 * from system catalogs of the server. The list is then stored in pg_con
 * connection structure and it is used to map field type names, such as int2,
 * int4, float4, etc. to Oids. Every PostgreSQL server can map field types to
 * different Oids so we need to store the mapping array in the connection
 * structure.
 * @param con A structure representing connection to PostgreSQL server.
 * @retval 0 If executed successfully.
 * @retval A negative number on error.
 */
static int get_oids(db_con_t* con)
{
	struct pg_con* pcon;
	PGresult* res = NULL;
	str sql;

	pcon = DB_GET_PAYLOAD(con);
	if (build_select_oid_sql(&sql) < 0) goto error;
	res = PQexec(pcon->con, sql.s);
	pkg_free(sql.s);
	if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) goto error;
	pcon->oid = pg_new_oid_table(res);
	PQclear(res);
	if (pcon->oid == NULL) goto error;
	return 0;

 error:
	if (res) PQclear(res);
	return -1;
}


/** Free all memory allocated for a pg_con structure.
 * This function function frees all memory that is in use by
 * a pg_con structure.
 * @param con A generic db_con connection structure.
 * @param payload PostgreSQL specific payload to be freed.
 */
static void pg_con_free(db_con_t* con, struct pg_con* payload)
{
	if (!payload) return;
	
	/* Delete the structure only if there are no more references
	 * to it in the connection pool
	 */
	if (db_pool_remove((db_pool_entry_t*)payload) == 0) return;
	
	db_pool_entry_free(&payload->gen);
	pg_destroy_oid_table(payload->oid);
	if (payload->con) PQfinish(payload->con);
	pkg_free(payload);
}


int pg_con(db_con_t* con)
{
	struct pg_con* pcon;

	/* First try to lookup the connection in the connection pool and
	 * re-use it if a match is found
	 */
	pcon = (struct pg_con*)db_pool_get(con->uri);
	if (pcon) {
		DBG("postgres: Connection to %.*s:%.*s found in connection pool\n",
			con->uri->scheme.len, ZSW(con->uri->scheme.s),
			con->uri->body.len, ZSW(con->uri->body.s));
		goto found;
	}

	pcon = (struct pg_con*)pkg_malloc(sizeof(struct pg_con));
	if (!pcon) {
		LOG(L_ERR, "postgres: No memory left\n");
		goto error;
	}
	memset(pcon, '\0', sizeof(struct pg_con));
	if (db_pool_entry_init(&pcon->gen, pg_con_free, con->uri) < 0) goto error;

	DBG("postgres: Preparing new connection to: %.*s:%.*s\n",
		con->uri->scheme.len, ZSW(con->uri->scheme.s),
		con->uri->body.len, ZSW(con->uri->body.s));

	/* Put the newly created postgres connection into the pool */
	db_pool_put((struct db_pool_entry*)pcon);
	DBG("postgres: Connection stored in connection pool\n");

 found:
	/* Attach driver payload to the db_con structure and set connect and
	 * disconnect functions
	 */
	DB_SET_PAYLOAD(con, pcon);
	con->connect = pg_con_connect;
	con->disconnect = pg_con_disconnect;
	return 0;

 error:
	if (pcon) {
		db_pool_entry_free(&pcon->gen);
		pkg_free(pcon);
	}
	return -1;
}


int pg_con_connect(db_con_t* con)
{
	struct pg_con* pcon;
	struct pg_uri* puri;
	char* port_str;
	int ret, i = 0;
	const char *keywords[10], *values[10];
	char to[16];
	
	pcon = DB_GET_PAYLOAD(con);
	puri = DB_GET_PAYLOAD(con->uri);
	
	/* Do not reconnect already connected connections */
	if (pcon->flags & PG_CONNECTED) return 0;

	DBG("postgres: Connecting to %.*s:%.*s\n",
		con->uri->scheme.len, ZSW(con->uri->scheme.s),
		con->uri->body.len, ZSW(con->uri->body.s));

	if (puri->port > 0) {
		port_str = int2str(puri->port, 0);
		keywords[i] = "port";
		values[i++] = port_str;
	} else {
		port_str = NULL;
	}

	if (pcon->con) {
		PQfinish(pcon->con);
		pcon->con = NULL;
	}

	keywords[i] = "host";
	values[i++] = puri->host;
	keywords[i] = "dbname";
	values[i++] = puri->database;
	keywords[i] = "user";
	values[i++] = puri->username;
	keywords[i] = "password";
	values[i++] = puri->password;
	if (pg_timeout > 0) {
		snprintf(to, sizeof(to)-1, "%d", pg_timeout + 3);
		keywords[i] = "connect_timeout";
		values[i++] = to;
	}

	keywords[i] = values[i] = NULL;

	pcon->con = PQconnectdbParams(keywords, values, 1);
	
	if (pcon->con == NULL) {
		ERR("postgres: PQconnectdbParams ran out of memory\n");
		goto error;
	}
	
	if (PQstatus(pcon->con) != CONNECTION_OK) {
		ERR("postgres: %s\n", PQerrorMessage(pcon->con));
		goto error;
	}
	
	/* Override default notice processor */
	PQsetNoticeProcessor(pcon->con, notice_processor, 0);
	
#ifdef HAVE_PGSERVERVERSION
	DBG("postgres: Connected. Protocol version=%d, Server version=%d\n", 
	    PQprotocolVersion(pcon->con), PQserverVersion(pcon->con));
#else
	DBG("postgres: Connected. Protocol version=%d, Server version=%d\n", 
	    PQprotocolVersion(pcon->con), 0 );
#endif

#if defined(SO_KEEPALIVE) && defined(TCP_KEEPIDLE)
	if (pg_keepalive) {
		i = 1;
		setsockopt(PQsocket(pcon->con), SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));
		setsockopt(PQsocket(pcon->con), IPPROTO_TCP, TCP_KEEPIDLE, &pg_keepalive, sizeof(pg_keepalive));
	}
#endif

	ret = timestamp_format(pcon->con);
	if (ret == 1 || ret == -1) {
		/* Assume INT8 representation if detection fails */
		pcon->flags |= PG_INT8_TIMESTAMP;
	} else {
		pcon->flags &= ~PG_INT8_TIMESTAMP;
	}

	if (get_oids(con) < 0) goto error;

	pcon->flags |= PG_CONNECTED;
	return 0;

 error:
	if (pcon->con) PQfinish(pcon->con);
	pcon->con = NULL;
	return -1;
}


void pg_con_disconnect(db_con_t* con)
{
	struct pg_con* pcon;

	pcon = DB_GET_PAYLOAD(con);
	if ((pcon->flags & PG_CONNECTED) == 0) return;

	DBG("postgres: Disconnecting from %.*s:%.*s\n",
		con->uri->scheme.len, ZSW(con->uri->scheme.s),
		con->uri->body.len, ZSW(con->uri->body.s));

	PQfinish(pcon->con);
	pcon->con = NULL;
	pcon->flags &= ~PG_CONNECTED;
	pcon->flags &= ~PG_INT8_TIMESTAMP;
}

/** @} */

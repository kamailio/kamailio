/* 
 * $Id$
 *
 * Portions Copyright (C) 2001-2003  FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pg_con.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include <string.h>
#include <netinet/in.h>
#include <time.h>


/*
 * Override the default notice processor to output the messages 
 * using SER's output subsystem.
 */
static void notice_processor(void* arg, const char* message)
{
	LOG(L_NOTICE, "postgres: %s\n", message);
}



/*
 * Determine the format used by the server to store timestamp data type
 * The function returns 1 if the server stores timestamps as int8 and 0
 * if it is stored as double
 */
static int timestamp_format(PGconn* con)
{
	unsigned long long offset;
	PGresult* res = 0;
	char* val;

	res = PQexecParams(con, "select timestamp '2000-01-01 00:00:00' + time '00:00:01'", 0, 0, 0, 0, 0, 1);	

	if (PQfformat(res, 0) != 1) {
		ERR("Binary format expected but server sent text\n");
		goto err;
	}

	if (PQntuples(res) != 1) {
		ERR("1 column expected, %d received\n", PQntuples(res));
		goto err;
	}

	if (PQnfields(res) != 1) {
		ERR("1 Row expected, %d received\n", PQnfields(res));
		goto err;
	}

	val = PQgetvalue(res, 0, 0);
	offset = ((unsigned long long)ntohl(((unsigned int*)val)[0]) << 32) 
		+ ntohl(((unsigned int*)val)[1]);

	PQclear(res);

	     /* Server using int8 timestamps would return 1000000, because it stores
	      * timestamps in microsecond resolution across the whole range. Server using
	      * double timestamps would return 1 (encoded as double) here because subsection
	      * fraction is stored as fractional part in the IEEE representation.
	      * 1 stored as double would result in 4607182418800017408 when the memory location
	      * occupied by the variable is read as unsigned long long.
	      */
	if (offset == 1000000) {
	        DBG("Server uses int8 format for timestamps.\n");
		return 1;
	} else {
		DBG("Server uses double format for timestamps.\n");
		return 0;
	}
	
 err:
	PQclear(res);
	return -1;
}


/*
 * Create a new connection structure,
 * open the Postgres connection and set reference count to 1
 */
struct pg_con* pg_new_connection(struct db_id* id)
{
	struct pg_con* ptr;
	char* port_str;
	int ret;
	
	if (!id) {
		ERR("Invalid parameter value\n");
		return 0;
	}
	
	ptr = (struct pg_con*)pkg_malloc(sizeof(struct pg_con));
	if (!ptr) {
		ERR("No memory left\n");
		return 0;
	}
	memset(ptr, 0, sizeof(struct pg_con));
	ptr->ref = 1;
	
	if (id->port > 0) {
		port_str = int2str(id->port, 0);
	} else {
		port_str = NULL;
	}

	if (id->port) {
		DBG("Opening connection to: %s://%s:%s@%s:%d/%s\n",
		    ZSW(id->scheme),
		    ZSW(id->username),
		    ZSW(id->password),
		    ZSW(id->host),
		    id->port,
		    ZSW(id->database)
		    );
	} else {
		DBG("Opening connection to: %s://%s:%s@%s/%s\n",
		    ZSW(id->scheme),
		    ZSW(id->username),
		    ZSW(id->password),
		    ZSW(id->host),
		    ZSW(id->database)
		    );
	}
	
	ptr->con = PQsetdbLogin(id->host, port_str,
				NULL, NULL, id->database,
				id->username, id->password);
	
	if (ptr->con == 0) {
		ERR("PQsetdbLogin ran out of memory\n");
		goto err;
	}
	
	if (PQstatus(ptr->con) != CONNECTION_OK) {
		ERR("postgres:new_connection: %s\n",
		    PQerrorMessage(ptr->con));
		goto err;
	}

	     /* Override default notice processor */
	PQsetNoticeProcessor(ptr->con, notice_processor, 0);

	DBG("Connected. Protocol version=%d, Server version=%d\n", 
	    PQprotocolVersion(ptr->con),
#ifdef HAVE_PGSERVERVERSION
	    PQserverVersion(ptr->con)
#else
	    0
#endif
	    );

	ptr->timestamp = time(0);
	ptr->id = id;

	ret = timestamp_format(ptr->con);
	if (ret == 1 || ret == -1) {
		     /* Assume INT8 representation if detection fails */
		ptr->flags |= PG_INT8_TIMESTAMP;
	}

	return ptr;

 err:
	if (ptr && ptr->con) PQfinish(ptr->con);
	if (ptr) pkg_free(ptr);
	return 0;
}


/*
 * Close the connection and release memory
 */
void pg_free_connection(struct pg_con* con)
{
	if (!con) return;
	if (con->id) free_db_id(con->id);
	if (con->con) PQfinish(con->con);
	pkg_free(con);
}

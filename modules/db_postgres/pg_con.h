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

#ifndef _PG_CON_H
#define _PG_CON_H

/** \addtogroup postgres
 * @{ 
 */

/** \file 
 * Implementation of PostgreSQL connection related data structures and functions.
 */

#include "pg_oid.h"

#include "../../lib/srdb2/db_pool.h"
#include "../../lib/srdb2/db_con.h"
#include "../../lib/srdb2/db_uri.h"

#include <time.h>
#include <libpq-fe.h>

/** 
 * Per-connection flags for PostgreSQL connections.
 */
enum pg_con_flags {
	PG_CONNECTED      = (1 << 0), /**< The connection has been connected successfully */
	PG_INT8_TIMESTAMP = (1 << 1)  /**< The server uses 8-byte integer format for timestamps */
};


/** A structure representing a connection to PostgreSQL server.
 * This structure represents connections to PostgreSQL servers. It contains
 * PostgreSQL specific data, such as PostgreSQL connection handle, connection
 * flags, and an array with data types supported by the server.
 */
typedef struct pg_con {
	db_pool_entry_t gen;  /**< Generic part of the structure */
	PGconn* con;          /**< Postgres connection handle */
	unsigned int flags;   /**< Flags (currently only binary data format) */
	pg_type_t* oid;       /**< Data types and their Oids obtained from the server */
} pg_con_t;


/** Create a new pg_con structure.
 * This function creates a new pg_con structure and attachs the structure to
 * the generic db_con structure in the parameter.
 * @param con A generic db_con structure to be extended with PostgreSQL
 *            payload
 * @retval 0 on success
 * @retval A negative number on error
 */
int pg_con(db_con_t* con);


/** Establish a new connection to server.  
 * This function is called when a SER module calls db_connect to establish a
 * new connection to the database server. After the connection is established
 * the function sends an SQL query to the server to determine the format of
 * timestamp fields and also obtains the list of supported field types.
 * @param con A structure representing database connection.
 * @retval 0 on success.
 * @retval A negative number on error.
 */
int pg_con_connect(db_con_t* con);


/** Disconnected from PostgreSQL server.
 * Disconnects a previously connected connection to PostgreSQL server.
 * @param con A structure representing the connection to be disconnected.
 */
void pg_con_disconnect(db_con_t* con);

/** @} */

#endif /* _PG_CON_H */

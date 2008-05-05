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

#ifndef _PG_OID_H
#define _PG_OID_H

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Data structures and functions implementing support for PostgreSQL Oid
 * identifiers.
 */

#include <libpq-fe.h>

/** Structure mapping field names to Oids.
 * This structure is used to map field names or data type names to their
 * Oids/field types.
 */
typedef struct pg_type {
	Oid oid;    /**< PostgreSQL Oid (object identifier) */
	char* name; /**< Field name */
} pg_type_t;


/** Enumeration of supported PostreSQL types.
 * This is the enumeration of all PostgreSQL types supported
 * by this driver, that means this driver will be able to convert
 * to and from these types.
 *
 * This enum is primarilly used as index to arrays of pg_type_t that are
 * stored in pg_con structures. Upon connecting to a PostgreSQL server the
 * driver retrieves the list of supported data types and oids from the server
 * and stores then in an array. Different PostgreSQL servers can have
 * different Oids for various data types so we have to have one array that
 * maps symbolic names below to Oids per connection/server.
 */
enum pg_type_id {
	PG_BOOL = 0,    /**< Boolean, true/false */
	PG_BYTE,        /**< Binary data */
	PG_CHAR,        /**< Single character */
	PG_INT8,        /**< Integer with 8-byte storage */
	PG_INT2,        /**< Integer with 2-byte storage */
	PG_INT4,        /**< Integer with 4-byte storage */
	PG_TEXT,        /**< Variable-length string, no limit specified */
	PG_FLOAT4,      /**< Single-precision floating point number, 4-byte storage */
	PG_FLOAT8,      /**< Double-precision floating point number, 8-byte storage */
	PG_INET,        /**< IP address/netmask, host address */
	PG_BPCHAR,      /**< Blank-padded string, fixed storage length */
	PG_VARCHAR,     /**< Non-blank padded string, variable storage length */
	PG_TIMESTAMP,   /**< Date and time */
	PG_TIMESTAMPTZ, /**< Date and time with time zone */
	PG_BIT,         /**< Fixed-length bit string */
	PG_VARBIT,      /**< Variable-length bit string */
	PG_ID_MAX       /**< Bumper, this must be the last element of the enum */
};


/** Creates a new Oid mapping table.
 * The function creates a new Oid mapping table and initalizes the contents
 * of the table with values obtained from the PostgreSQL result in parameter.
 * Each element of the table maps field type name to oid and vice versa.
 * @param res A PostgreSQL result structure used to initialize the array.
 * @retval A pointer to the resulting array.
 * @retval NULL on error.
 */
pg_type_t* pg_new_oid_table(PGresult* res);


/** Frees all memory used by the table
 * @param table A pointer to table to be freed
 */
void pg_destroy_oid_table(pg_type_t* table);


/** Maps a field type name to Oid.
 * @param oid The resulting oid
 * @param table The mapping table
 * @param name Field type name
 * @retval 0 on success
 * @retval 1 if the type name is unknown
 * @retval -1 on error.
 */
int pg_name2oid(Oid* oid, pg_type_t* table, const char* name);


/** Maps a field type name to Oid.
 * @param oid The resulting oid
 * @param table The mapping table
 * @param name Field type name
 * @retval 0 on success
 * @retval 1 if the type name is unknown
 * @retval -1 on error.
 */
int pg_oid2name(const char** name, pg_type_t* table, Oid oid);

/** @} */

#endif /* _PG_OID_H */

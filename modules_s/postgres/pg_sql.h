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

#ifndef _PG_SQL_H
#define _PG_SQL_H

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Implementation of various functions that assemble SQL query strings for
 * PostgreSQL.
 */

#include "../../lib/srdb2/db_cmd.h"
#include "../../str.h"


/** Builds an UPDATE SQL statement.
 * This function creates an UPDATE SQL statement where column values are
 * replaced with special markers. The resulting SQL statement is suitable
 * for submitting as prepared statement. The result string is zero terminated.
 * @param sql_cmd Pointer to a str variable where the resulting SQL statement
 *                will be stored. The buffer in sql_cmd->s is allocated using
 *                pkg_malloc and the caller of the function is responsible for
 *                freeing it.
 * @param cmd The command whose data will be used to generate the query.
 * @return 0 on success, negative number on error
 */
int build_update_sql(str* sql_cmd, db_cmd_t* cmd);


/** Builds an INSERT SQL statement.
 * This function creates an INSERT SQL statement where column values are
 * replaced with special markers. The resulting SQL statement is suitable
 * for submitting as prepared statement. The result string is zero terminated.
 * @param sql_cmd Pointer to a str variable where the resulting SQL statement
 *                will be stored. The buffer in sql_cmd->s is allocated using
 *                pkg_malloc and the caller of the function is responsible for *                freeing it.
 * @param cmd The command whose data will be used to generate the query.
 * @return 0 on success, negative number on error
 */
int build_insert_sql(str* sql_cmd, db_cmd_t* cmd);


/** Builds a DELETE SQL statement.
 * This function creates a DELETE SQL statement where column values are
 * replaced with special markers. The resulting SQL statement is suitable
 * for submitting as prepared statement. The result string is zero terminated.
 * @param sql_cmd Pointer to a str variable where the resulting SQL statement
 *                will be stored. The buffer in sql_cmd->s is allocated using
 *                pkg_malloc and the caller of the function is responsible for
 *                freeing it.
 * @param cmd The command whose data will be used to generate the query.
 * @return 0 on success, negative number on error
 */
int build_delete_sql(str* sql_cmd, db_cmd_t* cmd);


/** Builds a SELECT SQL statement.
 * This function creates a SELECT SQL statement where column values are
 * replaced with special markers. The resulting SQL statement is suitable
 * for submitting as prepared statement. The result string is zero terminated.
 * @param sql_cmd Pointer to a str variable where the resulting SQL statement
 *                will be stored. The buffer in sql_cmd->s is allocated using
 *                pkg_malloc and the caller of the function is responsible for
 *                freeing it.
 * @param cmd The command whose data will be used to generate the query.
 * @return 0 on success, negative number on error
 */
int build_select_sql(str* sql_cmd, db_cmd_t* cmd);


/* Builds SQL query used to obtain the list of supported field types.
 * This function builds a special SQL query that is used to obtain the list
 * of supported field type from the server's system catalogs.
 */
int build_select_oid_sql(str* sql_cmd);


/** Builds the SQL query used to determine the format of timestamp fields.
 * This function builds a special SQL query that is sent to the server
 * immediately after establishing a connection to determine the format of
 * timestamp fields used on the server.
 */
int build_timestamp_format_sql(str* sql_cmd);

/** @} */

#endif /* _PG_SQL_H */

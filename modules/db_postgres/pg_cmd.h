/* 
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

#ifndef _PG_CMD_H
#define _PG_CMD_H


/*!
 * \file
 * \brief DB_POSTGRES :: * Declaration of pg_cmd data structure
 * 
 * Declaration of pg_cmd data structure  that contains PostgreSQL specific data
 * stored in db_cmd structures and related functions.
 * \ingroup db_postgres
 * Module: \ref db_postgres
 */

#include "pg_oid.h"

#include "../../lib/srdb2/db_drv.h"
#include "../../lib/srdb2/db_cmd.h"
#include "../../lib/srdb2/db_res.h"
#include "../../str.h"

#include <stdarg.h>
#include <libpq-fe.h>

struct pg_params {
	int n;
	const char** val;
	int* len;
	int* fmt;
};


/** Extension structure of db_cmd adding PostgreSQL specific data.
 * This data structure extends the generic data structure db_cmd in the
 * database API with data specific to the postgresql driver.
 */
struct pg_cmd {
	db_drv_t gen; /**< Generic part of the data structure (must be first */
	char* name;   /**< Name of the prepared query on the server */
	str sql_cmd;  /**< Database command represented in SQL language */

	struct pg_params params;
	PGresult* types;
};


/** Creates a new pg_cmd data structure.
 * This function allocates and initializes memory for a new pg_cmd data
 * structure. The data structure is then attached to the generic db_cmd
 * structure in cmd parameter.
 * @param cmd A generic db_cmd structure to which the newly created pg_cmd
 *            structure will be attached.
 */
int pg_cmd(db_cmd_t* cmd);


/** The main execution function in postgres SER driver.
 * This is the main execution function in this driver. It is executed whenever
 * a SER module calls db_exec and the target database of the commands is
 * PostgreSQL. The function contains all the necessary logic to detect reset
 * or disconnected database connections and uploads commands to the server if
 * necessary.
 * @param res A pointer to (optional) result structure if the command returns
 *            a result.
 * @param cmd executed command
 * @retval 0 if executed successfully
 * @retval A negative number if the database server failed to execute command
 * @retval A positive number if there was an error on client side (SER)
 */
int pg_cmd_exec(db_res_t* res, db_cmd_t* cmd);


/** Retrieves the first record from a result set received from PostgreSQL server.
 * This function is executed whenever a SER module calls db_first to retrieve
 * the first record from a result. The function retrieves the first record
 * from a PGresult structure and converts the fields from PostgreSQL to
 * internal SER representation.
 * 
 * @param res A result set retrieved from PostgreSQL server.
 * @retval 0 If executed successfully.
 * @retval 1 If the result is empty.
 * @retval A negative number on error.
 */
int pg_cmd_first(db_res_t* res);


/** Retrieves the next record from a result set received from PostgreSQL server.
 * This function is executed whenever a SER module calls db_next to retrieve
 * the first record from a result. The function advances current cursor
 * position in the result, retrieves the next record from a PGresult structure
 * and converts the fields from PostgreSQL to internal SER representation.
 * 
 * @param res A result set retrieved from PostgreSQL server.
 * @retval 0 If executed successfully.
 * @retval 1 If there are no more records in the result.
 * @retval A negative number on error.
 */
int pg_cmd_next(db_res_t* res);


/** Retrieves the value of an db_cmd option.
 * This function is called when a SER module uses db_getopt to retrieve the
 * value of db_cmd parameter.
 * @param cmd A db_cmd structure representing the command.
 * @param optname Name of the option.
 * @param ap A pointer the result variable.
 * @retval 0 on success.
 * @retval A positive number of the option is not supported/implemented.
 * @retval A negative number on error.
 */
int pg_getopt(db_cmd_t* cmd, char* optname, va_list ap);


/** Sets the value of an db_cmd option.
 * This function is called when a SER module uses db_setopt to set the
 * value of db_cmd parameter.
 * @param cmd A db_cmd structure representing the command.
 * @param optname Name of the option.
 * @param ap A variable with the value to be set.
 * @retval 0 on success.
 * @retval A positive number of the option is not supported/implemented.
 * @retval A negative number on error.
 */
int pg_setopt(db_cmd_t* cmd, char* optname, va_list ap);

#endif /* _PG_CMD_H */

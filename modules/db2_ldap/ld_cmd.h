/*
 * $Id$
 *
 * LDAP Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _LD_CMD_H
#define _LD_CMD_H

/** \addtogroup ldap
 * @{
 */

/** \file
 * Declaration of ld_cmd data structure that contains LDAP specific data
 * stored in db_cmd structures and related functions.
 */

#include "../../lib/srdb2/db_drv.h"
#include "../../lib/srdb2/db_cmd.h"
#include "../../lib/srdb2/db_res.h"
#include "../../str.h"

#include <stdarg.h>
#include <sys/time.h>


/** Extension structure of db_cmd adding LDAP specific data.
 * This data structure extends the generic data structure db_cmd in the
 * database API with data specific to the ldap driver.
 */
struct ld_cmd {
	db_drv_t gen; /**< Generic part of the data structure (must be first */
	char* base;   /**< Search base of the command */
	int scope;    /**< Scope of the search */
	str filter;   /**< To be added to the search filter */
	char** result; /**< An array with result attribute names for ldap_search */
	int sizelimit; /**< retrieve at most sizelimit entries for a search */
	struct timeval timelimit; /**< wait at most timelimit seconds for a search to complete */
	int chase_references;  /**< dereference option for LDAP library */
	int chase_referrals;   /**< follow referrals option for LDAP library */
};


/** Creates a new ld_cmd data structure.
 * This function allocates and initializes memory for a new ld_cmd data
 * structure. The data structure is then attached to the generic db_cmd
 * structure in cmd parameter.
 * @param cmd A generic db_cmd structure to which the newly created ld_cmd
 *            structure will be attached.
 */
int ld_cmd(db_cmd_t* cmd);


/** The main execution function in ldap SER driver.
 * This is the main execution function in this driver. It is executed whenever
 * a SER module calls db_exec and the target database of the commands is
 * ldap.
 * @param res A pointer to (optional) result structure if the command returns
 *            a result.
 * @retval 0 if executed successfully
 * @retval A negative number if the database server failed to execute command
 * @retval A positive number if there was an error on client side (SER)
 */
int ld_cmd_exec(db_res_t* res, db_cmd_t* cmd);


int ld_cmd_first(db_res_t* res);


int ld_cmd_next(db_res_t* res);

int ld_cmd_setopt(db_cmd_t* cmd, char* optname, va_list ap);

/** @} */

#endif /* _LD_CMD_H */

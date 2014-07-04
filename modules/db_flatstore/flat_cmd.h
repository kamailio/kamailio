/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _FLAT_CMD_H
#define _FLAT_CMD_H

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * Inmplementation of flatstore commands.
 */

#include "../../lib/srdb2/db_drv.h"
#include "../../lib/srdb2/db_cmd.h"


/** Extension structure of db_cmd adding flatstore specific data.
 * This data structure extends the generic data structure db_cmd in the
 * database API with data specific to the flatstore driver.
 */
struct flat_cmd {
	db_drv_t gen; /**< Generic part of the data structure (must be first) */
	int file_index;
};


/** Creates a new flat_cmd data structure.
 * This function allocates and initializes memory for a new flat_cmd data
 * structure. The data structure is then attached to the generic db_cmd
 * structure in cmd parameter.
 * @param cmd A generic db_cmd structure to which the newly created flat_cmd
 *            structure will be attached.
 */
int flat_cmd(db_cmd_t* cmd);


/** The main execution function in flat SER driver.
 * This is the main execution function in this driver. It is executed whenever
 * a SER module calls db_exec and the target database of the commands is
 * flatstore.
 * @param res A pointer to (optional) result structure if the command returns
 *            a result.
 * @retval 0 if executed successfully
 * @retval A negative number if the database server failed to execute command
 * @retval A positive number if there was an error on client side (SER)
 */
int flat_put(db_res_t* res, db_cmd_t* cmd);


/** @} */

#endif /* _FLAT_CMD_H */

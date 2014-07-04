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

#ifndef _FLAT_CON_H
#define _FLAT_CON_H

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * Inmplementation of flatstore "connections".
 */

#include "../../lib/srdb2/db_pool.h"
#include "../../lib/srdb2/db_con.h"
#include "../../lib/srdb2/db_uri.h"

#include <stdio.h>


/** 
 * Per-connection flags for flatstore connections.
 */
enum flat_con_flags {
	FLAT_OPENED      = (1 << 0), /**< Handle opened successfully */
};


struct flat_file {
	char* filename; /**< Name of file within the directory */
	str table;      /**< Table name the file belongs to */
	FILE* f;        /**< File handle of the file */
};


/** A structure representing flatstore virtual connections.
 * Flatstore module is writing data to files on a local filesystem only so
 * there is no concept of real database connections.  In flatstore module a
 * connection is related with a directory on the filesystem and it contains
 * file handles to files in that directory. The file handles are then used
 * from commands to write data in them.
 */
struct flat_con {
	db_pool_entry_t gen;    /**< Generic part of the structure */
	struct flat_file* file;
	int n;                  /**< Size of the file array */
	unsigned int flags;     /**< Flags */
};


/** Create a new flat_con structure.
 * This function creates a new flat_con structure and attachs the structure to
 * the generic db_con structure in the parameter.
 * @param con A generic db_con structure to be extended with flatstore payload
 * @retval 0 on success
 * @retval A negative number on error
 */
int flat_con(db_con_t* con);


int flat_con_connect(db_con_t* con);


void flat_con_disconnect(db_con_t* con);


int flat_open_table(int *idx, db_con_t* con, str* name);

/** @} */

#endif /* _FLAT_CON_H */

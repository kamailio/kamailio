/*
 * BDB Database Driver for Kamailio
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _BDB_FLD_H_
#define _BDB_FLD_H_

/** \addtogroup bdb
 * @{
 */

/*! \file
 * Berkeley DB : 
 * Implementation of bdb_fld data structure representing BDB fields and
 * related functions.
 *
 * \ingroup database
 */

#include <db.h>

#include "../../lib/srdb2/db_gen.h"
#include "../../lib/srdb2/db_fld.h"

typedef struct _bdb_fld {
	db_drv_t gen;
	char* name;
	int is_null;
	unsigned long length;
	str buf;
	int col_pos;
} bdb_fld_t, *bdb_fld_p;


/** Creates a new BDB specific payload.
 * This function creates a new BDB specific payload structure and
 * attaches the structure to the generic db_fld structure.
 * @param fld A generic db_fld structure to be exended.
 * @param table Name of the table on the server.
 * @retval 0 on success.
 * @retval A negative number on error.
 */
int bdb_fld(db_fld_t* fld, char* table);

/** @} */

#endif /* _BDB_FLD_H_ */

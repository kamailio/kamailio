/* 
 * $Id$ 
 *
 * Flatstore module interface
 *
 * Copyright (C) 2004 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#ifndef _KM_FLATSTORE_H
#define _KM_FLATSTORE_H

#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_con.h"


/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* flat_db_init(const str* _url);


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int flat_use_table(db1_con_t* h, const str* t);


void flat_db_close(db1_con_t* h);


/*
 * Insert a row into specified table
 * h: structure representing database connection
 * k: key names
 * v: values of the keys
 * n: number of key=value pairs
 */
int flat_db_insert(const db1_con_t* h, const db_key_t* k, const db_val_t* v,
		const int n);


#endif /* _KM_FLATSTORE_H */

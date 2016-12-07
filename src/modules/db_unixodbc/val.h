/* 
 * $Id$ 
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
 * Copyright (C) 2008 1&1 Internet AG
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
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */


#ifndef VAL_H
#define VAL_H

#include <stdio.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db.h"


/*
 * Used when converting the query to a result
 */
int db_unixodbc_str2val(const db_type_t _t, db_val_t* _v, const char* _s, const int _l,
		const unsigned int _cpy);

/*
 * Used when converting result from a query
 */
int db_unixodbc_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len);


#endif /* VAL_H */

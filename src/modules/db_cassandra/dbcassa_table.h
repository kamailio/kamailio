/*
 * $Id$
 *
 * Copyright (C) 2012 1&1 Internet Development
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
 * History:
 * --------
 * 2012-01 initial version (Anca Vamanu)
 * 
 */


#ifndef _DBCASSA_LIB_H_
#define _DBCASSA_LIB_H_

#include "../../str.h"
#include "../../lib/srdb1/db_val.h"

#define DBCASSA_DELIM     ':'
#define DBCASSA_DELIM_C   ' '
#define DBCASSA_DELIM_R   '\n'

typedef struct _dbcassa_column
{
	str name;
	db_type_t type;
	int flag;
	struct _dbcassa_column *next;
} dbcassa_column_t, *dbcassa_column_p;

typedef struct _dbcassa_table
{
	str dbname;
	str name;
	int hash;
	time_t mt;
	int nrcols;
	int key_len;
	int seckey_len;
	dbcassa_column_p cols;
	dbcassa_column_p *key;
	dbcassa_column_p *sec_key;
	dbcassa_column_p ts_col; /* timestamp col- special col used as the timestamp for the row */
	struct _dbcassa_table *next;
} dbcassa_table_t, *dbcassa_table_p;

void dbcassa_lock_release(dbcassa_table_p tbc);
dbcassa_table_p dbcassa_db_get_table(const str* dbn, const str* tbn);
int dbcassa_read_table_schemas(void);
void dbcassa_destroy_htable(void);

#endif

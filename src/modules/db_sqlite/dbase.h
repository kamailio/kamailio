/*
 * $Id$
 *
 * SQlite module core functions
 *
 * Copyright (C) 2010 Timo Teräs
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


#ifndef DBASE_H
#define DBASE_H

#include <sqlite3.h>

#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"

#define DB_SQLITE_MAX_BINDS 64

struct sqlite_connection {
	struct pool_con hdr;

	sqlite3 *conn;
	int bindpos;

	sqlite3_stmt *stmt;
	const db_val_t *bindarg[DB_SQLITE_MAX_BINDS];
};

#define CON_SQLITE(db_con)	((struct sqlite_connection *) db_con->tail)

db1_con_t* db_sqlite_init(const str* _sqlurl);
void db_sqlite_close(db1_con_t* _h);

int db_sqlite_free_result(db1_con_t* _h, db1_res_t* _r);

int db_sqlite_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc,
		const db_key_t _o, db1_res_t** _r);
int db_sqlite_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n);
int db_sqlite_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, int _n);
int db_sqlite_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
		int _n, int _un);
int db_sqlite_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r);

int db_sqlite_use_table(db1_con_t* _h, const str* _t);


#endif /* DBASE_H */

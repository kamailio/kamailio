/* 
 * $Id: perlvdbfunc.h 770 2007-01-22 10:16:34Z bastian $
 *
 * Perl virtual database module interface
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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

#ifndef _PERLVDBFUNC_H
#define _PERLVDBFUNC_H

#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../str.h"


/*
 * Initialize/close database module
 * No function should be called before/after this
 */
db1_con_t* perlvdb_db_init(const char* _url);
void perlvdb_db_close(db1_con_t* h);

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int perlvdb_use_table(db1_con_t* h, const str* t);

int perlvdb_db_insert(db1_con_t* h, db_key_t* k, db_val_t* v, int n);
int perlvdb_db_replace(db1_con_t* h, db_key_t* k, db_val_t* v, int n);
int perlvdb_db_delete(db1_con_t* h, db_key_t* k, db_op_t* o, db_val_t* v, int n);
int perlvdb_db_update(db1_con_t* h, db_key_t* k, db_op_t* o, db_val_t* v,
	      db_key_t* uk, db_val_t* uv, int n, int un);

int perlvdb_db_query(db1_con_t* h, db_key_t* k, db_op_t* op, db_val_t* v,
			db_key_t* c, int n, int nc,
			db_key_t o, db1_res_t** r);

int perlvdb_db_free_result(db1_con_t* _h, db1_res_t* _r);

#endif /* _PERLVDBFUNC_H */

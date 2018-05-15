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
#include "../../core/str.h"


/*
 * Initialize/close database module
 * No function should be called before/after this
 */
db1_con_t* perlvdb_db_init(const str* _url);
void perlvdb_db_close(db1_con_t* h);

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int perlvdb_use_table(db1_con_t* h, const str* t);

int perlvdb_db_insert(const db1_con_t* h, const db_key_t* k, const db_val_t* v, const int n);
int perlvdb_db_replace(const db1_con_t* h, const db_key_t* k, const db_val_t* v,
		const int n, const int un, const int m);
int perlvdb_db_delete(const db1_con_t* h, const db_key_t* k, const db_op_t* o,
		const db_val_t* v, const int n);
int perlvdb_db_update(const db1_con_t* h, const db_key_t* k, const db_op_t* o,
		const db_val_t* v, const db_key_t* uk, const db_val_t* uv,
		const int n, const int un);

int perlvdb_db_query(const db1_con_t* h, const db_key_t* k, const db_op_t* op,
		const db_val_t* v, const db_key_t* c, const int n, const int nc,
		const db_key_t o, db1_res_t** r);
int perlvdb_db_free_result(db1_con_t* _h, db1_res_t* _r);

#endif /* _PERLVDBFUNC_H */

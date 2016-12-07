/* $Id$
 *
 * Copyright (C) 2006-2007 Sippy Software, Inc. <sales@sippysoft.com>
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef _BDB_H_
#define _BDB_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <string.h>
#include <db.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../timer.h"
#include "../../lib/srdb2/db_con.h"
#include "../../lib/srdb2/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"

#include "bdb_res.h"
#include "bdb_api.h"
#include "bdb_vals.h"

extern bdb_table_p	bdb_tables;

db_con_t* bdb_init(const char* _sqlurl);
void bdb_close(db_con_t* _h);
int bdb_free_result(db_con_t* _h, db_res_t* _r);
int bdb_query(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
			db_key_t* _c, int _n, int _nc, db_key_t _o, db_res_t** _r);
int bdb_raw_query(db_con_t* _h, char* _s, db_res_t** _r);
int bdb_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);
int bdb_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);
int bdb_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un);

#endif


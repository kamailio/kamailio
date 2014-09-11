/*
 * Core functions.
 * See db/db.h for description.
 *
 * Copyright (C) 2005 RingCentral Inc.
 * Created by Dmitry Semyonov <dsemyonov@ringcentral.com>
 *
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
 */

#ifndef DBASE_H
#define DBASE_H

#include <oci.h>

#include "../../lib/srdb2/db_con.h"
#include "../../lib/srdb2/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"


/* A pointer to this structure is assigned to CON_TAIL(_h). */
typedef struct _ora_con_t
{
  union {
    OCIEnv    *ptr;
    dvoid     *dvoid_ptr;
  } env;

  union {
    OCIError  *ptr;
    dvoid     *dvoid_ptr;
  } err;

  union {
    OCISvcCtx *ptr;
    dvoid     *dvoid_ptr;
  } svc;

  union {
    OCIStmt   *ptr;
    dvoid     *dvoid_ptr;
  } stmt;

} ora_con_t;

#define CON_ORA(_h)    ((ora_con_t *)(CON_TAIL(_h)))


typedef struct _ora_param_t
{
  void  *p_data;
  ub4    size;
  ub2    type;
  OCIInd ind;
  
} ora_param_t;


int db_use_table(db_con_t* _h, const char* _t);

db_con_t* db_init(const char* _sqlurl);

void db_close(db_con_t* _h); 

int db_query(db_con_t* _h, db_key_t* _k, 
	         db_op_t* _op, db_val_t* _v, 
             db_key_t* _c, int _n, int _nc,
             db_key_t _o, db_res_t** _r);

int db_raw_query(db_con_t* _h, char* _s, db_res_t** _r);

int db_free_result(db_con_t* _h, db_res_t* _r);

int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);

int db_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);

int db_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
              db_key_t* _uk, db_val_t* _uv, int _n, int _un);


#endif /* DBASE_H */

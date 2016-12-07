/*
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

#ifndef _PREPARE_H
#define _PREPARE_H

#include "../../db/db_key.h"
#include "../../db/db_val.h"
#include "../../db/db_op.h"


void prepare_where(db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n);

/*
 * prepare_select(), prepare_insert(), prepare_delete(), and prepare_update()
 * functions automatically begin new statement. Other functions continue
 * to build current statement.
 */

void prepare_select(db_key_t* _c, int _nc);
void prepare_from(const char* _t);
void prepare_order_by(db_key_t _o);

void prepare_insert(const char* _t);
void prepare_insert_columns(db_key_t* _k, int _n);
void prepare_insert_values(db_val_t* _v, int _n);

void prepare_delete(const char* _t);

void prepare_update(const char* _t);
void prepare_update_set(db_key_t* _k, db_val_t* _v, int _n);


const char *prepared_sql(void);
size_t prepared_sql_len(void);


#endif /* _PREPARE_H */

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


#ifndef _BDB_VALS_H_
#define _BDB_VALS_H_

/* table row */
int bdb_row_db2bdb(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n, bdb_row_p *_r);

void bdb_free_row(bdb_row_p _r);
void bdb_free_row_list(bdb_row_p _r);

int bdb_set_key(bdb_row_p _r, bdb_val_p _v);

void bdb_push_field(bdb_row_p _r, bdb_val_p _v);
void bdb_free_field(bdb_val_p _v);
void bdb_free_field_list(bdb_val_p _v);

void bdb_push_data(bdb_row_p _r, bdb_val_p _v);
void bdb_merge_tail(bdb_row_p _r);

int bdb_field_db2bdb(bdb_val_p v, db_val_t* _v);


/* search row */
int bdb_srow_db2bdb(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n, bdb_srow_p *_r);
void bdb_free_srow(bdb_srow_p _r);

void bdb_set_skey(bdb_srow_p _r, bdb_sval_p _v);

void bdb_push_sfield(bdb_srow_p _r, bdb_sval_p _v);
void bdb_free_sfield(bdb_sval_p _v);
void bdb_free_sfield_list(bdb_sval_p _v);

int bdb_sfield_db2bdb(bdb_sval_p v, db_val_t* _v, db_op_t _op);

/* result row */
int bdb_rrow_db2bdb(db_con_t* _h, db_key_t* _k, int _n, bdb_rrow_p *_r);
void bdb_free_rrow(bdb_rrow_p _r);

/* update row */
int bdb_urow_db2bdb(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n, bdb_urow_p *_r);
void bdb_free_urow(bdb_urow_p _r);

void bdb_push_ufield(bdb_urow_p _r, bdb_uval_p _v);
void bdb_free_ufield(bdb_uval_p _v);
void bdb_free_ufield_list(bdb_uval_p _v);

int bdb_ufield_db2bdb(bdb_uval_p v, db_val_t* _v);

int bdb_set_row(db_con_t* _h, bdb_urow_p u_r, bdb_val_p _v, bdb_row_p _r);

/* query */
int bdb_get_db_row(db_con_t* _h, DBT* _data, bdb_val_p *_v);

#endif

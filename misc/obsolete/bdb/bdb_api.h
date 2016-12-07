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


#ifndef _BDB_API_H_
#define _BDB_API_H_

int bdb_open_table(db_con_t* _h, const char* _t);
int bdb_use_table(db_con_t* _h, const char* _t);
int bdb_close_table(db_con_t* _h);

int bdb_describe_table(modparam_t type, void* val);

bdb_table_p bdb_find_table(const char* _t);
void bdb_push_table(bdb_table_p _t);
void bdb_free_table(bdb_table_p _t);
void bdb_free_table_list(bdb_table_p _t);

bdb_column_p bdb_find_column(bdb_table_p _t, const char* _c);
void bdb_push_column(bdb_table_p _t, bdb_column_p _c);
void bdb_free_column(bdb_column_p _c);
void bdb_free_column_list(bdb_column_p _c);

int bdb_query_table(db_con_t* _h, bdb_srow_p s_r, bdb_rrow_p r_r, int _n, db_res_t** _r);
int bdb_row_match(db_con_t* _h, bdb_val_p _v, bdb_srow_p s_r);
int bdb_push_res_row(db_con_t* _h, db_res_t** _r, bdb_rrow_p _r_r, int _n, bdb_val_p _v);

int bdb_update_table(db_con_t* _h, bdb_srow_p s_r, bdb_urow_p u_r);

int bdb_delete_table(db_con_t* _h, bdb_srow_p s_r);

#endif

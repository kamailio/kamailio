/* sp-ul_db module
 *
 * Copyright (C) 2007 1&1 Internet AG
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

#include "ul_db_failover_func.h"

#include "../../dprint.h"
#include "p_usrloc_mod.h"
#include "ul_db.h"

static str autocommit_off = str_init("SET AUTOCOMMIT=0");
static str fail_isolation_level = str_init("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");
static str start_transaction = str_init("START TRANSACTION");
static str commit = str_init("COMMIT");
static str rollback = str_init("ROLLBACK");
static str autocommit_on = str_init("SET AUTOCOMMIT=1");
static char query[UL_DB_QUERY_LEN];

int ul_db_failover_prepare(db_func_t * dbf, db1_con_t * dbh) {
	if(dbf->raw_query(dbh, &autocommit_off, NULL) < 0) {
		LM_ERR("could not "
				"set autocommit off!\n");
		return -2;
	}
	if(dbf->raw_query(dbh, &fail_isolation_level, NULL) < 0) {
		LM_ERR("could not "
				"set transaction isolation level!\n");
		return -2;
	}
	if(dbf->raw_query(dbh, &start_transaction, NULL) < 0) {
		LM_ERR("could not "
				"start transaction!\n");
		return -2;
	}
	return 0;
}

int ul_db_failover_commit(db_func_t * dbf, db1_con_t * dbh) {
	if(dbf->raw_query(dbh, &commit, NULL) < 0) {
		LM_ERR("transaction commit "
				"failed.\n");
		return -1;
	}
	if(dbf->raw_query(dbh, &autocommit_on, NULL) < 0) {
		LM_ERR("could not turn "
				"transaction autocommit on.\n");
		return -2;
	}
	return 0;
}

int ul_db_failover_rollback(db_func_t * dbf, db1_con_t * dbh) {
	LM_ERR("rolling back failover "
			"transaction.\n");
	if(dbf->raw_query(dbh, &rollback, NULL) < 0) {
		LM_ERR("could not rollback "
				"transaction.\n");
		return -1;
	}
	if(dbf->raw_query(dbh, &autocommit_on, NULL) < 0) {
		LM_ERR("could not set "
				"autocommit on.\n");
		return -2;
	}
	return 0;
}

int get_max_no_of_db_id(db_func_t * dbf, db1_con_t * dbh, int id){
	db1_res_t * res;
	db_row_t * row;
	int query_len, max;
	str tmp;

	query_len = 50 + reg_table.len + id_col.len + num_col.len;
	if(query_len > UL_DB_QUERY_LEN){
		LM_ERR("weird: query too long.\n");
		return -1;
	}
	memset(query, 0, UL_DB_QUERY_LEN);
	if (sprintf(query,
			"SELECT MAX(%.*s) "
			"FROM %.*s "
			"WHERE %.*s='%i'",
			num_col.len, num_col.s,
			reg_table.len, reg_table.s,
			id_col.len, id_col.s, id) < 0) {
		LM_ERR("could not print query\n");
		return -1;
	}
	tmp.s = query;
	tmp.len = strlen(query);
	
	if(dbf->raw_query(dbh, &tmp, &res) < 0){
		LM_ERR("weird: could not query %.*s.\n",
		   reg_table.len, reg_table.s);
		return -1;
	}
	if(RES_ROW_N(res) == 0){
		LM_ERR("weird: no data found for id %i\n", id);
		dbf->free_result(dbh, res);
		return -1;
	}
	row = RES_ROWS(res);
	max = VAL_INT(ROW_VALUES(row));
	dbf->free_result(dbh, res);
	return max;
}

int store_handle_data(db_func_t * dbf, db1_con_t * dbh, ul_db_t * db, int id, int old_num, int new_id){
	db_key_t cols[8];
	db_val_t vals[8];
	db_key_t keys[2];
	db_val_t key_vals[8];
	db_op_t op[2];
	
	cols[0] = &id_col;
	vals[0].type = DB1_INT;
	vals[0].nul = 0;
	vals[0].val.int_val = new_id;
	
	cols[1] = &num_col;
	vals[1].type = DB1_INT;
	vals[1].nul = 0;
	vals[1].val.int_val = db->no;
	
	cols[2] = &url_col;
	vals[2].type = DB1_STRING;
	vals[2].nul = 0;
	vals[2].val.string_val = db->url.s;
	
	cols[3] = &error_col;
	vals[3].type = DB1_INT;
	vals[3].nul = 0;
	vals[3].val.int_val = db->errors;
	
	cols[4] = &failover_time_col;
	vals[4].type = DB1_DATETIME;
	vals[4].nul = 0;
	vals[4].val.time_val = db->failover_time;
	
	cols[5] = &spare_col;
	vals[5].type = DB1_INT;
	vals[5].nul = 0;
	vals[5].val.int_val = db->spare;
	
	cols[6] = &status_col;
	vals[6].type = DB1_INT;
	vals[6].nul = 0;
	vals[6].val.int_val = db->status;
	
	cols[7] = &risk_group_col;
	vals[7].type = DB1_INT;
	vals[7].nul = 0;
	vals[7].val.int_val = db->rg;
	
	keys[0] = &id_col;
	op[0] = OP_EQ;
	key_vals[0].type = DB1_INT;
	key_vals[0].nul = 0;
	key_vals[0].val.int_val = id;
	
	keys[1] = &num_col;
	op[1] = OP_EQ;
	key_vals[1].type = DB1_INT;
	key_vals[1].nul = 0;
	key_vals[1].val.int_val = old_num;
	
	if(dbf->use_table(dbh, &reg_table) < 0){
		LM_ERR("could not use reg_table.\n");
		return -1;
	}
	if(dbf->update(dbh, keys, op, key_vals, cols, vals, 2, 7) < 0){
		LM_ERR("could insert handle data.\n");
		return -1;
	}
	return 0;
}

int check_handle_data(db_func_t * dbf, db1_con_t * dbh, ul_db_t * db, int id){
	db_key_t cols[2];
	db_key_t keys[4];
	db_val_t key_vals[4];
	db_op_t op[4];
	db1_res_t * res;
	
	cols[0] = &id_col;
	
	keys[0] = &id_col;
	op[0] = OP_EQ;
	key_vals[0].type = DB1_INT;
	key_vals[0].nul = 0;
	key_vals[0].val.int_val = id;
	
	keys[1] = &num_col;
	op[1] = OP_EQ;
	key_vals[1].type = DB1_INT;
	key_vals[1].nul = 0;
	key_vals[1].val.int_val = db->no;
	
	keys[2] = &url_col;
	op[2] = OP_EQ;
	key_vals[2].type = DB1_STRING;
	key_vals[2].nul = 0;
	key_vals[2].val.string_val = db->url.s;
	
	if(dbf->use_table(dbh, &reg_table) < 0){
		LM_ERR("could not use "
				"reg table.\n");
		return -1;
	}
	if(dbf->query(dbh, keys, op, key_vals, cols, 3, 1, NULL, &res) < 0){
		LM_ERR("could not use "
				"query table.\n");
		return -1;
	}
	if(RES_ROW_N(res) == 0){
		dbf->free_result(dbh, res);
		return 1;
	}
	dbf->free_result(dbh, res);
	return 0;
}

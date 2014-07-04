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

#include "ul_db_failover.h"
#include "ul_db_failover_func.h"
#include "ul_db_handle.h"
#include "ul_db.h"
#include "p_usrloc_mod.h"

static ul_db_handle_t spare;

static ul_db_t tmp;

static char query[UL_DB_QUERY_LEN];

static int ul_db_failover_get_spare(db_func_t * dbf, db1_con_t * dbh, ul_db_t * db);

static int ul_db_failover_switch(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no);

static int ul_db_failover_normal(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no);

int db_failover(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no) {
	if(failover_level & FAILOVER_MODE_NORMAL){
		if(ul_db_failover_normal(dbf, dbh, handle, no) < 0){
			LM_ERR("could not switch to spare, try to "
					"turn off broken db id %i, db %i.\n", 
					handle->id, no);
		} else {
			return 0;
		}
	}
	if(failover_level & (FAILOVER_MODE_NONE | FAILOVER_MODE_NORMAL)){
		if(db_failover_deactivate(dbf, dbh, handle, no) < 0){
			LM_ERR("could not deactivate "
					"id %i, db %i.\n",
				handle->id, no);
			return -1;
		}
	}
	return 0;
}

static int ul_db_failover_normal(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no){
	ul_db_t * db = NULL;
	if(ul_db_failover_prepare(dbf, dbh) < 0){
		LM_ERR("could not "
				"initiate failover transaction, rollback.\n");
		ul_db_failover_rollback(dbf, dbh);
		return -1;
	}
	if((db = get_db_by_num(handle, no)) == NULL){
		LM_ERR("could not find id %i, "
				"db %i.\n", handle->id, no);
		ul_db_failover_rollback(dbf, dbh);
		return -1;
	}
	if(ul_db_failover_get_spare(dbf, dbh, db) < 0){
		LM_ERR("no spare found. "
				"id %i, db %i.\n", handle->id, no);
		ul_db_failover_rollback(dbf, dbh);
		return -1;
	}
	if(ul_db_failover_switch(dbf, dbh, handle, no) < 0){
		LM_ERR("switch to spare on "
				"id %i, db %i.\n", handle->id, no);
		ul_db_failover_rollback(dbf, dbh);
		return -1;
	}
	if(ul_db_failover_commit(dbf, dbh) < 0){
		LM_ERR("could not "
				"commit failover transaction, rollback.\n");
		ul_db_failover_rollback(dbf, dbh);
		return -1;
	}
	return 0;
}

static int ul_db_failover_get_spare(db_func_t * dbf, db1_con_t * dbh, ul_db_t * db) {
	db1_res_t * res = NULL;
	db_row_t * row;
	int query_len;
	str tmp;

	if(!dbf || !dbh || !db) {
		LM_ERR("Null pointer as parameter.\n");
		return -1;
	}

	memset(&spare, 0, sizeof(ul_db_handle_t));
	memset(query, 0, UL_DB_QUERY_LEN);
	
	query_len = 100 
			+ id_col.len
			+ num_col.len
			+ url_col.len
			+ 2 * risk_group_col.len
			+ reg_table.len
			+ spare_col.len
			+ status_col.len;
	
	if(query_len >= UL_DB_QUERY_LEN){
		LM_ERR("weird: extremely long query.\n");
		return -1;
	}
	
	if (sprintf(query,
			"SELECT "
			"%.*s, "
			"%.*s, "
			"%.*s, "
			"%.*s "
			"FROM %.*s "
			"WHERE "
			"%.*s=1 AND "
			"%.*s!=%i AND "
			"%.*s=%i "
			"LIMIT 1 "
			"FOR UPDATE",
			id_col.len, id_col.s,
			num_col.len, num_col.s,
			url_col.len, url_col.s,
			risk_group_col.len, risk_group_col.s,
			reg_table.len, reg_table.s,
			spare_col.len, spare_col.s,
			risk_group_col.len, risk_group_col.s, db->rg,
			status_col.len, status_col.s, DB_ON) < 0) {
		LM_ERR("could not print query\n");
		return -1;
	}
	tmp.s = query;
	tmp.len = strlen(query);

	if(dbf->raw_query(dbh, &tmp, &res) < 0) {
		LM_ERR("could not query database.\n");
		return -1;
	}

	if(RES_ROW_N(res) == 0) {
		LM_ERR("no spare left.\n");
		dbf->free_result(dbh, res);
		return -1;
	}
	
	row = RES_ROWS(res);
	spare.id = VAL_INT(ROW_VALUES(row) + 0);
	spare.db[0].no = VAL_INT(ROW_VALUES(row) + 1);
	if(strlen(VAL_STRING(ROW_VALUES(row) + 2)) >= UL_DB_URL_LEN){
		LM_ERR("weird: "
				"db URL longer than %i.\n", UL_DB_URL_LEN);
		dbf->free_result(dbh, res);
		return -1;
	}
	strcpy(spare.db[0].url.s, VAL_STRING(ROW_VALUES(row) + 2));
	spare.db[0].url.len = strlen(spare.db[0].url.s);
	spare.db[0].rg = VAL_INT(ROW_VALUES(row) + 3);
		
	dbf->free_result(dbh, res);
	return 0;
}

static int ul_db_failover_switch(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no) {
	ul_db_t * db;
	int check;
	int old_num, new_num;
	
	if((db = get_db_by_num(handle, no)) == NULL){
		LM_ERR("could not find id %i, "
				"db %i.\n", handle->id, no);
		return -1;
	}
	memset(&tmp, 0, sizeof(ul_db_t));
	memmove(&tmp, db, sizeof(ul_db_t));
	memset(tmp.url.s, 0, UL_DB_URL_LEN);
	memmove(tmp.url.s, db->url.s, UL_DB_URL_LEN);
	
	check = check_handle_data(dbf, dbh, db, handle->id);
	if(check < 0){
		LM_ERR("data check failed.\n");
		return -1;
	} else if(check > 0){
		LM_ERR("failover already done.\n");
		return 0;
	}
	if((new_num = get_max_no_of_db_id(dbf, dbh, spare.id)) < 0){
		LM_ERR("getting highest num failed.\n");
		return -1;
	}
	new_num++;
	old_num = tmp.no;
	tmp.no = new_num;
	tmp.status = DB_OFF;
	tmp.failover_time = time(NULL);
	tmp.spare = 1;
	if(store_handle_data(dbf, dbh, &tmp, handle->id, old_num, spare.id) < 0){
		LM_ERR("storing data of broken db failed.\n");
		return -1;
	}
	spare.db[0].failover_time = time(NULL);
	spare.db[0].errors = 0;
	old_num = spare.db[0].no;
	spare.db[0].no = db->no;
	if(store_handle_data(dbf, dbh, &spare.db[0], spare.id, old_num, handle->id) < 0){
		LM_ERR("storing data of activated spare db failed.\n");
		return -1;
	}
	return 0;
}

int db_failover_deactivate(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no) {
	db_key_t cols[3];
	db_key_t keys[3];
	db_val_t vals[3];
	db_val_t key_vals[3];
	db_op_t op[2];

	cols[0] = &status_col;
	vals[0].type = DB1_INT;
	vals[0].nul = 0;
	vals[0].val.int_val = DB_OFF;
	
	cols[1] = &failover_time_col;
	vals[1].type  = DB1_DATETIME;
	vals[1].nul = 0;
	vals[1].val.time_val = time(NULL);
	
	keys[0] = &id_col;
	op[0] = OP_EQ;
	key_vals[0].type = DB1_INT;
	key_vals[0].nul = 0;
	key_vals[0].val.int_val = handle->id;
	
	keys[1] = &num_col;
	op[1] = OP_EQ;
	key_vals[1].type = DB1_INT;
	key_vals[1].nul = 0;
	key_vals[1].val.int_val = no;

	if(dbf->use_table(dbh, &reg_table) < 0) {
		LM_ERR("could not use reg_table.\n");
		return -1;
	}

	if(dbf->update(dbh, keys, op, key_vals, cols, vals, 2, 2) < 0) {
		LM_ERR("could not update reg_table.\n");
		return -1;
	}
	return 0;
}



int db_failover_reactivate(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int no) {
	db_key_t cols[3];
	db_key_t keys[2];
	db_val_t vals[3];
	db_val_t key_vals[2];
	db_op_t op[2];

	cols[0] = &status_col;
	vals[0].type = DB1_INT;
	vals[0].nul = 0;
	vals[0].val.int_val = DB_ON;
	
	cols[1] = &failover_time_col;
	vals[1].type  = DB1_DATETIME;
	vals[1].nul = 0;
	vals[1].val.time_val = time(NULL);
	
	cols[2] = &error_col;
	vals[2].type  = DB1_INT;
	vals[2].nul = 0;
	vals[2].val.time_val = 0;
	
	keys[0] = &id_col;
	op[0] = OP_EQ;
	key_vals[0].type = DB1_INT;
	key_vals[0].nul = 0;
	key_vals[0].val.int_val = handle->id;
	
	keys[1] = &num_col;
	op[1] = OP_EQ;
	key_vals[1].type = DB1_INT;
	key_vals[1].nul = 0;
	key_vals[1].val.int_val = no;

	if(dbf->use_table(dbh, &reg_table) < 0) {
		LM_ERR("could not use reg_table.\n");
		return -1;
	}

	if(dbf->update(dbh, keys, op, key_vals, cols, vals, 2, 3) < 0) {
		LM_ERR("could not update reg_table.\n");
		return -1;
	}
	return 0;
}

int db_failover_reset(db_func_t * dbf, db1_con_t * dbh,int id, int no){
	db_key_t cols[1];
	db_key_t keys[2];
	db_val_t vals[1];
	db_val_t key_vals[2];
	db_op_t op[2];
	
	cols[0] = &failover_time_col;
	vals[0].type  = DB1_DATETIME;
	vals[0].nul = 0;
	vals[0].val.time_val = UL_DB_ZERO_TIME;
	
	keys[0] = &id_col;
	op[0] = OP_EQ;
	key_vals[0].type = DB1_INT;
	key_vals[0].nul = 0;
	key_vals[0].val.int_val = id;
	
	keys[1] = &num_col;
	op[1] = OP_EQ;
	key_vals[1].type = DB1_INT;
	key_vals[1].nul = 0;
	key_vals[1].val.int_val = no;

	

	if(dbf->use_table(dbh, &reg_table) < 0) {
		LM_ERR("could not use reg_table.\n");
		return -1;
	}
	if(dbf->update(dbh, keys, op, key_vals, cols, vals, 2, 1) < 0) {
		LM_ERR("could not update reg_table.\n");
		return -1;
	}
	return 0;
}

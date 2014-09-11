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

#include "../../lib/srdb1/db.h"
#include "ul_db.h"
#include "p_usrloc_mod.h"
#include "ul_db_failover.h"
#include "ul_db_ins.h"
#include "ul_db_repl.h"
#include "ul_db_ins_upd.h"
#include "ul_db_upd.h"
#include "ul_db_del.h"
#include "ul_db_query.h"
#include "ul_check.h"
#include <unistd.h>

ul_db_handle_t dbh_tmp;

ul_master_db_set_t mdb;

int required_caps = DB_CAP_QUERY | DB_CAP_RAW_QUERY | DB_CAP_INSERT | DB_CAP_DELETE | DB_CAP_UPDATE | DB_CAP_INSERT_UPDATE;

static char query[UL_DB_QUERY_LEN];

typedef struct db_dbf_dbres {
	db1_res_t * res;
	db_func_t * dbf;
} db_dbf_dbres_t;

#define UL_DB_RES_LIMIT 20

db_dbf_dbres_t results[UL_DB_RES_LIMIT];

static int add_dbf(db1_res_t * res, db_func_t * dbf);

static db_func_t * get_and_remove_dbf(db1_res_t * res);

int ul_db_init(void) {
	mdb.read.url = &read_db_url;
	mdb.write.url = &write_db_url;
	
	memset(results, 0, sizeof(results));

	if(db_master_write){
		if(db_bind_mod(mdb.write.url, &mdb.write.dbf) < 0) {
			LM_ERR("could not bind api for write db.\n");
			return -1;
		}
		if(!(mdb.write.dbf.cap & required_caps)) {
			LM_ERR("db api of write db doesn't support required operation.\n");
			return -1;
		}
		LM_INFO("write db initialized");
	}
	
	if(db_bind_mod(mdb.read.url, &mdb.read.dbf) < 0) {
		LM_ERR("could not bind db api for read db.\n");
		return -1;
	}
	if(!(mdb.read.dbf.cap & required_caps)) {
		LM_ERR("db api of read db doesn't support required operation.\n");
		return -1;
	}
	LM_INFO("read db initialized");
	return 0;
}

int ul_db_child_init(void) {
	if(mdb.read.dbh){
		mdb.read.dbf.close(mdb.read.dbh);
		mdb.read.dbh = NULL;
	}
	if(mdb.write.dbh){
		mdb.write.dbf.close(mdb.write.dbh);
		mdb.write.dbh = NULL;
	}
	if((mdb.read.dbh  = mdb.read.dbf.init(mdb.read.url)) == NULL) {
		LM_ERR("could not connect to sip master db (read).\n");
		return -1;
	}
	LM_INFO("read db connection for children initialized");
	
	if(ul_db_child_locnr_init() == -1) return -1;
	
	LM_INFO("location number is %d\n", max_loc_nr);
	if(db_master_write){
		if((mdb.write.dbh  = mdb.write.dbf.init(mdb.write.url)) == NULL) {
			LM_ERR("could not connect to sip master db (write).\n");
			return -1;
		}
		LM_INFO("write db connection for children initialized");
	}
	return 0;
}

int ul_db_child_locnr_init(void) {
	if(!mdb.read.dbh){
		LM_ERR("Sip master DB connection(read) is down");
		return -1;
	}
	if(load_location_number(&mdb.read.dbf, mdb.read.dbh, &max_loc_nr) != 0){
		LM_ERR("could not load location number\n");
		return -1;
	}
	return 0;
}

void ul_db_shutdown(void) {
	destroy_handles();
	if(mdb.read.dbh){
		mdb.read.dbf.close(mdb.read.dbh);
	}
	if(mdb.write.dbh){
		mdb.write.dbf.close(mdb.write.dbh);
	}
	return;
}


int db_handle_error(ul_db_handle_t * handle, int no) {
	int query_len;
	ul_db_t * db;
	int i;
	str tmp;
	
	if(!handle){
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}
	
	if(!db_master_write){
		return 0;
	}

	query_len = 35 + reg_table.len
			+ error_col.len * 2 + id_col.len;
	
	if(query_len > UL_DB_QUERY_LEN){
		LM_ERR("query too long\n");
		return -1;
	}
	
	if((db = get_db_by_num(handle, no)) == NULL){
		LM_ERR("can't get db.\n");
		return -1;
	}
	
	if (sprintf(query, "UPDATE %.*s "
				   "SET %.*s=%.*s+1 "
				   "WHERE %.*s=%i "
				   "AND %.*s=%i",
				   reg_table.len, reg_table.s,
				   error_col.len, error_col.s, error_col.len, error_col.s, 
				   id_col.len, id_col.s, handle->id,
		   		   num_col.len, num_col.s, db->no) < 0) {
		LM_ERR("could not print the query\n");
		return -1;
	}
	tmp.s = query;
	tmp.len = strlen(query);

	if (mdb.write.dbf.raw_query (mdb.write.dbh, &tmp, NULL)) {
		LM_ERR("error in database update.\n");
		return -1;
	}
	
	for(i=0; i<DB_NUM; i++){
		if (handle->db[i].dbh && handle->db[i].dbf.close){
			handle->db[i].dbf.close(handle->db[i].dbh);
			handle->db[i].dbh = NULL;
		}
	}

	if(load_data(&mdb.read.dbf, mdb.read.dbh, &dbh_tmp, handle->id) < 0){
		LM_ERR("could not load id %i\n", handle->id);
		return -1;
	}
	refresh_handle(handle, &dbh_tmp, 0);
	LM_ERR("error on id %i, db %i, "
		    "errors occured: %i, threshold: %i\n",
		handle->id, db->no, db->errors, db_error_threshold);
	if(db->errors >= db_error_threshold) {
		LM_DBG("db_handle_error: now doing failover");
		if((db_failover(&mdb.write.dbf, mdb.write.dbh, handle, no)) < 0) {
			LM_ERR("error in doing failover.\n");
			return -1;
		}
		if(load_data(&mdb.read.dbf, mdb.read.dbh, &dbh_tmp, handle->id) < 0){
			return -1;
		}
		refresh_handle(handle, &dbh_tmp, 0);
		set_must_refresh();
	}
	return 0;
}

int db_check_policy(int pol, int ok, int working) {
#define DB_POL_N_1 0
#define DB_POL_N_HALF 1
#define DB_POL_N_ALL 2

	switch(policy) {
			case DB_POL_N_1:
			switch(pol) {
					case DB_POL_OP: if(ok >= (DB_NUM - 1)) {
						return 0;
					} else {
						return -1;
					}
					break;
					case DB_POL_QUERY: if(ok >= 1) {
						return 0;
					} else {
						return -1;
					}
					case DB_POL_MOD: if((ok == working) && (working >= (DB_NUM - 1))) {
						return 0;
					} else {
						return -1;
					}
					default: LM_ERR("wrong mode given.\n");
						return -1;
					}
			case DB_POL_N_HALF:
			switch(pol) {
					case DB_POL_OP: if(ok >= (DB_NUM / 2)) {
						return 0;
					} else {
						return -1;
					}
					break;
					case DB_POL_QUERY: if(ok >= 1) {
						return 0;
					} else {
						return -1;
					}
					case DB_POL_MOD: if((ok == working) && (working >= (DB_NUM / 2))) {
						return 0;
					} else {
						return -1;
					}
					default: LM_ERR("wrong mode given.\n");
						return -1;
					}

			case DB_POL_N_ALL:
			switch(pol) {
					case DB_POL_OP: if(ok == DB_NUM) {
						return 0;
					} else {
						return -1;
					}
					break;
					case DB_POL_QUERY: if(ok >= 1) {
						return 0;
					} else {
						return -1;
					}
					case DB_POL_MOD: if(ok == DB_NUM) {
						return 0;
					} else {
						return -1;
					}
					default: LM_ERR("wrong mode given.\n");
					return -1;
			}
			default:
				return -1;
	}
}

int ul_db_insert(str * table, str * first, str * second,
                 db_key_t* _k, db_val_t* _v, int _n) {
	ul_db_handle_t * handle;
	if(!db_write){
		LM_ERR("not allowed in read only mode, abort.\n");
		return -1;
	}
	if((handle = get_handle(&mdb.read.dbf, mdb.read.dbh, first, second))  == NULL) {
		LM_ERR("could not retrieve db handle.\n");
		return -1;
	}
	return db_insert(handle, table, _k, _v, _n);
}

int ul_db_replace(str * table, str * first, str * second,
                 db_key_t* _k, db_val_t* _v, int _n, int  _un) {
	ul_db_handle_t * handle;
	if(!db_write){
		LM_ERR("not allowed in read only mode, abort.\n");
		return -1;
	}
	if((handle = get_handle(&mdb.read.dbf, mdb.read.dbh, first, second))  == NULL) {
		LM_ERR("could not retrieve db handle.\n");
		return -1;
	}
	return db_replace(handle, table, _k, _v, _n, _un);
}

int ul_db_update(str * table, str * first, str * second,
				db_key_t* _k, db_op_t * _op, db_val_t* _v,
				db_key_t* _uk, db_val_t* _uv, int _n, int _un) {
	ul_db_handle_t * handle;
	if(!db_write){
		LM_ERR("not allowed in read only mode, abort.\n");
		return -1;
	}
	if((handle = get_handle(&mdb.read.dbf, mdb.read.dbh, first, second))  == NULL) {
		LM_ERR("could not retrieve db handle.\n");
		return -1;
	}
	return db_update(handle, table, _k, _op, _v, _uk, _uv, _n, _un);
}

int ul_db_insert_update(str * table, str * first, str * second,
                        db_key_t* _k, db_val_t* _v, int _n) {
	ul_db_handle_t * handle;
	if(!db_write){
		LM_ERR("not allowed in read only mode, abort.\n");
		return -1;
	}
	if((handle = get_handle(&mdb.read.dbf, mdb.read.dbh, first, second))  == NULL) {
		LM_ERR("could not retrieve db handle.\n");
		return -1;
	}
	return db_insert_update(handle, table, _k, _v, _n);
}

int ul_db_delete(str * table, str * first, str * second,
                 db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n) {
	ul_db_handle_t * handle;
	if(!db_write){
		LM_ERR("not allowed in read only mode, abort.\n");
		return -1;
	}
	if((handle = get_handle(&mdb.read.dbf, mdb.read.dbh, first, second))  == NULL) {
		LM_ERR("could not retrieve db handle.\n");
		return -1;
	}
	return db_delete(handle, table, _k, _o, _v, _n);
}

int ul_db_query(str * table, str * first, str * second, db1_con_t *** _r_h, 
				db_key_t* _k, db_op_t* _op,	db_val_t* _v,
                db_key_t* _c, int _n, int _nc,	db_key_t _o, db1_res_t** _r) {
	ul_db_handle_t * handle;
	db_func_t * f;
	int ret;
	if((handle = get_handle(&mdb.read.dbf, mdb.read.dbh, first, second)) == NULL) {
		LM_ERR("could not retrieve db handle.\n");
		return -1;
	}
	if((ret = db_query(handle, _r_h, &f, table, _k, _op, _v, _c, _n, _nc, _o, _r, db_master_write)) < 0){
		return ret;
	}
	add_dbf(*_r, f);
	return ret;
}

int ul_db_free_result(db1_con_t ** dbh, db1_res_t * res){
	db_func_t * f;
	if(!dbh){
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}
	if((f = get_and_remove_dbf(res)) == NULL){
		return -1;
	}
	return f->free_result(*dbh, res);
}

int db_reactivate(ul_db_handle_t * handle, int no){
	if(!db_master_write){
		LM_ERR("running in read only mode, abort.\n");
		return -1;
	}
	return db_failover_reactivate(&mdb.write.dbf, mdb.write.dbh, handle, no);
}

int db_reset_failover_time(ul_db_handle_t * handle, int no){
	if(!db_master_write){
		LM_ERR("running in read only mode, abort.\n");
		return -1;
	}
	return db_failover_reset(&mdb.write.dbf, mdb.write.dbh, handle->id, no);
}

int ul_db_check(ul_db_handle_t * handle){
	if(db_master_write){
		return check_handle(&mdb.write.dbf, mdb.write.dbh, handle);
	} else {
		LM_ERR("checking is useless in read-only mode\n");
		return 0;
	}
}

static int add_dbf(db1_res_t * res, db_func_t * dbf){
	int i=0;
	for(i=0;i<UL_DB_RES_LIMIT;i++){
		if(!results[i].res){
			results[i].res = res;
			results[i].dbf = dbf;
			return 0;
		}
	}
	LM_ERR("no free dbf tmp mem, maybe forgotten to cleanup result sets?\n");
	return -1;
}

static db_func_t * get_and_remove_dbf(db1_res_t * res){
	int i=0;
	db_func_t * f;
	for(i=0; i<UL_DB_RES_LIMIT; i++){
		if(results[i].res == res){
			f = results[i].dbf;
			memset(&results[i], 0, sizeof(db_dbf_dbres_t));
			return f;
		}
	}
	LM_ERR("weird: dbf not found\n");
	return NULL;
}

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

#include "ul_db_form_query.h"
#include "ul_db_tran.h"
#include "ul_db.h"
#include "p_usrloc_mod.h"

static int db_do_query(ul_db_op_t ul_op, db_func_t * dbf, db1_con_t * dbh, str * table, db_key_t* _k, db_op_t* _o,
                       db_val_t* _v, db_key_t* _uk, db_val_t* _uv, int _n, int _un);

int db_submit_query(ul_db_op_t ul_op, ul_db_handle_t * handle, str * table, db_key_t* _k, db_op_t* _o,
                    db_val_t* _v, db_key_t* _uk, db_val_t* _uv, int _n, int _un) {
	int i;
	int working_c[DB_NUM];
	int working_r[DB_NUM];
	int errors = 0;
	int w;

	if(!handle || !table || !table->s) {
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}

	if(db_use_transactions) {
		for(i=0; i<DB_NUM; i++) {
			working_c[i] = 0;
			working_r[i] = 0;
		}

		if(ul_db_tran_start(handle, working_r) < 0) {
			LM_ERR("error during starting transaction"
			    " on table %.*s with id %i.\n", table->len, table->s, handle->id);
			w = get_working_sum(working_r, DB_NUM);
			if(db_check_policy(DB_POL_MOD, w, handle->working) < 0) {
				ul_db_tran_rollback(handle, working_r);
				return -1;
			}
		}

		for(i=0; i<DB_NUM; i++) {
			working_c[i] = working_r[i];
			if((handle->db[i].status == DB_ON) && (working_c[i])) {
				if(db_do_query(ul_op, &handle->db[i].dbf, handle->db[i].dbh, table, _k, _o, _v, _uk, _uv, _n, _un) < 0) {
					LM_ERR("error during querying "
					    "table %.*s with id %i on db %i.\n",
					    table->len, table->s, handle->id, i);
					if(db_handle_error(handle, handle->db[i].no) < 0) {
						LM_CRIT("could not handle error on db %i, handle, %i\n",
						    handle->id, handle->db[i].no);
					}
					errors++;
					working_c[i] = 0;
				} else {
					working_r[i] = 0;
				}
			}
		}

		w = get_working_sum(working_c, DB_NUM);
		if(errors > 0) {
			ul_db_tran_rollback(handle, working_r);
			if(db_check_policy(DB_POL_MOD, w, handle->working) < 0) {
				ul_db_tran_rollback(handle, working_c);
				return -1;
			}
		}
		return ul_db_tran_commit(handle, working_c);
	} else {
		for(i=0; i<DB_NUM; i++) {
			if(handle->db[i].status == DB_ON) {
				if(db_do_query(ul_op, &handle->db[i].dbf, handle->db[i].dbh, table, _k, _o, _v, _uk, _uv, _n, _un) < 0) {
					if(db_handle_error(handle, handle->db[i].no) < 0) {
						LM_CRIT("could not handle error on db %i, handle, %i\n",
						    handle->id, handle->db[i].no);
					}
					return -1;
				}
			}
		}
		return 0;
	}
}

static int db_do_query(ul_db_op_t ul_op, db_func_t * dbf, db1_con_t * dbh, str * table, db_key_t* _k, db_op_t* _o,
                       db_val_t* _v, db_key_t* _uk, db_val_t* _uv, int _n, int _un) {
	if(dbf->use_table(dbh, table) < 0) {
		LM_ERR("error in use table %.*s.\n", table->len, table->s);
		return -1;
	}
	switch(ul_op) {
			case UL_DB_INS:
			if(dbf->insert(dbh, _k, _v, _n) < 0) {
				LM_ERR("error in inserting into "
				    "table %.*s.\n", table->len, table->s);
				return -1;
			}
			return 0;
			case UL_DB_REPL:
			if(dbf->replace(dbh, _k, _v, _n, _un, 0) < 0) {
				LM_ERR("error in replacing in "
				    "table %.*s.\n", table->len, table->s);
				return -1;
			}
			return 0;
			case UL_DB_INS_UPD:
			if(dbf->insert_update(dbh, _k, _v, _n ) < 0) {
				LM_ERR("error in inserting/updating in "
				    "table %.*s.\n", table->len, table->s);
				return -1;
			}
			return 0;
			case UL_DB_UPD:
			if(dbf->update(dbh, _k, _o, _v, _uk, _uv, _n, _un) < 0) {
				LM_ERR("error in updating "
				    "table %.*s.\n", table->len, table->s);
				return -1;
			}
			return 0;
			case UL_DB_DEL:
			if(dbf->delete(dbh, _k, _o, _v, _n) < 0) {
				LM_ERR("error in deleting from table %.*s.\n", table->len, table->s);
				return -1;
			}
			return 0;
			default: return -1;
	}
	return 0;
}

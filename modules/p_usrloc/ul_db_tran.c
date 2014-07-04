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

#include "ul_db_tran.h"
#include "ul_db.h"
#include "p_usrloc_mod.h"

static str autocommit_off = str_init("SET AUTOCOMMIT=0");
static str start_transaction = str_init("START TRANSACTION");
static str commit = str_init("COMMIT");
static str autocommit_on = str_init("SET AUTOCOMMIT=1");
static str rollback = str_init("ROLLBACK");

static int submit_tran_start(db_func_t * dbf, db1_con_t * dbh);
static int submit_tran_commit(db_func_t * dbf, db1_con_t * dbh);
static int submit_tran_rollback(db_func_t * dbf, db1_con_t * dbh);

int ul_db_tran_start(ul_db_handle_t * handle, int working[]) {
	int i;
	int errors = 0;
	int w = 0;

	if(!handle || !working) {
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}

	for(i=0; i<DB_NUM; i++) {
		if(handle->db[i].status == DB_ON) {
			if(submit_tran_start(&handle->db[i].dbf, handle->db[i].dbh) < 0) {
				LM_ERR("error while starting "
				    "transaction on id %i, db %i.\n", handle->id, handle->db[i].no);
				if(db_handle_error(handle, handle->db[i].no) < 0) {
					LM_ERR("error during handling error "
					    "on id %i on db %i, trying again.\n", handle->id, handle->db[i].no);
					errors++;
				} else {
					if(submit_tran_start(&handle->db[i].dbf, handle->db[i].dbh) < 0) {
						LM_ERR("error while starting "
						    "transaction on id %i, db %i.\n", handle->id, handle->db[i].no);
						errors++;
					}
				}

			} else {
				working[i] = 1;
				w++;
			}
		}
	}
	if((errors > 0) || (w < handle->working)) {
		return -1;
	}
	return 0;
}

int ul_db_tran_commit(ul_db_handle_t * handle, int working[]) {
	int i;
	int errors = 0;
	int w = 0;

	if(!handle || !working) {
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}

	for(i=0; i<DB_NUM; i++) {
		if((handle->db[i].status == DB_ON) && (working[i])) {
			if(submit_tran_commit(&handle->db[i].dbf, handle->db[i].dbh) < 0) {
				LM_ERR("error while committing "
				    "transaction on id %i, db %i.\n", handle->id, handle->db[i].no);
				if(db_handle_error(handle, handle->db[i].no) < 0) {
					LM_ERR("error during handling error "
					    "on id %i on db %i, trying again.\n", handle->id, handle->db[i].no);
				}
				errors++;
			} else {
				w++;
			}
		}
	}
	if((errors > 0) || (w < get_working_sum(working, DB_NUM))) {
		return -1;
	}
	return 0;
}

int ul_db_tran_rollback(ul_db_handle_t * handle, int working[]) {
	int i;
	int errors = 0;
	int w = 0;

	if(!handle || !working) {
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}

	for(i=0; i<DB_NUM; i++) {
		if((handle->db[i].status == DB_ON) && (working[i])) {
			if(submit_tran_rollback(&handle->db[i].dbf, handle->db[i].dbh) < 0) {
				LM_ERR("error while rolling back "
				    "transaction on id %i, db %i.\n", handle->id, handle->db[i].no);
				errors++;
			} else {
				w++;
			}
		}
	}
	if((errors > 0) || (w < get_working_sum(working, DB_NUM))) {
		return -1;
	}
	return 0;
}

static int submit_tran_start(db_func_t * dbf, db1_con_t * dbh) {
	int errors = 0;
	str tmp;
	if(dbh) {
		if(dbf->raw_query(dbh, &autocommit_off, NULL) < 0) {
			LM_ERR("error while turning off "
			    "autocommit.\n");
			errors++;
		}
		tmp.s  = isolation_level;
		tmp.len = strlen(isolation_level);
		if(dbf->raw_query(dbh, &tmp, NULL) < 0) {
			LM_ERR("error while setting "
			    "isolation level.\n");
			errors++;
		}
		if(dbf->raw_query(dbh, &start_transaction, NULL) < 0) {
			LM_ERR("error while starting "
			    "transaction.\n");
			errors++;
		}
	} else {
		LM_ERR("no db handle.\n");
		return -1;
	}
	if(errors > 0) {
		return -1;
	}
	return 0;
}

static int submit_tran_commit(db_func_t * dbf, db1_con_t * dbh) {
	int errors = 0;

	if(dbh) {
		if(dbf->raw_query(dbh, &commit, NULL) < 0) {
			LM_ERR("error during commit.\n");
			errors++;
		}
		if(dbf->raw_query(dbh, &autocommit_on, NULL) < 0) {
			LM_ERR("error while turning "
			    "on autocommit.\n");
			errors++;
		}
	} else {
		LM_ERR("no db handle.\n");
		return -1;
	}

	if(errors > 0) {
		return -1;
	}
	return 0;
}

static int submit_tran_rollback(db_func_t * dbf, db1_con_t * dbh) {
	int errors = 0;

	if(dbh) {
		if(dbf->raw_query(dbh, &rollback, NULL) < 0) {
			LM_ERR("error during "
			    "rollback.\n");
			errors++;
		}
		if(dbf->raw_query(dbh, &autocommit_on, NULL) < 0) {
			LM_ERR("error while "
			    "turning on autocommit.\n");
			errors++;
		}
	} else {
		LM_ERR("no db handle.\n");
		return -1;
	}

	if(errors > 0) {
		return -1;
	}
	return 0;
}

int get_working_sum(int working[], int no) {
	int i;
	int sum = 0;
	if(!working) {
		return -1;
	}
	for(i=0; i<no; i++) {
		sum += working[i];
	}
	return sum;
}

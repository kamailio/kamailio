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

#include "ul_db_query.h"
#include "ul_db.h"


static int order_dbs(ul_db_handle_t * handle, int order[]);

static int db_exec_query(db_func_t * dbf, db1_con_t * dbh, str * table,
                         db_key_t* _k, db_op_t * _op, db_val_t * _v,
                         db_key_t * _c, int _n, int _nc, db_key_t _o,
                         db1_res_t ** _r);

int db_query(ul_db_handle_t * handle, db1_con_t *** _r_h, db_func_t ** _r_f,
             str * table, db_key_t* _k, db_op_t * _op, db_val_t * _v,
             db_key_t * _c, int _n, int _nc, db_key_t _o, db1_res_t ** _r, int rw) {
	int order[DB_NUM];
	int i;
	int err[DB_NUM];
	int ret = -1;
	order_dbs(handle, order);
	memset(err, 0 , sizeof(int) * DB_NUM);

	if(!handle || !table || !table->s || !_r_h) {
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}
	i = 0;
	do {
		LM_DBG("now trying id %i, db %i.\n", handle->id, handle->db[order[i]].no);
		if(handle->db[order[i]].status == DB_ON) {
			if((ret = db_exec_query(&handle->db[order[i]].dbf, handle->db[order[i]].dbh, table, _k, _op, _v, _c, _n, _nc, _o, _r)) < 0) {
				LM_ERR("could not query table %.*s error on id %i, db %i.\n", table->len, table->s, handle->id, handle->db[order[i]].no);
				if(rw) {
					if(err[i] == 0 && handle->db[order[i]].status == DB_ON) {
						if(db_handle_error(handle, handle->db[order[i]].no) < 0) {
							LM_ERR("could not handle error on id %i, db %i.\n", handle->id, handle->db[order[i]].no);
						} else {
							err[i] = 1;
							i--;
						}
					}
				}
			}
		}
		i++;
	} while((ret < 0) && (i < DB_NUM));
	i--;
	LM_DBG("returned handle is for id %i, db %i\n", handle->id, handle->db[order[i]].no);
	*_r_h = &handle->db[order[i]].dbh;
	*_r_f = &handle->db[order[i]].dbf;
	return ret;
}

static int order_dbs(ul_db_handle_t * handle, int order[]) {
	int i,j, tmp;
	for(i=0; i<DB_NUM; i++) {
		order[i] = i;
	}
	for(i=0; i<DB_NUM; i++) {
		for(j=i+1; j<DB_NUM; j++) {
			if((handle->db[i].status == DB_OFF || handle->db[i].status == DB_INACTIVE) && (handle->db[j].status == DB_ON)) {
				tmp = order[i];
				order[i] = order[j];
				order[j] = tmp;
			} else if(handle->db[i].failover_time > handle->db[j].failover_time) {
				tmp = order[i];
				order[i] = order[j];
				order[j] = tmp;
			}
		}
	}
	return 0;
}

static int db_exec_query(db_func_t * dbf, db1_con_t * dbh, str * table,
                         db_key_t* _k, db_op_t * _op, db_val_t * _v,
                         db_key_t * _c, int _n, int _nc, db_key_t _o,
                         db1_res_t ** _r) {
	if(!dbf || !dbh || !table || !table->s) {
		LM_ERR("NULL pointer in parameter.\n");
		return -1;
	}
	if(dbf->use_table(dbh, table) < 0) {
		LM_ERR("could not use table %.*s.\n", table->len, table->s);
		return -1;
	}
	if(dbf->query(dbh, _k, _op, _v, _c, _n, _nc, _o, _r) < 0) {
		LM_ERR("could not query table %.*s.\n", table->len, table->s);
		return -1;
	}
	return 0;
}

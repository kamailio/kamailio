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

#include "ul_db_handle.h"
#include "p_usrloc_mod.h"
#include "ul_db.h"
#include "ul_db_watch.h"
#include "../../crc.h"

static ul_db_handle_list_t * db_handles = NULL;

static ul_db_handle_t tmp;

static ul_db_handle_t * allocate_handle(void);

static ul_db_handle_list_t * allocate_handle_list(void);

static void free_handle(ul_db_handle_list_t * element);

static int check_status(ul_db_handle_t * a, ul_db_handle_t * b);

static int compute_id(str* first, str* second);


static int release_handle(ul_db_handle_t * handle);

static int activate_handle(ul_db_handle_t * handle);

ul_db_handle_t * get_handle(db_func_t * dbf, db1_con_t * dbh, str * first, str * second) {
	struct ul_db_handle_list * element;
	ul_db_handle_t * ret = NULL;
	int db_ok = 0;
	int id;

	switch(alg_location){
	case 1:default:{
		/* atm this is the only matching mode */
		if( max_loc_nr == 0){
			LM_WARN("max_loc_nr is 0 . Try to recompute value\n");
			if( load_location_number(dbf, dbh, &max_loc_nr) != 0 ){
				LM_ERR("Could not get location number\n");
				return NULL;
			}
		} 
		if((id = compute_id(first, second)) < 0){
			return NULL;
		}
		break;
	}
/*	default:{
		LM_ERR("No suitable selection for location\n");
		return NULL;
	}
*/
	}
	
	if(load_data(dbf, dbh, &tmp, id) < 0){
		// load_data should explain this
		return NULL;
	}

	element  = db_handles;
	db_ok = 0;
	while(element && element->handle) {
		if(element->handle->id == tmp.id) {
			LM_DBG("found handle with id %i\n", element->handle->id);
			element->handle->expires = time(NULL) + connection_expires;
			if(check_status(element->handle, &tmp) == 0) {
				db_ok = 1;
			}
			ret = element->handle;
		}
		if((element->handle->expires < time(NULL)) && element->handle->active){
			release_handle(element->handle);
		}
		element = element->next;
	}
	if(db_ok) {
		goto ret;
	}

	element = NULL;

	if(ret == NULL) {
		LM_DBG("didn't find handle with id %i\n", tmp.id);
		if((element = allocate_handle_list()) == NULL) {
			LM_ERR("could not allocate handle.\n");
			return NULL;
		}
		ret = element->handle;
		ret->id = tmp.id;
		activate_handle(ret);
		element->next = db_handles;
		db_handles = element;
	}
	if(refresh_handle(ret, &tmp, db_write) < 0) {
		ret = NULL;
	}
ret:
	if(ret && !ret->active){
		activate_handle(ret);
	}
	return ret;
}

int refresh_handle(ul_db_handle_t * handle, ul_db_handle_t * new_data, int error_handling) {
	int db_ok = 0;
	int i, ret;
	str tmpurl;
	handle->id = new_data->id;
	
	handle->working = 0;

	handle->expires = time(NULL) + connection_expires;

	for(i=0; i<DB_NUM; i++) {
		handle->db[i].status = new_data->db[i].status;
		handle->db[i].errors = new_data->db[i].errors;
		handle->db[i].failover_time = new_data->db[i].failover_time;
		handle->db[i].rg = new_data->db[i].rg;
		handle->db[i].no = new_data->db[i].no;
		
		if((handle->db[i].url.len != new_data->db[i].url.len) || (strcmp(handle->db[i].url.s, new_data->db[i].url.s) != 0)) {
			memset(handle->db[i].url.s, 0, UL_DB_URL_LEN);
			strcpy(handle->db[i].url.s, new_data->db[i].url.s);
			handle->db[i].url.len = new_data->db[i].url.len;
			if(handle->db[i].dbh) {
				handle->db[i].dbf.close(handle->db[i].dbh);
				handle->db[i].dbh = NULL;
			}
			memset(&handle->db[i].dbf, 0, sizeof(db_func_t));
			tmpurl.len = handle->db[i].url.len;
			tmpurl.s = handle->db[i].url.s;
			if(db_bind_mod(&tmpurl, &handle->db[i].dbf) < 0){
				LM_ERR("could not bind db module.\n");
				return -1;
			}
		}
		if(handle->db[i].status == DB_ON) {
			handle->working++;
			if(handle->db[i].dbh) {
				db_ok++;
			} else {
				LM_DBG("connect id %i db %i.\n", handle->id, handle->db[i].no);
				tmpurl.len = handle->db[i].url.len;
				tmpurl.s = handle->db[i].url.s;
				if((handle->db[i].dbh = handle->db[i].dbf.init(&tmpurl)) == NULL) {
					LM_ERR("id: %i could not "
					    "connect database %i.\n", handle->id, handle->db[i].no);
					if(error_handling){
						if(db_handle_error(handle, handle->db[i].no) < 0){
							LM_ERR("id: %i could not "
								"handle error on database %i.\n", handle->id, handle->db[i].no);
						}
					}
				} else {
					db_ok++;
				}
			}
		} else if (handle->db[i].status == DB_INACTIVE) {
			if(handle->db[i].dbh){
				LM_DBG("deactivate id %i db %i.\n", handle->id, handle->db[i].no);
				handle->db[i].dbf.close(handle->db[i].dbh);
				handle->db[i].dbh = NULL;
			}
		} else {
			if(handle->db[i].dbh) {
				LM_DBG("shutdown id %i db %i.\n", handle->id, handle->db[i].no);
				handle->db[i].dbf.close(handle->db[i].dbh);
				handle->db[i].dbh = NULL;
			}
		}
	}
	if((ret = db_check_policy(DB_POL_OP, db_ok, handle->working)) < 0){
		LM_ERR("id %i: too few dbs working\n", handle->id);
	}
	return ret;
}

static int release_handle(ul_db_handle_t * handle){
	int i;
	LM_NOTICE("deactivating handle id %i, db 1:  %.*s, db2:  %.*s\n", handle->id, handle->db[0].url.len, handle->db[0].url.s, handle->db[1].url.len, handle->db[1].url.s);
	for(i=0; i<DB_NUM; i++){
		if(handle->db[i].dbh){
			handle->db[i].dbf.close(handle->db[i].dbh);
			handle->db[i].dbh = NULL;
		}
	}
	handle->active = 0;
	return ul_unregister_watch_db(handle->id);
}

static int activate_handle(ul_db_handle_t * handle){
	LM_NOTICE("activating handle id %i, db 1: %.*s, db2: %.*s\n", handle->id, handle->db[0].url.len, handle->db[0].url.s, handle->db[1].url.len, handle->db[1].url.s);
	handle->active = 1;
	return ul_register_watch_db(handle->id);
}

static ul_db_handle_list_t * allocate_handle_list(void) {
	ul_db_handle_list_t * ret;
	if((ret = (ul_db_handle_list_t *)pkg_malloc(sizeof(ul_db_handle_list_t))) == NULL) {
		LM_ERR("couldn't allocate private memory.\n");
		return NULL;
	}
	if((ret->handle = allocate_handle()) == NULL) {
		pkg_free(ret);
		return NULL;
	}
	return ret;
}

static ul_db_handle_t * allocate_handle(void) {
	ul_db_handle_t * ret;
	if((ret = (ul_db_handle_t *)pkg_malloc(sizeof(ul_db_handle_t))) == NULL) {
		LM_ERR("couldn't allocate pkg mem.\n");
		return NULL;
	}
	memset(ret, 0, sizeof(ul_db_handle_t));
	if((ret->check = get_new_element()) == NULL) {
		pkg_free(ret);
		return NULL;
	}
	return ret;
}
int load_location_number(db_func_t * dbf, db1_con_t* dbh, int *loc_nr){
	static char query[UL_DB_QUERY_LEN];
	db1_res_t * res;
	db_row_t * row;
	int query_len;
	str tmp;

	if(!loc_nr || !dbf || !dbh){
		LM_ERR("NULL parameter passed \n");
		return -1;
	}

	query_len = 30 + id_col.len + reg_table.len + status_col.len;
	if(query_len > UL_DB_QUERY_LEN) {
		LM_ERR("weird: query larger than %i bytes.\n", UL_DB_QUERY_LEN);
		return -1;
	}
	
	memset(query, 0, UL_DB_QUERY_LEN);
	
	if(sprintf(query,
			"SELECT MAX(%.*s) "
			"FROM "
			"%.*s "
			"WHERE %.*s = 1;", id_col.len, id_col.s, reg_table.len, reg_table.s, status_col.len, status_col.s) < 0){
			LM_ERR("could not sprinf query\n");
			return -1;
	}
	LM_DBG("%s\n",query);
	
	tmp.s = query;
	tmp.len = strlen(query);

	if (dbf->raw_query (dbh, &tmp, &res) < 0) {
		LM_ERR("in database query.\n");
		return -1;
	}

	if (RES_ROW_N (res) == 0) {
		dbf->free_result (dbh, res);
		LM_DBG ("no data found\n");
		return 1;
	}

	row = RES_ROWS(res) + 0; /* only one row in answer */
	
	if (VAL_NULL (ROW_VALUES(row) + 0)) {
		LM_ERR("Weird: Empty Max ID Number\n");
		dbf->free_result (dbh, res);
		return 1;
	}

	*loc_nr = VAL_INT (ROW_VALUES(row) + 0);
	dbf->free_result (dbh, res);
	if(*loc_nr == 0){
		LM_ERR("No location in DB?!\n");
		return 1;
	}
	return 0;
}

int load_handles(db_func_t * dbf, db1_con_t * dbh) {
	static char query[UL_DB_QUERY_LEN];
	db1_res_t * res;
	db_row_t * row;
	ul_db_handle_list_t * element;
	int i, id, query_len;
	str tmp;

	if(!dbf || !dbh){
		LM_ERR("NULL parameter passed \n");
		return -1;
	}

	query_len = 25 + id_col.len + reg_table.len;

	if(query_len > UL_DB_QUERY_LEN) {
		LM_ERR("weird: query larger than %i bytes.\n", UL_DB_QUERY_LEN);
		return -1;
	}

	memset(query, 0, UL_DB_QUERY_LEN);

	if (sprintf(query,
	        "SELECT DISTINCT "
	        "%.*s "
	        "FROM "
	        "%.*s",
	        id_col.len, id_col.s,
	        reg_table.len, reg_table.s) < 0) {
		LM_ERR("could not print query\n");
		return -1;
	}
	tmp.s = query;
	tmp.len = strlen(query);

	if (dbf->raw_query (dbh, &tmp, &res) < 0) {
		LM_ERR("in database query.\n");
		return -1;
	}

	if (RES_ROW_N (res) == 0) {
		dbf->free_result (dbh, res);
		LM_DBG ("no data found\n");
		return 1;
	}

	for (i = 0; i < RES_ROW_N (res); ++i) {
		row = RES_ROWS (res) + i;

		if((element = allocate_handle_list()) == NULL) {
			LM_ERR("couldnt allocate handle.\n");
			goto errout;
		}

		if (VAL_NULL (ROW_VALUES(row) + 0)) {
			LM_ERR("Weird: Empty ID-Field\n");
			goto errout;
		}

		id = VAL_INT (ROW_VALUES(row) + 0);
		if(load_data(dbf, dbh, element->handle, id) < 0){
			LM_ERR("couldn't load handle data.\n");
			goto errout;
		}
		element->next = db_handles;
		db_handles = element;
	}
	dbf->free_result (dbh, res);
	return 0;
errout:
	dbf->free_result (dbh, res);
	return -1;
}

int refresh_handles(db_func_t * dbf, db1_con_t * dbh) {
	ul_db_handle_list_t * element;
	int i;
	element = db_handles;
	while(element) {
		for(i=0; i<DB_NUM; i++) {
			if(element->handle->db[i].dbh) {
				dbf->close(element->handle->db[i].dbh);
				element->handle->db[i].dbh = NULL;
			}
		}
		if(load_data(dbf, dbh, &tmp, element->handle->id) < 0){
			LM_ERR("couldn't load handle data.\n");
			return -1;
		}
		if(refresh_handle(element->handle, &tmp, db_write) < 0) {
			LM_ERR("couldn't refresh handle data.\n");
			return -1;
		}
		element = element->next;
	}
	return 1;
}

void destroy_handles(void){
	ul_db_handle_list_t * element, * del;
	int i;
	element = db_handles;
	while(element){
		for(i=0; i<DB_NUM; i++){
			if(element->handle->db[i].dbh){
				element->handle->db[i].dbf.close(element->handle->db[i].dbh);
				element->handle->db[i].dbh = NULL;
			}
		}
		del = element;
		element = element->next;
		free_handle(del);
	}
}

ul_db_t * get_db_by_num(ul_db_handle_t * handle, int no) {
	int i;
	for(i=0; i<DB_NUM; i++) {
		if(handle->db[i].no == no) {
			return &handle->db[i];
		}
	}
	return NULL;
}

int check_handle(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle){
	int i;
	str tmpurl;
	LM_INFO("checking id %i\n", handle->id);
	if(load_data(dbf, dbh, &tmp, handle->id) < 0){
		return -1;
	}
	refresh_handle(handle, &tmp, 1);
	for(i=0; i<DB_NUM; i++) {
		if(handle->db[i].url.len > 0){
			LM_INFO("checking id %i no %i, url %.*s, status %s\n", handle->id, handle->db[i].no, 
				handle->db[i].url.len, handle->db[i].url.s, (handle->db[i].status == DB_ON ? "ON" : (handle->db[i].status == DB_OFF ? "OFF" : "DEACTIVATED")));
			if(handle->db[i].status == DB_OFF) {
				tmpurl.len = handle->db[i].url.len;
				tmpurl.s = handle->db[i].url.s;
				if((handle->db[i].dbh = handle->db[i].dbf.init(&tmpurl)) != NULL){
					if(db_reactivate(handle, handle->db[i].no) < 0) {
						LM_ERR("could not reactivate id %i, db %i.\n",
							handle->id, handle->db[i].no);
						handle->db[i].dbf.close(handle->db[i].dbh);
						handle->db[i].dbh = NULL;
					} else {
						handle->db[i].status = DB_ON;
						set_must_reconnect();
					}
				} else {
					LM_NOTICE("%s: db id %i, no %i url %.*s is still down\n", __FUNCTION__, 
						handle->id, handle->db[i].no, handle->db[i].url.len, handle->db[i].url.s);
				}
			} else if((handle->db[i].status == DB_ON) && handle->db[i].dbh) {
				if((handle->db[i].failover_time < (time(NULL) - expire_time)) && (handle->db[i].failover_time != UL_DB_ZERO_TIME)){
					LM_ERR("%s: failover_time: %ld, now: %ld, delta: %ld, now going to reset failover time\n", __FUNCTION__, 
						(long int)handle->db[i].failover_time, (long int)time(NULL), (long int)(time(NULL) - handle->db[i].failover_time));
					if(db_reset_failover_time(handle, handle->db[i].no) < 0) {
						LM_ERR("could not reset failover time for id %i, db %i.\n",
							handle->id, handle->db[i].no);
					}
				}
			}
		} else {
			LM_ERR("id %i, no url to check\n", handle->id);
		}
	}	
	return 1;
}

static void free_handle(ul_db_handle_list_t * element) {
	if(element){
		if(element->handle){
			pkg_free(element->handle);
		}
		pkg_free(element);
	}
	return;
}

static int check_status(ul_db_handle_t * a, ul_db_handle_t * b){
	int i;
	if(!a->active){
		LM_NOTICE("id %i is inactive\n", a->id);
		return -1;
	}
	if(must_refresh(a->check)){
		LM_NOTICE("id %i must be refreshed\n", a->id);
		return -1;
	}
	if(must_reconnect(a->check)){
		LM_NOTICE("id %i must be reconnected\n", a->id);
		return -1;
	}
	for(i=0; i<DB_NUM; i++){
		if(strcmp(a->db[i].url.s, b->db[i].url.s) != 0){
			LM_NOTICE("id %i, db %s has different url\n", a->id, a->db[i].url.s);
			return -1;
		}
		if(a->db[i].status != b->db[i].status){
			LM_NOTICE("id %i, db %s has different status\n", a->id, a->db[i].url.s);
			return -1;
		}
		if(a->db[i].no != b->db[i].no){
			LM_NOTICE("id %i, db %s has different no\n", a->id, a->db[i].url.s);
			return -1;
		}
		if((a->db[i].status == DB_ON) && (!a->db[i].dbh)){
			LM_NOTICE("id %i, db %s has inconsistent status (1)\n", a->id, a->db[i].url.s);
			return -1;
		}
		if((a->db[i].status == DB_OFF) && (a->db[i].dbh)){	
			LM_NOTICE("id %i, db %s has inconsistent status (2)\n", a->id, a->db[i].url.s);
			return -1;
		}
	}
	return 0;
}

static int compute_id(str* first, str* second){
	unsigned int crc32_val;
#define BUF_MAXSIZE  1024
	char aux[BUF_MAXSIZE];
	str tmp;
	if(!first){
		LM_ERR("Null first parameter received\n");
		return -1;
	}
	
	if(use_domain){
		//compute crc32(user@domain)
		LM_DBG("XDBGX: compute_id HAS second key : %.*s", first->len, first->s);
		if(!second){
			LM_ERR("Null second parameter received and use_domain set to true\n");
			return -1;
		}

		tmp.len = first->len + second->len + 1;
		if( tmp.len > BUF_MAXSIZE - 1 ){
			LM_ERR("Very long user or domain\n");
			return -1;
		}
		memcpy(aux, first->s, first->len);
		aux[first->len] = '@';
		memcpy(aux + first->len + 1, second->s, second->len);
		tmp.s = aux;

		crc32_uint(&tmp, &crc32_val);
		return crc32_val % max_loc_nr + 1;
	}else{
		crc32_uint(first, &crc32_val);
		LM_DBG("crc32 for %.*s is %u\n", first->len, first->s, crc32_val);
		return crc32_val % max_loc_nr + 1;
	}
}


int load_data(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int id){
	db1_res_t *res;
	db_row_t *row;
	db_key_t cols[7];
	db_key_t keys[2];
	db_val_t key_vals[2];
	db_op_t op[2];
	db_key_t order;
	int i, ret = 0;

	if(!dbf || !dbh || !handle){
		LM_ERR("NULL-Pointer in Parameter\n");
		return -1;
	}

	memset(handle, 0, sizeof(ul_db_handle_t));
	
	cols[0] = &num_col;
	cols[1] = &url_col;
	cols[2] = &status_col;
	cols[3] = &failover_time_col;
	cols[4] = &spare_col;
	cols[5] = &error_col;
	cols[6] = &risk_group_col;
	
	order = &num_col;

	keys[0] = &id_col;
	op[0] = OP_EQ;
	key_vals[0].type = DB1_INT;
	key_vals[0].nul = 0;
	key_vals[0].val.int_val = id;
	
	if(dbf->use_table(dbh, &reg_table) < 0){
		LM_ERR("could't use table.\n");
		return -1;
	}
	if(dbf->query(dbh, keys, op, key_vals, cols, 1, 7, order, &res) < 0){
		LM_ERR("error while doing db query.\n");
		return -1;
	}
	if(RES_ROW_N(res) < DB_NUM) {
		LM_ERR("keys have too few location databases\n");
		ret = -1;
		goto ret;
	}

	handle->id = id;
	
	for(i=0; i<DB_NUM; i++) {
		row = RES_ROWS(res) + i;
		handle->db[i].no = (int)VAL_INT(ROW_VALUES(row));
		if(VAL_NULL(ROW_VALUES(row) + 1)){
			LM_ERR("Weird: Empty database URL\n");
			ret = -1;
			goto ret;
		}
		if(strlen((char *)VAL_STRING(ROW_VALUES(row) + 1)) >= (UL_DB_URL_LEN - 1)){
			LM_ERR("weird: very large URL (%d Bytes)\n",
					(int)(strlen((char *)VAL_STRING(ROW_VALUES(row) + 1)) + 1));
			ret = -1;
			goto ret;
		}
		strcpy(handle->db[i].url.s, (char *)VAL_STRING(ROW_VALUES(row) + 1));
		handle->db[i].url.len = strlen(handle->db[i].url.s);
		handle->db[i].status = (int)VAL_INT(ROW_VALUES(row) + 2);
		handle->db[i].failover_time = VAL_TIME (ROW_VALUES(row) + 3);
		handle->db[i].spare = VAL_INT (ROW_VALUES(row) + 4);
		handle->db[i].errors = VAL_INT (ROW_VALUES(row) + 5);
		handle->db[i].rg = VAL_INT (ROW_VALUES(row) + 6);
	}
ret:
	dbf->free_result(dbh, res);
	return ret;
}

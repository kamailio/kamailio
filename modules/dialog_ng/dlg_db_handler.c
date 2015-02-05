/*
 * $Id$
 *
 * Copyright (C) 2007 Voice System SRL
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2007-05-10  initial version (ancuta)
 * 2007-07-06 additional information saved in the database: cseq, contact, 
 *  		   route set and socket_info for both caller and callee (ancuta)
 */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../lib/srdb1/db.h"
#include "../../str.h"
#include "../../socket_info.h"
#include "../../lib/srutils/srjson.h"
#include "../../lib/kcore/statistics.h"

#include "dlg_hash.h"
#include "dlg_var.h"
#include "dlg_profile.h"
#include "dlg_db_handler.h"
#include "dlg_ng_stats.h"

str id_column			= str_init(ID_COL);		// 0
str h_entry_column		= str_init(HASH_ENTRY_COL);	// 1
str h_id_column			= str_init(HASH_ID_COL);	// 2
str did_column			= str_init(DID_COL);		// 3
str call_id			= str_init(CALL_ID_COL);	// 4
str from_uri_column		= str_init(FROM_URI_COL);	// 5
str from_tag_column		= str_init(FROM_TAG_COL);	// 6
str caller_original_cseq_column	= str_init(CALLER_ORIGINAL_CSEQ_COL); // 7
str req_uri_column		= str_init(REQ_URI_COL);	// 8
str caller_route_set_column	= str_init(CALLER_ROUTESET_COL);// 9
str caller_contact_column	= str_init(CALLER_CONTACT_COL);	// 10
str caller_sock_column		= str_init(CALLER_SOCK);	// 11
str timeout_column		= str_init(TIMEOUT_COL);	// 12
str state_column		= str_init(STATE_COL);		// 13
str start_time_column		= str_init(START_TIME_COL);	// 14
str sflags_column		= str_init(SFLAGS_COL);		// 15
str to_route_name_column	= str_init(TOROUTE_NAME_COL);	// 16
str to_route_index_column	= str_init(TOROUTE_INDEX_COL);	// 17

// dialog_out exclusive columns
str to_uri_column		= str_init(TO_URI_COL);
str to_tag_column		= str_init(TO_TAG_COL);
str caller_cseq_column		= str_init(CALLER_CSEQ_COL);
str callee_cseq_column		= str_init(CALLEE_CSEQ_COL);
str callee_contact_column	= str_init(CALLEE_CONTACT_COL);
str callee_routeset_column	= str_init(CALLEE_ROUTESET_COL);
str callee_sock_column		= str_init(CALLEE_SOCK);

typedef enum dialog_in_field_idx {
	DLGI_ID_COL_IDX	= 0,
	DLGI_HASH_ENTRY_COL_IDX,
	DLGI_HASH_ID_COL_IDX,
	DLGI_DID_COL_IDX,
	DLGI_CALLID_COL_IDX,
	DLGI_FROM_URI_COL_IDX,
	DLGI_FROM_TAG_COL_IDX,
	DLGI_CALLER_CSEQ_COL_IDX,
	DLGI_REQ_URI_COL_IDX,
	DLGI_CALLER_ROUTESET_COL_IDX,
	DLGI_CALLER_CONTACT_COL_IDX,
	DLGI_CALLER_SOCK_IDX,
	DLGI_TIMEOUT_COL_IDX,
	DLGI_STATE_COL_IDX,
	DLGI_START_TIME_COL_IDX,
	DLGI_SFLAGS_COL_IDX,
	DLGI_TOROUTE_NAME_COL_IDX,
	DLGI_TOROUTE_INDEX_COL_IDX
} dialog_in_field_idx_t;

typedef enum dialog_out_field_idx {
	DLGO_ID_COL_IDX	= 0,
	DLGO_HASH_ENTRY_COL_IDX,
	DLGO_HASH_ID_COL_IDX,
	DLGO_DID_COL_IDX,
	DLGO_TO_URI_IDX,
	DLGO_TO_TAG_IDX,
	DLGO_CALLER_CSEQ_IDX,
	DLGO_CALLEE_CSEQ_IDX,
	DLGO_CALLEE_CONTACT_IDX,
	DLGO_CALLEE_ROUTESET_IDX,
	DLGO_CALLEE_SOCK_IDX,

} dialog_out_field_idx_t;

str dialog_in_table_name	=	str_init(DIALOG_IN_TABLE_NAME);
str dialog_out_table_name	=	str_init(DIALOG_OUT_TABLE_NAME);

int dlg_db_mode			=	DB_MODE_NONE;

str vars_h_id_column		=	str_init(VARS_HASH_ID_COL);
str vars_h_entry_column		=	str_init(VARS_HASH_ENTRY_COL);
str vars_key_column		=	str_init(VARS_KEY_COL);
str vars_value_column		=	str_init(VARS_VALUE_COL);
str dialog_vars_table_name	=	str_init(DIALOG_VARS_TABLE_NAME);

static db1_con_t* dialog_db_handle    = 0; /* database connection handle */
static db_func_t dialog_dbf;

extern int dlg_enable_stats;

extern struct dialog_ng_counters_h dialog_ng_cnts_h;

#define GET_FIELD_IDX(_val, _idx)\
		(_val + _idx)

#define SET_STR_VALUE(_val, _str)\
	do{\
			VAL_STR((_val)).s 		= (_str).s;\
			VAL_STR((_val)).len 	= (_str).len;\
	}while(0);

#define SET_NULL_FLAG(_vals, _i, _max, _flag)\
	do{\
		for((_i) = 0;(_i)<(_max); (_i)++)\
			VAL_NULL((_vals)+(_i)) = (_flag);\
	}while(0);

#define SET_PROPER_NULL_FLAG(_str, _vals, _index)\
	do{\
		if( (_str).len == 0)\
			VAL_NULL( (_vals)+(_index) ) = 1;\
		else\
			VAL_NULL( (_vals)+(_index) ) = 0;\
	}while(0);

#define GET_STR_VALUE(_res, _values, _index, _not_null, _unref)\
	do{\
		if (VAL_NULL((_values)+ (_index))) { \
			if (_not_null) {\
				if (_unref) unref_dlg(dlg,1);\
				goto next_dialog; \
			} else { \
				(_res).s = 0; \
				(_res).len = 0; \
			}\
		} else { \
			(_res).s = VAL_STR((_values)+ (_index)).s;\
			(_res).len = strlen(VAL_STR((_values)+ (_index)).s);\
		} \
	}while(0);

static int select_dialog_out_by_did(str *did, db1_res_t ** res, int fetch_num_rows);
static int load_dialog_info_from_db(int dlg_hash_size, int fetch_num_rows);
static int load_dialog_vars_from_db(int fetch_num_rows);

int dlg_connect_db(const str *db_url)
{
	if (dialog_db_handle) {
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}
	if ((dialog_db_handle = dialog_dbf.init(db_url)) == 0)
		return -1;
	return 0;
}


int init_dlg_db(const str *db_url, int dlg_hash_size , int db_update_period, int fetch_num_rows)
{
	/* Find a database module */
	if (db_bind_mod(db_url, &dialog_dbf) < 0){
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if (dlg_connect_db(db_url)!=0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	if(db_check_table_version(&dialog_dbf, dialog_db_handle, &dialog_in_table_name, DLG_TABLE_VERSION) < 0) {
		LM_ERR("error during dialog-table version check.\n");
		return -1;
	}

	if(db_check_table_version(&dialog_dbf, dialog_db_handle, &dialog_vars_table_name, DLG_VARS_TABLE_VERSION) < 0) {
		LM_ERR("error during dialog-vars version check.\n");
		return -1;
	}

	if( (dlg_db_mode==DB_MODE_DELAYED) && 
	(register_timer( dialog_update_db, 0, db_update_period)<0 )) {
		LM_ERR("failed to register update db\n");
		return -1;
	}

	if( (load_dialog_info_from_db(dlg_hash_size, fetch_num_rows) ) !=0 ){
		LM_ERR("unable to load the dialog data\n");
		return -1;
	}
	if( (load_dialog_vars_from_db(fetch_num_rows) ) !=0 ){
		LM_ERR("unable to load the dialog data\n");
		return -1;
	}

	dialog_dbf.close(dialog_db_handle);
	dialog_db_handle = 0;

	return 0;
}

void destroy_dlg_db(void)
{
	/* close the DB connection */
	if (dialog_db_handle) {
		dialog_dbf.close(dialog_db_handle);
		dialog_db_handle = 0;
	}
}

static int use_dialog_out_table(void)
{
	if(!dialog_db_handle){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (dialog_dbf.use_table(dialog_db_handle, &dialog_out_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

static int use_dialog_table(void)
{
	if(!dialog_db_handle){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (dialog_dbf.use_table(dialog_db_handle, &dialog_in_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

static int use_dialog_vars_table(void)
{
	if(!dialog_db_handle){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (dialog_dbf.use_table(dialog_db_handle, &dialog_vars_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

static int select_dialog_out_by_did(str *did, db1_res_t ** res, int fetch_num_rows)
{
	db_key_t query_cols[DIALOG_IN_TABLE_COL_NO] = {
							&id_column, 			&h_entry_column,
							&h_id_column, 			&did_column,
							&to_uri_column, 		&to_tag_column,
							&caller_cseq_column, 	&callee_cseq_column,
							&callee_contact_column,	&callee_routeset_column,
							&callee_sock_column };

	db_key_t where[1] = {
							&did_column
						};
	db_val_t values[1];

	if(use_dialog_out_table() != 0) {
		return -1;
	}

	VAL_TYPE(values) = DB1_STR;
	VAL_NULL(values) = 0;

	SET_STR_VALUE(values, (*did));

	if (DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(dialog_dbf.query(dialog_db_handle, where, 0, values, query_cols, 1, DIALOG_OUT_TABLE_COL_NO, 0, 0) < 0) {
			LM_ERR("Error while querying (fetch) database\n");
			return -1;
		}
		if(dialog_dbf.fetch_result(dialog_db_handle, res, fetch_num_rows) < 0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	}
	else {
		if(dialog_dbf.query(dialog_db_handle, where, 0, values, query_cols, 1, DIALOG_OUT_TABLE_COL_NO, 0, res) < 0) {
			LM_ERR("Error while querying database\n");
			return -1;
		}
	}

	return 0;
}

static int select_entire_dialog_in_table(db1_res_t ** res, int fetch_num_rows)
{
	db_key_t query_cols[DIALOG_IN_TABLE_COL_NO] = {
						&id_column, 			&h_entry_column,
						&h_id_column, 			&did_column,
						&call_id, 				&from_uri_column,
						&from_tag_column, 		&caller_original_cseq_column,
						&req_uri_column,		&caller_route_set_column,
						&caller_contact_column, &caller_sock_column, &timeout_column,
						&state_column, 			&start_time_column,
						&sflags_column,
						&to_route_name_column, 	&to_route_index_column };

	if(use_dialog_table() != 0) {
		return -1;
	}

	/* select the whole table and all the columns */
	if (DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(dialog_dbf.query(dialog_db_handle,0,0,0,query_cols, 0, 
		DIALOG_IN_TABLE_COL_NO, 0, 0) < 0) {
			LM_ERR("Error while querying (fetch) database\n");
			return -1;
		}
		if(dialog_dbf.fetch_result(dialog_db_handle, res, fetch_num_rows) < 0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if(dialog_dbf.query(dialog_db_handle,0,0,0,query_cols, 0,
		DIALOG_IN_TABLE_COL_NO, 0, res) < 0) {
			LM_ERR("Error while querying database\n");
			return -1;
		}
	}

	return 0;
}

struct socket_info * create_socket_info(db_val_t * vals, int n){

	struct socket_info * sock;
	char* p;
	str host;
	int port, proto;

	/* socket name */
	p = (VAL_STR(vals+n)).s;

	if (VAL_NULL(vals+n) || p==0 || p[0]==0){
		sock = 0;
	} else {
		if (parse_phostport( p, &host.s, &host.len, 
		&port, &proto)!=0) {
			LM_ERR("bad socket <%s>\n", p);
			return 0;
		}
		sock = grep_sock_info( &host, (unsigned short)port, proto);
		if (sock==0) {
			LM_WARN("non-local socket <%s>...ignoring\n", p);
		}
	}

	return sock;
}

static int load_dialog_out_from_db(struct dlg_cell *dlg, str *did, int fetch_num_rows)
{
	db1_res_t * res = NULL;
	db_val_t * values;
	db_row_t * rows;
	int i, nr_rows;
	str to_uri, to_tag, /*caller_cseq,*/
		callee_cseq, callee_contact,
		callee_route_set;

	struct dlg_cell_out *dlg_out;
	if((nr_rows = select_dialog_out_by_did(did, &res, fetch_num_rows)) < 0) {
		LM_WARN("No dialog_out for did [%.*s]", did->len, did->s);
		return -1;
	}

	nr_rows = RES_ROW_N(res);
	LM_ALERT("the database has information about %i dialog_out's\n", nr_rows);
	rows = RES_ROWS(res);

	do {
		for(i=0; i<nr_rows; i++) {
			values = ROW_VALUES(rows + i);

			if (VAL_NULL(GET_FIELD_IDX(values, DLGO_TO_URI_IDX)) ||
				VAL_NULL(GET_FIELD_IDX(values, DLGO_TO_TAG_IDX))) {
				LM_ERR("Columns [%.*s] or/and [%.*s] cannot be null\n",
								to_tag_column.len, to_tag_column.s,
								to_uri_column.len, to_uri_column.s);
				return -1;
			}

			GET_STR_VALUE(to_uri,	values, DLGO_TO_URI_IDX, 	1, 0);
			GET_STR_VALUE(to_tag,	values, DLGO_TO_TAG_IDX, 	1, 0);

			dlg_out	= build_new_dlg_out(dlg, &to_uri, &to_tag);

			if (!dlg_out) {
				LM_ERR("Error creating dlg_out cell\n");
				return -1;
			}

			GET_STR_VALUE(callee_cseq, 		values, DLGO_CALLEE_CSEQ_IDX, 		1, 0);
			GET_STR_VALUE(callee_contact, 	values, DLGO_CALLEE_CONTACT_IDX,	1, 0);
			GET_STR_VALUE(callee_route_set, values, DLGO_CALLEE_ROUTESET_IDX, 	1, 0);

			dlg_out->callee_bind_addr = create_socket_info(values, DLGO_CALLEE_SOCK_IDX);

			update_dlg_out_did(dlg_out, did);

			link_dlg_out(dlg, dlg_out, 0);

			if (dlg_set_leg_info(dlg, &to_tag, &callee_route_set, &callee_contact, &callee_cseq, dlg_out->callee_bind_addr, DLG_CALLEE_LEG) < 0) {
				LM_ERR("Error setting leg info");
				return -1;
			}

	next_dialog:
			;
		}

		/* any more data to be fetched ?*/
		if (DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows) < 0) {
				LM_ERR("re-fetching rows failed\n");
				return -1;
			}
			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		} else
			nr_rows = 0;
	}
	while(nr_rows>0);

	return 0;
}

static int load_dialog_info_from_db(int dlg_hash_size, int fetch_num_rows)
{
	db1_res_t * res;
	db_val_t * values;
	db_row_t * rows;
	struct dlg_entry *d_entry;
	int i, nr_rows;
	struct dlg_cell *dlg  = NULL;
	str callid, from_uri, from_tag, req_uri,
		caller_cseq, caller_contact, caller_rroute,
		toroute_name, did;
	unsigned int next_id;
	
	res = 0;
	if((nr_rows = select_entire_dialog_in_table(&res, fetch_num_rows)) < 0)
		goto end;

	nr_rows = RES_ROW_N(res);

	LM_ALERT("the database has information about %i dialogs\n", nr_rows);

	rows = RES_ROWS(res);

	do {
		/* for every row---dialog */
		for(i=0; i<nr_rows; i++){
			values = ROW_VALUES(rows + i);

			if (VAL_NULL(GET_FIELD_IDX(values, DLGI_HASH_ID_COL_IDX)) ||
				VAL_NULL(GET_FIELD_IDX(values, DLGI_HASH_ENTRY_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
					h_entry_column.len, h_entry_column.s,
					h_id_column.len, h_id_column.s);
				continue;
			}

			if (VAL_NULL(GET_FIELD_IDX(values, DLGI_START_TIME_COL_IDX)) ||
				VAL_NULL(GET_FIELD_IDX(values, DLGI_STATE_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
					start_time_column.len, start_time_column.s,
					state_column.len, state_column.s);
				continue;
			}

			/*restore the dialog info*/
			GET_STR_VALUE(callid, 	values, DLGI_CALLID_COL_IDX, 	1, 0);
			GET_STR_VALUE(from_uri, values, DLGI_FROM_URI_COL_IDX, 	1, 0);
			GET_STR_VALUE(from_tag, values, DLGI_FROM_TAG_COL_IDX, 	1, 0);
			GET_STR_VALUE(req_uri, 	values, DLGI_REQ_URI_COL_IDX,	1, 0);


			if((dlg=build_new_dlg(&callid, &from_uri, &from_tag, &req_uri))==0) {
				LM_ERR("failed to build new dialog\n");
				goto error;
			}

			if(dlg->h_entry != VAL_INT(GET_FIELD_IDX(values, DLGI_HASH_ENTRY_COL_IDX))) {
				LM_ERR("inconsistent hash data in the dialog database: "
					"you may have restarted Kamailio using a different "
					"hash_size: please erase %.*s database and restart\n", 
					dialog_in_table_name.len, dialog_in_table_name.s);
				shm_free(dlg);
				goto error;
			}

			/*link the dialog*/
			link_dlg(dlg, 0);

			GET_STR_VALUE(did, 		values, DLGI_DID_COL_IDX,		1, 0);
			update_dlg_did(dlg, &did);

			dlg->h_id = VAL_INT(GET_FIELD_IDX(values, DLGI_HASH_ID_COL_IDX));
			next_id = d_table->entries[dlg->h_entry].next_id;

			d_table->entries[dlg->h_entry].next_id = (next_id < dlg->h_id) ? (dlg->h_id+1) : next_id;

			dlg->start_ts	= VAL_INT(GET_FIELD_IDX(values, DLGI_START_TIME_COL_IDX));
			dlg->state 		= VAL_INT(GET_FIELD_IDX(values, DLGI_STATE_COL_IDX));

			if (dlg->state==DLG_STATE_CONFIRMED) {
				counter_inc(dialog_ng_cnts_h.active);
			}
			else if (dlg->state==DLG_STATE_EARLY) {
				counter_inc(dialog_ng_cnts_h.active);
			}

			dlg->tl.timeout = (unsigned int)(VAL_INT(GET_FIELD_IDX(values, DLGI_TIMEOUT_COL_IDX)));
			LM_DBG("db dialog timeout is %u (%u/%u)\n", dlg->tl.timeout, get_ticks(), (unsigned int)time(0));

			if (dlg->tl.timeout<=(unsigned int)time(0))
				dlg->tl.timeout = 0;
			else
				dlg->tl.timeout -= (unsigned int)time(0);

			dlg->lifetime = dlg->tl.timeout;

			GET_STR_VALUE(caller_cseq, values, DLGI_CALLER_CSEQ_COL_IDX , 1, 1);
			GET_STR_VALUE(caller_rroute, values, DLGI_CALLER_ROUTESET_COL_IDX, 0, 0);
			GET_STR_VALUE(caller_contact, values,DLGI_CALLER_CONTACT_COL_IDX, 1, 1);

			dlg->caller_bind_addr = create_socket_info(values, DLGI_CALLER_SOCK_IDX);
			//dlg->bind_addr[DLG_CALLEE_LEG] = create_socket_info(values, 17);

			if ( (dlg_set_leg_info( dlg, &from_tag, &caller_rroute,
									&caller_contact, &caller_cseq, dlg->caller_bind_addr,
									DLG_CALLER_LEG) != 0) ) {
				LM_ERR("dlg_set_leg_info failed\n");
				unref_dlg(dlg,1);
				continue;
			}

			dlg->sflags = (unsigned int)VAL_INT(GET_FIELD_IDX(values, DLGI_SFLAGS_COL_IDX));

			GET_STR_VALUE(toroute_name, values, DLGI_TOROUTE_NAME_COL_IDX, 0, 0);
			dlg_set_toroute(dlg, &toroute_name);

			/*restore the timer values */
			if (0 != insert_dlg_timer( &(dlg->tl), (int)dlg->tl.timeout )) {
				LM_CRIT("Unable to insert dlg %p [%u:%u] "
					"with clid '%.*s'\n",
					dlg, dlg->h_entry, dlg->h_id,
					dlg->callid.len, dlg->callid.s);
				unref_dlg(dlg,1);
				continue;
			}

			ref_dlg(dlg,1);
			LM_DBG("current dialog timeout is %u (%u)\n", dlg->tl.timeout,
					get_ticks());

			dlg->dflags = 0;
			next_dialog:
			;
		}

		/* any more data to be fetched ?*/
		if (DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows) < 0) {
				LM_ERR("re-fetching rows failed\n");
				goto error;
			}
			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		} else {
			nr_rows = 0;
		}

	}
	while (nr_rows>0);

	if (dlg != NULL) {
		d_entry = &(d_table->entries[dlg->h_entry]);
		dlg		= d_entry->first;

		while (dlg) {
			load_dialog_out_from_db(dlg, &dlg->did, fetch_num_rows);
			dlg = dlg->next;
		}
	}

	if (dlg_db_mode==DB_MODE_SHUTDOWN) {
		if (dialog_dbf.delete(dialog_db_handle, 0, 0, 0, 0) < 0) {
			LM_ERR("failed to clear dialog table\n");
			goto error;
		}
	}

end:
	dialog_dbf.free_result(dialog_db_handle, res);
	return 0;
error:
	dialog_dbf.free_result(dialog_db_handle, res);
	return -1;

}

static int select_entire_dialog_vars_table(db1_res_t ** res, int fetch_num_rows)
{
	db_key_t query_cols[DIALOG_VARS_TABLE_COL_NO] = {
			&vars_h_entry_column,
			&vars_h_id_column,
			&vars_key_column,
			&vars_value_column };

	if(use_dialog_vars_table() != 0){
		return -1;
	}

	/* select the whole tabel and all the columns */
	if (DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(dialog_dbf.query(dialog_db_handle,0,0,0,query_cols, 0, 
		DIALOG_VARS_TABLE_COL_NO, 0, 0) < 0) {
			LM_ERR("Error while querying (fetch) database\n");
			return -1;
		}
		if(dialog_dbf.fetch_result(dialog_db_handle, res, fetch_num_rows) < 0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if(dialog_dbf.query(dialog_db_handle,0,0,0,query_cols, 0,
		DIALOG_VARS_TABLE_COL_NO, 0, res) < 0) {
			LM_ERR("Error while querying database\n");
			return -1;
		}
	}

	return 0;
}

static int load_dialog_vars_from_db(int fetch_num_rows)
{
	db1_res_t * res;
	db_val_t * values;
	db_row_t * rows;
	struct dlg_cell  * dlg; 
	int i, nr_rows;

	res = 0;
	if((nr_rows = select_entire_dialog_vars_table(&res, fetch_num_rows)) < 0)
		goto end;

	nr_rows = RES_ROW_N(res);

	LM_DBG("the database has information about %i dialog variables\n", nr_rows);

	rows = RES_ROWS(res);

	do {
		/* for every row---dialog */
		for(i=0; i<nr_rows; i++){

			values = ROW_VALUES(rows + i);

			if (VAL_NULL(values) || VAL_NULL(values+1)) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
					vars_h_entry_column.len, vars_h_entry_column.s,
					vars_h_id_column.len, vars_h_id_column.s);
				continue;
			}

			if (VAL_NULL(values+2) || VAL_NULL(values+3)) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
					vars_key_column.len, vars_key_column.s,
					vars_value_column.len, vars_value_column.s);
				continue;
			}
			if (VAL_INT(values) < d_table->size) {
				dlg = (d_table->entries)[VAL_INT(values)].first;
				while (dlg) {
					if (dlg->h_id == VAL_INT(values+1)) {
						str key = { VAL_STR(values+2).s, strlen(VAL_STRING(values+2)) };
						str value = { VAL_STR(values+3).s, strlen(VAL_STRING(values+3)) };
						set_dlg_variable_unsafe(dlg, &key, &value, 1);
						break;
					}
					dlg = dlg->next;
					if (!dlg) {
						LM_WARN("inconsistent data: the dialog h_entry/h_id does not exist!\n");
					}
				}
			} else {
				LM_WARN("inconsistent data: the h_entry in the DB does not exist!\n");
			}
		}

		/* any more data to be fetched ?*/
		if (DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows) < 0) {
				LM_ERR("re-fetching rows failed\n");
				goto error;
			}
			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		} else {
			nr_rows = 0;
		}

	}
	while (nr_rows>0);

	if (dlg_db_mode==DB_MODE_SHUTDOWN) {
		if (dialog_dbf.delete(dialog_db_handle, 0, 0, 0, 0) < 0) {
			LM_ERR("failed to clear dialog variable table\n");
			goto error;
		}
	}

end:
	dialog_dbf.free_result(dialog_db_handle, res);
	return 0;
error:
	dialog_dbf.free_result(dialog_db_handle, res);
	return -1;

}

/*this is only called from destroy_dlg, where the cell's entry lock is acquired*/
int remove_dialog_in_from_db(struct dlg_cell * cell)
{
	db_val_t values[2];
	db_key_t match_keys[2] = { &h_entry_column, &h_id_column};
	db_key_t vars_match_keys[2] = { &vars_h_entry_column, &vars_h_id_column};
	struct dlg_cell_out *dlg_out	= cell->dlg_entry_out.first;

	/*if the dialog hasn 't been yet inserted in the database*/
	LM_DBG("trying to remove dialog [%.*s], update_flag is %i\n",
			cell->callid.len, cell->callid.s,
			cell->dflags);
	if (cell->dflags & DLG_FLAG_NEW) 
		return 0;

	if (use_dialog_table()!=0)
		return -1;

	VAL_TYPE(values) = DB1_INT;
	VAL_TYPE(values + 1) = DB1_INT;

	VAL_NULL(values) = 0;
	VAL_NULL(values + 1) = 0;

	VAL_INT(values)	= cell->h_entry;
	VAL_INT(values + 1) = cell->h_id;

	if(dialog_dbf.delete(dialog_db_handle, match_keys, 0, values, 2) < 0) {
		LM_ERR("failed to delete database information\n");
		return -1;
	}

	if (use_dialog_vars_table()!=0)
		return -1;

	if(dialog_dbf.delete(dialog_db_handle, vars_match_keys, 0, values, 2) < 0) {
		LM_ERR("failed to delete database information\n");
		return -1;
	}

	if (use_dialog_out_table() !=0 )
		return -1;

	while(dlg_out) {
	    LM_DBG("deleting dlg_out from db with h_entry:h_id [%u:%u]\n", dlg_out->h_entry, dlg_out->h_id);
		VAL_INT(values)	= dlg_out->h_entry;
		VAL_INT(values + 1) = dlg_out->h_id;

		if(dialog_dbf.delete(dialog_db_handle, match_keys, 0, values, 2) < 0) {
			LM_ERR("failed to delete dlg_out row\n");
			return -1;
		}
		dlg_out	= dlg_out->next;
	}

	LM_DBG("callid was %.*s\n", cell->callid.len, cell->callid.s );

	return 0;
}


int update_dialog_vars_dbinfo(struct dlg_cell * cell, struct dlg_var * var)
{
	db_val_t values[DIALOG_VARS_TABLE_COL_NO];

	db_key_t insert_keys[DIALOG_VARS_TABLE_COL_NO] = { &vars_h_entry_column,
			&vars_h_id_column,	&vars_key_column,	&vars_value_column };

	if(use_dialog_vars_table()!=0)
		return -1;

	VAL_TYPE(values) = VAL_TYPE(values+1) = DB1_INT;
	VAL_TYPE(values+2) = VAL_TYPE(values+3) = DB1_STR;
	VAL_NULL(values) = VAL_NULL(values+1) = VAL_NULL(values+2) = VAL_NULL(values+3) = 0;
	SET_STR_VALUE(values+2, var->key);

	VAL_INT(values)			= cell->h_entry;
	VAL_INT(values+1)		= cell->h_id;
	
	if((var->vflags & DLG_FLAG_DEL) != 0) {
		/* delete the current variable */
		db_key_t vars_match_keys[3] = { &vars_h_entry_column, &vars_h_id_column, &vars_key_column};

		if (use_dialog_vars_table()!=0)
			return -1;

		if(dialog_dbf.delete(dialog_db_handle, vars_match_keys, 0, values, 3) < 0) {
			LM_ERR("failed to delete database information\n");
			return -1;
		}
	} else if((var->vflags & DLG_FLAG_NEW) != 0) {
		/* save all the current dialogs information*/
		SET_STR_VALUE(values+3, var->value);

		LM_DBG("Inserting into dlg vars table for [%u:%u]\n", cell->h_entry, cell->h_id);
		if((dialog_dbf.insert(dialog_db_handle, insert_keys, values, 
								DIALOG_VARS_TABLE_COL_NO)) !=0){
			LM_ERR("could not add another dialog-var to db\n");
			goto error;
		}
		var->vflags &= ~(DLG_FLAG_NEW|DLG_FLAG_CHANGED);
	} else if((var->vflags & DLG_FLAG_CHANGED) != 0) {
		/* save only dialog's state and timeout */
		SET_STR_VALUE(values+3, var->value);

		if((dialog_dbf.update(dialog_db_handle, insert_keys, 0, 
						values, (insert_keys+3), (values+3), 3, 1)) !=0){
			LM_ERR("could not update database info\n");
			goto error;
		}
		var->vflags &= ~DLG_FLAG_CHANGED;
	} else {
		return 0;
	}
	return 0;
error:
	return -1;
}

int update_dialog_out_dbinfo_unsafe(struct dlg_cell * cell)
{
	struct dlg_cell_out *dlg_out	= cell->dlg_entry_out.first;
	str x = {0,0};
	if(use_dialog_out_table()!=0)
		return -1;

	if ((cell->dflags & DLG_FLAG_NEW) != 0) {
		db_val_t values[DIALOG_OUT_TABLE_COL_NO];
		db_key_t insert_keys[DIALOG_OUT_TABLE_COL_NO] = {
								&id_column,				&h_entry_column,
								&h_id_column, 			&did_column,
								&to_uri_column,			&to_tag_column,
								&caller_cseq_column, 	&callee_cseq_column,
								&callee_contact_column,	&callee_routeset_column,
								&callee_sock_column		};

		VAL_TYPE(GET_FIELD_IDX(values, DLGO_ID_COL_IDX))		= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_HASH_ENTRY_COL_IDX))= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_HASH_ID_COL_IDX))	= DB1_INT;

		VAL_TYPE(GET_FIELD_IDX(values, DLGO_DID_COL_IDX))		= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_TO_URI_IDX))		= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_TO_TAG_IDX))		= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_CALLER_CSEQ_IDX))	= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_CALLEE_CSEQ_IDX))	= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_CALLEE_CONTACT_IDX))= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_CALLEE_ROUTESET_IDX))= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGO_CALLEE_SOCK_IDX))	= DB1_STR;

		VAL_NULL(GET_FIELD_IDX(values, DLGO_ID_COL_IDX))= 1;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_HASH_ENTRY_COL_IDX))= 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_HASH_ID_COL_IDX))	= 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_DID_COL_IDX))		= 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_TO_URI_IDX)) 		= 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_TO_TAG_IDX)) 		= 0;
		//VAL_NULL(GET_FIELD_IDX(values, DLGO_CALLER_CSEQ_IDX)) 	= 0;
		//VAL_NULL(GET_FIELD_IDX(values, DLGO_CALLEE_CSEQ_IDX)) 	= 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_CALLEE_CONTACT_IDX))= 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGO_CALLEE_SOCK_IDX)) 	= 0;

		do {
		    VAL_INT(GET_FIELD_IDX(values, DLGO_ID_COL_IDX))	= 0;
		    VAL_INT(GET_FIELD_IDX(values, DLGO_HASH_ENTRY_COL_IDX))	= dlg_out->h_entry;
		    VAL_INT(GET_FIELD_IDX(values, DLGO_HASH_ID_COL_IDX))	= dlg_out->h_id;

		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_DID_COL_IDX), dlg_out->did);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_TO_URI_IDX), dlg_out->to_uri);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_TO_TAG_IDX), dlg_out->to_tag);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_CALLER_CSEQ_IDX), dlg_out->caller_cseq);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_CALLEE_CSEQ_IDX), dlg_out->callee_cseq);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_CALLEE_CONTACT_IDX), dlg_out->callee_contact);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_CALLEE_ROUTESET_IDX), dlg_out->callee_route_set);
		    SET_STR_VALUE(GET_FIELD_IDX(values, DLGO_CALLEE_SOCK_IDX), dlg_out->callee_bind_addr?dlg_out->callee_bind_addr->sock_str:x);

		    SET_PROPER_NULL_FLAG(dlg_out->callee_route_set, values, DLGO_CALLEE_ROUTESET_IDX);
		    SET_PROPER_NULL_FLAG(dlg_out->caller_cseq, values, DLGO_CALLER_CSEQ_IDX);
		    SET_PROPER_NULL_FLAG(dlg_out->callee_cseq, values, DLGO_CALLEE_CSEQ_IDX);

		    LM_DBG("Inserting into dialog out table for dlg_in: [%u:%u] and dlg_out [%u:%u]\n", cell->h_entry, cell->h_id, dlg_out->h_entry, dlg_out->h_id);
		    if((dialog_dbf.insert(dialog_db_handle, insert_keys, values, DIALOG_OUT_TABLE_COL_NO)) !=0){
			    LM_ERR("could not add another dialog_out to db\n");
			    goto error;
		    }
		    dlg_out	= dlg_out->next;
		}
		while(dlg_out && dlg_out != cell->dlg_entry_out.first);
	}
	else if((cell->dflags & DLG_FLAG_CHANGED) != 0) {
		db_val_t values[4];
		db_key_t insert_keys[4] = {	&h_entry_column,	&h_id_column,
									&caller_cseq_column,&callee_cseq_column
									};

		/* save only dialog's state and timeout */
		VAL_TYPE(GET_FIELD_IDX(values, 0))	= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, 1)) 	= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, 2)) 	= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, 3)) 	= DB1_STR;

		VAL_INT(GET_FIELD_IDX(values, 0))	= dlg_out->h_entry;
		VAL_INT(GET_FIELD_IDX(values, 1))	= dlg_out->h_id;
		SET_STR_VALUE(GET_FIELD_IDX(values, 2), dlg_out->caller_cseq);
		SET_STR_VALUE(GET_FIELD_IDX(values, 3), dlg_out->callee_cseq);

		VAL_NULL(GET_FIELD_IDX(values, 0))	= 0;
		VAL_NULL(GET_FIELD_IDX(values, 1)) 	= 0;

		SET_PROPER_NULL_FLAG(dlg_out->caller_cseq, values, 2);
		SET_PROPER_NULL_FLAG(dlg_out->callee_cseq, values, 3);

		if((dialog_dbf.update(dialog_db_handle, insert_keys, 0, values, insert_keys, values, 2, 4)) !=0 ){
			LM_ERR("could not update database info\n");
			goto error;
		}
	}

	return 0;

error:
	return -1;
}

int update_dialog_dbinfo_unsafe(struct dlg_cell * cell)
{
	struct dlg_var *var;

	if( (cell->dflags & DLG_FLAG_NEW) != 0  || (cell->dflags & DLG_FLAG_CHANGED_VARS) != 0) {
		/* iterate the list */
		for(var=cell->vars ; var ; var=var->next) {
			if (update_dialog_vars_dbinfo(cell, var) != 0)
				return -1;
		}
		/* Remove the flag */
		cell->dflags &= ~DLG_FLAG_CHANGED_VARS;
	}

	if (update_dialog_out_dbinfo_unsafe(cell) != 0)
		goto error;

	if(use_dialog_table()!=0)
		return -1;

	if((cell->dflags & DLG_FLAG_NEW) != 0){
		db_val_t values[DIALOG_IN_TABLE_COL_NO];
		db_key_t insert_keys[DIALOG_IN_TABLE_COL_NO] = {
								&id_column,				&h_entry_column,
								&h_id_column, 			&did_column,
								&call_id, 				&from_uri_column,
								&from_tag_column, 		&caller_original_cseq_column,
								&req_uri_column,		&caller_route_set_column,
								&caller_contact_column, &caller_sock_column, &timeout_column,
								&state_column, 			&start_time_column,
								&sflags_column,			&to_route_name_column, 	&to_route_index_column };

		/* save all the current dialogs information*/
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_ID_COL_IDX)) = DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_HASH_ENTRY_COL_IDX))= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_HASH_ID_COL_IDX)) = DB1_INT;

		VAL_TYPE(GET_FIELD_IDX(values, DLGI_STATE_COL_IDX)) = DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_START_TIME_COL_IDX)) = DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_TIMEOUT_COL_IDX)) = DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_SFLAGS_COL_IDX)) = DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_TOROUTE_INDEX_COL_IDX))	= DB1_INT;

		VAL_TYPE(GET_FIELD_IDX(values, DLGI_DID_COL_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_CALLID_COL_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_FROM_URI_COL_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_FROM_TAG_COL_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_CALLER_CSEQ_COL_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_REQ_URI_COL_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_CALLER_ROUTESET_COL_IDX))= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_CALLER_CONTACT_COL_IDX))= DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_CALLER_SOCK_IDX)) = DB1_STR;
		VAL_TYPE(GET_FIELD_IDX(values, DLGI_TOROUTE_NAME_COL_IDX))	= DB1_STR;

		VAL_INT(GET_FIELD_IDX(values, DLGI_HASH_ENTRY_COL_IDX))	= cell->h_entry;
		VAL_INT(GET_FIELD_IDX(values, DLGI_HASH_ID_COL_IDX))	= cell->h_id;
		VAL_INT(GET_FIELD_IDX(values, DLGI_START_TIME_COL_IDX))	= cell->start_ts;
		VAL_INT(GET_FIELD_IDX(values, DLGI_STATE_COL_IDX))		= cell->state;
		VAL_INT(GET_FIELD_IDX(values, DLGI_TIMEOUT_COL_IDX))	= (unsigned int)( (unsigned int)time(0) + cell->tl.timeout - get_ticks() );

		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_CALLID_COL_IDX), cell->callid);
		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_DID_COL_IDX), cell->did);
		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_FROM_URI_COL_IDX), cell->from_uri);
		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_FROM_TAG_COL_IDX), cell->from_tag);
		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_CALLER_CSEQ_COL_IDX), cell->first_req_cseq);

		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_CALLER_SOCK_IDX), cell->caller_bind_addr->sock_str);

		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_CALLER_ROUTESET_COL_IDX), cell->caller_route_set);
		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_CALLER_CONTACT_COL_IDX), cell->caller_contact);

		SET_PROPER_NULL_FLAG(cell->caller_route_set, values, DLGI_CALLER_ROUTESET_COL_IDX);

		VAL_NULL(GET_FIELD_IDX(values, DLGI_SFLAGS_COL_IDX)) = 0;
		VAL_INT(GET_FIELD_IDX(values, DLGI_SFLAGS_COL_IDX))  = cell->sflags;

		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_TOROUTE_NAME_COL_IDX), cell->toroute_name);
		SET_STR_VALUE(GET_FIELD_IDX(values, DLGI_REQ_URI_COL_IDX), cell->req_uri);

		SET_PROPER_NULL_FLAG(cell->callid, 	values, DLGI_CALLID_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->did, 	values, DLGI_DID_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->from_uri,values, DLGI_FROM_URI_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->from_tag,values, DLGI_FROM_TAG_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->caller_route_set,values, DLGI_CALLER_ROUTESET_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->req_uri, values, DLGI_REQ_URI_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->toroute_name, values, DLGI_TOROUTE_NAME_COL_IDX);
		SET_PROPER_NULL_FLAG(cell->first_req_cseq, values, DLGI_CALLER_CSEQ_COL_IDX);

		VAL_NULL(GET_FIELD_IDX(values, DLGI_ID_COL_IDX)) = 1;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_HASH_ENTRY_COL_IDX)) = 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_HASH_ID_COL_IDX)) = 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_STATE_COL_IDX)) = 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_START_TIME_COL_IDX)) = 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_TIMEOUT_COL_IDX)) = 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_CALLER_CONTACT_COL_IDX)) = 0;
		VAL_NULL(GET_FIELD_IDX(values, DLGI_CALLER_SOCK_IDX)) = 0;

		LM_DBG("Inserting dialog into dialog_in table [%u:%u]\n", cell->h_entry, cell->h_id);
		if((dialog_dbf.insert(dialog_db_handle, insert_keys, values, DIALOG_IN_TABLE_COL_NO)) !=0){
			LM_ERR("could not add another dialog_in to db\n");
			goto error;
		}

		cell->dflags &= ~(DLG_FLAG_NEW|DLG_FLAG_CHANGED);
		cell->dflags |= DLG_FLAG_INSERTED;
		
	} else if((cell->dflags & DLG_FLAG_CHANGED) != 0) {

		db_val_t values[5];
		db_key_t insert_keys[5] = {	&h_entry_column,	&h_id_column,
									&state_column,		&timeout_column,
									&caller_original_cseq_column};

		/* save only dialog's state and timeout */
		VAL_TYPE(GET_FIELD_IDX(values, 0))	= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, 1)) 	= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, 2)) 	= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, 3)) 	= DB1_INT;
		VAL_TYPE(GET_FIELD_IDX(values, 4))	= DB1_STR;

		VAL_INT(GET_FIELD_IDX(values, 0))	= cell->h_entry;
		VAL_INT(GET_FIELD_IDX(values, 1))	= cell->h_id;
		VAL_INT(GET_FIELD_IDX(values, 2))	= cell->state;
		VAL_INT(GET_FIELD_IDX(values, 3))	= (unsigned int)( (unsigned int)time(0) + cell->tl.timeout - get_ticks() );
		SET_STR_VALUE(GET_FIELD_IDX(values, 4), cell->first_req_cseq);

		VAL_NULL(GET_FIELD_IDX(values, 0))	= 0;
		VAL_NULL(GET_FIELD_IDX(values, 1)) 	= 0;
		VAL_NULL(GET_FIELD_IDX(values, 2)) 	= 0;
		VAL_NULL(GET_FIELD_IDX(values, 3)) 	= 0;
		VAL_NULL(GET_FIELD_IDX(values, 4)) 	= 0;

		LM_DBG("Updating dialog in dialog_in table [%u:%u]\n", cell->h_entry, cell->h_id);
		if((dialog_dbf.update(dialog_db_handle, insert_keys, 0, values, insert_keys, values, 2, 4)) !=0 ){
			LM_ERR("could not update database info\n");
			goto error;
		}
		cell->dflags &= ~(DLG_FLAG_CHANGED);
	}

	return 0;

error:

	return -1;
}

int update_dialog_dbinfo(struct dlg_cell * cell)
{
	struct dlg_entry entry;
	/* lock the entry */
	entry = (d_table->entries)[cell->h_entry];
	dlg_lock( d_table, &entry);
	if (update_dialog_dbinfo_unsafe(cell) != 0) {
		dlg_unlock( d_table, &entry);
		return -1;
	} 
	dlg_unlock( d_table, &entry);
	return 0;
}

void dialog_update_db(unsigned int ticks, void * param)
{
	int index;
	struct dlg_entry entry;
	struct dlg_cell  * cell; 

	LM_DBG("saving current_info \n");
	
	for(index = 0; index< d_table->size; index++){
		/* lock the whole entry */
		entry = (d_table->entries)[index];
		dlg_lock( d_table, &entry);

		for(cell = entry.first; cell != NULL; cell = cell->next){
			if (update_dialog_dbinfo_unsafe(cell) != 0) {
				dlg_unlock( d_table, &entry);
				goto error;
			}
		}
		dlg_unlock( d_table, &entry);

	}

	return;

error:
	dlg_unlock( d_table, &entry);
}

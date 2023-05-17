/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Database interface
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/timer.h"
#include "../../lib/srdb1/db.h"
#include "../../core/str.h"
#include "../../core/socket_info.h"
#include "../../core/counters.h"
#include "dlg_hash.h"
#include "dlg_var.h"
#include "dlg_cb.h"
#include "dlg_profile.h"
#include "dlg_db_handler.h"


str call_id_column = str_init(CALL_ID_COL);
str from_uri_column = str_init(FROM_URI_COL);
str from_tag_column = str_init(FROM_TAG_COL);
str to_uri_column = str_init(TO_URI_COL);
str to_tag_column = str_init(TO_TAG_COL);
str h_id_column = str_init(HASH_ID_COL);
str h_entry_column = str_init(HASH_ENTRY_COL);
str state_column = str_init(STATE_COL);
str start_time_column = str_init(START_TIME_COL);
str timeout_column = str_init(TIMEOUT_COL);
str to_cseq_column = str_init(TO_CSEQ_COL);
str from_cseq_column = str_init(FROM_CSEQ_COL);
str to_route_column = str_init(TO_ROUTE_COL);
str from_route_column = str_init(FROM_ROUTE_COL);
str to_contact_column = str_init(TO_CONTACT_COL);
str from_contact_column = str_init(FROM_CONTACT_COL);
str to_sock_column = str_init(TO_SOCK_COL);
str from_sock_column = str_init(FROM_SOCK_COL);
str sflags_column = str_init(SFLAGS_COL);
str iflags_column = str_init(IFLAGS_COL);
str toroute_name_column = str_init(TOROUTE_NAME_COL);
str req_uri_column = str_init(REQ_URI_COL);
str xdata_column = str_init(XDATA_COL);
str dialog_table_name = str_init(DIALOG_TABLE_NAME);
int dlg_db_mode = DB_MODE_NONE;

str vars_h_id_column = str_init(VARS_HASH_ID_COL);
str vars_h_entry_column = str_init(VARS_HASH_ENTRY_COL);
str vars_key_column = str_init(VARS_KEY_COL);
str vars_value_column = str_init(VARS_VALUE_COL);
str dialog_vars_table_name = str_init(DIALOG_VARS_TABLE_NAME);

static db1_con_t *dialog_db_handle = 0; /* database connection handle */
static db_func_t dialog_dbf;

extern int dlg_enable_stats;
extern int dlg_h_id_start;
extern int dlg_h_id_step;

#define SET_STR_VALUE(_val, _str)         \
	do {                                  \
		VAL_STR((_val)).s = (_str).s;     \
		VAL_STR((_val)).len = (_str).len; \
	} while(0);

#define SET_NULL_FLAG(_vals, _i, _max, _flag)   \
	do {                                        \
		for((_i) = 0; (_i) < (_max); (_i)++)    \
			VAL_NULL((_vals) + (_i)) = (_flag); \
	} while(0);

#define SET_PROPER_NULL_FLAG(_str, _vals, _index) \
	do {                                          \
		if((_str).len == 0)                       \
			VAL_NULL((_vals) + (_index)) = 1;     \
		else                                      \
			VAL_NULL((_vals) + (_index)) = 0;     \
	} while(0);

#define GET_STR_VALUE(_res, _values, _index, _not_null, _unref)   \
	do {                                                          \
		if(VAL_NULL((_values) + (_index))) {                      \
			if(_not_null) {                                       \
				if(_unref)                                        \
					dlg_unref(dlg, 1);                            \
				goto next_dialog;                                 \
			} else {                                              \
				(_res).s = 0;                                     \
				(_res).len = 0;                                   \
			}                                                     \
		} else {                                                  \
			(_res).s = VAL_STR((_values) + (_index)).s;           \
			(_res).len = strlen(VAL_STR((_values) + (_index)).s); \
		}                                                         \
	} while(0);

static int load_dialog_vars_from_db(
		int fetch_num_rows, int mode, dlg_iuid_t *mval);

int dlg_connect_db(const str *db_url)
{
	if(dialog_db_handle) {
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}
	if((dialog_db_handle = dialog_dbf.init(db_url)) == 0)
		return -1;
	return 0;
}


int init_dlg_db(const str *db_url, int dlg_hash_size, int db_update_period,
		int fetch_num_rows, int db_skip_load)
{
	/* Find a database module */
	if(db_bind_mod(db_url, &dialog_dbf) < 0) {
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if(dlg_connect_db(db_url) != 0) {
		LM_ERR("Unable to connect to the database\n");
		return -1;
	}

	if(db_check_table_version(&dialog_dbf, dialog_db_handle, &dialog_table_name,
			   DLG_TABLE_VERSION)
			< 0) {
		DB_TABLE_VERSION_ERROR(dialog_table_name);
		goto dberror;
	}

	if(db_check_table_version(&dialog_dbf, dialog_db_handle,
			   &dialog_vars_table_name, DLG_VARS_TABLE_VERSION)
			< 0) {
		DB_TABLE_VERSION_ERROR(dialog_vars_table_name);
		goto dberror;
	}

	if((dlg_db_mode == DB_MODE_DELAYED)
			&& (register_timer(dialog_update_db, 0, db_update_period) < 0)) {
		LM_ERR("Failed to register update db timer\n");
		goto dberror;
	}

	if(db_skip_load == 0) {
		if((load_dialog_info_from_db(dlg_hash_size, fetch_num_rows, 0, NULL))
				!= 0) {
			LM_ERR("Unable to load the dialog data\n");
			goto dberror;
		}
		if((load_dialog_vars_from_db(fetch_num_rows, 0, NULL)) != 0) {
			LM_ERR("Unable to load the dialog variable data\n");
			goto dberror;
		}
	}
	dialog_dbf.close(dialog_db_handle);
	dialog_db_handle = 0;

	return 0;

dberror:
	dialog_dbf.close(dialog_db_handle);
	dialog_db_handle = 0;
	return -1;
}


void destroy_dlg_db(void)
{
	/* close the DB connection */
	if(dialog_db_handle) {
		dialog_dbf.close(dialog_db_handle);
		dialog_db_handle = 0;
	}
}


static int use_dialog_table(void)
{
	if(!dialog_db_handle) {
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if(dialog_dbf.use_table(dialog_db_handle, &dialog_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

static int use_dialog_vars_table(void)
{
	if(!dialog_db_handle) {
		LM_ERR("invalid database handle for dialog_vars\n");
		return -1;
	}

	if(dialog_dbf.use_table(dialog_db_handle, &dialog_vars_table_name) < 0) {
		LM_ERR("Error in use_table for dialog_vars\n");
		return -1;
	}

	return 0;
}

struct socket_info *create_socket_info(db_val_t *vals, int n)
{

	struct socket_info *sock;
	char *p;
	str host;
	int port, proto;

	/* socket name */
	p = (VAL_STR(vals + n)).s;

	if(VAL_NULL(vals + n) || p == 0 || p[0] == 0) {
		sock = 0;
	} else {
		if(parse_phostport(p, &host.s, &host.len, &port, &proto) != 0) {
			LM_ERR("bad socket <%s>\n", p);
			return 0;
		}
		sock = grep_sock_info(&host, (unsigned short)port, proto);
		if(sock == 0) {
			LM_WARN("non-local socket <%s>...ignoring\n", p);
		}
	}

	return sock;
}


int load_dialog_info_from_db(
		int dlg_hash_size, int fetch_num_rows, int mode, str *mval)
{
	db_key_t query_cols[DIALOG_TABLE_COL_NO] = {&h_entry_column, &h_id_column,
			&call_id_column, &from_uri_column, &from_tag_column, &to_uri_column,
			&to_tag_column, &start_time_column, &state_column, &timeout_column,
			&from_cseq_column, &to_cseq_column, &from_route_column,
			&to_route_column, &from_contact_column, &to_contact_column,
			&from_sock_column, &to_sock_column, &sflags_column,
			&toroute_name_column, &req_uri_column, &xdata_column,
			&iflags_column};
	db_key_t match_cols[1] = {&call_id_column};
	db_val_t match_vals[1];
	int match_cols_no = 0;
	db1_res_t *res;
	db_val_t *values;
	db_row_t *rows;
	int i, nr_rows;
	struct dlg_cell *dlg;
	str callid, from_uri, to_uri, from_tag, to_tag, req_uri;
	str cseq1, cseq2, contact1, contact2, rroute1, rroute2;
	str toroute_name;
	str xdata;
	unsigned int next_id;
	srjson_doc_t jdoc;
#define DLG_MAX_DB_LOAD_EXTRA 256
	dlg_iuid_t dbuid[DLG_MAX_DB_LOAD_EXTRA];
	int loaded_extra = 0;
	int loaded_extra_more = 0;
	dlg_cell_t *dit;

	if(use_dialog_table() != 0) {
		return -1;
	}

	if(mode == 1 && mval != NULL && mval->len > 0) {
		match_vals[0].type = DB1_STR;
		match_vals[0].nul = 0;
		match_vals[0].val.str_val = *mval;
		match_cols_no = 1;
	}

	res = 0;

	if(DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(dialog_dbf.query(dialog_db_handle, match_cols, 0, match_vals,
				   query_cols, match_cols_no, DIALOG_TABLE_COL_NO, 0, 0)
				< 0) {
			LM_ERR("Error while querying (fetch) database\n");
			goto error;
		}
		if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows)
				< 0) {
			LM_ERR("fetching rows failed\n");
			goto error;
		}
	} else {
		if(dialog_dbf.query(dialog_db_handle, match_cols, 0, match_vals,
				   query_cols, match_cols_no, DIALOG_TABLE_COL_NO, 0, &res)
				< 0) {
			LM_ERR("Error while querying database\n");
			goto error;
		}
	}

	nr_rows = RES_ROW_N(res);

	LM_DBG("the database has information about %i dialogs\n", nr_rows);

	rows = RES_ROWS(res);

	do {
		/* for every row---dialog */
		for(i = 0; i < nr_rows; i++) {

			values = ROW_VALUES(rows + i);

			if(VAL_NULL(values) || VAL_NULL(values + 1)) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						h_entry_column.len, h_entry_column.s, h_id_column.len,
						h_id_column.s);
				continue;
			}

			if(VAL_NULL(values + 7) || VAL_NULL(values + 8)) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						start_time_column.len, start_time_column.s,
						state_column.len, state_column.s);
				continue;
			}

			/*restore the dialog info*/
			GET_STR_VALUE(callid, values, 2, 1, 0);
			GET_STR_VALUE(from_uri, values, 3, 1, 0);
			GET_STR_VALUE(from_tag, values, 4, 1, 0);
			GET_STR_VALUE(to_uri, values, 5, 1, 0);
			GET_STR_VALUE(req_uri, values, 20, 1, 0);

			if((dlg = build_new_dlg(
						&callid, &from_uri, &to_uri, &from_tag, &req_uri))
					== 0) {
				LM_ERR("failed to build new dialog\n");
				goto error;
			}

			if(dlg->h_entry != VAL_INT(values)) {
				LM_ERR("inconsistent hash data in the dialog database: "
					   "you may have restarted Kamailio using a different "
					   "hash_size: please erase %.*s database and restart\n",
						dialog_table_name.len, dialog_table_name.s);
				shm_free(dlg);
				goto error;
			}

			if(mode != 0) {
				dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));
				/* loading at runtime - check dialog id conflicts */
				dit = (d_table->entries)[dlg->h_entry].first;
				while(dit) {
					if(dit->h_id == VAL_INT(values + 1)) {
						break;
					}
					dit = dit->next;
				}
				if(dit) {
					if(mode == 1) {
						LM_WARN("conflicting dialog id: %u/%u - skipping\n",
								dlg->h_entry,
								(unsigned int)VAL_INT(values + 1));
					} else {
						LM_DBG("conflicting dialog id: %u/%u - skipping\n",
								dlg->h_entry,
								(unsigned int)VAL_INT(values + 1));
					}
					dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
					shm_free(dlg);
					continue;
				}
			}

			/*link the dialog*/
			link_dlg(dlg, 0, 0);

			dlg->h_id = VAL_INT(values + 1);
			next_id = d_table->entries[dlg->h_entry].next_id;
			if(dlg_h_id_step == 1) {
				d_table->entries[dlg->h_entry].next_id =
						(next_id <= dlg->h_id) ? (dlg->h_id + 1) : next_id;
			} else {
				/* update next id only if matches this instance series */
				if((dlg->h_id - dlg_h_id_start) % dlg_h_id_step == 0) {
					d_table->entries[dlg->h_entry].next_id =
							(next_id <= dlg->h_id) ? (dlg->h_id + dlg_h_id_step)
												   : next_id;
				}
			}
			if(mode != 0) {
				dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
			}

			GET_STR_VALUE(to_tag, values, 6, 1, 1);

			dlg->start_ts = VAL_INT(values + 7);

			dlg->state = VAL_INT(values + 8);
			if(dlg->state == DLG_STATE_CONFIRMED_NA
					|| dlg->state == DLG_STATE_CONFIRMED) {
				if_update_stat(dlg_enable_stats, active_dlgs, 1);
			} else if(dlg->state == DLG_STATE_EARLY) {
				if_update_stat(dlg_enable_stats, early_dlgs, 1);
			}

			dlg->tl.timeout = (unsigned int)(VAL_INT(values + 9));
			LM_DBG("db dialog timeout is %u (%u/%u)\n", dlg->tl.timeout,
					get_ticks(), (unsigned int)time(0));
			if(dlg->tl.timeout <= (unsigned int)time(0)) {
				dlg->tl.timeout = 0;
				dlg->lifetime = 0;
			} else {
				dlg->lifetime = dlg->tl.timeout - dlg->start_ts;
				dlg->tl.timeout -= (unsigned int)time(0);
			}

			GET_STR_VALUE(cseq1, values, 10, 1, 1);
			GET_STR_VALUE(cseq2, values, 11, 1, 1);
			GET_STR_VALUE(rroute1, values, 12, 0, 0);
			GET_STR_VALUE(rroute2, values, 13, 0, 0);
			GET_STR_VALUE(contact1, values, 14, 1, 1);
			GET_STR_VALUE(contact2, values, 15, 1, 1);

			if((dlg_set_leg_info(dlg, &from_tag, &rroute1, &contact1, &cseq1,
						DLG_CALLER_LEG)
					   != 0)
					|| (dlg_set_leg_info(dlg, &to_tag, &rroute2, &contact2,
								&cseq2, DLG_CALLEE_LEG)
							!= 0)) {
				LM_ERR("dlg_set_leg_info failed\n");
				dlg_unref(dlg, 1);
				continue;
			}

			dlg->bind_addr[DLG_CALLER_LEG] = create_socket_info(values, 16);
			dlg->bind_addr[DLG_CALLEE_LEG] = create_socket_info(values, 17);

			dlg->sflags = (unsigned int)VAL_INT(values + 18);

			GET_STR_VALUE(toroute_name, values, 19, 0, 0);
			dlg_set_toroute(dlg, &toroute_name);

			GET_STR_VALUE(xdata, values, 21, 0, 0);
			if(xdata.len > 0 && xdata.s != NULL
					&& dlg->state != DLG_STATE_DELETED) {
				srjson_InitDoc(&jdoc, NULL);
				jdoc.buf = xdata;
				dlg_json_to_profiles(dlg, &jdoc);
				srjson_DestroyDoc(&jdoc);
			}
			dlg->iflags = (unsigned int)VAL_INT(values + 22);
			if(dlg->state == DLG_STATE_CONFIRMED)
				dlg_ka_add(dlg);

			if(!dlg->bind_addr[DLG_CALLER_LEG]
					|| !dlg->bind_addr[DLG_CALLEE_LEG]) {
				/* non-local socket, probably not our dialog */
				dlg->iflags &= ~DLG_IFLAG_DMQ_SYNC;
			}

			if(dlg->state == DLG_STATE_DELETED) {
				/* end_ts used for force clean up not stored - set it to now */
				dlg->end_ts = (unsigned int)time(0);
			}
			/*restore the timer values */
			if(0 != insert_dlg_timer(&(dlg->tl), (int)dlg->tl.timeout)) {
				LM_CRIT("Unable to insert dlg %p [%u:%u] "
						"with clid '%.*s' and tags '%.*s' '%.*s'\n",
						dlg, dlg->h_entry, dlg->h_id, dlg->callid.len,
						dlg->callid.s, dlg->tag[DLG_CALLER_LEG].len,
						dlg->tag[DLG_CALLER_LEG].s,
						dlg->tag[DLG_CALLEE_LEG].len,
						dlg->tag[DLG_CALLEE_LEG].s);
				dlg_unref(dlg, 1);
				continue;
			}
			dlg_ref(dlg, 1);
			LM_DBG("current dialog timeout is %u (%u)\n", dlg->tl.timeout,
					get_ticks());

			dlg->dflags = 0;

			if(mode != 0) {
				if(loaded_extra < DLG_MAX_DB_LOAD_EXTRA) {
					dbuid[loaded_extra].h_entry = dlg->h_entry;
					dbuid[loaded_extra].h_id = dlg->h_id;
					loaded_extra++;
				} else {
					dlg->dflags |= DLG_FLAG_DB_LOAD_EXTRA;
					loaded_extra_more = 1;
				}
				/* if loading at runtime run the callbacks for the loaded dialog */
				run_dlg_load_callbacks(dlg);
			}
		next_dialog:;
		}

		/* any more data to be fetched ?*/
		if(DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows)
					< 0) {
				LM_ERR("re-fetching rows failed\n");
				goto error;
			}
			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		} else {
			nr_rows = 0;
		}

	} while(nr_rows > 0);

	if(mode != 0) {
		for(i = 0; i < loaded_extra; i++) {
			load_dialog_vars_from_db(fetch_num_rows, 1, &dbuid[i]);
		}
		if(loaded_extra_more) {
			/* more dialogs loaded - scan hash table */
			for(i = 0; i < d_table->size; i++) {
				dlg_lock(d_table, &d_table->entries[i]);
				dlg = d_table->entries[i].first;
				while(dlg) {
					if(dlg->dflags & DLG_FLAG_DB_LOAD_EXTRA) {
						dbuid[0].h_entry = dlg->h_entry;
						dbuid[0].h_id = dlg->h_id;
						load_dialog_vars_from_db(fetch_num_rows, 1, &dbuid[0]);
						dlg->dflags &= ~DLG_FLAG_DB_LOAD_EXTRA;
					}
					dlg = dlg->next;
				}
				dlg_unlock(d_table, &d_table->entries[i]);
			}
		}
		goto end;
	}

	if(dlg_db_mode == DB_MODE_SHUTDOWN) {
		if(dialog_dbf.delete(dialog_db_handle, 0, 0, 0, 0) < 0) {
			LM_ERR("failed to clear dialog table\n");
			goto error;
		}
	}

end:
	dialog_dbf.free_result(dialog_db_handle, res);
	return 0;
error:
	if(res != NULL)
		dialog_dbf.free_result(dialog_db_handle, res);
	return -1;
}

static int load_dialog_vars_from_db(
		int fetch_num_rows, int mode, dlg_iuid_t *mval)
{
	db_key_t query_cols[DIALOG_VARS_TABLE_COL_NO] = {&vars_h_entry_column,
			&vars_h_id_column, &vars_key_column, &vars_value_column};
	db_key_t match_cols[2] = {&vars_h_entry_column, &vars_h_id_column};
	db_val_t match_vals[2];
	int match_cols_no = 0;
	db1_res_t *res;
	db_val_t *values;
	db_row_t *rows;
	struct dlg_cell *dlg;
	int i, nr_rows;

	if(use_dialog_vars_table() != 0) {
		return -1;
	}

	if(mode == 1 && mval != NULL) {
		VAL_TYPE(match_vals) = VAL_TYPE(match_vals + 1) = DB1_INT;
		VAL_NULL(match_vals) = VAL_NULL(match_vals + 1) = 0;
		VAL_INT(match_vals) = mval->h_entry;
		VAL_INT(match_vals + 1) = mval->h_id;
		match_cols_no = 2;
	}

	res = 0;
	/* select the whole table and all the columns */
	if(DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(dialog_dbf.query(dialog_db_handle, match_cols, 0, match_vals,
				   query_cols, match_cols_no, DIALOG_VARS_TABLE_COL_NO, 0, 0)
				< 0) {
			LM_ERR("Error while querying (fetch) database\n");
			goto error;
		}
		if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows)
				< 0) {
			LM_ERR("fetching rows failed\n");
			goto error;
		}
	} else {
		if(dialog_dbf.query(dialog_db_handle, match_cols, 0, match_vals,
				   query_cols, match_cols_no, DIALOG_VARS_TABLE_COL_NO, 0, &res)
				< 0) {
			LM_ERR("Error while querying database\n");
			goto error;
		}
	}

	nr_rows = RES_ROW_N(res);

	LM_DBG("the database has information about %i dialog variables\n", nr_rows);

	rows = RES_ROWS(res);

	do {
		/* for every row---dialog */
		for(i = 0; i < nr_rows; i++) {

			values = ROW_VALUES(rows + i);

			if(VAL_NULL(values) || VAL_NULL(values + 1)) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						vars_h_entry_column.len, vars_h_entry_column.s,
						vars_h_id_column.len, vars_h_id_column.s);
				continue;
			}

			if(VAL_NULL(values + 2) || VAL_NULL(values + 3)) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						vars_key_column.len, vars_key_column.s,
						vars_value_column.len, vars_value_column.s);
				continue;
			}
			if(VAL_INT(values) < d_table->size) {
				if(mode == 1 && mval != NULL) {
					dlg_lock(d_table, &(d_table->entries[VAL_INT(values)]));
				}
				dlg = (d_table->entries)[VAL_INT(values)].first;
				while(dlg) {
					if(dlg->h_id == VAL_INT(values + 1)) {
						str key = {VAL_STR(values + 2).s,
								strlen(VAL_STRING(values + 2))};
						str value = {VAL_STR(values + 3).s,
								strlen(VAL_STRING(values + 3))};
						set_dlg_variable_unsafe(dlg, &key, &value);
						break;
					}
					dlg = dlg->next;
					if(!dlg) {
						LM_WARN("inconsistent data: the dialog h_entry/h_id "
								"does not exist!\n");
					}
				}
				if(mode == 1 && mval != NULL) {
					dlg_unlock(d_table, &(d_table->entries[VAL_INT(values)]));
				}
			} else {
				LM_WARN("inconsistent data: the h_entry in the DB does not "
						"exist!\n");
			}
		}

		/* any more data to be fetched ?*/
		if(DB_CAPABILITY(dialog_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(dialog_dbf.fetch_result(dialog_db_handle, &res, fetch_num_rows)
					< 0) {
				LM_ERR("re-fetching rows failed\n");
				goto error;
			}
			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		} else {
			nr_rows = 0;
		}

	} while(nr_rows > 0);

	if(mode != 0) {
		goto end;
	}

	if(dlg_db_mode == DB_MODE_SHUTDOWN) {
		if(dialog_dbf.delete(dialog_db_handle, 0, 0, 0, 0) < 0) {
			LM_ERR("failed to clear dialog variable table\n");
			goto error;
		}
	}

end:
	dialog_dbf.free_result(dialog_db_handle, res);
	return 0;
error:
	if(res != 0)
		dialog_dbf.free_result(dialog_db_handle, res);
	return -1;
}

/*this is only called from destroy_dlg, where the cell's entry lock is acquired*/
int remove_dialog_from_db(struct dlg_cell *cell)
{
	db_val_t values[2];
	db_key_t match_keys[2] = {&h_entry_column, &h_id_column};
	db_key_t vars_match_keys[2] = {&vars_h_entry_column, &vars_h_id_column};

	/*if the dialog hasn 't been yet inserted in the database*/
	LM_DBG("trying to remove dialog [%.*s], update_flag is %i\n",
			cell->callid.len, cell->callid.s, cell->dflags);
	if(cell->dflags & DLG_FLAG_NEW)
		return 0;

	if(use_dialog_table() != 0)
		return -1;

	VAL_TYPE(values) = VAL_TYPE(values + 1) = DB1_INT;
	VAL_NULL(values) = VAL_NULL(values + 1) = 0;

	VAL_INT(values) = cell->h_entry;
	VAL_INT(values + 1) = cell->h_id;

	if(dialog_dbf.delete(dialog_db_handle, match_keys, 0, values, 2) < 0) {
		LM_ERR("failed to delete database information\n");
		return -1;
	}

	if(use_dialog_vars_table() != 0)
		return -1;

	if(dialog_dbf.delete(dialog_db_handle, vars_match_keys, 0, values, 2) < 0) {
		LM_ERR("failed to delete database information\n");
		return -1;
	}

	LM_DBG("callid was %.*s\n", cell->callid.len, cell->callid.s);

	return 0;
}


int update_dialog_vars_dbinfo(struct dlg_cell *cell, struct dlg_var *var)
{
	db_val_t values[DIALOG_VARS_TABLE_COL_NO];

	db_key_t insert_keys[DIALOG_VARS_TABLE_COL_NO] = {&vars_h_entry_column,
			&vars_h_id_column, &vars_key_column, &vars_value_column};

	if(use_dialog_vars_table() != 0)
		return -1;

	VAL_TYPE(values) = VAL_TYPE(values + 1) = DB1_INT;
	VAL_TYPE(values + 2) = VAL_TYPE(values + 3) = DB1_STR;
	VAL_NULL(values) = VAL_NULL(values + 1) = VAL_NULL(values + 2) =
			VAL_NULL(values + 3) = 0;
	SET_STR_VALUE(values + 2, var->key);

	VAL_INT(values) = cell->h_entry;
	VAL_INT(values + 1) = cell->h_id;

	if((var->vflags & DLG_FLAG_DEL) != 0) {
		/* delete the current variable */
		db_key_t vars_match_keys[3] = {
				&vars_h_entry_column, &vars_h_id_column, &vars_key_column};

		if(use_dialog_vars_table() != 0)
			return -1;

		if(dialog_dbf.delete(dialog_db_handle, vars_match_keys, 0, values, 3)
				< 0) {
			LM_ERR("failed to delete database information\n");
			return -1;
		}
	} else if((var->vflags & DLG_FLAG_NEW) != 0) {
		/* save all the current dialogs information*/
		SET_STR_VALUE(values + 3, var->value);

		if((dialog_dbf.insert(dialog_db_handle, insert_keys, values,
				   DIALOG_VARS_TABLE_COL_NO))
				!= 0) {
			LM_ERR("could not add another dialog-var to db\n");
			goto error;
		}
		var->vflags &= ~(DLG_FLAG_NEW | DLG_FLAG_CHANGED);
	} else if((var->vflags & DLG_FLAG_CHANGED) != 0) {
		/* save only dialog's state and timeout */
		SET_STR_VALUE(values + 3, var->value);

		if((dialog_dbf.update(dialog_db_handle, insert_keys, 0, values,
				   (insert_keys + 3), (values + 3), 3, 1))
				!= 0) {
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


int update_dialog_dbinfo_unsafe(struct dlg_cell *cell)
{
	int i;
	struct dlg_var *var;
	srjson_doc_t jdoc;
	str sempty = str_init("");

	db_val_t values[DIALOG_TABLE_COL_NO];

	db_key_t insert_keys[DIALOG_TABLE_COL_NO] = {&h_entry_column,		  /*0*/
			&h_id_column, /*1*/ &call_id_column, /*2*/ &from_uri_column,  /*3*/
			&from_tag_column, /*4*/ &to_uri_column, /*5*/ &to_tag_column, /*6*/
			&from_sock_column, /*7*/ &to_sock_column,					  /*8*/
			&start_time_column, /*9*/ &state_column,
			/*10*/ &timeout_column, /*11*/
			&from_cseq_column, /*12*/ &to_cseq_column,
			/*13*/ &from_contact_column, /*14*/
			&to_contact_column, /*15*/ &from_route_column,
			/*16*/ &to_route_column, /*17*/
			&sflags_column, /*18*/ &toroute_name_column,
			/*19*/ &req_uri_column, /*20*/
			&xdata_column, /*21*/ &iflags_column /*22*/};

	if(cell->state < DLG_STATE_EARLY || cell->state == DLG_STATE_DELETED) {
		LM_DBG("not storing dlg in db during initial or deleted states\n");
		return 0;
	}

	i = 0;
	if((cell->dflags & DLG_FLAG_NEW) != 0
			|| (cell->dflags & DLG_FLAG_CHANGED_VARS) != 0) {
		/* iterate the list */
		for(var = cell->vars; var; var = var->next) {
			if(update_dialog_vars_dbinfo(cell, var) != 0)
				return -1;
			i++;
		}
		/* Remove the flag */
		cell->dflags &= ~DLG_FLAG_CHANGED_VARS;
		LM_DBG("updated %d vars for dlg [%d:%d]\n", i, cell->h_entry,
				cell->h_id);
	}

	if(use_dialog_table() != 0)
		return -1;

	srjson_InitDoc(&jdoc, NULL);

	if((cell->dflags & DLG_FLAG_NEW) != 0) {
		/* save all the current dialogs information*/
		VAL_TYPE(values) = VAL_TYPE(values + 1) = VAL_TYPE(values + 9) =
				VAL_TYPE(values + 10) = VAL_TYPE(values + 11) = DB1_INT;

		VAL_TYPE(values + 2) = VAL_TYPE(values + 3) = VAL_TYPE(
				values + 4) = VAL_TYPE(values + 5) = VAL_TYPE(values + 6) =
				VAL_TYPE(values + 7) = VAL_TYPE(values + 8) = VAL_TYPE(
						values + 12) = VAL_TYPE(values + 13) =
						VAL_TYPE(values + 14) = VAL_TYPE(values + 15) =
								VAL_TYPE(values + 16) = VAL_TYPE(values + 17) =
										VAL_TYPE(values + 20) = DB1_STR;

		SET_NULL_FLAG(values, i, DIALOG_TABLE_COL_NO - 6, 0);
		VAL_TYPE(values + 18) = DB1_INT;
		VAL_TYPE(values + 19) = DB1_STR;
		VAL_TYPE(values + 21) = DB1_STR;
		VAL_TYPE(values + 22) = DB1_INT;

		VAL_INT(values) = cell->h_entry;
		VAL_INT(values + 1) = cell->h_id;
		VAL_INT(values + 9) = cell->start_ts;
		VAL_INT(values + 10) = cell->state;
		VAL_INT(values + 11) = (unsigned int)((unsigned int)time(0)
											  + cell->tl.timeout - get_ticks());

		SET_STR_VALUE(values + 2, cell->callid);
		SET_STR_VALUE(values + 3, cell->from_uri);
		SET_STR_VALUE(values + 4, cell->tag[DLG_CALLER_LEG]);
		SET_STR_VALUE(values + 5, cell->to_uri);
		SET_STR_VALUE(values + 6, cell->tag[DLG_CALLEE_LEG]);
		SET_PROPER_NULL_FLAG(cell->tag[DLG_CALLEE_LEG], values, 6);


		if(cell->bind_addr[DLG_CALLER_LEG]) {
			LM_DBG("caller sock_info is %.*s\n",
					cell->bind_addr[DLG_CALLER_LEG]->sock_str.len,
					cell->bind_addr[DLG_CALLER_LEG]->sock_str.s);
			SET_STR_VALUE(
					values + 7, cell->bind_addr[DLG_CALLER_LEG]->sock_str);
		} else {
			LM_DBG("no caller sock_info\n");
			SET_STR_VALUE(values + 7, sempty);
		}
		if(cell->bind_addr[DLG_CALLEE_LEG]) {
			LM_DBG("callee sock_info is %.*s\n",
					cell->bind_addr[DLG_CALLEE_LEG]->sock_str.len,
					cell->bind_addr[DLG_CALLEE_LEG]->sock_str.s);
			SET_STR_VALUE(
					values + 8, cell->bind_addr[DLG_CALLEE_LEG]->sock_str);
		} else {
			LM_DBG("no callee sock_info\n");
			SET_STR_VALUE(values + 8, sempty);
		}

		SET_STR_VALUE(values + 12, cell->cseq[DLG_CALLER_LEG]);
		SET_STR_VALUE(values + 13, cell->cseq[DLG_CALLEE_LEG]);
		SET_STR_VALUE(values + 14, cell->contact[DLG_CALLER_LEG]);
		SET_STR_VALUE(values + 15, cell->contact[DLG_CALLEE_LEG]);
		SET_STR_VALUE(values + 16, cell->route_set[DLG_CALLER_LEG]);
		SET_STR_VALUE(values + 17, cell->route_set[DLG_CALLEE_LEG]);

		SET_PROPER_NULL_FLAG(cell->contact[DLG_CALLER_LEG], values, 14);
		SET_PROPER_NULL_FLAG(cell->contact[DLG_CALLEE_LEG], values, 15);
		SET_PROPER_NULL_FLAG(cell->route_set[DLG_CALLER_LEG], values, 16);
		SET_PROPER_NULL_FLAG(cell->route_set[DLG_CALLEE_LEG], values, 17);

		VAL_NULL(values + 18) = 0;
		VAL_INT(values + 18) = cell->sflags;

		SET_STR_VALUE(values + 19, cell->toroute_name);
		SET_PROPER_NULL_FLAG(cell->toroute_name, values, 19);
		SET_STR_VALUE(values + 20, cell->req_uri);
		SET_PROPER_NULL_FLAG(cell->req_uri, values, 20);

		dlg_profiles_to_json(cell, &jdoc);
		if(jdoc.buf.s != NULL) {
			SET_STR_VALUE(values + 21, jdoc.buf);
			SET_PROPER_NULL_FLAG(jdoc.buf, values, 21);
		} else {
			VAL_NULL(values + 21) = 1;
		}

		VAL_NULL(values + 22) = 0;
		VAL_INT(values + 22) = cell->iflags;

		if((dialog_dbf.insert(
				   dialog_db_handle, insert_keys, values, DIALOG_TABLE_COL_NO))
				!= 0) {
			LM_ERR("could not add another dialog to db\n");
			goto error;
		}
		cell->dflags &= ~(DLG_FLAG_NEW | DLG_FLAG_CHANGED);

	} else if((cell->dflags & DLG_FLAG_CHANGED) != 0) {
		/* save only dialog's state and timeout */
		VAL_TYPE(values) = VAL_TYPE(values + 1) = VAL_TYPE(values + 10) =
				VAL_TYPE(values + 11) = DB1_INT;

		VAL_TYPE(values + 12) = VAL_TYPE(values + 13) = DB1_STR;
		VAL_TYPE(values + 14) = VAL_TYPE(values + 15) = DB1_STR;

		VAL_INT(values) = cell->h_entry;
		VAL_INT(values + 1) = cell->h_id;
		VAL_INT(values + 10) = cell->state;
		VAL_INT(values + 11) = (unsigned int)((unsigned int)time(0)
											  + cell->tl.timeout - get_ticks());

		SET_STR_VALUE(values + 12, cell->cseq[DLG_CALLER_LEG]);
		SET_STR_VALUE(values + 13, cell->cseq[DLG_CALLEE_LEG]);
		SET_STR_VALUE(values + 14, cell->contact[DLG_CALLER_LEG]);
		SET_STR_VALUE(values + 15, cell->contact[DLG_CALLEE_LEG]);


		VAL_NULL(values) = VAL_NULL(values + 1) = VAL_NULL(values + 10) =
				VAL_NULL(values + 11) = VAL_NULL(values + 12) =
						VAL_NULL(values + 13) = VAL_NULL(values + 14) =
								VAL_NULL(values + 15) = 0;

		if((dialog_dbf.update(dialog_db_handle, (insert_keys), 0, (values),
				   (insert_keys + 10), (values + 10), 2, 6))
				!= 0) {
			LM_ERR("could not update database info\n");
			goto error;
		}
		cell->dflags &= ~(DLG_FLAG_CHANGED);
	} else {
		return 0;
	}

	if(jdoc.buf.s != NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);

	return 0;

error:
	if(jdoc.buf.s != NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);
	return -1;
}

int update_dialog_dbinfo(struct dlg_cell *cell)
{
	/* lock the entry */
	dlg_lock(d_table, &d_table->entries[cell->h_entry]);
	if(update_dialog_dbinfo_unsafe(cell) != 0) {
		dlg_unlock(d_table, &d_table->entries[cell->h_entry]);
		return -1;
	}
	dlg_unlock(d_table, &d_table->entries[cell->h_entry]);
	return 0;
}

void dialog_update_db(unsigned int ticks, void *param)
{
	int i;
	struct dlg_cell *cell;

	LM_DBG("saving current_info \n");

	for(i = 0; i < d_table->size; i++) {
		/* lock the slot */
		dlg_lock(d_table, &d_table->entries[i]);
		for(cell = d_table->entries[i].first; cell != NULL; cell = cell->next) {
			/* if update fails for one dlg, still do it for the next ones */
			update_dialog_dbinfo_unsafe(cell);
		}
		dlg_unlock(d_table, &d_table->entries[i]);
	}
	return;
}

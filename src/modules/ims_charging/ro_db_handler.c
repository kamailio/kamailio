#include "ro_db_handler.h"
#include "ims_charging_mod.h"
#include "dialog.h"
#include "../../lib/srdb1/db.h"

extern ims_dlg_api_t dlgb;
extern struct cdp_binds cdpb;
extern int ro_db_mode;

static db1_con_t *ro_db_handle = 0; /* database connection handle */
static db_func_t ro_dbf;

str ro_session_table_name = str_init(RO_SESSION_TABLE_NAME);
str id_column = str_init(ID_COL);
str h_entry_column = str_init(HASH_ENTRY_COL);
str h_id_column = str_init(HASH_ID_COL);
str session_id_column = str_init(SESSION_ID_COL);
str dlg_h_entry_column = str_init(DLG_HASH_ENTRY_COL);
str dlg_h_id_column = str_init(DLG_HASH_ID_COL);
str direction_column = str_init(DIRECTION_COL);
str asserted_column = str_init(ASSERTED_ID_COL);
str callee_column = str_init(CALLEE_COL);
str start_time_col = str_init(START_TIME_COL);
str last_event_ts_column = str_init(LAST_EVENT_TS_COL);
str reserved_sec_column = str_init(RESERVED_SECS_COL);
str valid_for_column = str_init(VALID_FOR_COL);
str state_column = str_init(STATE_COL);
str incoming_trunk_id_column = str_init(INCOMING_TRUNK_ID_COL);
str outgoing_trunk_id_column = str_init(OUTGOING_TRUNK_ID_COL);
str rating_group_column = str_init(RATING_GROUP_COL);
str service_identifier_column = str_init(SERVICE_IDENTIFIER_COL);
str auth_app_id_column = str_init(AUTH_APP_ID_COL);
str auth_session_type_column = str_init(AUTH_SESSION_TYPE_COL);
str pani_column = str_init(PANI_COL);
str mac_column = str_init(MAC_COL);
str app_provided_party_column = str_init(APP_PROVIDED_PARTY_COL);
str is_final_allocation_column = str_init(IS_FINAL_ALLOCATION_COL);
str origin_host_column = str_init(ORIGIN_HOST_COL);

typedef enum ro_session_field_idx
{
	ID_COL_IDX = 0,
	HASH_ENTRY_COL_IDX,
	HASH_ID_COL_IDX,
	SESSION_ID_COL_IDX,
	DLG_HASH_ENTRY_COL_IDX,
	DLG_HASH_ID_COL_IDX,
	DIRECTION_COL_IDX,
	ASSERTED_ID_COL_IDX,
	CALLEE_COL_IDX,
	START_TIME_COL_IDX,
	LAST_EVENT_TS_COL_IDX,
	RESERVED_SECS_COL_IDX,
	VALID_FOR_COL_IDX,
	STATE_COL_IDX,
	INCOMING_TRUNK_ID_COL_IDX,
	OUTGOING_TRUNK_ID_COL_IDX,
	RATING_GROUP_COL_IDX,
	SERVICE_IDENTIFIER_COL_IDX,
	AUTH_APP_ID_COL_IDX,
	AUTH_SESSION_TYPE_COL_IDX,
	PANI_COL_IDX,
	MAC_COL_IDX,
	APP_PROVIDED_PARTY_COL_IDX,
	IS_FINAL_ALLOCATION_COL_IDX,
	ORIGIN_HOST_COL_IDX

} ro_session_field_idx_t;

#define GET_FIELD_IDX(_val, _idx) (_val + _idx)

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
					unref_ro_session(session, 1, 0);              \
				goto next_ro_session;                             \
			} else {                                              \
				(_res).s = 0;                                     \
				(_res).len = 0;                                   \
			}                                                     \
		} else {                                                  \
			(_res).s = VAL_STR((_values) + (_index)).s;           \
			(_res).len = strlen(VAL_STR((_values) + (_index)).s); \
		}                                                         \
	} while(0);

int init_ro_db(const str *db_url, int dlg_hash_size, int db_update_period,
		int fetch_num_rows)
{
	/* Find a database module */
	if(db_bind_mod(db_url, &ro_dbf) < 0) {
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if(ro_connect_db(db_url) != 0) {
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	if(db_check_table_version(
			   &ro_dbf, ro_db_handle, &ro_session_table_name, RO_TABLE_VERSION)
			< 0) {
		DB_TABLE_VERSION_ERROR(ro_session_table_name);
		goto dberror;
	}

	if((load_ro_info_from_db(dlg_hash_size, fetch_num_rows)) != 0) {
		LM_ERR("unable to load the ro session data\n");
		goto dberror;
	}

	ro_dbf.close(ro_db_handle);
	ro_db_handle = 0;

	return 0;

dberror:
	ro_dbf.close(ro_db_handle);
	ro_db_handle = 0;
	return -1;
}

static int use_ro_table(void)
{
	if(!ro_db_handle) {
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if(ro_dbf.use_table(ro_db_handle, &ro_session_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}

static int select_entire_ro_session_table(db1_res_t **res, int fetch_num_rows)
{
	db_key_t query_cols[RO_SESSION_TABLE_COL_NUM] = {&id_column,
			&h_entry_column, &h_id_column, &session_id_column,
			&dlg_h_entry_column, &dlg_h_id_column, &direction_column,
			&asserted_column, &callee_column, &start_time_col,
			&last_event_ts_column, &reserved_sec_column, &valid_for_column,
			&state_column, &incoming_trunk_id_column, &outgoing_trunk_id_column,
			&rating_group_column, &service_identifier_column,
			&auth_app_id_column, &auth_session_type_column, &pani_column,
			&mac_column, &app_provided_party_column,
			&is_final_allocation_column, &origin_host_column};

	if(use_ro_table() != 0) {
		return -1;
	}

	/* select the whole table and all the columns */
	if(DB_CAPABILITY(ro_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
		if(ro_dbf.query(ro_db_handle, 0, 0, 0, query_cols, 0,
				   RO_SESSION_TABLE_COL_NUM, 0, 0)
				< 0) {
			LM_ERR("Error while querying (fetch) database\n");
			return -1;
		}
		if(ro_dbf.fetch_result(ro_db_handle, res, fetch_num_rows) < 0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if(ro_dbf.query(ro_db_handle, 0, 0, 0, query_cols, 0,
				   RO_SESSION_TABLE_COL_NUM, 0, res)
				< 0) {
			LM_ERR("Error while querying database\n");
			return -1;
		}
	}

	return 0;
}

static int get_timer_value(
		struct ro_session *session, time_t time_since_last_event)
{
	int timer_value;
	if(session->reserved_secs < (session->valid_for - time_since_last_event)) {
		if(session->reserved_secs > session->ro_timer_buffer) {
			timer_value =
					session->reserved_secs - time_since_last_event
					- (session->is_final_allocation ? 0
													: session->ro_timer_buffer);
		} else {
			timer_value = session->reserved_secs - time_since_last_event;
		}
	} else {
		if(session->valid_for > session->ro_timer_buffer) {
			timer_value =
					session->valid_for - time_since_last_event
					- (session->is_final_allocation ? 0
													: session->ro_timer_buffer);
		} else {
			timer_value = session->valid_for - time_since_last_event;
		}
	}

	/* let cdp connections settle a bit after startup */
	if(timer_value < 5) {
		timer_value = 5;
	}

	return timer_value;
}

int load_ro_info_from_db(int hash_size, int fetch_num_rows)
{
	db1_res_t *res;
	db_val_t *values;
	db_row_t *rows;
	struct dlg_cell *dlg = NULL;
	struct ro_session *session = 0;
	int i, nr_rows, dir, active_rating_group, active_service_identifier,
			reservation_units, dlg_h_entry, dlg_h_id;
	str session_id, asserted_identity, called_asserted_identity,
			incoming_trunk_id, outgoing_trunk_id, pani, app_provided_party, mac,
			origin_host;
	time_t now = get_current_time_micro();
	time_t time_since_last_event;
	AAASession *auth = 0;
	unsigned int next_id;

	res = 0;
	if((nr_rows = select_entire_ro_session_table(&res, fetch_num_rows)) < 0)
		goto end;

	nr_rows = RES_ROW_N(res);

	LM_ALERT("the database has information about %i ro sessions\n", nr_rows);

	rows = RES_ROWS(res);

	do {
		/* for every row---ro session */
		for(i = 0; i < nr_rows; i++) {
			values = ROW_VALUES(rows + i);

			if(VAL_NULL(GET_FIELD_IDX(values, HASH_ID_COL_IDX))
					|| VAL_NULL(GET_FIELD_IDX(values, HASH_ENTRY_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						h_entry_column.len, h_entry_column.s, h_id_column.len,
						h_id_column.s);
				continue;
			}

			if(VAL_NULL(GET_FIELD_IDX(values, SESSION_ID_COL_IDX))
					|| VAL_NULL(GET_FIELD_IDX(values, DIRECTION_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						session_id_column.len, session_id_column.s,
						direction_column.len, direction_column.s);
				continue;
			}

			if(VAL_NULL(GET_FIELD_IDX(values, DLG_HASH_ENTRY_COL_IDX))
					|| VAL_NULL(GET_FIELD_IDX(values, DLG_HASH_ID_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						dlg_h_entry_column.len, dlg_h_entry_column.s,
						dlg_h_id_column.len, dlg_h_id_column.s);
				continue;
			}

			if(VAL_NULL(GET_FIELD_IDX(values, ASSERTED_ID_COL_IDX))
					|| VAL_NULL(GET_FIELD_IDX(values, CALLEE_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						asserted_column.len, asserted_column.s,
						callee_column.len, callee_column.s);
				continue;
			}

			if(VAL_NULL(GET_FIELD_IDX(values, AUTH_APP_ID_COL_IDX))
					|| VAL_NULL(
							GET_FIELD_IDX(values, AUTH_SESSION_TYPE_COL_IDX))) {
				LM_ERR("columns %.*s or/and %.*s cannot be null -> skipping\n",
						auth_app_id_column.len, auth_app_id_column.s,
						auth_session_type_column.len,
						auth_session_type_column.s);
				continue;
			}

			if(VAL_NULL(GET_FIELD_IDX(values, IS_FINAL_ALLOCATION_COL_IDX))) {
				LM_ERR("columns %.*s cannot be null -> skipping\n",
						is_final_allocation_column.len,
						is_final_allocation_column.s);
				continue;
			}

			dlg_h_entry =
					VAL_INT(GET_FIELD_IDX(values, DLG_HASH_ENTRY_COL_IDX));
			dlg_h_id = VAL_INT(GET_FIELD_IDX(values, DLG_HASH_ID_COL_IDX));

			dlg = dlgb.get_dlg_hash(dlg_h_entry, dlg_h_id);
			if(!dlg) {
				LM_ERR("cannot lookup dialog -> skipping\n");
				continue;
			}

			/*restore the ro session info*/
			dir = VAL_INT(GET_FIELD_IDX(values, DIRECTION_COL_IDX));
			GET_STR_VALUE(session_id, values, SESSION_ID_COL_IDX, 1, 0);
			GET_STR_VALUE(asserted_identity, values, ASSERTED_ID_COL_IDX, 1, 0);
			GET_STR_VALUE(
					called_asserted_identity, values, CALLEE_COL_IDX, 1, 0);
			GET_STR_VALUE(
					incoming_trunk_id, values, INCOMING_TRUNK_ID_COL_IDX, 0, 0);
			GET_STR_VALUE(
					outgoing_trunk_id, values, OUTGOING_TRUNK_ID_COL_IDX, 0, 0);
			GET_STR_VALUE(pani, values, PANI_COL_IDX, 0, 0);
			GET_STR_VALUE(mac, values, MAC_COL_IDX, 0, 0);
			GET_STR_VALUE(app_provided_party, values,
					APP_PROVIDED_PARTY_COL_IDX, 0, 0);
			GET_STR_VALUE(origin_host, values, ORIGIN_HOST_COL_IDX, 0, 0);
			active_rating_group =
					VAL_INT(GET_FIELD_IDX(values, RATING_GROUP_COL_IDX));
			active_service_identifier =
					VAL_INT(GET_FIELD_IDX(values, SERVICE_IDENTIFIER_COL_IDX));
			reservation_units =
					VAL_INT(GET_FIELD_IDX(values, RESERVED_SECS_COL_IDX));

			session = build_new_ro_session(dir, 0, 0, &session_id, &dlg->callid,
					&asserted_identity, &called_asserted_identity, &mac,
					dlg_h_entry, dlg_h_id, reservation_units, 0,
					active_rating_group, active_service_identifier,
					&incoming_trunk_id, &outgoing_trunk_id, &pani,
					&app_provided_party, ro_timer_buffer);

			if(!session) {
				LM_ERR("Couldn't restore Ro Session - this is BAD!\n");
				dlgb.release_dlg(dlg);
				goto error;
			}

			if(session->h_entry
					!= VAL_INT(GET_FIELD_IDX(values, HASH_ENTRY_COL_IDX))) {
				LM_ERR("inconsistent hash data in the ro session database: "
					   "you may have restarted Kamailio using a different "
					   "hash_size: please erase %.*s database and restart\n",
						ro_session_table_name.len, ro_session_table_name.s);
				shm_free(session);
				dlgb.release_dlg(dlg);
				goto error;
			}

			session->ro_session_id.s = (char *)shm_malloc(session_id.len);
			if(!session->ro_session_id.s) {
				LM_ERR("no more shm mem\n");
				dlgb.release_dlg(dlg);
				goto error;
			}
			session->ro_session_id.len = session_id.len;
			memcpy(session->ro_session_id.s, session_id.s, session_id.len);

			if(origin_host.s && origin_host.len > 0) {
				session->origin_host.s = (char *)shm_malloc(origin_host.len);
				if(!session->origin_host.s) {
					LM_ERR("no more shm mem\n");
					goto error;
				}

				session->origin_host.len = origin_host.len;
				memcpy(session->origin_host.s, origin_host.s, origin_host.len);
			}

			session->active = VAL_INT(GET_FIELD_IDX(values, STATE_COL_IDX));
			session->last_event_timestamp =
					VAL_TIME(GET_FIELD_IDX(values, LAST_EVENT_TS_COL_IDX))
					* 1000000;
			session->start_time =
					VAL_TIME(GET_FIELD_IDX(values, START_TIME_COL_IDX))
					* 1000000;
			session->valid_for =
					VAL_INT(GET_FIELD_IDX(values, VALID_FOR_COL_IDX));
			session->reserved_secs =
					VAL_INT(GET_FIELD_IDX(values, RESERVED_SECS_COL_IDX));
			session->is_final_allocation =
					VAL_INT(GET_FIELD_IDX(values, IS_FINAL_ALLOCATION_COL_IDX));
			session->auth_appid =
					VAL_INT(GET_FIELD_IDX(values, AUTH_APP_ID_COL_IDX));
			session->auth_session_type =
					VAL_INT(GET_FIELD_IDX(values, AUTH_SESSION_TYPE_COL_IDX));
			session->flags |= RO_SESSION_FLAG_INSERTED;

			link_ro_session(session, 0);
			session->h_id = VAL_INT(GET_FIELD_IDX(values, HASH_ID_COL_IDX));
			next_id = ro_session_table->entries[session->h_entry].next_id;
			ro_session_table->entries[session->h_entry].next_id =
					(next_id < session->h_id) ? (session->h_id + 1) : next_id;

			if(dlgb.register_dlgcb(dlg,
					   DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED
							   | DLGCB_CONFIRMED,
					   dlg_callback_received, (void *)session, NULL)
					!= 0) {
				LM_CRIT("cannot register callback for dialog confirmation\n");
				dlgb.release_dlg(dlg);
				goto error;
			}

			auth = cdpb.AAAMakeSession(session->auth_appid,
					session->auth_session_type, session->ro_session_id);
			if(!auth) {
				LM_ERR("Could not create AAA session\n");
				dlgb.release_dlg(dlg);
				continue;
			}
			auth->u.cc_acc.state = ACC_CC_ST_OPEN;
			cdpb.AAASessionsUnlock(auth->hash);

			if(session->active) {
				now = get_current_time_micro();
				time_since_last_event =
						(now - session->last_event_timestamp) / 1000000;
				session->event_type = answered;
				session->billed = session->start_time
								  - session->last_event_timestamp / 1000000;

				int ret = 0;
				ret = insert_ro_timer(&session->ro_tl,
						get_timer_value(session, time_since_last_event));
				if(ret != 0) {
					LM_CRIT("unable to insert timer for Ro Session [%.*s]\n",
							session->ro_session_id.len,
							session->ro_session_id.s);
				} else {
					ref_ro_session(session, 1, 0);
				}
			}

			dlgb.release_dlg(dlg);

		next_ro_session:;
		}

		/* any more data to be fetched ?*/
		if(DB_CAPABILITY(ro_dbf, DB_CAP_FETCH) && (fetch_num_rows > 0)) {
			if(ro_dbf.fetch_result(ro_db_handle, &res, fetch_num_rows) < 0) {
				LM_ERR("re-fetching rows failed\n");
				goto error;
			}
			nr_rows = RES_ROW_N(res);
			rows = RES_ROWS(res);
		} else {
			nr_rows = 0;
		}

	} while(nr_rows > 0);

	if(ro_db_mode == DB_MODE_SHUTDOWN) {
		if(ro_dbf.delete(ro_db_handle, 0, 0, 0, 0) < 0) {
			LM_ERR("failed to clear ro session table\n");
			goto error;
		}
	}

end:
	ro_dbf.free_result(ro_db_handle, res);
	return 0;
error:
	ro_dbf.free_result(ro_db_handle, res);
	return -1;
}

int ro_connect_db(const str *db_url)
{
	if(ro_db_handle) {
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}
	if((ro_db_handle = ro_dbf.init(db_url)) == 0)
		return -1;

	/* use default table */
	if(ro_dbf.use_table(ro_db_handle, &ro_session_table_name) != 0) {
		LM_ERR("Error in use table for table name [%.*s]\n",
				ro_session_table_name.len, ro_session_table_name.s);
		return -1;
	}

	return 0;
}

void db_set_int_val(db_val_t *values, int index, int val)
{
	VAL_TYPE(GET_FIELD_IDX(values, index)) = DB1_INT;
	VAL_NULL(GET_FIELD_IDX(values, index)) = 0;
	VAL_INT(GET_FIELD_IDX(values, index)) = val;
}

void db_set_str_val(db_val_t *values, int index, str *val)
{
	VAL_TYPE(GET_FIELD_IDX(values, index)) = DB1_STR;
	VAL_NULL(GET_FIELD_IDX(values, index)) = 0;
	SET_STR_VALUE(GET_FIELD_IDX(values, index), *val);
}

void db_set_datetime_val(db_val_t *values, int index, time_t val)
{
	VAL_TYPE(GET_FIELD_IDX(values, index)) = DB1_DATETIME;
	VAL_NULL(GET_FIELD_IDX(values, index)) = 0;
	VAL_TIME(GET_FIELD_IDX(values, index)) = val;
}

int update_ro_dbinfo_unsafe(struct ro_session *ro_session)
{
	/* We check for RO_SESSION_FLAG_DELETED first. If DB_MODE_DELAYED is used,
	it might not have RO_SESSION_FLAG_INSERTED flag set which would cause the
	record to be inserted instead of being deleted. */
	if((ro_session->flags & RO_SESSION_FLAG_DELETED) != 0) {
		db_val_t values[3];
		db_key_t match_keys[3] = {
				&h_entry_column, &h_id_column, &session_id_column};

		db_set_int_val(values, HASH_ENTRY_COL_IDX - 1, ro_session->h_entry);
		db_set_int_val(values, HASH_ID_COL_IDX - 1, ro_session->h_id);
		db_set_str_val(
				values, SESSION_ID_COL_IDX - 1, &ro_session->ro_session_id);

		if(ro_dbf.delete(ro_db_handle, match_keys, 0, values, 3) < 0) {
			LM_ERR("failed to delete ro session database information... "
				   "continuing\n");
			return -1;
		}
	} else if((ro_session->flags & RO_SESSION_FLAG_NEW) != 0
			  && (ro_session->flags & RO_SESSION_FLAG_INSERTED) == 0) {

		db_val_t values[RO_SESSION_TABLE_COL_NUM];
		db_key_t insert_keys[RO_SESSION_TABLE_COL_NUM] = {&id_column,
				&h_entry_column, &h_id_column, &session_id_column,
				&dlg_h_entry_column, &dlg_h_id_column, &direction_column,
				&asserted_column, &callee_column, &start_time_col,
				&last_event_ts_column, &reserved_sec_column, &valid_for_column,
				&state_column, &incoming_trunk_id_column,
				&outgoing_trunk_id_column, &rating_group_column,
				&service_identifier_column, &auth_app_id_column,
				&auth_session_type_column, &pani_column, &mac_column,
				&app_provided_party_column, &is_final_allocation_column,
				&origin_host_column};

		VAL_TYPE(GET_FIELD_IDX(values, ID_COL_IDX)) = DB1_INT;
		VAL_NULL(GET_FIELD_IDX(values, ID_COL_IDX)) = 1;

		db_set_int_val(values, HASH_ENTRY_COL_IDX, ro_session->h_entry);
		db_set_int_val(values, HASH_ID_COL_IDX, ro_session->h_id);
		db_set_str_val(values, SESSION_ID_COL_IDX, &ro_session->ro_session_id);
		db_set_int_val(values, DLG_HASH_ENTRY_COL_IDX, ro_session->dlg_h_entry);
		db_set_int_val(values, DLG_HASH_ID_COL_IDX, ro_session->dlg_h_id);
		db_set_int_val(values, DIRECTION_COL_IDX, ro_session->direction);
		db_set_str_val(
				values, ASSERTED_ID_COL_IDX, &ro_session->asserted_identity);
		db_set_str_val(
				values, CALLEE_COL_IDX, &ro_session->called_asserted_identity);
		db_set_datetime_val(
				values, START_TIME_COL_IDX, ro_session->start_time / 1000000);
		db_set_datetime_val(values, LAST_EVENT_TS_COL_IDX,
				ro_session->last_event_timestamp / 1000000);
		db_set_int_val(
				values, RESERVED_SECS_COL_IDX, ro_session->reserved_secs);
		db_set_int_val(values, VALID_FOR_COL_IDX, ro_session->valid_for);
		db_set_int_val(values, STATE_COL_IDX, ro_session->active);
		db_set_str_val(values, INCOMING_TRUNK_ID_COL_IDX,
				&ro_session->incoming_trunk_id);
		db_set_str_val(values, OUTGOING_TRUNK_ID_COL_IDX,
				&ro_session->outgoing_trunk_id);
		db_set_int_val(values, RATING_GROUP_COL_IDX, ro_session->rating_group);
		db_set_int_val(values, SERVICE_IDENTIFIER_COL_IDX,
				ro_session->service_identifier);
		db_set_int_val(values, AUTH_APP_ID_COL_IDX, ro_session->auth_appid);
		db_set_int_val(values, AUTH_SESSION_TYPE_COL_IDX,
				ro_session->auth_session_type);
		db_set_str_val(values, PANI_COL_IDX, &ro_session->pani);
		db_set_str_val(values, MAC_COL_IDX, &ro_session->mac);
		db_set_str_val(values, APP_PROVIDED_PARTY_COL_IDX,
				&ro_session->app_provided_party);
		db_set_int_val(values, IS_FINAL_ALLOCATION_COL_IDX,
				ro_session->is_final_allocation);
		db_set_str_val(values, ORIGIN_HOST_COL_IDX, &ro_session->origin_host);


		LM_DBG("Inserting ro_session into database\n");
		if((ro_dbf.insert(
				   ro_db_handle, insert_keys, values, RO_SESSION_TABLE_COL_NUM))
				!= 0) {
			LM_ERR("could not add new Ro session into DB.... continuing\n");
			goto error;
		}
		ro_session->flags &= ~(RO_SESSION_FLAG_NEW | RO_SESSION_FLAG_CHANGED);
		ro_session->flags |= RO_SESSION_FLAG_INSERTED;

	} else if((ro_session->flags & RO_SESSION_FLAG_CHANGED) != 0
			  && (ro_session->flags & RO_SESSION_FLAG_INSERTED) != 0) {

		db_val_t values[RO_SESSION_TABLE_COL_NUM - 1];
		db_key_t update_keys[RO_SESSION_TABLE_COL_NUM - 1] = {&h_entry_column,
				&h_id_column, &session_id_column, &dlg_h_entry_column,
				&dlg_h_id_column, &direction_column, &asserted_column,
				&callee_column, &start_time_col, &last_event_ts_column,
				&reserved_sec_column, &valid_for_column, &state_column,
				&incoming_trunk_id_column, &outgoing_trunk_id_column,
				&rating_group_column, &service_identifier_column,
				&auth_app_id_column, &auth_session_type_column, &pani_column,
				&mac_column, &app_provided_party_column,
				&is_final_allocation_column, &origin_host_column};

		db_set_int_val(values, HASH_ENTRY_COL_IDX - 1, ro_session->h_entry);
		db_set_int_val(values, HASH_ID_COL_IDX - 1, ro_session->h_id);
		db_set_str_val(
				values, SESSION_ID_COL_IDX - 1, &ro_session->ro_session_id);
		db_set_int_val(
				values, DLG_HASH_ENTRY_COL_IDX - 1, ro_session->dlg_h_entry);
		db_set_int_val(values, DLG_HASH_ID_COL_IDX - 1, ro_session->dlg_h_id);
		db_set_int_val(values, DIRECTION_COL_IDX - 1, ro_session->direction);
		db_set_str_val(values, ASSERTED_ID_COL_IDX - 1,
				&ro_session->asserted_identity);
		db_set_str_val(values, CALLEE_COL_IDX - 1,
				&ro_session->called_asserted_identity);
		db_set_datetime_val(values, START_TIME_COL_IDX - 1,
				ro_session->start_time / 1000000);
		db_set_datetime_val(values, LAST_EVENT_TS_COL_IDX - 1,
				ro_session->last_event_timestamp / 1000000);
		db_set_int_val(
				values, RESERVED_SECS_COL_IDX - 1, ro_session->reserved_secs);
		db_set_int_val(values, VALID_FOR_COL_IDX - 1, ro_session->valid_for);
		db_set_int_val(values, STATE_COL_IDX - 1, ro_session->active);
		db_set_str_val(values, INCOMING_TRUNK_ID_COL_IDX - 1,
				&ro_session->incoming_trunk_id);
		db_set_str_val(values, OUTGOING_TRUNK_ID_COL_IDX - 1,
				&ro_session->outgoing_trunk_id);
		db_set_int_val(
				values, RATING_GROUP_COL_IDX - 1, ro_session->rating_group);
		db_set_int_val(values, SERVICE_IDENTIFIER_COL_IDX - 1,
				ro_session->service_identifier);
		db_set_int_val(values, AUTH_APP_ID_COL_IDX - 1, ro_session->auth_appid);
		db_set_int_val(values, AUTH_SESSION_TYPE_COL_IDX - 1,
				ro_session->auth_session_type);
		db_set_str_val(values, PANI_COL_IDX - 1, &ro_session->pani);
		db_set_str_val(values, MAC_COL_IDX - 1, &ro_session->mac);
		db_set_str_val(values, APP_PROVIDED_PARTY_COL_IDX - 1,
				&ro_session->app_provided_party);
		db_set_int_val(values, IS_FINAL_ALLOCATION_COL_IDX - 1,
				ro_session->is_final_allocation);
		db_set_str_val(
				values, ORIGIN_HOST_COL_IDX - 1, &ro_session->origin_host);

		LM_DBG("Updating ro_session in database\n");
		if((ro_dbf.update(ro_db_handle, update_keys /*match*/, 0 /*match*/,
				   values /*match*/, update_keys /*update*/, values /*update*/,
				   3 /*match*/, 24 /*update*/))
				!= 0) {
			LM_ERR("could not update Ro session information in DB... "
				   "continuing\n");
			goto error;
		}
		ro_session->flags &= ~RO_SESSION_FLAG_CHANGED;
	} else {
		LM_WARN("Asked to update Ro session in strange state [%d]\n",
				ro_session->flags);
	}

	return 0;

error:
	return -1;
}

int update_ro_dbinfo(struct ro_session *ro_session)
{
	struct ro_session_entry entry;
	/* lock the entry */
	entry = (ro_session_table->entries)[ro_session->h_entry];
	ro_session_lock(ro_session_table, &entry);
	if(update_ro_dbinfo_unsafe(ro_session) != 0) {
		LM_ERR("failed to update ro_session in DB\n");
		ro_session_unlock(ro_session_table, &entry);
		return -1;
	}
	ro_session_unlock(ro_session_table, &entry);
	return 0;
}

void ro_update_db(unsigned int ticks, void *param)
{
	int index;
	struct ro_session_entry ro_session_entry;
	struct ro_session *ro_session;

	for(index = 0; index < ro_session_table->size; index++) {
		/* lock the whole ro_session_entry */
		ro_session_entry = (ro_session_table->entries)[index];
		ro_session_lock(ro_session_table, &ro_session_entry);

		for(ro_session = ro_session_entry.first; ro_session != NULL;
				ro_session = ro_session->next) {
			if(update_ro_dbinfo_unsafe(ro_session) != 0) {
				LM_ERR("failed to update ro_session in DB\n");
			}
		}
		ro_session_unlock(ro_session_table, &ro_session_entry);
	}

	return;
}

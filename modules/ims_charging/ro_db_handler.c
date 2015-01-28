#include "ro_db_handler.h"
#include "../../lib/srdb1/db.h"

static db1_con_t* ro_db_handle = 0; /* database connection handle */
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

typedef enum ro_session_field_idx {
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
    SERVICE_IDENTIFIER_COL_IDX
	    
} ro_session_field_idx_t;

#define GET_FIELD_IDX(_val, _idx)\
		(_val + _idx)

#define SET_STR_VALUE(_val, _str)\
	do{\
			VAL_STR((_val)).s = (_str).s;\
			VAL_STR((_val)).len = (_str).len;\
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

int init_ro_db(const str *db_url, int dlg_hash_size, int db_update_period, int fetch_num_rows) {
    /* Find a database module */
    if (db_bind_mod(db_url, &ro_dbf) < 0) {
	LM_ERR("Unable to bind to a database driver\n");
	return -1;
    }

    if (ro_connect_db(db_url) != 0) {
	LM_ERR("unable to connect to the database\n");
	return -1;
    }

    if (db_check_table_version(&ro_dbf, ro_db_handle, &ro_session_table_name, RO_TABLE_VERSION) < 0) {
	LM_ERR("error during dialog-table version check.\n");
	return -1;
    }

    //	if( (dlg_db_mode==DB_MODE_DELAYED) && 
    //	(register_timer( dialog_update_db, 0, db_update_period)<0 )) {
    //		LM_ERR("failed to register update db\n");
    //		return -1;
    //	}

    if ((load_ro_info_from_db(dlg_hash_size, fetch_num_rows)) != 0) {
	LM_ERR("unable to load the dialog data\n");
	return -1;
    }

    ro_dbf.close(ro_db_handle);
    ro_db_handle = 0;

    return 0;
}

int load_ro_info_from_db(int hash_size, int fetch_num_rows) {
    LM_WARN("not supported yet");
    return 0;
}

int ro_connect_db(const str *db_url) {
    if (ro_db_handle) {
	LM_CRIT("BUG - db connection found already open\n");
	return -1;
    }
    if ((ro_db_handle = ro_dbf.init(db_url)) == 0)
	return -1;

    /* use default table */
    if (ro_dbf.use_table(ro_db_handle, &ro_session_table_name) != 0) {
	LM_ERR("Error in use table for table name [%.*s]\n", ro_session_table_name.len, ro_session_table_name.s);
	return -1;
    }

    return 0;
}

void db_set_int_val(db_val_t* values, int index, int val) {
    VAL_TYPE(GET_FIELD_IDX(values, index)) = DB1_INT;
    VAL_NULL(GET_FIELD_IDX(values, index)) = 0;
    VAL_INT(GET_FIELD_IDX(values, index)) = val;
}

void db_set_str_val(db_val_t* values, int index, str* val) {
    VAL_TYPE(GET_FIELD_IDX(values, index)) = DB1_STR;
    VAL_NULL(GET_FIELD_IDX(values, index)) = 0;
    SET_STR_VALUE(GET_FIELD_IDX(values, index), *val);
}

void db_set_datetime_val(db_val_t* values, int index, time_t val) {
    VAL_TYPE(GET_FIELD_IDX(values, index)) = DB1_DATETIME;
    VAL_NULL(GET_FIELD_IDX(values, index)) = 0;
    VAL_TIME(GET_FIELD_IDX(values, index)) = val;
}

int update_ro_dbinfo_unsafe(struct ro_session* ro_session) {
    if ((ro_session->flags & RO_SESSION_FLAG_NEW) != 0 && (ro_session->flags & RO_SESSION_FLAG_INSERTED) == 0) {

	db_val_t values[RO_SESSION_TABLE_COL_NUM];
	db_key_t insert_keys[RO_SESSION_TABLE_COL_NUM] = {
	    &id_column, &h_entry_column, &h_id_column, &session_id_column, &dlg_h_entry_column, &dlg_h_id_column,
	    &direction_column, &asserted_column, &callee_column, &start_time_col, &last_event_ts_column,
	    &reserved_sec_column, &valid_for_column, &state_column, &incoming_trunk_id_column, &outgoing_trunk_id_column, &rating_group_column, &service_identifier_column
	};

	VAL_TYPE(GET_FIELD_IDX(values, ID_COL_IDX)) = DB1_INT;
	VAL_NULL(GET_FIELD_IDX(values, ID_COL_IDX)) = 1;

	db_set_int_val(values, HASH_ENTRY_COL_IDX, ro_session->h_entry);
	db_set_int_val(values, HASH_ID_COL_IDX, ro_session->h_id);
	db_set_str_val(values, SESSION_ID_COL_IDX, &ro_session->ro_session_id);
	db_set_int_val(values, DLG_HASH_ENTRY_COL_IDX, ro_session->dlg_h_entry);
	db_set_int_val(values, DLG_HASH_ID_COL_IDX, ro_session->dlg_h_id);
	db_set_int_val(values, DIRECTION_COL_IDX, ro_session->direction);
	db_set_str_val(values, ASSERTED_ID_COL_IDX, &ro_session->asserted_identity);
	db_set_str_val(values, CALLEE_COL_IDX, &ro_session->called_asserted_identity);
	db_set_datetime_val(values, START_TIME_COL_IDX, ro_session->start_time);
	db_set_datetime_val(values, LAST_EVENT_TS_COL_IDX, ro_session->last_event_timestamp);
	db_set_int_val(values, RESERVED_SECS_COL_IDX, ro_session->reserved_secs);
	db_set_int_val(values, VALID_FOR_COL_IDX, ro_session->valid_for);
	db_set_int_val(values, STATE_COL_IDX, ro_session->active);
	db_set_str_val(values, INCOMING_TRUNK_ID_COL_IDX, &ro_session->incoming_trunk_id);
	db_set_str_val(values, OUTGOING_TRUNK_ID_COL_IDX, &ro_session->outgoing_trunk_id);
	db_set_int_val(values, RATING_GROUP_COL_IDX, ro_session->rating_group);
	db_set_int_val(values, SERVICE_IDENTIFIER_COL_IDX, ro_session->service_identifier);
	

	LM_DBG("Inserting ro_session into database\n");
	if ((ro_dbf.insert(ro_db_handle, insert_keys, values, RO_SESSION_TABLE_COL_NUM)) != 0) {
	    LM_ERR("could not add new Ro session into DB.... continuing\n");
	    goto error;
	}
	ro_session->flags &= ~(RO_SESSION_FLAG_NEW | RO_SESSION_FLAG_CHANGED);
	ro_session->flags |= RO_SESSION_FLAG_INSERTED;

    } else if ((ro_session->flags & RO_SESSION_FLAG_CHANGED) != 0 && (ro_session->flags & RO_SESSION_FLAG_INSERTED) != 0) {

	db_val_t values[RO_SESSION_TABLE_COL_NUM-1];
	db_key_t update_keys[RO_SESSION_TABLE_COL_NUM-1] = {
	    &h_entry_column, &h_id_column, &session_id_column, &dlg_h_entry_column, &dlg_h_id_column,
	    &direction_column, &asserted_column, &callee_column, &start_time_col, &last_event_ts_column,
	    &reserved_sec_column, &valid_for_column, &state_column, &incoming_trunk_id_column, &outgoing_trunk_id_column, &rating_group_column, &service_identifier_column
	};

	db_set_int_val(values, HASH_ENTRY_COL_IDX - 1, ro_session->h_entry);
	db_set_int_val(values, HASH_ID_COL_IDX - 1, ro_session->h_id);
	db_set_str_val(values, SESSION_ID_COL_IDX - 1, &ro_session->ro_session_id);
	db_set_int_val(values, DLG_HASH_ENTRY_COL_IDX - 1, ro_session->dlg_h_entry);
	db_set_int_val(values, DLG_HASH_ID_COL_IDX - 1, ro_session->dlg_h_id);
	db_set_int_val(values, DIRECTION_COL_IDX - 1, ro_session->direction);
	db_set_str_val(values, ASSERTED_ID_COL_IDX - 1, &ro_session->asserted_identity);
	db_set_str_val(values, CALLEE_COL_IDX - 1, &ro_session->called_asserted_identity);
	db_set_datetime_val(values, START_TIME_COL_IDX - 1, ro_session->start_time);
	db_set_datetime_val(values, LAST_EVENT_TS_COL_IDX - 1, ro_session->last_event_timestamp);
	db_set_int_val(values, RESERVED_SECS_COL_IDX - 1, ro_session->reserved_secs);
	db_set_int_val(values, VALID_FOR_COL_IDX - 1, ro_session->valid_for);
	db_set_int_val(values, STATE_COL_IDX - 1, ro_session->active);
	db_set_str_val(values, INCOMING_TRUNK_ID_COL_IDX - 1, &ro_session->incoming_trunk_id);
	db_set_str_val(values, OUTGOING_TRUNK_ID_COL_IDX - 1, &ro_session->outgoing_trunk_id);
	db_set_int_val(values, RATING_GROUP_COL_IDX - 1, ro_session->rating_group);
	db_set_int_val(values, SERVICE_IDENTIFIER_COL_IDX - 1, ro_session->service_identifier);

	LM_DBG("Updating ro_session in database\n");
	if ((ro_dbf.update(ro_db_handle, update_keys/*match*/, 0/*match*/, values/*match*/, update_keys/*update*/, values/*update*/, 3/*match*/, 13/*update*/)) != 0) {
	    LM_ERR("could not update Ro session information in DB... continuing\n");
	    goto error;
	}
	ro_session->flags &= ~RO_SESSION_FLAG_CHANGED;
    } else if ((ro_session->flags & RO_SESSION_FLAG_DELETED) != 0) {
	db_val_t values[3];
	db_key_t match_keys[3] = {&h_entry_column, &h_id_column, &session_id_column};
	
	db_set_int_val(values, HASH_ENTRY_COL_IDX - 1, ro_session->h_entry);
	db_set_int_val(values, HASH_ID_COL_IDX - 1, ro_session->h_id);
	db_set_str_val(values, SESSION_ID_COL_IDX - 1, &ro_session->ro_session_id);
	
	if(ro_dbf.delete(ro_db_handle, match_keys, 0, values, 3) < 0) {
		LM_ERR("failed to delete ro session database information... continuing\n");
		return -1;
	}
    } else {
	LM_WARN("Asked to update Ro session in strange state [%d]\n", ro_session->flags);
    }

    return 0;

error:
    return -1;
}

int update_ro_dbinfo(struct ro_session* ro_session) {
    struct ro_session_entry entry;
    /* lock the entry */
    entry = (ro_session_table->entries)[ro_session->h_entry];
    ro_session_lock(ro_session_table, &entry);
    if (update_ro_dbinfo_unsafe(ro_session) != 0) {
	LM_ERR("failed to update ro_session in DB\n");
	ro_session_unlock(ro_session_table, &entry);
	return -1;
    }
    ro_session_unlock(ro_session_table, &entry);
    return 0;
}

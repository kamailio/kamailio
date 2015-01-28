/* 
 * File:   ro_db_handler.h
 * Author: jaybeepee
 *
 * Created on 02 September 2014, 11:20 AM
 */

#ifndef RO_DB_HANDLER_H
#define	RO_DB_HANDLER_H

#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "ro_session_hash.h"

#define RO_TABLE_VERSION            1
#define RO_SESSION_TABLE_NAME       "ro_session"
#define RO_SESSION_TABLE_COL_NUM    18

#define ID_COL                      "id"
#define HASH_ENTRY_COL              "hash_entry"
#define HASH_ID_COL                 "hash_id"
#define SESSION_ID_COL              "session_id"
#define DLG_HASH_ENTRY_COL          "dlg_hash_entry"
#define DLG_HASH_ID_COL             "dlg_hash_id"
#define DIRECTION_COL               "direction"
#define ASSERTED_ID_COL             "asserted_identity"
#define CALLEE_COL                  "callee"
#define START_TIME_COL              "start_time"
#define LAST_EVENT_TS_COL           "last_event_timestamp"
#define RESERVED_SECS_COL           "reserved_secs"
#define VALID_FOR_COL               "valid_for"
#define STATE_COL                   "state"
#define RATING_GROUP_COL            "rating_group"
#define SERVICE_IDENTIFIER_COL      "service_identifier"
#define INCOMING_TRUNK_ID_COL	    "incoming_trunk_id"
#define OUTGOING_TRUNK_ID_COL		    "outgoing_trunk_id"

int init_ro_db(const str *db_url, int dlg_hash_size , int db_update_period, int fetch_num_rows);
int load_ro_info_from_db(int hash_size, int fetch_num_rows);
int ro_connect_db(const str *db_url);
int update_ro_dbinfo_unsafe(struct ro_session* ro_session);
int update_ro_dbinfo(struct ro_session* ro_session);

#endif	/* RO_DB_HANDLER_H */


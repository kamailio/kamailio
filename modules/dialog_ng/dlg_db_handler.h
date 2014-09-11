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
 */


#ifndef _DLG_DB_HANDLER_H_
#define _DLG_DB_HANDLER_H_

#include "../../str.h"
#include "../../lib/srdb1/db.h"

#define ID_COL					"id"
#define HASH_ENTRY_COL			"hash_entry"
#define HASH_ID_COL				"hash_id"
#define DID_COL					"did"
#define CALL_ID_COL				"callid"
#define FROM_URI_COL			"from_uri"
#define FROM_TAG_COL			"from_tag"
#define CALLER_ORIGINAL_CSEQ_COL "caller_original_cseq"
#define REQ_URI_COL				"req_uri"
#define CALLER_ROUTESET_COL		"caller_route_set"
#define CALLER_CONTACT_COL		"caller_contact"
#define CALLER_SOCK				"caller_sock"
#define STATE_COL				"state"
#define START_TIME_COL			"start_time"
#define TIMEOUT_COL				"timeout"
#define SFLAGS_COL				"sflags"
#define TOROUTE_NAME_COL		"toroute_name"
#define TOROUTE_INDEX_COL		"toroute_index"

// dialog_out exclusive columns
#define TO_URI_COL				"to_uri"
#define TO_TAG_COL				"to_tag"
#define CALLER_CSEQ_COL			"caller_cseq"
#define CALLEE_CSEQ_COL			"callee_cseq"
#define CALLEE_CONTACT_COL		"callee_contact"
#define CALLEE_ROUTESET_COL		"callee_route_set"
#define CALLEE_SOCK				"callee_sock"

//#define XDATA_COL				"xdata"
#define DIALOG_IN_TABLE_NAME	"dialog_in"
#define DIALOG_OUT_TABLE_NAME	"dialog_out"
#define DLG_TABLE_VERSION		7
#define DIALOG_IN_TABLE_COL_NO 	18
#define DIALOG_OUT_TABLE_COL_NO 11

#define VARS_HASH_ID_COL 			"hash_id"
#define VARS_HASH_ENTRY_COL			"hash_entry"
#define VARS_KEY_COL				"dialog_key"
#define VARS_VALUE_COL				"dialog_value"
#define DIALOG_VARS_TABLE_NAME		"dialog_vars"
#define DLG_VARS_TABLE_VERSION		1
#define DIALOG_VARS_TABLE_COL_NO 	4

/*every minute the dialogs' information will be refreshed*/
#define DB_DEFAULT_UPDATE_PERIOD	60
#define DB_MODE_NONE				0
#define DB_MODE_REALTIME			1
#define DB_MODE_DELAYED				2
#define DB_MODE_SHUTDOWN			3

/* Dialog table */
extern str call_id_column; 
extern str from_uri_column;
extern str from_tag_column;
extern str to_uri_column;
extern str to_tag_column;
extern str h_id_column;
extern str h_entry_column;
extern str state_column;
extern str start_time_column;
extern str timeout_column;
extern str to_cseq_column;
extern str from_cseq_column;
extern str to_route_column;
extern str from_route_column;
extern str to_contact_column;
extern str from_contact_column;
extern str to_sock_column;
extern str from_sock_column;
extern str sflags_column;
extern str toroute_name_column;
extern str dialog_in_table_name;
extern int dlg_db_mode;

/* Dialog-Vars Table */
extern str vars_h_id_column;
extern str vars_h_entry_column;
extern str vars_key_column;
extern str vars_value_column;
extern str dialog_vars_table_name;


int init_dlg_db(const str *db_url, int dlg_hash_size, int db_update_period, int fetch_num_rows);
int dlg_connect_db(const str *db_url);
void destroy_dlg_db(void);

int remove_dialog_in_from_db(struct dlg_cell * cell);
int update_dialog_dbinfo(struct dlg_cell * cell);
void dialog_update_db(unsigned int ticks, void * param);

#endif

/*
 * $Id: dlg_db_handler.h 2108 2007-04-30 20:29:36Z bogdan_iancu $
 *
 * Copyright (C) 2007 Voice System SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
#include "../../db/db.h"

#define CALL_ID_COL				"callid"
#define FROM_URI_COL			"from_uri"
#define FROM_TAG_COL			"from_tag"
#define TO_URI_COL				"to_uri"
#define TO_TAG_COL				"to_tag"
#define HASH_ID_COL				"hash_id"
#define HASH_ENTRY_COL			"hash_entry"
#define STATE_COL				"state"
#define START_TIME_COL			"start_time"
#define TIMEOUT_COL				"timeout"
#define TO_CSEQ_COL				"callee_cseq"
#define FROM_CSEQ_COL			"caller_cseq"
#define TO_ROUTE_COL			"callee_route_set"
#define FROM_ROUTE_COL			"caller_route_set"
#define TO_CONTACT_COL			"callee_contact"
#define FROM_CONTACT_COL		"caller_contact"
#define FROM_SOCK_COL			"caller_sock"
#define TO_SOCK_COL				"callee_sock"
#define DIALOG_TABLE_NAME		"dialog"

#define DLG_TABLE_VERSION		2

/*every minute the dialogs' information will be refreshed*/
#define DB_DEFAULT_UPDATE_PERIOD	60
#define DB_MODE_NONE				0
#define DB_MODE_REALTIME			1
#define DB_MODE_DELAYED				2

#define DIALOG_TABLE_COL_NO 		18


extern char* call_id_column; 
extern char* from_uri_column;
extern char* from_tag_column;
extern char* to_uri_column;
extern char* to_tag_column;
extern char* h_id_column;
extern char* h_entry_column;
extern char* state_column;
extern char* start_time_column;
extern char* timeout_column;
extern char* to_cseq_column;
extern char* from_cseq_column;
extern char* to_route_column;
extern char* from_route_column;
extern char* to_contact_column;
extern char* from_contact_column;
extern char* to_sock_column;
extern char* from_sock_column;
extern char* dialog_table_name;
extern int dlg_db_mode;

int init_dlg_db(char *db_url, int dlg_hash_size, int db_update_period);
int dlg_connect_db(char *db_url);
void destroy_dlg_db();

int remove_dialog_from_db(struct dlg_cell * cell);
int update_dialog_dbinfo(struct dlg_cell * cell);
void dialog_update_db(unsigned int ticks, void * param);

#endif

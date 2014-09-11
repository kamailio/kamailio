/*
 * Presence Agent, module interface
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PA_MOD_H
#define PA_MOD_H

#include "../../parser/msg_parser.h"
#include "../../modules/tm/tm_load.h"
#include "../../lib/srdb2/db.h"
#include "../dialog/dlg_mod.h"
#include "auth.h"

/* we have to use something from this module */
#include "../xcap/xcap_mod.h"

extern int default_expires;
extern int max_subscription_expiration;  /* max expires value for SUBSCRIBE */
extern int max_publish_expiration;  /* max expires value for PUBLISH */
extern double default_priority;
extern int timer_interval;

/* TM bind */
extern struct tm_binds tmb;

extern dlg_func_t dlg_func;

/* DB module bind */
extern db_func_t pa_dbf;
extern db_con_t* pa_db;
extern fill_xcap_params_func fill_xcap_params;

/* PA database */
extern int use_db;
extern int use_place_table;
extern str db_url;
extern str pa_domain;

extern char *presentity_table;
extern char *presentity_contact_table;
extern char *presentity_notes_table;
extern char *extension_elements_table;
extern char *watcherinfo_table;
extern char *place_table;
extern char *tuple_notes_table;
extern char *tuple_extensions_table;

/* columns in DB tables */

extern char *col_uri;
extern char *col_pdomain;
extern char *col_uid;
extern char *col_pres_id;
extern char *col_xcap_params;
extern char *col_tupleid;
extern char *col_basic;
extern char *col_contact;
extern char *col_etag;
extern char *col_published_id;
extern char *col_priority;
extern char *col_expires;
extern char *col_dbid;
extern char *col_note;
extern char *col_lang;
extern char *col_element;
extern char *col_status_extension;

extern char *col_s_id;
extern char *col_w_uri;
extern char *col_package;
extern char *col_status;
extern char *col_display_name;
extern char *col_accepts;
extern char *col_event;
extern char *col_dialog;
extern char *col_server_contact;
extern char *col_doc_index;

extern char *col_watcher;
extern char *col_events;
extern char *col_domain;
extern char *col_created_on;
extern char *col_expires_on;


extern int use_bsearch;
extern int use_location_package;
extern auth_params_t pa_auth_params;
extern auth_params_t winfo_auth_params;
extern int watcherinfo_notify;
extern int use_callbacks;
extern int subscribe_to_users;
extern str pa_subscription_uri;
extern int use_offline_winfo;
extern char *offline_winfo_table;
extern int ignore_408_on_notify;
extern int notify_is_refresh;

extern str pres_rules_file; /* filename for XCAP queries */
db_con_t* create_pa_db_connection();
void close_pa_db_connection(db_con_t* db);

#endif /* PA_MOD_H */

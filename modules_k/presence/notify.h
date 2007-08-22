/*
 * $Id$
 *
 * presence module -presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2006-08-15  initial version (anca)
 */

#include "../../str.h"
#include "subscribe.h"
#include "presentity.h"

#ifndef NOTIFY_H
#define NOTIFY_H

#define FULL_STATE_FLAG (1<<0)
#define PARTIAL_STATE_FLAG (1<<1)
//extern char p_event[10] ;
//extern char wi_event[16] ;
#define PRES_LEN 8
#define PWINFO_LEN 14
#define BLA_LEN 10


typedef struct watcher
{
	str uri;
	str id;
	int status;
	str event;
	str display_name;
	str expiration;
	str duration_subscribed;
	struct watcher* next;
}watcher_t;

typedef struct wid_cback
{
	str pres_uri;
	str ev_name;
	str to_tag;   /* to identify the exact record */
	subs_t* wi_subs;
}c_back_param;

void PRINT_DLG(FILE* out, dlg_t* _d);

void printf_subs(subs_t* subs);

int query_db_notify(str* pres_uri,pres_ev_t* event, subs_t* watcher_subs );

int publ_notify(presentity_t* p, str* body, str* offline_etag, str* rules_doc);

int notify(subs_t* subs, subs_t* watcher_subs, str* n_body, int force_null_body);

int send_notify_request(subs_t* subs, subs_t * watcher_subs,
		str* n_body,int force_null_body);
#endif

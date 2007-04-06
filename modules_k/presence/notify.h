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

#ifndef NOTIFY_H
#define NOTIFY_H

#define FULL_STATE_FLAG (1<<0)
#define PARTIAL_STATE_FLAG (1<<1)
//extern char p_event[10] ;
//extern char wi_event[16] ;
#define PRES_LEN 8
#define PWINFO_LEN 14
#define BLA_LEN 10

#define PRES_RULES 1;
#define RESOURCE_LIST 2;
#define RLS_SERVICE 3;

typedef struct watcher
{
	str uri;
	str id;
	str status;
	str event;
	str display_name;
	str expiration;
	str duration_subscribed;
}watcher_t;

typedef struct wid_cback
{
	char* w_id;
	subs_t* wi_subs;
}c_back_param;

void PRINT_DLG(FILE* out, dlg_t* _d);

void printf_subs(subs_t* subs);

//str* build_str_hdr(str event, str status, int expires_t, str reason);

int free_tm_dlg(dlg_t *td);

dlg_t* build_dlg_t (str p_uri, subs_t* subs);

int query_db_notify(str* p_user, str* p_domain, ev_t* event,
		subs_t *subs, str* etag, str* sender);

int notify(subs_t* subs, subs_t* watcher_subs, str* n_body, int force_null_body);

#endif

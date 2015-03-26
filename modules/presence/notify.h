/*
 * presence module -presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

/*! \file
 * \brief Kamailio presence module :: NOTIFY support
 * \ingroup presence 
 */


#include "../../str.h"
#include "../../modules/tm/dlg.h"
#include "subscribe.h"
#include "presentity.h"

#ifndef NOTIFY_H
#define NOTIFY_H

#define FULL_STATE_FLAG (1<<0)
#define PARTIAL_STATE_FLAG (1<<1)

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
	str from_tag;
	str callid;
}c_back_param;

extern str str_to_user_col;
extern str str_username_col;
extern str str_domain_col;
extern str str_body_col;
extern str str_to_domain_col;
extern str str_from_user_col;
extern str str_from_domain_col;
extern str str_watcher_username_col;
extern str str_watcher_domain_col;
extern str str_event_id_col;
extern str str_event_col;
extern str str_etag_col;
extern str str_from_tag_col;
extern str str_to_tag_col;
extern str str_callid_col;
extern str str_local_cseq_col;
extern str str_remote_cseq_col;
extern str str_record_route_col;
extern str str_contact_col;
extern str str_expires_col;
extern str str_status_col;
extern str str_reason_col;
extern str str_socket_info_col;
extern str str_local_contact_col;
extern str str_version_col;
extern str str_presentity_uri_col;
extern str str_inserted_time_col;
extern str str_received_time_col;
extern str str_id_col;
extern str str_sender_col;
extern str str_updated_col;
extern str str_updated_winfo_col;
extern str str_priority_col;

void PRINT_DLG(FILE* out, dlg_t* _d);

void printf_subs(subs_t* subs);

int query_db_notify(str* pres_uri,pres_ev_t* event, subs_t* watcher_subs );

int publ_notify(presentity_t* p, str pres_uri, str* body, str* offline_etag,
		str* rules_doc);
int publ_notify_notifier(str pres_uri, pres_ev_t *event);
int set_updated(subs_t *sub);
int set_wipeer_subs_updated(str *pres_uri, pres_ev_t *event, int full);

int notify(subs_t* subs, subs_t* watcher_subs, str* n_body,int force_null_body);

int send_notify_request(subs_t* subs, subs_t * watcher_subs,
		str* n_body,int force_null_body);

char* get_status_str(int flag);

str *get_p_notify_body(str pres_uri, pres_ev_t *event, str *etag, str *contact);
void free_notify_body(str *body, pres_ev_t *ev);
void pres_timer_send_notify(unsigned int ticks, void *param);
#endif

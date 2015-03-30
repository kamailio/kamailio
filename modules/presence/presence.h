/*
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief Kamailio presence module :: Core
 * \ingroup presence 
 */


#ifndef PA_MOD_H
#define PA_MOD_H

#include "../../parser/msg_parser.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_from.h"
#include "event_list.h"
#include "hash.h"

/* DB modes */

/** subscriptions are stored only in memory */
#define NO_DB            0
/** subscriptions are written in memory and in DB synchronously and read only from memory */
#define WRITE_THROUGH    1
/** subscriptions are stored in memory and periodically updated in DB */
#define WRITE_BACK       2
/** subscriptions are stored only in database */
#define DB_ONLY          3

#define NO_UPDATE_TYPE	-1
#define UPDATED_TYPE	1

/** TM bind */
extern struct tm_binds tmb;

extern sl_api_t slb;

/* DB module bind */
extern db_func_t pa_dbf;
extern db1_con_t* pa_db;

/* PRESENCE database */
extern str db_url;
extern str presentity_table;
extern str active_watchers_table;
extern str watchers_table; 

extern int counter;
extern int pid;
extern int startup_time;
extern char *to_tag_pref;
extern int expires_offset;
extern str server_address;
extern int min_expires;
extern int min_expires_action;
extern int max_expires;
extern int subs_dbmode;
extern int publ_cache_enabled;
extern int sphere_enable;
extern int timeout_rm_subs;
extern int send_fast_notify;
extern int shtable_size;
extern shtable_t subs_htable;

extern int pres_fetch_rows;

extern int pres_waitn_time;
extern int pres_notifier_poll_rate;
extern int pres_notifier_processes;
extern str pres_xavp_cfg;
extern int pres_retrieve_order;

extern int phtable_size;
extern phtable_t* pres_htable;

extern db_locking_t db_table_lock;

int update_watchers_status(str pres_uri, pres_ev_t* ev, str* rules_doc);
int pres_auth_status(struct sip_msg* msg, str watcher_uri, str presentity_uri);

typedef int (*sip_uri_match_f) (str* s1, str* s2);
extern sip_uri_match_f presence_sip_uri_match;

int pv_get_subscription(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);
int pv_parse_subscription_name(pv_spec_p sp, str *in);

#endif /* PA_MOD_H */

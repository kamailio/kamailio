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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2006-10-09  first version (anca)
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

/** subscriptions are held in memory and periodically updated to db, but retrieved from db only at startup */
#define DB_MEMORY_ONLY 0
/** same as memory_only, but if a subscription is not found, it falls back to db */
#define DB_FALLBACK 1
/** subscriptions are held only in database */
#define DB_ONLY 2

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
extern int max_expires;
extern int dbmode;
extern int sphere_enable;
extern int timeout_rm_subs;
extern int send_fast_notify;
extern int shtable_size;
extern shtable_t subs_htable;

extern int phtable_size;
extern phtable_t* pres_htable;

int update_watchers_status(str pres_uri, pres_ev_t* ev, str* rules_doc);

#endif /* PA_MOD_H */

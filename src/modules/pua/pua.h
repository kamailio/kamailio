/*
 * pua module - presence user agent module
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
 */

#ifndef PUBLISH_H
#define PUBLISH_H

#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "hash.h"

#define INSERT_TYPE      1<<1
#define UPDATE_TYPE      1<<2

#define REQ_OTHER  0
#define REQ_ME     1

#define PUA_DB_DEFAULT 0
#define PUA_DB_MODE1_RESERVED 1
#define PUA_DB_ONLY 2

extern str default_domain;
extern struct tm_binds tmb;
extern htable_t* HashT;
extern int HASH_SIZE;
extern int min_expires;
extern int pua_ul_publish;
extern int default_expires;
extern str outbound_proxy;
extern int check_remote_contact;
extern int dbmode;

int reginfo_increase_version;

extern int update_pua(ua_pres_t* p);
extern int clean_puadb( int update_period, int min_expires );

extern db_func_t pua_dbf;
extern db1_con_t *pua_db;
extern int pua_fetch_rows;

extern str db_table;

#endif

/*
 * pua_db headers - presence user agent db headers
 *
 * Copyright (C) 2011 Crocodile RCS Ltd
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

#ifndef PUA_DB_H
#define PUA_DB_H

#include "../../lib/srdb1/db.h"
#include "../rls/list.h"

#define PUA_PRES_URI (1<<0) 
#define PUA_PRES_ID (1<<1)
#define PUA_EVENT (1<<2)
#define PUA_EXPIRES (1<<3)
#define PUA_DESIRED_EXPIRES (1<<4)
#define PUA_FLAG (1<<5)
#define PUA_ETAG (1<<6)
#define PUA_TUPLE_ID (1<<7)
#define PUA_WATCHER_URI (1<<8)
#define PUA_CALL_ID (1<<9)
#define PUA_TO_TAG (1<<10)
#define PUA_FROM_TAG (1<<11)
#define PUA_CSEQ  (1<<12)
#define PUA_RECORD_ROUTE (1<<13)
#define PUA_CONTACT (1<<14)
#define PUA_REMOTE_CONTACT (1<<15)
#define PUA_VERSION (1<<16)
#define PUA_EXTRA_HEADERS (1<<17)

void free_results_puadb( db1_res_t *res );
int is_dialog_puadb(ua_pres_t *pres);
int get_record_id_puadb(ua_pres_t *pres, str **rec_id );

int convert_temporary_dialog_puadb(ua_pres_t *pres);
int insert_dialog_puadb(ua_pres_t* pres);
ua_pres_t *get_dialog_puadb(str pres_id, str *pres_uri, ua_pres_t *result, db1_res_t **res);
int delete_dialog_puadb(ua_pres_t *pres);
int update_dialog_puadb(ua_pres_t *pres, int expires, str *contact);
list_entry_t *get_subs_list_puadb(str *did);

int insert_record_puadb(ua_pres_t* pres);
ua_pres_t *get_record_puadb(str pres_id, str *etag, ua_pres_t *result, db1_res_t **res);
int delete_record_puadb(ua_pres_t *pres);
int update_record_puadb(ua_pres_t *pres, int expires, str *contact);
int update_version_puadb(ua_pres_t *pres);
int update_contact_puadb(ua_pres_t *pres, str *contact);

#endif

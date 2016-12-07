/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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



#ifndef _AVP_OPS_DB_H_
#define _AVP_OPS_DB_H_

#include "../../lib/srdb1/db.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../sr_module.h"
#include "../../pvar.h"


/* definition of a DB scheme*/
struct db_scheme
{
	str name;
	str uuid_col;
	str username_col;
	str domain_col;
	str value_col;
	str table;
	int db_flags;
	struct db_scheme *next;
};


int avpops_db_bind(const str* db_url);

int avpops_db_init(const str* db_url, const str* db_table, str **db_columns);

db1_res_t *db_load_avp( str *uuid, str *username, str *domain,
		char *attr, const str *table, struct db_scheme *scheme);

void db_close_query( db1_res_t *res );

int db_store_avp( db_key_t *keys, db_val_t *vals, int n, const str *table);

int db_delete_avp( str *uuid, str *username, str *domain,
		char *attr, const str *table);

int db_query_avp(struct sip_msg* msg, char *query, pvname_list_t* dest);

int avp_add_db_scheme( modparam_t type, void* val);

struct db_scheme *avp_get_db_scheme( str *name );

#endif

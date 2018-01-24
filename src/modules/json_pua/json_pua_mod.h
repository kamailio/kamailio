/*
 * JSON_PUA module interface
 *
 * Copyright (C) 2016 Weave Communications
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This module was based on the Kazoo module created by 2600hz.
 * Thank you to 2600hz and their brilliant VoIP developers.
 *
 */

#ifndef __JSON_PUA_MOD_H_
#define __JSON_PUA_MOD_H_

#include <json.h>
#include "../../lib/srdb1/db.h"
#include "json_pua_publish.h"

#define PRESENTITY_TABLE "presentity"

int dbn_pua_mode = 1;
int dbn_include_entity = 1;

/* database connection */
db1_con_t *json_pa_db = NULL;
db_func_t json_pa_dbf;
str json_presentity_table = str_init(PRESENTITY_TABLE);
str json_db_url = {NULL, 0};
int db_table_lock_type = 1;
db_locking_t db_table_lock = DB_LOCKING_WRITE;

static int mod_init(void);
static int mod_child_init(int);

#endif

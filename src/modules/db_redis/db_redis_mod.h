/*
 * Copyright (C) 2018 Andreas Granig (sipwise.com)
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


#ifndef _DB_REDIS_MOD_H
#define _DB_REDIS_MOD_H

#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_query.h"
#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_id.h"
#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"

#include "../../core/mem/mem.h"

#include "../../core/dprint.h"
#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../core/str_hash.h"
#include "../../core/ut.h"

#define REDIS_DIRECT_PREFIX "entry"
#define REDIS_DIRECT_PREFIX_LEN 5

#define REDIS_HT_SIZE 8

extern str redis_keys;
extern str redis_schema_path;

#endif /* _DB_REDIS_MOD_H */
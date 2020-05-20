/**
 * Copyright (C) 2020 Julien Chavanton
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _MQUEUE_DB_H_
#define _MQUEUE_DB_H_

#include "../../lib/srdb1/db.h"
#include "mqueue_api.h"

extern str mqueue_db_url;

int mqueue_db_load_queue(str *name);
int mqueue_db_save_queue(str *name);
#endif

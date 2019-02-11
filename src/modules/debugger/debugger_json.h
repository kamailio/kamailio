/**
 *
 * Copyright (C) 2013-2015 Victor Seva (sipwise.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#ifndef _DEBUGGER_JSON_H
#define _DEBUGGER_JSON_H

#include "../../lib/srutils/srjson.h"
#include "../../core/route_struct.h"

int dbg_get_json(struct sip_msg* msg, unsigned int mask, srjson_doc_t *jdoc,
	srjson_t *head);
int dbg_dump_json(struct sip_msg* msg, unsigned int mask, int level);
#endif

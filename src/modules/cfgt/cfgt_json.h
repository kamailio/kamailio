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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef _CFGT_JSON_H
#define _CFGT_JSON_H

#include "../../core/route_struct.h"
#include "../../lib/srutils/srjson.h"

#define CFGT_DP_NULL 1
#define CFGT_DP_AVP 2
#define CFGT_DP_SCRIPTVAR 4
#define CFGT_DP_XAVP 8
#define CFGT_DP_OTHER 16
#define CFGT_DP_ALL 31

int cfgt_get_json(struct sip_msg *msg, unsigned int mask, srjson_doc_t *jdoc,
		srjson_t *head);
#endif

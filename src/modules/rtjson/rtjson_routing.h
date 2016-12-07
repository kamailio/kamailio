/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _RTJSON_ROUTING_H_
#define _RTJSON_ROUTING_H_

#include "../../parser/msg_parser.h"

int rtjson_init_routes(sip_msg_t *msg, str *rdoc);
int rtjson_push_routes(sip_msg_t *msg);
int rtjson_next_route(sip_msg_t *msg);
int rtjson_update_branch(sip_msg_t *msg);
int rtjson_init(void);

#endif

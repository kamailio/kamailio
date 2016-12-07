/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _JANSSONRPC_FUNCS_H_
#define _JANSSONRPC_FUNCS_H_

#include <stdbool.h>
#include "../../parser/msg_parser.h"

int jsonrpc_request(struct sip_msg* _m,
		char* _conn,
		char* _method,
		char* _params,
		char* _options);

int jsonrpc_notification(struct sip_msg* msg,
		char* conn,
		char* method,
		char* params);

int mod_jsonrpc_request(
		struct sip_msg* msg,
		str conn,
		str method,
		str params,
		str route,
		bool notify_only,
		int retry,
		unsigned int timeout
	);

typedef int (*mod_jsonrpc_request_f)(
		struct sip_msg* msg,
		str conn,
		str method,
		str params,
		str route,
		bool notify_only,
		unsigned int retry,
		unsigned int timeout
	);

#endif /* _JSONRPC_FUNCS_H_ */

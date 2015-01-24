/*
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
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

#ifndef _JSONRPC_REQUEST_H_
#define _JSONRPC_REQUEST_H_
#include "../../parser/msg_parser.h"

int jsonrpc_request(struct sip_msg* msg, char* method, char* params, char* cb_route, char* err_route, char* cb_pv);
int jsonrpc_notification(struct sip_msg* msg, char* method, char* params);
int cmd_pipe;

#endif /* _JSONRPC_REQUEST_H_ */

/**
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#ifndef _XHTTP_API_H_
#define _XHTTP_API_H_

#include "../../sr_module.h"

typedef int (*xhttp_reply_f)(sip_msg_t *msg, int code, str *reason,
		str *ctype, str *body);

typedef struct xhttp_api {
	xhttp_reply_f reply;
} xhttp_api_t;

typedef int (*bind_xhttp_f)(xhttp_api_t* api);
int bind_xhttp(xhttp_api_t* api);

/**
 * @brief Load the XHTTP API
 */
static inline int xhttp_load_api(xhttp_api_t *api)
{
	bind_xhttp_f bindxhttp;

	bindxhttp = (bind_xhttp_f)find_export("bind_xhttp", 0, 0);
	if(bindxhttp == 0) {
		LM_ERR("cannot find bind_xhttp\n");
		return -1;
	}
	if(bindxhttp(api)<0)
	{
		LM_ERR("cannot bind xhttp api\n");
		return -1;
	}
	return 0;
}

#endif

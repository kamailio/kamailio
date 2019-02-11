/*
 * rls db - RLS database support
 *
 * Copyright (C) 2011 Crocodile RCS Ltd
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
 */

#ifndef RLS_API_H
#define RLS_API_H
#include "../../core/str.h"

typedef int (*rls_handle_subscribe_t)(struct sip_msg*, str, str);
typedef int (*rls_handle_subscribe0_t)(struct sip_msg*);
typedef int (*rls_handle_notify_t)(struct sip_msg*, char*, char*);

typedef struct rls_binds {
	rls_handle_subscribe_t rls_handle_subscribe;
	rls_handle_subscribe0_t rls_handle_subscribe0;
	rls_handle_notify_t rls_handle_notify;
} rls_api_t;

typedef int (*bind_rls_f)(rls_api_t*);

int bind_rls(struct rls_binds*);

inline static int rls_load_api(rls_api_t *pxb)
{
	bind_rls_f bind_rls_exports;
	if (!(bind_rls_exports = (bind_rls_f)find_export("bind_rls", 1, 0)))
	{
		LM_ERR("Failed to import bind_rls\n");
		return -1;
	}
	return bind_rls_exports(pxb);
}

#endif /*RLS_API_H*/

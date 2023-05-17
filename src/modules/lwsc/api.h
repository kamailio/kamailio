/*
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
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

/**
 * @file
 * @brief LWSC - API definitions
 * @ingroup lwsc
 * Module: @ref lwsc
 */

#ifndef _LWSC_API_H_
#define _LWSC_API_H_

#include "../../core/str.h"

typedef int (*lwsc_api_request_f)(
		str *wsurl, str *wsproto, str *sdata, str *rdata, int rtimeout);

/**
 * @brief Stateless (sl) API structure
 */
typedef struct lwsc_api
{
	int loaded;
	lwsc_api_request_f request; /* send and receice data */
} lwsc_api_t;

typedef int (*bind_lwsc_f)(lwsc_api_t *api);

/**
 * @brief Load the LWSX API
 */
static inline int lwsc_load_api(lwsc_api_t *lwscb)
{
	bind_lwsc_f bindlwsc;

	bindlwsc = (bind_lwsc_f)find_export("bind_lwsc", 0, 0);
	if(bindlwsc == 0) {
		LM_ERR("cannot find bind_lwsc exported function\n");
		return -1;
	}
	if(bindlwsc(lwscb) == -1) {
		LM_ERR("cannot bind lwsc api\n");
		return -1;
	}
	lwscb->loaded = 1;

	return 0;
}

#endif

/*
 * Domain module internal API
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version
 *
 * sip-router is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _DOMAIN_API_H
#define _DOMAIN_API_H

#include "../../sr_module.h"
#include "../../dprint.h"
#include "domain.h"

typedef struct domain_api {
	is_domain_local_f is_domain_local;
} domain_api_t;

typedef int (*bind_domain_f)(domain_api_t* api);
int bind_domain(domain_api_t* api);

static inline int load_domain_api(domain_api_t* api)
{
	bind_domain_f bind_domain;

	bind_domain = (bind_domain_f)find_export("bind_domain", 0, 0);

	if (bind_domain == NULL) {
		ERR("Cannot import bind_domain function from domain module\n");
		return -1;
	}

	if (bind_domain(api) == -1) {
		return -1;
	}
	return 0;
}


#endif /* _DOMAIN_API_H */

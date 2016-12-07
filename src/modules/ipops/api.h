/*
 * $Id$
 *
 * Functions that operate on IP addresses
 *
 * Copyright (C) 2012 Hugh Waite (crocodile-rcs.com)
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
 *
 */


#ifndef _IPOPS_API_H_
#define _IPOPS_API_H_

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"

typedef int (*compare_ips_f)(const str *const, const str *const);
int ipopsapi_compare_ips(const str *const ip1, const str *const ip2);

typedef int (*ip_is_in_subnet_f)(const str *const, const str *const);
int ipopsapi_ip_is_in_subnet(const str *const ip1, const str *const ip2);

typedef int (*is_ip_f)(const str * const ip);
int ipopsapi_is_ip(const str * const ip);
/**
 * @brief IPOPS API structure
 */
typedef struct ipops_api {
	compare_ips_f       compare_ips;
	ip_is_in_subnet_f   ip_is_in_subnet;
	is_ip_f   is_ip;
} ipops_api_t;

typedef int (*bind_ipops_f)(ipops_api_t* api);
int bind_ipops(ipops_api_t* api);

/**
 * @brief Load the IPOPS API
 */
static inline int ipops_load_api(ipops_api_t *api)
{
	bind_ipops_f bindipops;

	bindipops = (bind_ipops_f)find_export("bind_ipops", 0, 0);
	if(bindipops == 0) {
		LM_ERR("cannot find bind_ipops\n");
		return -1;
	}
	if (bindipops(api) < 0)
	{
		LM_ERR("cannot bind ipops api\n");
		return -1;
	}
	return 0;
}


#endif

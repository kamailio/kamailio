/*
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
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

#ifndef _SANITY_API_H_
#define _SANITY_API_H_

#include "../../parser/msg_parser.h"

typedef int (*sanity_check_f)(struct sip_msg* msg, int msg_checks,
		int uri_checks);
typedef int (*sanity_check_defaults_f)(struct sip_msg* msg);

typedef struct sanity_api {
	sanity_check_f check;
	sanity_check_defaults_f check_defaults;
} sanity_api_t;

typedef int (*bind_sanity_f)(sanity_api_t* api);

/**
 * @brief Load the Sanity API
 */
static inline int sanity_load_api(sanity_api_t *api)
{
	bind_sanity_f bindsanity;

	bindsanity = (bind_sanity_f)find_export("bind_sanity", 0, 0);
	if(bindsanity == 0) {
		LM_ERR("cannot find bind_sanity\n");
		return -1;
	}
	if(bindsanity(api)<0)
	{
		LM_ERR("cannot bind sanity api\n");
		return -1;
	}
	return 0;
}


#endif /* _SANITY_API_H_ */

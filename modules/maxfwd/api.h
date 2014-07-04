/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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


#ifndef _MAXFWD_API_H_
#define _MAXFWD_API_H_

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"

typedef int (*process_maxfwd_f)(struct sip_msg *msg, int limit);

/**
 * @brief MAXFWD API structure
 */
typedef struct maxfwd_api {
	process_maxfwd_f process_maxfwd;
} maxfwd_api_t;

typedef int (*bind_maxfwd_f)(maxfwd_api_t* api);

/**
 * @brief Load the MAXFWD API
 */
static inline int maxfwd_load_api(maxfwd_api_t *api)
{
	bind_maxfwd_f bindmaxfwd;

	bindmaxfwd = (bind_maxfwd_f)find_export("bind_maxfwd", 0, 0);
	if(bindmaxfwd == 0) {
		LM_ERR("cannot find bind_maxfwd\n");
		return -1;
	}
	if (bindmaxfwd(api)==-1)
	{
		LM_ERR("cannot bind maxfwd api\n");
		return -1;
	}
	return 0;
}

#endif


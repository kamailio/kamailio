/*
 * $Id$
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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


#ifndef _CFGUTILS_API_H_
#define _CFGUTILS_API_H_

#include "../../str.h"

typedef int (*cfgutils_lock_f)(str *lkey);
typedef int (*cfgutils_unlock_f)(str *lkey);

/**
 * @brief CFGUTILS API structure
 */
typedef struct cfgutils_api {
	cfgutils_lock_f mlock;
	cfgutils_unlock_f munlock;
} cfgutils_api_t;

typedef int (*bind_cfgutils_f)(cfgutils_api_t* api);

/**
 * @brief Load the CFGUTILS API
 */
static inline int cfgutils_load_api(cfgutils_api_t *api)
{
	bind_cfgutils_f bindcfgutils;

	bindcfgutils = (bind_cfgutils_f)find_export("bind_cfgutils", 0, 0);
	if(bindcfgutils == 0) {
		LM_ERR("cannot find bind_cfgutils\n");
		return -1;
	}
	if (bindcfgutils(api)<0)
	{
		LM_ERR("cannot bind cfgutils api\n");
		return -1;
	}
	return 0;
}

#endif

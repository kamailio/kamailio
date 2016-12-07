/**
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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

/*! \file
 * \ingroup dispatcher
 * \brief Dispatcher :: API
 */

#ifndef _DISPATCHER_API_H_
#define _DISPATCHER_API_H_

#include "../../sr_module.h"

typedef int (*ds_select_dst_f)(struct sip_msg *msg, int set,
			int alg, int mode);
typedef int (*ds_next_dst_f)(struct sip_msg *msg, int mode);
typedef int (*ds_mark_dst_f)(struct sip_msg *msg, int mode);

typedef int (*ds_is_from_list_f)(struct sip_msg *_m, int group);

typedef struct dispatcher_api {
	ds_select_dst_f   select;
	ds_next_dst_f     next;
	ds_mark_dst_f     mark;
	ds_is_from_list_f is_from;
} dispatcher_api_t;

typedef int (*bind_dispatcher_f)(dispatcher_api_t* api);
int bind_dispatcher(dispatcher_api_t* api);

/**
 * @brief Load the dispatcher API
 */
static inline int dispatcher_load_api(dispatcher_api_t *api)
{
	bind_dispatcher_f binddispatcher;

	binddispatcher = (bind_dispatcher_f)find_export("bind_dispatcher", 0, 0);
	if(binddispatcher == 0) {
		LM_ERR("cannot find bind_dispatcher\n");
		return -1;
	}
	if(binddispatcher(api)<0)
	{
		LM_ERR("cannot bind dispatcher api\n");
		return -1;
	}
	return 0;
}


#endif


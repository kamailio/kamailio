/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2017 Guillaume Bour <guillaume@bour.cc>
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

/*! \file
 * \ingroup keepalive
 * \brief Keepalive :: Send keepalives
 */

#ifndef __KEEPALIVE_API_H_
#define __KEEPALIVE_API_H_

#include "../../core/sr_module.h"
#include "keepalive.h"

typedef int ka_state;

#define KA_STATE_UNKNOWN 0
#define KA_STATE_UP 1
#define KA_STATE_DOWN 2

typedef int (*ka_add_dest_f)(str *uri, str *owner, int flags,
        int ping_interval, ka_statechanged_f callback, void *user_attr);
typedef ka_state (*ka_dest_state_f)(str *uri);
typedef int (*ka_del_destination_f)(str *uri, str *owner);
typedef int (*ka_find_destination_f)(str *uri, str *owner,ka_dest_t **target,ka_dest_t **head);
typedef int (*ka_lock_destination_list_f)();
typedef int (*ka_unlock_destination_list_f)();

typedef struct keepalive_api
{
	ka_add_dest_f add_destination;
	ka_dest_state_f destination_state;
	ka_del_destination_f del_destination;
	ka_find_destination_f find_destination;
	ka_lock_destination_list_f lock_destination_list;
	ka_unlock_destination_list_f unlock_destination_list;
} keepalive_api_t;

typedef int (*bind_keepalive_f)(keepalive_api_t *api);
int bind_keepalive(keepalive_api_t *api);

/**
 * @brief Load the dispatcher API
 */
static inline int keepalive_load_api(keepalive_api_t *api)
{
	bind_keepalive_f bindkeepalive;

	bindkeepalive = (bind_keepalive_f)find_export("bind_keepalive", 0, 0);
	if(bindkeepalive == 0) {
		LM_ERR("cannot find bind_keepalive\n");
		return -1;
	}

	if(bindkeepalive(api) < 0) {
		LM_ERR("cannot bind keepalive api\n");
		return -1;
	}
	return 0;
}

#endif /* __KEEPALIVE_API_H__ */

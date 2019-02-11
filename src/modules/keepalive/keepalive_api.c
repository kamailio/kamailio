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

/*! \defgroup keepalive Keepalive :: Probing remote gateways by sending keepalives
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../tm/tm_load.h"

#include "keepalive.h"
#include "api.h"


/*
 * Regroup all exported functions in keepalive_api_t structure
 *
 */
int bind_keepalive(keepalive_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	api->add_destination = ka_add_dest;
	api->destination_state = ka_destination_state;
	return 0;
}

/*
 * Add a new destination in keepalive pool
 */
int ka_add_dest(str *uri, str *owner, int flags, ka_statechanged_f callback,
		void *user_attr)
{
	struct sip_uri _uri;
	ka_dest_t *dest;

	LM_INFO("adding destination: %.*s\n", uri->len, uri->s);

	dest = (ka_dest_t *)shm_malloc(sizeof(ka_dest_t));
	if(dest == NULL) {
		LM_ERR("no more memory.\n");
		goto err;
	}
	memset(dest, 0, sizeof(ka_dest_t));

	if(uri->len >= 4 && (!strncasecmp("sip:", uri->s, 4)
							   || !strncasecmp("sips:", uri->s, 5))) {
		// protocol found
		if(ka_str_copy(uri, &(dest->uri), NULL) < 0)
			goto err;
	} else {
		if(ka_str_copy(uri, &(dest->uri), "sip:") < 0)
			goto err;
	}

	// checking uri is valid
	if(parse_uri(dest->uri.s, dest->uri.len, &_uri) != 0) {
		LM_ERR("invalid uri <%.*s>\n", dest->uri.len, dest->uri.s);
		goto err;
	}

	if(ka_str_copy(owner, &(dest->owner), NULL) < 0)
		goto err;

	dest->flags = flags;
	dest->statechanged_clb = callback;
	dest->user_attr = user_attr;

	dest->next = ka_destinations_list->first;
	ka_destinations_list->first = dest;

	return 0;

err:
	if(dest) {
		if(dest->uri.s)
			shm_free(dest->uri.s);

		shm_free(dest);
	}
	return -1;
}

/*
 * TODO
 */
int ka_rm_dest()
{
	return -1;
}

/*
 *
 */
ka_state ka_destination_state(str *destination)
{
	ka_dest_t *ka_dest = NULL;

	for(ka_dest = ka_destinations_list->first; ka_dest != NULL;
			ka_dest = ka_dest->next) {
		if((destination->len == ka_dest->uri.len - 4)
				&& (strncmp(ka_dest->uri.s + 4, destination->s, ka_dest->uri.len - 4)
						== 0)) {
			break;
		}
	}

	if(ka_dest == NULL) {
		return (-1);
	}

	return ka_dest->state;
}

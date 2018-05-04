/*
 * Copyright (C) 2015 Hugh Waite
 * Copyright (C) 2016 Edvina AB, Olle E. Johansson
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

#include "functions.h"
#include "curl_api.h"

int bind_httpc_api(httpc_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->http_connect = curl_con_query_url;
	api->http_client_query = http_client_query;
	api->http_connection_exists = http_connection_exists;
	api->http_get_content_type = http_get_content_type;

	return 0;
}

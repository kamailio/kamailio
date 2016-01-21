/*
 * Copyright (C) 2015 Hugh Waite
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

/*!
 * \file
 * \brief Kamailio curl :: Core API include file
 * \ingroup curl
 * Module: \ref curl
 */
#ifndef _CURL_API_H_
#define _CURL_API_H_

#include "../../sr_module.h"
#include "functions.h"

typedef int (*curlapi_curlconnect_f)(struct sip_msg *msg, const str *connection, const str* _url, str* _result, const char *contenttype, const str* _post);

typedef struct curl_api {
	curlapi_curlconnect_f	curl_connect;
} curl_api_t;

typedef int (*bind_curl_api_f)(curl_api_t *api);
int bind_curl_api(curl_api_t *api);

/**
 * @brief Load the CURL API
 */
static inline int curl_load_api(curl_api_t *api)
{
	bind_curl_api_f bindcurl;

	bindcurl = (bind_curl_api_f)find_export("bind_curl", 0, 0);
	if(bindcurl == 0) {
		LM_ERR("cannot find bind_curl\n");
		return -1;
	}
	if (bindcurl(api) < 0)
	{
		LM_ERR("cannot bind curl api\n");
		return -1;
	}
	return 0;
}

#endif

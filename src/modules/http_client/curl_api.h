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
 * \brief Kamailio http_client :: Core API include file
 * \ingroup http_client
 * Module: \ref http_client
 */
#ifndef _CURL_API_H_
#define _CURL_API_H_

#include "../../core/sr_module.h"
#include "functions.h"
#include "curlcon.h"

typedef int (*httpcapi_httpconnect_f)(struct sip_msg *msg,
		const str *connection, const str *_url, str *_result,
		const char *contenttype, const str *_post);
typedef int (*httpcapi_httpquery_f)(
		struct sip_msg *_m, char *_url, str *_dst, char *_post, char *_hdrs);
typedef int (*httpcapi_curlcon_exists_f)(str *_name);
typedef char *(*httpcapi_res_content_type_f)(const str *_name);


typedef struct httpc_api
{
	httpcapi_httpconnect_f http_connect;
	httpcapi_httpquery_f http_client_query;
	httpcapi_curlcon_exists_f http_connection_exists;
	httpcapi_res_content_type_f http_get_content_type;
} httpc_api_t;

typedef int (*bind_httpc_api_f)(httpc_api_t *api);
int bind_httpc_api(httpc_api_t *api);

/**
 * @brief Load the http_client API
 */
static inline int httpc_load_api(httpc_api_t *api)
{
	bind_httpc_api_f bindhttpc;

	bindhttpc = (bind_httpc_api_f)find_export("bind_http_client", 0, 0);
	if(bindhttpc == 0) {
		LM_ERR("cannot find bind_http_client\n");
		return -1;
	}
	if(bindhttpc(api) < 0) {
		LM_ERR("cannot bind http_client api\n");
		return -1;
	}
	return 0;
}

#endif

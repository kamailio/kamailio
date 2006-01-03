/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcap/xcap_client.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <curl/curl.h>

static size_t write_data_func(void *ptr, size_t size, size_t nmemb, void *stream)
{
	int s = size * nmemb;
/*	TRACE_LOG("%d bytes writen\n", s);*/
	if (s != 0) {
		if (dstr_append((dstring_t*)stream, ptr, s) != 0) return 0;
	}
	return s;
}

int xcap_query(xcap_query_t *query, char **buf, int *bsize)
{
	CURLcode res = -1;
	CURL *handle;
	dstring_t data;
	char *auth = NULL;
	int i;
	long auth_methods;
	
	if (!query) return -1;
	if (!buf) return -1;

	i = 0;
	if (query->auth_user) i += strlen(query->auth_user);
	if (query->auth_pass) i += strlen(query->auth_pass);
	if (i > 0) {
		/* do authentication */
		auth = (char *)cds_malloc(i + 2);
		if (!auth) return -1;
		sprintf(auth, "%s:%s", query->auth_user ? query->auth_user: "",
				query->auth_pass ? query->auth_pass: "");
	}

	auth_methods = CURLAUTH_BASIC | CURLAUTH_DIGEST;
	
	dstr_init(&data, 512);
	
	handle = curl_easy_init();
	if (handle) {
		curl_easy_setopt(handle, CURLOPT_URL, query->uri);
		
		/* do not store data into a file - store them in memory */
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data_func);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);

		/* be quiet */
		curl_easy_setopt(handle, CURLOPT_MUTE, 1);
		
		/* non-2xx => error */
		curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1);

		/* auth */
		curl_easy_setopt(handle, CURLOPT_HTTPAUTH, auth_methods); /* TODO possibility of selection */
		curl_easy_setopt(handle, CURLOPT_NETRC, CURL_NETRC_IGNORED);
		curl_easy_setopt(handle, CURLOPT_USERPWD, auth);

		/* SSL */
		if (query->enable_unverified_ssl_peer) {
			curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		
		/* follow redirects (needed for apache mod_speling - case insesitive names) */
		curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
		
		/* Accept headers */
		
		res = curl_easy_perform(handle);
		curl_easy_cleanup(handle);
	}
	if (res == 0) {
		*bsize = dstr_get_data_length(&data);
		if (*bsize) {
			*buf = (char*)cds_malloc(*bsize);
			if (!*buf) {
				res = -1;
				*bsize = 0;
			}
			else dstr_get_data(&data, *buf);
		}
	}
	dstr_destroy(&data);
	if (auth) cds_free(auth);
	return res;
}


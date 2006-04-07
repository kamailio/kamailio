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
#include <cds/logger.h>

static const str_t *get_xcap_doc_dir(xcap_document_type_t doc_type)
{
	static str_t pres_rules = STR_STATIC_INIT("pres-rules");
	static str_t im_rules = STR_STATIC_INIT("im-rules");
	static str_t rls_services = STR_STATIC_INIT("rls-services");
	static str_t resource_lists = STR_STATIC_INIT("resource-lists");

	switch (doc_type) {
		case xcap_doc_pres_rules: return &pres_rules;
		case xcap_doc_im_rules: return &im_rules;
		case xcap_doc_rls_services: return &rls_services;
		case xcap_doc_resource_lists: return &resource_lists;
		/* when new doc_type added, there will be a warning -> add it there */
	}
	WARN_LOG("unknow XCAP document type\n");
	return NULL;
}

static const str_t *get_default_user_doc(xcap_document_type_t doc_type)
{
	static str_t pres_rules = STR_STATIC_INIT("presence-rules.xml");
	static str_t im_rules = STR_STATIC_INIT("im-rules.xml");
	static str_t rls_services = STR_STATIC_INIT("rls-services.xml");
	static str_t resource_lists = STR_STATIC_INIT("resource-list.xml");

	switch (doc_type) {
		case xcap_doc_pres_rules: return &pres_rules;
		case xcap_doc_im_rules: return &im_rules;
		case xcap_doc_rls_services: return &rls_services;
		case xcap_doc_resource_lists: return &resource_lists;
		/* when new doc_type added, there will be a warning -> add it there */
	}
	WARN_LOG("unknow XCAP document type\n");
	return NULL;
}

static int ends_with_separator(str_t *s)
{
	if (!is_str_empty(s))
		if (s->s[s->len - 1] == '/') return 1;
	return 0;
}

char *xcap_uri_for_users_document(xcap_document_type_t doc_type,
		const str_t *username, 
		const str_t*filename,
		xcap_query_params_t *params)
{
	dstring_t s;
	/* int res = RES_OK; */
	int l = 0;
	char *dst = NULL;

	dstr_init(&s, 128);
	if (params) {
		dstr_append_str(&s, &params->xcap_root);
		if (!ends_with_separator(&params->xcap_root))
			dstr_append(&s, "/", 1);
	}
	else dstr_append(&s, "/", 1);
	dstr_append_str(&s, get_xcap_doc_dir(doc_type));
	dstr_append_zt(&s, "/users/");
	dstr_append_str(&s, username);
	dstr_append(&s, "/", 1);
	if (filename) dstr_append_str(&s, filename);
	else {
		/* default filename if NULL */
		dstr_append_str(&s, get_default_user_doc(doc_type));
	}
	/* res = dstr_get_str(&s, dst); */
	
	l = dstr_get_data_length(&s);
	if (l > 0) {
		dst = (char *)cds_malloc(l + 1);
		if (dst) {
			dstr_get_data(&s, dst);
			dst[l] = 0;
		}
		else ERROR_LOG("can't allocate memory (%d bytes)\n", l);
	}
	
	dstr_destroy(&s);
	return dst;
}


char *xcap_uri_for_global_document(xcap_document_type_t doc_type,
		const str_t *filename, 
		xcap_query_params_t *params)
{
	dstring_t s;
	/* int res = RES_OK; */
	char *dst = NULL;
	int l = 0;

	dstr_init(&s, 128);
	if (params) {
		dstr_append_str(&s, &params->xcap_root);
		if (!ends_with_separator(&params->xcap_root))
			dstr_append(&s, "/", 1);
	}
	else dstr_append(&s, "/", 1);
	dstr_append_str(&s, get_xcap_doc_dir(doc_type));
	if (filename) {
		dstr_append_zt(&s, "/global/");
		dstr_append_str(&s, filename);
	}
	else {
		/* default filename if NULL */
		dstr_append_zt(&s, "/global/index");
	}
	/* res = dstr_get_str(&s, dst); */
	
	l = dstr_get_data_length(&s);
	if (l > 0) {
		dst = (char *)cds_malloc(l + 1);
		if (dst) {
			dstr_get_data(&s, dst);
			dst[l] = 0;
		}
	}
	
	dstr_destroy(&s);
	return dst;
}

#ifdef SER

#include "sr_module.h"

int xcap_query(const char *uri, 
		xcap_query_params_t *params, char **buf, int *bsize)
{
	static xcap_query_func query = NULL;
	static int initialized = 0;

	if (!initialized) {
		query = (xcap_query_func)find_export("xcap_query", 0, -1);
		initialized = 1;
		if (!query) WARN_LOG("No XCAP query support! (Missing module?)\n");
	}
	if (!query) {
		/* no function for doing XCAP queries */
		return -1;
	}
	
	/* all XCAP queries are done through XCAP module */
	return query(uri, params, buf, bsize);
}

#else /* compiled WITHOUT SER */

#include <curl/curl.h>

static size_t write_data_func(void *ptr, size_t size, size_t nmemb, void *stream)
{
	int s = size * nmemb;
/*	TRACE_LOG("%d bytes writen\n", s);*/
	if (s != 0) {
		if (dstr_append((dstring_t*)stream, ptr, s) != 0) {
			ERROR_LOG("can't append %d bytes into data buffer\n", s);
			return 0;
		}
	}
	return s;
}


int xcap_query(const str_t *uri, xcap_query_params_t *params, char **buf, int *bsize)
{
	CURLcode res = -1;
	static CURL *handle = NULL;
	dstring_t data;
	char *auth = NULL;
	int i;
	long auth_methods;
	
	if (!uri) return -1;
	if (!buf) return -1;

	i = 0;
	if (params) {
		if (params->auth_user) i += strlen(params->auth_user);
		if (params->auth_pass) i += strlen(params->auth_pass);
	}
	if (i > 0) {
		/* do authentication */
		auth = (char *)cds_malloc(i + 2);
		if (!auth) return -1;
		sprintf(auth, "%s:%s", params->auth_user ? params->auth_user: "",
				params->auth_pass ? params->auth_pass: "");
	}

	auth_methods = CURLAUTH_BASIC | CURLAUTH_DIGEST;
	
	dstr_init(&data, 512);
	
	if (!handle) handle = curl_easy_init(); 
	if (handle) {
		curl_easy_setopt(handle, CURLOPT_URL, uri);
		/* TRACE_LOG("uri: %s\n", uri ? uri : "<null>"); */
		
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
		if (params) {
			if (params->enable_unverified_ssl_peer) {
				curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
				curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
			}
		}
		
		/* follow redirects (needed for apache mod_speling - case insesitive names) */
		curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
		
	/*	curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 1);
		curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10);*/
		
		/* Accept headers */
		
		res = curl_easy_perform(handle);
		/* curl_easy_cleanup(handle); */ /* FIXME: experimental */
	}
	else ERROR_LOG("can't initialize curl handle\n");
	if (res == 0) {
		*bsize = dstr_get_data_length(&data);
		if (*bsize) {
			*buf = (char*)cds_malloc(*bsize);
			if (!*buf) {
				ERROR_LOG("can't allocate %d bytes\n", *bsize);
				res = -1;
				*bsize = 0;
			}
			else dstr_get_data(&data, *buf);
		}
	}
	else DEBUG_LOG("curl error: %d\n", res);
	dstr_destroy(&data);
	if (auth) cds_free(auth);
	return res;
}

#endif

void free_xcap_params_content(xcap_query_params_t *params)
{
	if (params) {
		str_free_content(&params->xcap_root);
		if (params->auth_user) cds_free(params->auth_user);
		if (params->auth_pass) cds_free(params->auth_pass);
		memset(params, 0, sizeof(*params));
	}
}

int dup_xcap_params(xcap_query_params_t *dst, xcap_query_params_t *src)
{
	int res = -10;
	
	if (dst) memset(dst, 0, sizeof(*dst));
	
	if (src && dst) {
		res = 0;
		
		res = str_dup(&dst->xcap_root, &src->xcap_root);
		
		if ((res == 0) && (src->auth_user)) {
			dst->auth_user = zt_strdup(src->auth_user);
			if (!dst->auth_user) res = -1;
		}
		if ((res == 0) && (src->auth_pass)) {
			dst->auth_pass = zt_strdup(src->auth_pass);
			if (!dst->auth_pass) res= -2;
		}

		if (res != 0) free_xcap_params_content(dst);
	}
	
	return res;
}

int str2xcap_params(xcap_query_params_t *dst, const str_t *src)
{
	return -1;
}

int xcap_params2str(str_t *dst, const xcap_query_params_t *src)
{
	return -1;
}


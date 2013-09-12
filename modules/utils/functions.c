/*
 * script functions of utils module
 *
 * Copyright (C) 2008 Juha Heinanen
 * Copyright (C) 2013 Carsten Bock, ng-voice GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!
 * \file
 * \brief SIP-router utils :: 
 * \ingroup utils
 * Module: \ref utils
 */


#include <curl/curl.h>

#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../route_struct.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../lvalue.h"

#include "utils.h"


/* 
 * curl write function that saves received data as zero terminated
 * to stream. Returns the amount of data taken care of.
 */
size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream)
{
    /* Allocate memory and copy */
    char* data;

    data = (char*)malloc((size* nmemb) + 1);
    if (data == NULL) {
	LM_ERR("cannot allocate memory for stream\n");
	return CURLE_WRITE_ERROR;
    }

    memcpy(data, (char*)ptr, size* nmemb);
    data[nmemb] = '\0';
        
    *((char**) stream) = data;
    
    return size* nmemb;
 }


/* 
 * Performs http_query and saves possible result (first body line of reply)
 * to pvar.
 */
int http_query(struct sip_msg* _m, char* _url, char* _dst, char* _post)
{
    CURL *curl;
    CURLcode res;  
    str value, post_value;
    char *url, *at, *post;
    char* stream;
    long stat;
    pv_spec_t *dst;
    pv_value_t val;
    double download_size;

    if (fixup_get_svalue(_m, (gparam_p)_url, &value) != 0) {
	LM_ERR("cannot get page value\n");
	return -1;
    }

    curl = curl_easy_init();
    if (curl == NULL) {
	LM_ERR("failed to initialize curl\n");
	return -1;
    }

    url = pkg_malloc(value.len + 1);
    if (url == NULL) {
	curl_easy_cleanup(curl);
	LM_ERR("cannot allocate pkg memory for url\n");
	return -1;
    }
    memcpy(url, value.s, value.len);
    *(url + value.len) = (char)0;
    curl_easy_setopt(curl, CURLOPT_URL, url);

    if (_post) {
        /* Now specify we want to POST data */ 
	curl_easy_setopt(curl, CURLOPT_POST, 1L);

    	if (fixup_get_svalue(_m, (gparam_p)_post, &post_value) != 0) {
		LM_ERR("cannot get post value\n");
		pkg_free(url);
		return -1;
    	}
        post = pkg_malloc(post_value.len + 1);
        if (post == NULL) {
		curl_easy_cleanup(curl);
		pkg_free(url);
        	LM_ERR("cannot allocate pkg memory for post\n");
        	return -1;
	}
	memcpy(post, post_value.s, post_value.len);
	*(post + post_value.len) = (char)0;
 	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
    }
       

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)http_query_timeout);

    stream = NULL;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);

    res = curl_easy_perform(curl);  
    pkg_free(url);
    if (_post) {
	pkg_free(post);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
	LM_ERR("failed to perform curl\n");
	return -1;
    }

    curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &stat);
    if ((stat >= 200) && (stat < 400)) {
	curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &download_size);
	LM_DBG("http_query download size: %u\n", (unsigned int)download_size);
	/* search for line feed */
	at = memchr(stream, (char)10, download_size);
	if (at == NULL) {
	    /* not found: use whole stream */
	    at = stream + (unsigned int)download_size;
	}
	val.rs.s = stream;
	val.rs.len = at - stream;
	LM_DBG("http)query result: %.*s\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_dst;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
    }
	
    return stat;
}

/*
 * script functions of curl module
 *
 * Copyright (C) 2015 Olle E. Johansson, Edvina AB
 *
 * Based on functions from siputil
 * 	Copyright (C) 2008 Juha Heinanen
 * 	Copyright (C) 2013 Carsten Bock, ng-voice GmbH
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
 * \brief Kamailio curl :: script functions
 * \ingroup curl
 * Module: \ref curl
 */


#include <curl/curl.h>

#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../route_struct.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../lvalue.h"

#include "curl.h"
#include "curlcon.h"

/* Forward declaration */
static int curL_query_url(struct sip_msg* _m, char* _url, char* _dst, const char *username, 
		const char *secret, const char *contenttype, char* _post, const unsigned int timeout,
		unsigned int http_follow_redirect);

/* 
 * curl write function that saves received data as zero terminated
 * to stream. Returns the amount of data taken care of.
 *
 * This function may be called multiple times for larger responses, 
 * so it reallocs + concatenates the buffer as needed.
 */
size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream_ptr)
{
    http_res_stream_t *stream = (http_res_stream_t *) stream_ptr;

    stream->buf = (char *) pkg_realloc(stream->buf, stream->curr_size + 
				(size * nmemb) + 1);

    if (stream->buf == NULL) {
	LM_ERR("cannot allocate memory for stream\n");
	return CURLE_WRITE_ERROR;
    }

    memcpy(&stream->buf[stream->pos], (char *) ptr, (size * nmemb));

    stream->curr_size += ((size * nmemb) + 1);
    stream->pos += (size * nmemb);

    stream->buf[stream->pos + 1] = '\0';

    return size * nmemb;
 }


/*! Send query to server, optionally post data.
 */
static int curL_query_url(struct sip_msg* _m, char* _url, char* _dst, const char *_username, const char *_secret, const char *contenttype, char* _post, unsigned int timeout, unsigned int http_follow_redirect)
{
    CURL *curl;
    CURLcode res;  
    str value, post_value;
    char *url, *at, *post;
    http_res_stream_t stream;
    long stat;
    pv_spec_t *dst;
    pv_value_t val;
    double download_size;
    double total_time;

    memset(&stream, 0, sizeof(http_res_stream_t));

#ifdef SKREP
    if (fixup_get_svalue(_m, (gparam_p)_url, &value) != 0) {
	LM_ERR("cannot get page value\n");
	return -1;
    }
#endif
	value.s = _url;
	value.len = strlen(_url);

    curl = curl_easy_init();
    if (curl == NULL) {
	LM_ERR("failed to initialize curl\n");
	return -1;
    }

    url = pkg_malloc(value.len + 1);
    if (url == NULL) {
	curl_easy_cleanup(curl);
	LM_ERR("cannot allocate pkg memory for url %d\n", (value.len + 1));
	return -1;
    }
    memcpy(url, value.s, value.len);
    *(url + value.len) = (char)0;
    LM_DBG("****** ##### CURL URL %s _url [%.*s]\n", url, value.len, value.s);
    res = curl_easy_setopt(curl, CURLOPT_URL, url);

    if (_post) {
        /* Now specify we want to POST data */ 
	res |= curl_easy_setopt(curl, CURLOPT_POST, 1L);
	/* Set the content-type of the DATA */

#ifdef SKREP
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
#endif
 	res |= curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _post);
	
    }

    if (_username) {
 	res |= curl_easy_setopt(curl, CURLOPT_USERNAME, _username);
	res |= curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (CURLAUTH_DIGEST|CURLAUTH_BASIC));
    }
    if (_secret) {
 	res |= curl_easy_setopt(curl, CURLOPT_PASSWORD, _secret);
    }

       

    res |= curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long) 1);
    res |= curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) timeout);
    res |= curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long) http_follow_redirect);


    res |= curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
    res |= curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    	if (res != CURLE_OK) {
		/* PANIC */
		LM_ERR("Could not set CURL options. Library error \n");
	} else {
    		res = curl_easy_perform(curl);  
	}
    pkg_free(url);
#ifdef SKREP
    if (_post) {
	pkg_free(post);
    }
#endif

    if (res != CURLE_OK) {
	/* http://curl.haxx.se/libcurl/c/libcurl-errors.html */
	if (res == CURLE_COULDNT_CONNECT) {
		LM_WARN("failed to connect() to host\n");
	} else if ( res == CURLE_COULDNT_RESOLVE_HOST ) {
		LM_WARN("couldn't resolve host\n");
	} else {
		LM_ERR("failed to perform curl (%d)\n", res);
	}

	curl_easy_cleanup(curl);
	if(stream.buf) {
		pkg_free(stream.buf);
	}
	counter_inc(connfail);
	return -1;
    }

	/* HTTP_CODE CHANGED TO CURLINFO_RESPONSE_CODE in curl > 7.10.7 */
    curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &stat);
/* CURLINFO_CONTENT_TYPE, char *    */

    if ((stat >= 200) && (stat < 500)) {
	curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &download_size);
    	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
	LM_DBG("http_query download size: %u Time : %ld \n", (unsigned int)download_size, (long) total_time);

	/* search for line feed */
	at = memchr(stream.buf, (char)10, download_size);
	if (at == NULL) {
	    /* not found: use whole stream */
	    at = stream.buf + (unsigned int)download_size;
	}
	val.rs.s = stream.buf;
	val.rs.len = at - stream.buf;
	LM_DBG("http_query result: %.*s\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_dst;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
    }
    	if (stat == 200) {
		counter_inc(connok);
	} else {
		counter_inc(connfail);
	}
	
	/* CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ... ); */
    curl_easy_cleanup(curl);
    pkg_free(stream.buf);
    return stat;
}


/*! Run a query based on a connection definition */
int curl_con_query_url(struct sip_msg* _m, char *connection, char* _url, char* _result, const char *contenttype, char* _post)
{
	curl_con_t *conn = NULL;
	str connstr;
	char usernamebuf[BUFSIZ/2];
	char passwordbuf[BUFSIZ/2];
	char connurlbuf[BUFSIZ/2];
	char urlbuf[512];
	unsigned int len = 0;

	memset(usernamebuf,0,sizeof(usernamebuf));
	memset(passwordbuf,0,sizeof(passwordbuf));
	memset(connurlbuf,0,sizeof(connurlbuf));
	memset(urlbuf,0,sizeof(urlbuf));

	/* Find connection if it exists */
	if (!connection) {
		LM_ERR("No cURL connection specified\n");
		return -1;
	}
	LM_DBG("******** CURL Connection %s\n", connection);
	connstr.s = connection;
	connstr.len = strlen(connection);
	conn = curl_get_connection(&connstr);
	if (conn == NULL) {
		LM_ERR("No cURL connection found: %s\n", connection);
		return -1;
	}
	strncpy(usernamebuf, conn->username.s, conn->username.len);
	strncpy(passwordbuf, conn->password.s, conn->password.len);
	strncpy(connurlbuf, conn->url.s, conn->url.len);

	strncpy(urlbuf,conn->schema.s, conn->schema.len);
	snprintf(&urlbuf[conn->schema.len],(sizeof(urlbuf) - conn->schema.len), "://%s%s%s", connurlbuf, 
		(_url[0] && _url[0] == '/')?"":(_url[0] != '\0' ? "/": ""), _url);
	LM_DBG("***** #### ***** CURL URL: %s \n", urlbuf);
	if (_post && *_post) {
		LM_DBG("***** #### ***** CURL POST data: %s \n", _post);
	}

	/* TODO: Concatenate URL in connection with URL given in function */
	return curL_query_url(_m, urlbuf, _result, usernamebuf, passwordbuf, (contenttype ? contenttype : "text/plain"), _post,
		conn->timeout, conn->http_follow_redirect );
}


/*!
 * Performs http_query and saves possible result (first body line of reply)
 * to pvar.
 * This is the same http_query as used to be in the utils module.
 */
int http_query(struct sip_msg* _m, char* _url, char* _dst, char* _post)
{
	int res;

	res =  curL_query_url(_m, _url, _dst, NULL, NULL, "text/plain", _post, default_connection_timeout, default_http_follow_redirect);

	return res;
}

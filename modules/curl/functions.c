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

#include "curl.h"
#include "curlcon.h"

/* Forward declaration */
static int curL_query_url(struct sip_msg* _m, const char* _url, str* _dst, const char *username,
		const char *secret, const char *contenttype, const char* _post, const unsigned int timeout,
		unsigned int http_follow_redirect, unsigned int oneline, unsigned int maxdatasize);

/* 
 * curl write function that saves received data as zero terminated
 * to stream. Returns the amount of data taken care of.
 *
 * This function may be called multiple times for larger responses, 
 * so it reallocs + concatenates the buffer as needed.
 */
size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream_ptr)
{
    curl_res_stream_t *stream = (curl_res_stream_t *) stream_ptr;


    if (stream->max_size == 0 || stream->curr_size < stream->max_size) {
        char *tmp = (char *) pkg_realloc(stream->buf, stream->curr_size + (size * nmemb));

        if (tmp == NULL) {
            LM_ERR("cannot allocate memory for stream\n");
            return CURLE_WRITE_ERROR;
    	}
        stream->buf = tmp;

    	memcpy(&stream->buf[stream->pos], (char *) ptr, (size * nmemb));

        stream->curr_size += ((size * nmemb));
    	stream->pos += (size * nmemb);

    }  else {
    	LM_DBG("****** ##### CURL Max datasize exceeded: max  %u current %u\n", (unsigned int) stream->max_size, (unsigned int)stream->curr_size);
    }

    return size * nmemb;
 }


/*! Send query to server, optionally post data.
 */
static int curL_query_url(struct sip_msg* _m, const char* _url, str* _dst, const char *_username, const char *_secret, const char *contenttype, const char* _post, unsigned int timeout, unsigned int http_follow_redirect, unsigned int oneline, unsigned int maxdatasize)
{
    CURL *curl;
    CURLcode res;  
    char *at = NULL;
    curl_res_stream_t stream;
    long stat;
    str rval;
    double download_size;
    double total_time;
    struct curl_slist *headerlist = NULL;

    memset(&stream, 0, sizeof(curl_res_stream_t));
    stream.max_size = (size_t) maxdatasize;

    curl = curl_easy_init();
    if (curl == NULL) {
	LM_ERR("failed to initialize curl\n");
	return -1;
    }

    LM_DBG("****** ##### CURL URL [%s] \n", _url);
    res = curl_easy_setopt(curl, CURLOPT_URL, _url);

    if (_post) {
	char ctype[256];

	ctype[0] = '\0';
	snprintf(ctype, sizeof(ctype), "Content-Type: %s", contenttype);

        /* Now specify we want to POST data */ 
	res |= curl_easy_setopt(curl, CURLOPT_POST, 1L);
	/* Set the content-type of the DATA */
	headerlist = curl_slist_append(headerlist, ctype);
	res |= curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);

	/* Tell CURL we want to upload using POST */

 	res |= curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _post);

    }

    if (maxdatasize) {
	/* Maximum data size to download - we always download full response, but
	   cut it off before moving to pvar */
    	LM_DBG("****** ##### CURL Max datasize %u\n", maxdatasize);
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
    if (headerlist) {
    	curl_slist_free_all(headerlist);
    }

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
    if(res == CURLE_OK) {
    	char *ct;

    	/* ask for the content-type of the response */
    	res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);

    	if(ct) {
        	LM_DBG("We received Content-Type: %s\n", ct);
        }
    }

    if ((stat >= 200) && (stat < 500)) {
	double datasize = download_size;

	curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &download_size);
    	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
	LM_DBG("  -- curl download size: %u Time : %ld \n", (unsigned int)download_size, (long) total_time);

	if (download_size > 0) {

	if (oneline) {
		/* search for line feed */
		at = memchr(stream.buf, (char)10, download_size);
		datasize = (double) (at - stream.buf);
		LM_DBG("  -- curl download size cut to first line: %d \n", (int) datasize);
	}
	if (at == NULL) {
		if (maxdatasize && ((unsigned int) download_size) > maxdatasize) {
			/* Limit at maximum data size */
			datasize = (double) maxdatasize;
			LM_DBG("  -- curl download size cut to maxdatasize : %d \n", (int) datasize);
		} else {
			/* Limit at actual downloaded data size */
			datasize = (double) download_size;
			LM_DBG("  -- curl download size cut to download_size : %d \n", (int) datasize);
	    		//at = stream.buf + (unsigned int) download_size;
		}
	}
		/* Create a STR object */

		rval.s = stream.buf;
		rval.len = datasize;
		/* Duplicate string to return */
		pkg_str_dup(_dst, &rval);
	LM_DBG("curl query result: Length %d %.*s \n", rval.len, rval.len, rval.s);
	} else {
		_dst->s = NULL;
		_dst->len = 0;
	}
    }
    if (stat == 200) {
	counter_inc(connok);
    } else {
	counter_inc(connfail);
    }

    /* CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ... ); */
    curl_easy_cleanup(curl);
    if (stream.buf != NULL)
	    pkg_free(stream.buf);
    return stat;
}


/*! Run a query based on a connection definition */
int curl_con_query_url(struct sip_msg* _m, const str *connection, const str* url, str* result, const char *contenttype, const str* post)
{
	curl_con_t *conn = NULL;
	char *urlbuf = NULL;
	char *username_str = NULL;
	char *password_str = NULL;
	char *postdata = NULL;

	unsigned int maxdatasize = default_maxdatasize;
	int res;

	/* Find connection if it exists */
	if (!connection) {
		LM_ERR("No cURL connection specified\n");
		return -1;
	}
	LM_DBG("******** CURL Connection %.*s\n", connection->len, connection->s);
	conn = curl_get_connection((str*)connection);
	if (conn == NULL) {
		LM_ERR("No cURL connection found: %.*s\n", connection->len, connection->s);
		return -1;
	}
	LM_DBG("******** CURL Connection found %.*s\n", connection->len, connection->s);
	if (conn->username.s != NULL && conn->username.len > 0)
	{
		username_str = as_asciiz(&conn->username);
	}
	if (conn->password.s != NULL && conn->password.len > 0)
	{
		password_str = as_asciiz(&conn->password);
	}
	maxdatasize = conn->maxdatasize;


	if (url && (url->len > 0) && (url->s != NULL)) {
		int url_len = conn->schema.len + 3 + conn->url.len + 1 + url->len + 1;
		urlbuf = pkg_malloc(url_len);
		if (urlbuf == NULL)
		{
			res = -1;
			goto error;
		}
		snprintf(urlbuf, url_len, "%.*s://%.*s%s%.*s",
			conn->schema.len, conn->schema.s,
			conn->url.len, conn->url.s,
			(url->s[0] == '/')? "" : "/",
			url->len, url->s);
	} else {
		int url_len = conn->schema.len + 3 + conn->url.len + 1;
		urlbuf = pkg_malloc(url_len);
		if (urlbuf == NULL)
		{
			res = -1;
			goto error;
		}
		snprintf(urlbuf, url_len, "%.*s://%.*s",
			conn->schema.len, conn->schema.s,
			conn->url.len, conn->url.s);
	}
	LM_DBG("***** #### ***** CURL URL: %s \n", urlbuf);

	if (post && (post->len > 0) && (post->s != NULL)) {

		/* Allocated using pkg_memory */
		postdata = as_asciiz((str*)post);
		if (postdata  == NULL) {
        	    ERR("Curl: No memory left\n");
		    res = -1;
		    goto error;
                }
		LM_DBG("***** #### ***** CURL POST data: %s Content-type %s\n", postdata, contenttype);
	}

	res = curL_query_url(_m, urlbuf, result, username_str, password_str, (contenttype ? contenttype : "text/plain"), postdata,
		conn->timeout, conn->http_follow_redirect, 0, (unsigned int) maxdatasize );

	LM_DBG("***** #### ***** CURL DONE : %s \n", urlbuf);
error:
	if (urlbuf != NULL) {
		pkg_free(urlbuf);
	}
	if (username_str != NULL) {
		pkg_free(username_str);
	}
	if (password_str != NULL) {
		pkg_free(password_str);
	}
	if (postdata != NULL) {
		pkg_free(postdata);
	}
	return res;
}


/*!
 * Performs http_query and saves possible result (first body line of reply)
 * to pvar.
 * This is the same http_query as used to be in the utils module.
 */
int http_query(struct sip_msg* _m, char* _url, str* _dst, char* _post)
{
	int res;

	res =  curL_query_url(_m, _url, _dst, NULL, NULL, "text/plain", _post, default_connection_timeout, default_http_follow_redirect, 1, 0);

	return res;
}

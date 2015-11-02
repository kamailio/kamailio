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
    	stream->buf = (char *) pkg_realloc(stream->buf, stream->curr_size + (size * nmemb) + 1);

    	if (stream->buf == NULL) {
		LM_ERR("cannot allocate memory for stream\n");
		return CURLE_WRITE_ERROR;
    	}

    	memcpy(&stream->buf[stream->pos], (char *) ptr, (size * nmemb));

    	stream->curr_size += ((size * nmemb) + 1);
    	stream->pos += (size * nmemb);

    	stream->buf[stream->pos + 1] = '\0';
    }  else {
    	LM_DBG("****** ##### CURL Max datasize exceeded: max  %u current %u\n", (unsigned int) stream->max_size, (unsigned int)stream->curr_size);
    }

    return size * nmemb;
 }


/*! Send query to server, optionally post data.
 */
static int curL_query_url(struct sip_msg* _m, char* _url, char* _dst, const char *_username, const char *_secret, const char *contenttype, char* _post, unsigned int timeout, unsigned int http_follow_redirect, unsigned int oneline, unsigned int maxdatasize)
{
    CURL *curl;
    CURLcode res;  
    str value;
    char *url, *at = NULL;
    curl_res_stream_t stream;
    long stat;
    pv_spec_t *dst;
    pv_value_t val;
    double download_size;
    double total_time;
    struct curl_slist *headerlist = NULL;

    memset(&stream, 0, sizeof(curl_res_stream_t));
    stream.max_size = (size_t) maxdatasize;

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
    pkg_free(url);
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
	int datasize = (int) download_size;

	curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &download_size);
    	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
	LM_DBG("  -- curl download size: %u Time : %ld \n", (unsigned int)download_size, (long) total_time);

	/* search for line feed */
	if (oneline) {
		at = memchr(stream.buf, (char)10, download_size);
		datasize = (int) (at - stream.buf);
		LM_DBG("  -- curl download size cut to first line: %d \n", datasize);
	}
	if (at == NULL) {
		if (maxdatasize && ((unsigned int) download_size) > maxdatasize) {
			/* Limit at maximum data size */
			datasize = (int) maxdatasize;
			LM_DBG("  -- curl download size cut to maxdatasize : %d \n", datasize);
		} else {
			/* Limit at actual downloaded data size */
			datasize = (int) download_size;
			LM_DBG("  -- curl download size cut to download_size : %d \n", datasize);
	    		//at = stream.buf + (unsigned int) download_size;
		}
	}
	/* Create a STR object */
	val.rs.s = stream.buf;
	val.rs.len = datasize;
	LM_DBG("curl query result: Length %d %.*s \n", val.rs.len, val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_dst;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
	LM_DBG("---------- curl result pvar set. \n");
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
	str urlbuf2;
	char *urlbuf3 = NULL;

	str postdatabuf;
	char *postdata = NULL;
	unsigned int maxdatasize = default_maxdatasize;
	int res;

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
	maxdatasize = conn->maxdatasize;

	LM_DBG("******** CURL Connection found %s\n", connection);

	if (_url && *_url) {
		if(pv_printf_s(_m, (pv_elem_t*) _url, &urlbuf2) != 0) {
               		LM_ERR("curl :: unable to handle post data %s\n", _url);
               		return -1;
        	}
        	if(urlbuf2.s==NULL || urlbuf2.len == 0) {
               		LM_ERR("curl :: invalid url parameter\n");
               		return -1;
        	}
		LM_DBG("******** CURL Connection URL parsed for  %s\n", connection);
		/* Allocated using pkg_memory */
		urlbuf3 = as_asciiz(&urlbuf2);
		if (urlbuf3  == NULL) {
       			ERR("Curl: No memory left\n");
          		return -1;
        	}
		LM_DBG("******** CURL URL string after PV parsing %s\n", urlbuf3);
	} else {
		LM_DBG("******** CURL URL string NULL no PV parsing %s\n", _url);
	}
	strncpy(urlbuf, conn->schema.s, conn->schema.len);
	if (urlbuf3 != NULL) {
		snprintf(&urlbuf[conn->schema.len],(sizeof(urlbuf) - conn->schema.len), "://%s%s%s", connurlbuf, 
			(urlbuf3[0] && urlbuf3[0] == '/')?"":(urlbuf3[0] != '\0' ? "/": ""), urlbuf3);
	} else {
		snprintf(&urlbuf[conn->schema.len],(sizeof(urlbuf) - conn->schema.len), "://%s%s%s", connurlbuf, 
			(_url[0] && _url[0] == '/')?"":(_url[0] != '\0' ? "/": ""), _url);
	}

	/* Release the memory allocated by as_asciiz */
	if (urlbuf3 != NULL) {
		pkg_free(urlbuf3);
	}
	LM_DBG("***** #### ***** CURL URL: %s \n", urlbuf);
	if (_post && *_post) {
		 if(pv_printf_s(_m, (pv_elem_t*)_post, &postdatabuf) != 0) {
                	LM_ERR("curl :: unable to handle post data %s\n", _post);
                	return -1;
        	}
        	if(postdatabuf.s==NULL || postdatabuf.len == 0) {
                	LM_ERR("curl :: invalid post data parameter\n");
                	return -1;
        	}
		/* Allocated using pkg_memory */
		postdata = as_asciiz(&postdatabuf);
		if (postdata  == NULL) {
        	    ERR("Curl: No memory left\n");
            	    return -1;
                }
		LM_DBG("***** #### ***** CURL POST data: %s Content-type %s\n", postdata, contenttype);
		
	}

	/* TODO: Concatenate URL in connection with URL given in function */
	res = curL_query_url(_m, urlbuf, _result, usernamebuf, passwordbuf, (contenttype ? contenttype : "text/plain"), postdata,
		conn->timeout, conn->http_follow_redirect, 0, (unsigned int) maxdatasize );

	LM_DBG("***** #### ***** CURL DONE : %s \n", urlbuf);
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
int http_query(struct sip_msg* _m, char* _url, char* _dst, char* _post)
{
	int res;

	res =  curL_query_url(_m, _url, _dst, NULL, NULL, "text/plain", _post, default_connection_timeout, default_http_follow_redirect, 1, 0);

	return res;
}

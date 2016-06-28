/*
 * script functions of http_client module
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
 * \brief Kamailio http_client :: script functions
 * \ingroup http_client
 * Module: \ref http_client
 */


#include <curl/curl.h>

#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../route_struct.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"

#include "http_client.h"
#include "curlcon.h"


typedef struct {
    char *username;
    char *secret;
    char *contenttype;
    char *post;
    char *clientcert;
    char *clientkey;
    char *cacert;
    char *ciphersuites;
    char *http_proxy;
    unsigned int authmethod;
    unsigned int http_proxy_port;
    unsigned int tlsversion;
    unsigned int verify_peer;
    unsigned int verify_host;
    unsigned int timeout;
    unsigned int http_follow_redirect;
    unsigned int oneline;
    unsigned int maxdatasize;
} curl_query_t;

/* Forward declaration */
static int curL_query_url(struct sip_msg* _m, const char* _url, str* _dst, const curl_query_t * const query_params);

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
static int curL_query_url(struct sip_msg* _m, const char* _url, str* _dst, const curl_query_t * const params)
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
    stream.max_size = (size_t) params->maxdatasize;

    curl = curl_easy_init();
    if (curl == NULL) {
	LM_ERR("failed to initialize curl\n");
	return -1;
    }

    LM_DBG("****** ##### CURL URL [%s] \n", _url);
    res = curl_easy_setopt(curl, CURLOPT_URL, _url);

    if (params->post) {
	char ctype[256];

	ctype[0] = '\0';
	snprintf(ctype, sizeof(ctype), "Content-Type: %s", params->contenttype);

        /* Now specify we want to POST data */ 
	res |= curl_easy_setopt(curl, CURLOPT_POST, 1L);
	/* Set the content-type of the DATA */
	headerlist = curl_slist_append(headerlist, ctype);
	res |= curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);

	/* Tell CURL we want to upload using POST */

	res |= curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params->post);

    }

    if (params->maxdatasize) {
	/* Maximum data size to download - we always download full response, but
	   cut it off before moving to pvar */
	LM_DBG("****** ##### CURL Max datasize %u\n", params->maxdatasize);
    }

    if (params->username) {
	res |= curl_easy_setopt(curl, CURLOPT_USERNAME, params->username);
	res |= curl_easy_setopt(curl, CURLOPT_HTTPAUTH, params->authmethod);
    }
    if (params->secret) {
	res |= curl_easy_setopt(curl, CURLOPT_PASSWORD, params->secret);
    }

    /* Client certificate */
    if (params->clientcert != NULL && params->clientkey != NULL) {
        res |= curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
        res |= curl_easy_setopt(curl, CURLOPT_SSLCERT, params->clientcert);

        res |= curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
        res |= curl_easy_setopt(curl, CURLOPT_SSLKEY, params->clientkey);
    }

    if (params->cacert != NULL) {
        res |= curl_easy_setopt(curl, CURLOPT_CAINFO, params->cacert);
    }

    if (params->tlsversion != CURL_SSLVERSION_DEFAULT) {
        res |= curl_easy_setopt(curl, CURLOPT_SSLVERSION, (long) params->tlsversion);
    }

    if (params->ciphersuites != NULL) {
        res |= curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, params->ciphersuites);
    }

    if (params->http_proxy  != NULL) {
	LM_DBG("****** ##### CURL proxy [%s] \n", params->http_proxy);
	res |= curl_easy_setopt(curl, CURLOPT_PROXY, params->http_proxy);
     } else {
	LM_DBG("****** ##### CURL proxy NOT SET \n");
     }

    if (params->http_proxy_port > 0) {
	res |= curl_easy_setopt(curl, CURLOPT_PROXYPORT, params->http_proxy_port);
    }


    res |= curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (long) params->verify_peer);
    res |= curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (long) params->verify_host?2:0);

    res |= curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long) 1);
    res |= curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) params->timeout);
    res |= curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long) params->http_follow_redirect);


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
	} else if ( res == CURLE_COULDNT_RESOLVE_PROXY ) {
		LM_WARN("couldn't resolve http_proxy host\n");
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

	if (params->oneline) {
		/* search for line feed */
		at = memchr(stream.buf, (char)10, download_size);
		datasize = (double) (at - stream.buf);
		LM_DBG("  -- curl download size cut to first line: %d \n", (int) datasize);
	}
	if (at == NULL) {
		if (params->maxdatasize && ((unsigned int) download_size) > params->maxdatasize) {
			/* Limit at maximum data size */
			datasize = (double) params->maxdatasize;
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
	char *postdata = NULL;
	curl_query_t query_params;

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

	memset(&query_params, 0, sizeof(curl_query_t));
	query_params.username = conn->username;
	query_params.secret = conn->password;
	query_params.authmethod = conn->authmethod;
	query_params.contenttype = contenttype ? (char*)contenttype : "text/plain";
	query_params.post = postdata;
	query_params.clientcert = conn->clientcert;
	query_params.clientkey = conn->clientkey;
	query_params.cacert = default_tls_cacert;
	query_params.ciphersuites = conn->ciphersuites;
	query_params.tlsversion = conn->tlsversion;
	query_params.verify_peer = conn->verify_peer;
	query_params.verify_host = conn->verify_host;
	query_params.timeout = conn->timeout;
	query_params.http_follow_redirect = conn->http_follow_redirect;
	query_params.oneline = 0;
	query_params.maxdatasize = maxdatasize;
	query_params.http_proxy_port = conn->http_proxy_port;
	if (conn->http_proxy) {
		query_params.http_proxy = conn->http_proxy;
		LM_DBG("****** ##### CURL proxy [%s] \n", query_params.http_proxy);
	} else {
		LM_DBG("**** Curl HTTP_proxy not set \n");
	}

	res = curL_query_url(_m, urlbuf, result, &query_params);

	LM_DBG("***** #### ***** CURL DONE : %s \n", urlbuf);
error:
	if (urlbuf != NULL) {
		pkg_free(urlbuf);
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
	curl_query_t query_params;

	memset(&query_params, 0, sizeof(curl_query_t));
	query_params.username = NULL;
	query_params.secret = NULL;
	query_params.authmethod = default_authmethod;
	query_params.contenttype = "text/plain";
	query_params.post = _post;
	query_params.clientcert = NULL;
	query_params.clientkey = NULL;
	query_params.cacert = NULL;
	query_params.ciphersuites = NULL;
	query_params.tlsversion = default_tls_version;
	query_params.verify_peer = default_tls_verify_peer;
	query_params.verify_host = default_tls_verify_host;
	query_params.timeout = default_connection_timeout;
	query_params.http_follow_redirect = default_http_follow_redirect;
	query_params.oneline = 1;
	query_params.maxdatasize = 0;
	if(default_http_proxy.s!=NULL && default_http_proxy.len>0) {
		query_params.http_proxy = default_http_proxy.s;
			if(default_http_proxy_port>0) {
				query_params.http_proxy_port = default_http_proxy_port;
			}
	}

	res =  curL_query_url(_m, _url, _dst, &query_params);

	return res;
}

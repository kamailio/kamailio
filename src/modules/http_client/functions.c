/*
 * Script functions of http_client module
 *
 * Copyright (C) 2015 Olle E. Johansson, Edvina AB
 *
 * Based on functions from siputil
 * 	Copyright (C) 2008 Juha Heinanen
 * 	Copyright (C) 2013 Carsten Bock, ng-voice GmbH
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include <openssl/ssl.h>

#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/route_struct.h"
#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/msg_parser.h"
#include "../tls_tracker/api.h"

#include "http_client.h"
#include "curlcon.h"

typedef struct
{
	char *username;
	char *secret;
	char *contenttype;
	char *post;
	char *clientcert;
	char *clientkey;
	char *cacert;
	char *ciphersuites;
	char *http_proxy;
	char *failovercon;
	char *useragent;
	char *hdrs;
	char *netinterface;
	unsigned int httpversion;
	unsigned int authmethod;
	unsigned int http_proxy_port;
	unsigned int tlsversion;
	unsigned int verify_peer;
	unsigned int verify_host;
	unsigned int timeout;
	unsigned int http_follow_redirect;
	unsigned int oneline;
	unsigned int maxdatasize;
	unsigned int keep_connections;
	curl_con_pkg_t *pconn;
} curl_query_t;

extern tls_tracker_ops_api_t hc_tls_tracker_ops;
extern int tls_tracker_loaded;

void ssl_info_callback(const SSL *s, int where, int ret)
{
	SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(s);
	ssl_shared_data_t *data =
			(ssl_shared_data_t *)SSL_CTX_get_app_data(ssl_ctx);
	if(!data) {
		LM_ERR("No app data attached to SSL context\n");
		return;
	}

	if(where & SSL_CB_HANDSHAKE_DONE) {
		char *remote_ip = NULL;
		char *local_ip = NULL;
		long remote_port = 0;
		long local_port = 0;

		CURLcode res =
				curl_easy_getinfo(data->curl, CURLINFO_PRIMARY_IP, &remote_ip);
		if(CURLE_OK == res) {
			LM_INFO("curl remote ip_address='%s'\n", remote_ip);
		} else {
			LM_ERR("Error remote IP address obtaining\n");
		}

		res = curl_easy_getinfo(
				data->curl, CURLINFO_PRIMARY_PORT, &remote_port);
		if(CURLE_OK == res) {
			LM_INFO("Connected to remote port: %ld\n", remote_port);
		} else {
			LM_ERR("Error remote port obtaining\n");
		}

		res = curl_easy_getinfo(data->curl, CURLINFO_LOCAL_IP, &local_ip);
		if(CURLE_OK == res) {
			LM_INFO("curl local ip_address='%s'\n", local_ip);
		} else {
			LM_ERR("Error local IP address obtaining\n");
		}

		res = curl_easy_getinfo(data->curl, CURLINFO_LOCAL_PORT, &local_port);
		if(CURLE_OK == res) {
			LM_INFO("Connected to local port: %ld\n", local_port);
		} else {
			LM_ERR("Error local port obtaining\n");
		}

		str remote_ip_str = {remote_ip, strlen(remote_ip)};
		str local_ip_str = {local_ip, strlen(local_ip)};

		int db_conn_id =
				hc_tls_tracker_ops.add_new_connection(data->connection_id,
						&remote_ip_str, &local_ip_str, remote_port, local_port);
		if(db_conn_id < 0) {
			LM_ERR("Error adding new connection into the DB\n");
		} else {
			data->db_id = db_conn_id;
			hc_tls_tracker_ops.add_session_key(db_conn_id, data->session_key);
		}
		pkg_free(data->session_key);
		data->session_key = NULL;
	}
}

void keylog_callback(const SSL *ssl, const char *line)
{
	SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(ssl);
	ssl_shared_data_t *data =
			(ssl_shared_data_t *)SSL_CTX_get_app_data(ssl_ctx);
	if(data) {
		LM_DBG("Called HTTPS keylog_callback for tcp connection_id=%d\n",
				data->connection_id);
		int session_key_len =
				(data->session_key ? strlen(data->session_key) : 0)
				+ strlen(line) + 2;
		char *session_key = pkg_mallocxz(session_key_len);
		if(NULL == session_key) {
			PKG_MEM_ERROR;
			return;
		}
		if(data->session_key) {
			snprintf(session_key, session_key_len, "%s\n%s", data->session_key,
					line);
			pkg_free(data->session_key);
		} else {
			snprintf(session_key, session_key_len, "%s", line);
		}
		data->session_key = session_key;
	} else {
		LM_ERR("no data attached to SSL descriptor\n");
	}
}

static CURLcode sslctx_function(CURL *curl, void *sslctx, void *parm)
{
	if(parm) {
		SSL_CTX_set_app_data((SSL_CTX *)sslctx, parm);
	}
	SSL_CTX_set_keylog_callback((SSL_CTX *)sslctx, keylog_callback);
	SSL_CTX_set_info_callback((SSL_CTX *)sslctx, ssl_info_callback);
	return CURLE_OK;
}

/**
 *
 */
typedef struct httpc_hdr
{
	str hbuf;
	str name;
	str body;
	struct httpc_hdr *next;
} httpc_hdr_t;

/**
 *
 */
httpc_hdr_t *_http_client_response_headers = NULL;

/**
 *
 */
httpc_hdr_t *httpc_hdr_block_add(httpc_hdr_t **head, char *s, int len)
{
	httpc_hdr_t *nv;
	nv = pkg_mallocxz(sizeof(httpc_hdr_t) + (len + 1) * sizeof(char));
	if(!nv) {
		PKG_MEM_ERROR;
		return 0;
	}
	nv->hbuf.s = (char *)nv + sizeof(httpc_hdr_t);
	memcpy(nv->hbuf.s, s, len);
	nv->hbuf.len = len;
	nv->next = *head;
	*head = nv;

	return nv;
}

/**
 *
 */
void http_client_response_headers_reset(void)
{
	httpc_hdr_t *it0;
	httpc_hdr_t *it1;
	it0 = _http_client_response_headers;
	while(it0 != NULL) {
		it1 = it0->next;
		pkg_free(it0);
		it0 = it1;
	}
	_http_client_response_headers = NULL;
}

/**
 *
 */
int http_client_response_headers_get(str *hname, str *hbody)
{
	httpc_hdr_t *it;
	char *p;

	if(_http_client_response_headers == NULL) {
		return -1;
	}
	if(hname == NULL || hname->len <= 0 || hbody == NULL) {
		return -1;
	}
	for(it = _http_client_response_headers; it != NULL; it = it->next) {
		if(it->name.len == 0 && it->hbuf.s[0] != ' ' && it->hbuf.s[0] != '\t'
				&& it->hbuf.s[0] != '\r' && it->hbuf.s[0] != '\n') {
			/* parsing */
			p = strchr(it->hbuf.s, ':');
			if(p == NULL) {
				continue;
			}
			it->name.s = it->hbuf.s;
			it->name.len = p - it->name.s;
			trim(&it->name);
			p++;
			it->body.s = p;
			it->body.len = it->hbuf.s + it->hbuf.len - it->body.s;
			trim(&it->body);
		}
		if(it->name.len == hname->len
				&& strncasecmp(it->name.s, hname->s, hname->len) == 0) {
			hbody->s = it->body.s;
			hbody->len = it->body.len;
			return 0;
		}
	}
	return -1;
}

/*
 * curl write function that saves received data as zero terminated
 * to stream. Returns the amount of data taken care of.
 *
 * This function may be called multiple times for larger responses,
 * so it reallocs + concatenates the buffer as needed.
 */
size_t write_function(void *ptr, size_t size, size_t nmemb, void *stream_ptr)
{
	curl_res_stream_t *stream = (curl_res_stream_t *)stream_ptr;


	if(stream->max_size == 0 || stream->curr_size < stream->max_size) {
		char *tmp = (char *)pkg_realloc(
				stream->buf, stream->curr_size + (size * nmemb));

		if(tmp == NULL) {
			LM_ERR("cannot allocate memory for stream\n");
			return CURLE_WRITE_ERROR;
		}
		stream->buf = tmp;

		memcpy(&stream->buf[stream->pos], (char *)ptr, (size * nmemb));

		stream->curr_size += ((size * nmemb));
		stream->pos += (size * nmemb);

	} else {
		LM_DBG("****** ##### CURL Max datasize exceeded: max  %u current %u\n",
				(unsigned int)stream->max_size,
				(unsigned int)stream->curr_size);
	}

	return size * nmemb;
}


size_t http_client_response_header_cb(
		char *b, size_t size, size_t nitems, void *userdata)
{
	size_t numbytes;

	numbytes = size * nitems;
	LM_DBG("http response header [%.*s]\n", (int)numbytes, b);

	httpc_hdr_block_add(&_http_client_response_headers, b, (int)numbytes);

	return numbytes;
	;
}

/*! Send query to server, optionally post data.
 */
static int curL_request_url(struct sip_msg *_m, const char *_met,
		const char *_url, str *_dst, const curl_query_t *const params)
{
	CURL *curl = NULL;
	CURLcode res;
	char *at = NULL;
	curl_res_stream_t stream;
	long stat = 0;
	str rval = STR_NULL;
	double download_size = 0;
	struct curl_slist *headerlist = NULL;
	ssl_shared_data_t *ssl_data = NULL;

	memset(&stream, 0, sizeof(curl_res_stream_t));
	stream.max_size = (size_t)params->maxdatasize;

	if(params->pconn) {
		LM_DBG("****** ##### We have a pconn - keep_connections: %d!\n",
				params->keep_connections);
		params->pconn->result_content_type[0] = '\0';
		params->pconn->redirecturl[0] = '\0';
		if(params->pconn->curl != NULL) {
			LM_DBG("         ****** ##### Reusing existing connection if "
				   "possible\n");
			curl = params->pconn->curl; /* Reuse existing handle */
			curl_easy_reset(curl);		/* Reset handle */
		}
	}


	if(curl == NULL) {
		curl = curl_easy_init();
	}
	if(curl == NULL) {
		LM_ERR("Failed to initialize curl connection\n");
		return -1;
	}

	LM_DBG("****** ##### CURL URL [%s] \n", _url);
	res = curl_easy_setopt(curl, CURLOPT_URL, _url);

	if(params->httpversion != 0) {
		curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, params->httpversion);
	}

	/* Limit to HTTP and HTTPS protocols */
#if LIBCURL_VERSION_NUM >= 0x075500
	/* #if defined(CURL_AT_LEAST_VERSION) && CURL_AT_LEAST_VERSION(7, 85, 0) */
	res = curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
	res = curl_easy_setopt(
			curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

#if LIBCURL_VERSION_NUM >= 0x075500
	/* #if defined(CURL_AT_LEAST_VERSION) && CURL_AT_LEAST_VERSION(7, 85, 0) */
	res = curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
	res = curl_easy_setopt(
			curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

	if(_met != NULL) {
		/* Enforce method (GET, PUT, ...) */
		res |= curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, _met);
	}
	if(params->post) {
		if(_met == NULL) {
			/* Now specify we want to POST data */
			res |= curl_easy_setopt(curl, CURLOPT_POST, 1L);
		}
		if(params->contenttype) {
			char ctype[256];

			ctype[0] = '\0';
			snprintf(ctype, sizeof(ctype), "Content-Type: %s",
					params->contenttype);

			/* Set the content-type of the DATA */
			headerlist = curl_slist_append(headerlist, ctype);
		}

		/* Tell CURL we want to upload using POST */
		res |= curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params->post);

	} else {
		/* Reset post option */
		res |= curl_easy_setopt(curl, CURLOPT_POST, 0L);
	}

	if(params->hdrs) {
		headerlist = curl_slist_append(headerlist, params->hdrs);
	}
	if(headerlist) {
		res |= curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	}

	if(params->maxdatasize) {
		/* Maximum data size to download - we always download full response,
		 * but cut it off before moving to pvar */
		LM_DBG("****** ##### CURL Max datasize %u\n", params->maxdatasize);
	}

	if(params->username) {
		res |= curl_easy_setopt(curl, CURLOPT_USERNAME, params->username);
		res |= curl_easy_setopt(curl, CURLOPT_HTTPAUTH, params->authmethod);
	}
	if(params->secret) {
		res |= curl_easy_setopt(curl, CURLOPT_PASSWORD, params->secret);
	}

	/* Client certificate */
	if(params->clientcert != NULL && params->clientkey != NULL) {
		res |= curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
		res |= curl_easy_setopt(curl, CURLOPT_SSLCERT, params->clientcert);

		res |= curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
		res |= curl_easy_setopt(curl, CURLOPT_SSLKEY, params->clientkey);
	}

	if(params->cacert != NULL) {
		res |= curl_easy_setopt(curl, CURLOPT_CAINFO, params->cacert);
	}

	if(params->tlsversion != CURL_SSLVERSION_DEFAULT) {
		res |= curl_easy_setopt(
				curl, CURLOPT_SSLVERSION, (long)params->tlsversion);
	}

	if(params->ciphersuites != NULL) {
		res |= curl_easy_setopt(
				curl, CURLOPT_SSL_CIPHER_LIST, params->ciphersuites);
	}

	if(params->http_proxy != NULL) {
		LM_DBG("****** ##### CURL proxy [%s] \n", params->http_proxy);
		res |= curl_easy_setopt(curl, CURLOPT_PROXY, params->http_proxy);
	} else {
		LM_DBG("****** ##### CURL proxy NOT SET \n");
	}

	if(params->http_proxy_port > 0) {
		res |= curl_easy_setopt(
				curl, CURLOPT_PROXYPORT, params->http_proxy_port);
	}


	res |= curl_easy_setopt(
			curl, CURLOPT_SSL_VERIFYPEER, (long)params->verify_peer);
	res |= curl_easy_setopt(
			curl, CURLOPT_SSL_VERIFYHOST, (long)params->verify_host ? 2 : 0);

	res |= curl_easy_setopt(curl, CURLOPT_NOSIGNAL, (long)1);

	/* timeout_mode parameter:
	 * - 0 : timeout is disabled.
	 * - 1 (default) : timeout value is in seconds.
	 * - 2 : timeout value is in milliseconds.
	 */
	if(timeout_mode == 1) { /* timeout is in seconds (default) */
		res |= curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)params->timeout);
	} else if(timeout_mode == 2) { /* timeout is in milliseconds */
		res |= curl_easy_setopt(
				curl, CURLOPT_TIMEOUT_MS, (long)params->timeout);
	}

	res |= curl_easy_setopt(
			curl, CURLOPT_FOLLOWLOCATION, (long)params->http_follow_redirect);
	if(params->http_follow_redirect) {
		LM_DBG("Following redirects for this request! \n");
	}
	if(params->netinterface != NULL) {
		res |= curl_easy_setopt(curl, CURLOPT_INTERFACE, params->netinterface);
	}

	if(http_client_response_headers_param != 0) {
		http_client_response_headers_reset();
		res |= curl_easy_setopt(
				curl, CURLOPT_HEADERFUNCTION, http_client_response_header_cb);
	}

	res |= curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
	res |= curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)(&stream));

	if(params->useragent)
		res |= curl_easy_setopt(curl, CURLOPT_USERAGENT, params->useragent);

	if(tls_tracker_loaded && _url && strlen(_url) > 5
			&& !strncmp(_url, "https", 5)) {
		do {
			ssl_data = (ssl_shared_data_t *)pkg_mallocxz(
					sizeof(ssl_shared_data_t));
			if(ssl_data == NULL) {
				PKG_MEM_ERROR;
				break;
			}
			ssl_data->connection_id = params->pconn->conid;
			ssl_data->curl = curl;

			res |= curl_easy_setopt(
					curl, CURLOPT_SSL_CTX_FUNCTION, *sslctx_function);
			res |= curl_easy_setopt(curl, CURLOPT_SSL_CTX_DATA, ssl_data);
		} while(0);
	}

	if(res != CURLE_OK) {
		/* PANIC */
		LM_ERR("Could not set CURL options. Library error \n");
	} else {
		double totaltime, connecttime;

		res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totaltime);
		curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &connecttime);
		LM_DBG("HTTP Call performed in %f s (connect time %f) \n", totaltime,
				connecttime);
		if(params->pconn) {
			params->pconn->querytime = totaltime;
			params->pconn->connecttime = connecttime;
		}
	}

	/* Cleanup */
	if(headerlist) {
		curl_slist_free_all(headerlist);
	}

	if(res != CURLE_OK) {
		/* http://curl.haxx.se/libcurl/c/libcurl-errors.html */
		if(res == CURLE_COULDNT_CONNECT) {
			LM_WARN("failed to connect() to host (url: %s)\n", _url);
		} else if(res == CURLE_COULDNT_RESOLVE_HOST) {
			LM_WARN("Couldn't resolve host (url: %s)\n", _url);
		} else if(res == CURLE_COULDNT_RESOLVE_PROXY) {
			LM_WARN("Couldn't resolve http proxy host (url: %s - proxy: %s)\n",
					_url,
					(params && params->http_proxy) ? params->http_proxy : "");
		} else if(res == CURLE_UNSUPPORTED_PROTOCOL) {
			LM_WARN("URL Schema not supported by curl (url: %s)\n", _url);
		} else if(res == CURLE_URL_MALFORMAT) {
			LM_WARN("Malformed URL used in http client (url: %s)\n", _url);
		} else if(res == CURLE_OUT_OF_MEMORY) {
			LM_WARN("Curl library out of memory (url: %s)\n", _url);
		} else if(res == CURLE_OPERATION_TIMEDOUT) {
			LM_WARN("Curl library timed out on request (url: %s)\n", _url);
		} else if(res == CURLE_SSL_CONNECT_ERROR) {
			LM_WARN("TLS error in curl connection (url: %s)\n", _url);
		} else if(res == CURLE_SSL_CERTPROBLEM) {
			LM_WARN("TLS local certificate error (url: %s)\n", _url);
		} else if(res == CURLE_SSL_CIPHER) {
			LM_WARN("TLS cipher error (url: %s)\n", _url);
		} else if(res == CURLE_SSL_CACERT) {
			LM_WARN("TLS server certificate validation error"
					" (No valid CA cert) (url: %s)\n",
					_url);
		} else if(res == CURLE_SSL_CACERT_BADFILE) {
			LM_WARN("TLS CA certificate read error (url: %s)\n", _url);
		} else if(res == CURLE_SSL_ISSUER_ERROR) {
			LM_WARN("TLS issuer certificate check error (url: %s)\n", _url);
		} else if(res == CURLE_PEER_FAILED_VERIFICATION) {
			LM_WARN("TLS verification error (url: %s)\n", _url);
		} else if(res == CURLE_TOO_MANY_REDIRECTS) {
			LM_WARN("Too many redirects (url: %s)\n", _url);
		} else {
			LM_ERR("failed to perform curl (%d) (url: %s)\n", res, _url);
		}

		if(params->pconn) {
			params->pconn->last_result = res;
		}
		if(params->pconn && params->keep_connections) {
			params->pconn->curl = curl; /* Save connection, don't close */
		} else {
			/* Cleanup and close - bye bye and thank you for all the bytes */
			curl_easy_cleanup(curl);
		}
		if(stream.buf) {
			pkg_free(stream.buf);
		}
		if(ssl_data) {
			if(ssl_data->db_id > 0) {
				hc_tls_tracker_ops.handle_tcp_connection_ended(ssl_data->db_id);
			}
			pkg_free(ssl_data);
		}
		counter_inc(connfail);
		if(params->failovercon != NULL) {
			LM_ERR("FATAL FAILURE: Trying failover to curl con (%s)\n",
					params->failovercon);
			return (1000 + res);
		}
		return res;
	}

	/* HTTP_CODE CHANGED TO CURLINFO_RESPONSE_CODE in curl > 7.10.7 */
	curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &stat);
	if(res == CURLE_OK) {
		char *ct = NULL;
		char *url = NULL;

		/* ask for the content-type of the response */
		res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
		res |= curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);

		if(ct) {
			LM_DBG("We received Content-Type: %s\n", ct);
			if(params->pconn
					&& strlen(ct) < sizeof(params->pconn->result_content_type)
											- 1) {
				strcpy(params->pconn->result_content_type, ct);
			}
		}
		if(url) {
			LM_DBG("We visited URL: %s\n", url);
			if(params->pconn
					&& strlen(url) < sizeof(params->pconn->redirecturl) - 1) {
				strcpy(params->pconn->redirecturl, url);
			}
		}
	}
	if(params->pconn) {
		params->pconn->last_result = stat;
	}

	if((stat >= 200) && (stat < 500)) {
		double datasize = 0;

#if LIBCURL_VERSION_NUM >= 0x073700
		/* #if defined(CURL_AT_LEAST_VERSION) && CURL_AT_LEAST_VERSION(7, 55, 0) */
		curl_off_t dlsize;
		curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dlsize);
		download_size = (double)dlsize;
#else
		curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &download_size);
#endif
		LM_DBG("  -- curl download size: %u \n", (unsigned int)download_size);
		datasize = download_size;

		if(download_size > 0) {

			if(params->oneline) {
				/* search for line feed */
				at = memchr(stream.buf, (char)10, download_size);
				if(at != NULL) {
					datasize = (double)(at - stream.buf);
					LM_DBG("  -- curl download size cut to first line: %d\n",
							(int)datasize);
				} else {
					LM_DBG("no end of line found for one line result type\n");
				}
			}
			if(at == NULL) {
				if(params->maxdatasize
						&& ((unsigned int)download_size)
								   > params->maxdatasize) {
					/* Limit at maximum data size */
					datasize = (double)params->maxdatasize;
					LM_DBG("  -- curl download size cut to maxdatasize : %d \n",
							(int)datasize);
				} else {
					/* Limit at actual downloaded data size */
					datasize = (double)download_size;
					LM_DBG("  -- curl download size cut to download_size : %d "
						   "\n",
							(int)datasize);
					// at = stream.buf + (unsigned int) download_size;
				}
			}
			/* Create a STR object */
			rval.s = stream.buf;
			rval.len = datasize;
			/* Duplicate string to return */
			pkg_str_dup(_dst, &rval);
			LM_DBG("curl query result - length: %d data: [%.*s]\n", rval.len,
					rval.len, rval.s);
		} else {
			_dst->s = NULL;
			_dst->len = 0;
		}
	}
	if(stat == 200) {
		counter_inc(connok);
	} else {
		counter_inc(connfail);
		if(stat >= 500) {
			if(params->failovercon != NULL) {
				LM_ERR("FAILURE: Trying failover to curl con (%s)\n",
						params->failovercon);
				if(ssl_data) {
					if(ssl_data->db_id > 0) {
						hc_tls_tracker_ops.handle_tcp_connection_ended(
								ssl_data->db_id);
					}
					pkg_free(ssl_data);
				}
				return (1000 + stat);
			}
		}
	}

	/* CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ... ); */
	if(params->pconn && params->keep_connections) {
		params->pconn->curl = curl; /* Save connection, don't close */
	} else {
		curl_easy_cleanup(curl);
	}
	if(stream.buf != NULL) {
		pkg_free(stream.buf);
	}

	if(ssl_data) {
		if(ssl_data->db_id > 0) {
			hc_tls_tracker_ops.handle_tcp_connection_ended(ssl_data->db_id);
		}
		pkg_free(ssl_data);
	}
	return stat;
}

/*! Run a query based on a connection definition */
int curl_get_redirect(struct sip_msg *_m, const str *connection, str *result)
{
	curl_con_t *conn = NULL;
	curl_con_pkg_t *pconn = NULL;
	str rval;
	result->s = NULL;
	result->len = 0;

	/* Find connection if it exists */
	if(!connection) {
		LM_ERR("No cURL connection specified\n");
		return -1;
	}
	LM_DBG("******** CURL Connection %.*s\n", connection->len, connection->s);
	conn = curl_get_connection((str *)connection);
	if(conn == NULL) {
		LM_ERR("No cURL connection found: %.*s\n", connection->len,
				connection->s);
		return -1;
	}
	pconn = curl_get_pkg_connection(conn);
	if(pconn == NULL) {
		LM_ERR("No cURL connection data found: %.*s\n", connection->len,
				connection->s);
		return -1;
	}
	/* Create a STR object */
	rval.s = pconn->redirecturl;
	rval.len = strlen(pconn->redirecturl);
	/* Duplicate string to return */
	pkg_str_dup(result, &rval);
	LM_DBG("curl last redirect URL: Length %d %.*s \n", rval.len, rval.len,
			rval.s);

	return 1;
}


/*! Run a query based on a connection definition */
int curl_con_query_url_f(struct sip_msg *_m, const str *connection,
		const str *url, str *result, const char *contenttype, const str *post,
		int failover)
{
	curl_con_t *conn = NULL;
	curl_con_pkg_t *pconn = NULL;
	char *urlbuf = NULL;
	char *postdata = NULL;
	char *failovercon = NULL;
	curl_query_t query_params;

	unsigned int maxdatasize = default_maxdatasize;
	int res;

	/* Find connection if it exists */
	if(!connection) {
		LM_ERR("No cURL connection specified\n");
		return -1;
	}
	LM_DBG("******** CURL Connection %.*s\n", connection->len, connection->s);
	conn = curl_get_connection((str *)connection);
	if(conn == NULL) {
		LM_ERR("No cURL connection found: %.*s\n", connection->len,
				connection->s);
		return -1;
	}
	pconn = curl_get_pkg_connection(conn);
	if(pconn == NULL) {
		LM_ERR("No cURL connection data found: %.*s\n", connection->len,
				connection->s);
		return -1;
	}

	LM_DBG("******** CURL Connection found %.*s\n", connection->len,
			connection->s);
	maxdatasize = conn->maxdatasize;

	if(url && (url->len > 0) && (url->s != NULL)) {
		int url_len = conn->schema.len + 3 + conn->url.len + 1 + url->len + 1;
		urlbuf = pkg_malloc(url_len);
		if(urlbuf == NULL) {
			res = -1;
			goto error;
		}
		snprintf(urlbuf, url_len, "%.*s://%.*s%s%.*s", conn->schema.len,
				conn->schema.s, conn->url.len, conn->url.s,
				(url->s[0] == '/') ? "" : "/", url->len, url->s);
	} else {
		int url_len = conn->schema.len + 3 + conn->url.len + 1;
		urlbuf = pkg_malloc(url_len);
		if(urlbuf == NULL) {
			res = -1;
			goto error;
		}
		snprintf(urlbuf, url_len, "%.*s://%.*s", conn->schema.len,
				conn->schema.s, conn->url.len, conn->url.s);
	}
	LM_DBG("***** #### ***** CURL URL: %s \n", urlbuf);

	if(post && (post->len > 0) && (post->s != NULL)) {

		/* Allocated using pkg_memory */
		postdata = as_asciiz((str *)post);
		if(postdata == NULL) {
			ERR("Curl: No memory left\n");
			res = -1;
			goto error;
		}
		LM_DBG("***** #### ***** CURL POST data: %s Content-type %s\n",
				postdata, contenttype);
	}

	memset(&query_params, 0, sizeof(curl_query_t));
	query_params.username = conn->username;
	query_params.secret = conn->password;
	query_params.authmethod = conn->authmethod;
	query_params.contenttype = contenttype ? (char *)contenttype : "text/plain";
	query_params.post = postdata;
	query_params.clientcert = conn->clientcert;
	query_params.clientkey = conn->clientkey;
	query_params.cacert = default_tls_cacert;
	query_params.ciphersuites = conn->ciphersuites;
	query_params.tlsversion = conn->tlsversion;
	query_params.useragent = conn->useragent;
	query_params.verify_peer = conn->verify_peer;
	query_params.verify_host = conn->verify_host;
	query_params.timeout = conn->timeout;
	query_params.http_follow_redirect = conn->http_follow_redirect;
	query_params.keep_connections = conn->keep_connections;
	query_params.oneline = 0;
	query_params.maxdatasize = maxdatasize;
	query_params.netinterface = default_netinterface;
	query_params.http_proxy_port = conn->http_proxy_port;
	if(conn->failover.s) {
		failovercon = as_asciiz(&conn->failover);
	}
	query_params.failovercon = failovercon;
	query_params.pconn = pconn;
	if(conn->http_proxy) {
		query_params.http_proxy = conn->http_proxy;
		LM_DBG("****** ##### CURL proxy [%s] \n", query_params.http_proxy);
	} else {
		LM_DBG("**** Curl HTTP_proxy not set \n");
	}

	res = curL_request_url(_m, NULL, urlbuf, result, &query_params);

	if(res > 1000 && conn->failover.s) {
		int counter = failover + 1;
		if(counter >= 2) {
			LM_DBG("**** No more failovers - returning failure\n");
			res = (res - 1000);
			goto error;
		}
		/* Time for failover */
		res = curl_con_query_url_f(
				_m, &conn->failover, url, result, contenttype, post, counter);
	}

	LM_DBG("***** #### ***** CURL DONE: %s (%d)\n", urlbuf, res);
error:
	if(failovercon != NULL) {
		pkg_free(failovercon);
	}
	if(urlbuf != NULL) {
		pkg_free(urlbuf);
	}
	if(postdata != NULL) {
		pkg_free(postdata);
	}
	return res;
}

/* Wrapper */
int curl_con_query_url(struct sip_msg *_m, const str *connection,
		const str *url, str *result, const char *contenttype, const str *post)
{
	return curl_con_query_url_f(
			_m, connection, url, result, contenttype, post, 0);
}

/*!
 * Performs http request and saves possible result (first body line of reply)
 * to pvar.
 * Similar to http_client_request but supports setting a content type attribute.
 */
int http_client_request_c(sip_msg_t *_m, char *_url, str *_dst, char *_body,
		char *_ctype, char *_hdrs, char *_met, unsigned int _httpver)
{
	int res;
	curl_query_t query_params;

	memset(&query_params, 0, sizeof(curl_query_t));
	query_params.username = NULL;
	query_params.secret = NULL;
	query_params.authmethod = default_authmethod;
	query_params.httpversion = _httpver;
	query_params.contenttype = _ctype;
	query_params.hdrs = _hdrs;
	query_params.post = _body;
	query_params.clientcert = NULL;
	query_params.clientkey = NULL;
	query_params.cacert = NULL;
	query_params.ciphersuites = NULL;
	query_params.tlsversion = default_tls_version;
	query_params.verify_peer = default_tls_verify_peer;
	query_params.verify_host = default_tls_verify_host;
	query_params.timeout = default_connection_timeout;
	query_params.http_follow_redirect = default_http_follow_redirect;
	query_params.oneline = default_query_result;
	query_params.maxdatasize = default_query_maxdatasize;
	query_params.netinterface = default_netinterface;
	if(default_useragent.s != NULL && default_useragent.len > 0) {
		query_params.useragent = default_useragent.s;
	}
	if(default_http_proxy.s != NULL && default_http_proxy.len > 0) {
		query_params.http_proxy = default_http_proxy.s;
		if(default_http_proxy_port > 0) {
			query_params.http_proxy_port = default_http_proxy_port;
		}
	}
	if(default_tls_clientcert.s != NULL && default_tls_clientcert.len > 0) {
		query_params.clientcert = default_tls_clientcert.s;
	}
	if(default_tls_clientkey.s != NULL && default_tls_clientkey.len > 0) {
		query_params.clientkey = default_tls_clientkey.s;
	}
	if(default_tls_cacert != NULL) {
		query_params.cacert = default_tls_cacert;
	}
	if(default_cipher_suite_list.s != NULL && default_cipher_suite_list.len) {
		query_params.ciphersuites = default_cipher_suite_list.s;
	}

	res = curL_request_url(_m, _met, _url, _dst, &query_params);

	return res;
}

/*!
 * Performs http request and saves possible result (first body line of reply)
 * to pvar.
 * This is the same http_query as used to be in the utils module.
 */
int http_client_request(sip_msg_t *_m, char *_url, str *_dst, char *_body,
		char *_hdrs, char *_met, unsigned int _httpver)
{
	return http_client_request_c(
			_m, _url, _dst, _body, NULL, _hdrs, _met, _httpver);
}

/*!
 * Performs http_query and saves possible result (first body line of reply)
 * to pvar.
 * This is the same http_query as used to be in the utils module.
 */
int http_client_query(
		struct sip_msg *_m, char *_url, str *_dst, char *_post, char *_hdrs)
{
	return http_client_request(_m, _url, _dst, _post, _hdrs, NULL, 0);
}

/*!
 * Performs http_query and saves possible result (first body line of reply)
 * to pvar.
 * This is the same http_query as used to be in the utils module.
 */
int http_client_query_c(struct sip_msg *_m, char *_url, str *_dst, char *_post,
		char *_ctype, char *_hdrs)
{
	return http_client_request_c(_m, _url, _dst, _post, _ctype, _hdrs, NULL, 0);
}

char *http_get_content_type(const str *connection)
{
	curl_con_t *conn = NULL;
	curl_con_pkg_t *pconn = NULL;

	/* Find connection if it exists */
	if(!connection) {
		LM_ERR("No cURL connection specified\n");
		return NULL;
	}
	LM_DBG("******** CURL Connection %.*s\n", connection->len, connection->s);
	conn = curl_get_connection((str *)connection);
	if(conn == NULL) {
		LM_ERR("No cURL connection found: %.*s\n", connection->len,
				connection->s);
		return NULL;
	}
	pconn = curl_get_pkg_connection(conn);
	if(pconn == NULL) {
		LM_ERR("No cURL connection data found: %.*s\n", connection->len,
				connection->s);
		return NULL;
	}

	return pconn->result_content_type;
}

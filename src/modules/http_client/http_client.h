/*
 * header file of http_client.c
 *
 * Copyright (C) 2008 Juha Heinanen
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
 * \brief Kamailio http_client :: Core include file
 * \ingroup http_client
 * Module: \ref http_client
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <curl/curl.h>

#include "../../core/str.h"
#include "../../core/counters.h"
#include "../../lib/srdb1/db.h"

extern unsigned int default_connection_timeout;
extern char *
		default_tls_cacert; /*!< File name: Default CA cert to use for curl TLS connection */
extern str
		default_tls_clientcert; /*!< File name: Default client certificate to use for curl TLS connection */
extern str
		default_tls_clientkey; /*!< File name: Key in PEM format that belongs to client cert */
extern str default_cipher_suite_list;	/*!< List of allowed cipher suites */
extern unsigned int default_tls_version; /*!< 0 = Use libcurl default */
extern unsigned int
		default_tls_verify_peer; /*!< 0 = Do not verify TLS server cert. 1 = Verify TLS cert (default) */
extern unsigned int
		default_tls_verify_host; /*!< 0 = Do not verify TLS server CN/SAN. 2 = Verify TLS server CN/SAN (default) */
extern str default_http_proxy;   /*!< Default HTTP proxy to use */
extern unsigned int
		default_http_proxy_port; /*!< Default HTTP proxy port to use */
extern unsigned int
		default_http_follow_redirect; /*!< Follow HTTP redirects CURLOPT_FOLLOWLOCATION */
extern str
		default_useragent; /*!< Default CURL useragent. Default "Kamailio Curl " */
extern unsigned int default_maxdatasize; /*!< Default Maximum download size */
extern unsigned int
		default_authmethod; /*!< authentication method - Basic, Digest or both */
extern unsigned int
		default_keep_connections; /*!< Keep http connections open for reuse */
extern unsigned int default_query_result; /*!< Default query result mode */
extern unsigned int default_query_maxdatasize; /*!< Default query result maximum download size */

extern counter_handle_t connections; /* Number of connection definitions */
extern counter_handle_t connok;		 /* Successful Connection attempts */
extern counter_handle_t connfail;	/* Failed Connection attempts */

/* Curl  stream object  */
typedef struct
{
	char *buf;
	size_t curr_size;
	size_t pos;
	size_t max_size;
} curl_res_stream_t;

enum connection_status
{
	AVAILABLE = 0, /*!< Available */
	NOTREACHABLE,  /*!< Failure */
	DISABLED,	  /*!< Disabled by action (RPC etc) */
	/* DELETED */  /*!< Should be deleted by pkg process, not listed */
};


/*! Predefined connection objects  - stored in shared memory */
typedef struct _curl_con
{
	str name;						  /*!< Connection name */
	unsigned int conid;				  /*!< Connection ID */
	enum connection_status connstate; /*!< Connection status */
	str url;		/*!< The URL without schema (host + base URL)*/
	str schema;		/*!< The URL schema */
	char *username; /*!< The username to use for auth */
	char *password; /*!< The password to use for auth */
	unsigned int
			authmethod; /*!< Authentication method -digest or basic or both */
	str failover;		/*!< Another connection to use if this one fails */
	char *useragent;	/*!< Useragent to use for this connection */
	char *cacert;		/*!< File name of CA cert to use */
	char *clientcert;   /*!< File name of CA client cert */
	char *clientkey;	/*!< File name of CA client key */
	char *ciphersuites; /*!< List of allowed cipher suites */
	unsigned int tlsversion;  /*!< SSL/TLS version to use */
	unsigned int verify_peer; /*!< TRUE if server cert to be verified */
	unsigned int verify_host; /*!< TRUE if server CN/SAN to be verified */
	int http_follow_redirect; /*!< TRUE if we should follow HTTP 302 redirects */
	unsigned int keep_connections; /*!< TRUE to keep curl connections open */
	unsigned int port;			   /*!< The port to connect to */
	int timeout;				   /*!< Timeout for this connection */
	unsigned int maxdatasize;	  /*!< Maximum data download on GET or POST */
	curl_res_stream_t *stream;	 /*!< Curl stream */
	char *http_proxy;			   /*!< HTTP proxy for this connection */
	unsigned int http_proxy_port;  /*!< HTTP proxy port for this connection */
	struct _curl_con *next;		   /*!< next connection */
} curl_con_t;


/*! Per-process copy of connection object -stored in pkg memory */
typedef struct _curl_con_pkg
{
	unsigned int conid; /*!< Connection ID (referring to core connection id */
	char redirecturl
			[512]; /*!< Last redirect URL - to use for $curlredirect(curlcon) pv */
	unsigned int last_result; /*!< Last result of accessing this connection */
	char result_content_type[512]; /*!< Response content-type */
	CURL *curl;					   /*!< Curl connection handle */
	double querytime;			   /*!< Seconds used for last request */
	double connecttime;			   /*!< Seconds used for connecting last request inc TLS setup  - see
					     https://curl.haxx.se/libcurl/c/CURLINFO_APPCONNECT_TIME.html */

	/* Potential candidates:	Last TLS fingerprint used 

	*/
	struct _curl_con_pkg *next; /*!< next connection */
} curl_con_pkg_t;

/*! Returns true if CURL supports TLS */
extern int curl_support_tls();

/*! Returns TRUE if curl supports IPv6 */
extern int curl_support_ipv6();

#endif /* HTTP_CLIENT_H */

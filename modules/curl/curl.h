/*
 * header file of curl.c
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
 * \brief Kamailio curl :: Core include file
 * \ingroup curl
 * Module: \ref curl
 */

#ifndef CURL_H
#define CURL_H

#include "../../str.h"
#include "../../counters.h"
#include "../../lib/srdb1/db.h"

extern unsigned int	default_connection_timeout;
extern char	*default_tls_cacert;			/*!< File name: Default CA cert to use for curl TLS connection */
extern char	*default_tls_clientcert;		/*!< File name: Default client certificate to use for curl TLS connection */
extern char	*default_tls_clientkey;			/*!< File name: Key in PEM format that belongs to client cert */
extern unsigned int	default_tls_verifyserver;		/*!< 0 = Do not verify TLS server cert. 1 = Verify TLS cert (default) */
extern char 	*default_http_proxy;			/*!< Default HTTP proxy to use */
extern unsigned int	default_http_proxy_port;		/*!< Default HTTP proxy port to use */
extern unsigned int	default_http_follow_redirect;	/*!< Follow HTTP redirects CURLOPT_FOLLOWLOCATION */
extern char 	*default_useragent;			/*!< Default CURL useragent. Default "Kamailio Curl " */

extern counter_handle_t connections;	/* Number of connection definitions */
extern counter_handle_t connok;	/* Successful Connection attempts */
extern counter_handle_t connfail;	/* Failed Connection attempts */

/* Curl  stream object  */
typedef struct {
	char		*buf;
	size_t		curr_size;
	size_t		pos;
} http_res_stream_t;


/*! Predefined connection objects */
typedef struct _curl_con
{
	str name;			/*!< Connection name */
	unsigned int conid;		/*!< Connection ID */
	str url;			/*!< The URL without schema (host + base URL)*/
	str schema;			/*!< The URL schema */
	str username;			/*!< The username to use for auth */
	str password;			/*!< The password to use for auth */
	str failover;			/*!< Another connection to use if this one fails */
	str cacert;			/*!< File name of CA cert to use */
	str clientcert;			/*!< File name of CA client cert */
	str useragent;			/*!< Useragent to use for this connection */
	int tls_verifyserver;		/*!< TRUE if server cert needs to be verified */
	int http_follow_redirect;	/*!< TRUE if we should follow HTTP 302 redirects */
	unsigned int port;		/*!< The port to connect to */
	int timeout;			/*!< Timeout for this connection */
	http_res_stream_t *stream;	/*!< Curl stream */
	struct _curl_con *next;		/*!< next connection */
	char redirecturl[512];		/*!< Last redirect URL - to use for $curlredirect(curlcon) pv */
} curl_con_t;


/*! Returns true if CURL supports TLS */
extern int curl_support_tls();

/*! Returns TRUE if curl supports IPv6 */
extern int curl_support_ipv6();

#endif /* CURL_H */

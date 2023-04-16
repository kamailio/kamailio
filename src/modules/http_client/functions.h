/*
 * headers of script functions of http_client module
 *
 * Copyright (C) 2008 Juha Heinanen
 * Copyright (C) 2013 Carsten Bock, ng-voice GmbH
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
 * \brief Kamailio http_client :: script functions include file
 * \ingroup http_client
 * Module: \ref http_client
 */


#ifndef CURL_FUNCTIONS_H
#define CURL_FUNCTIONS_H

#include "../../core/parser/msg_parser.h"

/*! Use predefined connection to run HTTP get or post
 */
int curl_con_query_url(struct sip_msg *_m, const str *connection,
		const str *_url, str *_result, const char *contenttype,
		const str *_post);

/*! Get redirect URL from last connection pkg memory storage */
int curl_get_redirect(struct sip_msg *_m, const str *connection, str *result);


/*
 * Performs http_client_query and saves possible result
 * (first body line of reply) to pvar.
 */
int http_client_query(
		struct sip_msg *_m, char *_url, str *_dst, char *_post, char *_hdrs);

/*
 * Performs http_client_query and saves possible result
 * (first body line of reply) to pvar.
 */
int http_client_query_c(
		struct sip_msg *_m, char *_url, str *_dst, char *_post, char *_ctype, char *_hdrs);

/*
 * Performs http request and saves possible result
 * (first body line of reply) to pvar.
 */
int http_client_request(
		sip_msg_t *_m, char *_url, str *_dst, char *_body, char *_hdrs, char *_met);

/*
 * Performs http request and saves possible result
 * (first body line of reply) to pvar.
 */
int http_client_request_c(
		sip_msg_t *_m, char *_url, str *_dst, char *_body, char *_ctype, char *_hdrs, char *_met);


char *http_get_content_type(const str *connection);

#endif /* CURL_FUNCTIONS_H */

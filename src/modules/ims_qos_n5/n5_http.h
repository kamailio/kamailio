/*
 * ims_qos_n5 - P-CSCF media authorisation over N5 (Npcf_PolicyAuthorization)
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
 */

#ifndef _IMS_QOS_N5_HTTP_H_
#define _IMS_QOS_N5_HTTP_H_

#include "../../core/str.h"

/* result of one SBI request; strings are pkg-allocated, 0-terminated */
typedef struct n5_http_res
{
	long code;	  /* HTTP status, 0 if the request was not sent */
	str location; /* Location header of a 201, if any */
	str body;	  /* response body, if any */
} n5_http_res_t;

int n5_http_mod_init(void);
void n5_http_mod_destroy(void);

/* Send one request over a per-process HTTP/2 connection (prior knowledge).
 * body and bearer (an OAuth2 access token) may be NULL. Returns 0 if a response was received (any status),
 * -1 on transport failure or timeout. */
int n5_http_request(const char *method, const char *url, const char *ctype,
		const char *body, const char *bearer, n5_http_res_t *res);

void n5_http_res_free(n5_http_res_t *res);

#endif

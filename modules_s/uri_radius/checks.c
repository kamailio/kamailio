/* checks.c v 0.1 2003/1/20
 *
 * Radius based checks
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-11: Code cleanup (janakj)
 */


#include <radiusclient.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "ser_radius.h"
#include "checks.h"


/*
 * Check from Radius if request URI belongs to a local user.
 * User-Name is user@host of request Uri and Service-Type is Call-Check.
 */
int radius_does_uri_exist(struct sip_msg* _m, char* _s1, char* _s2)
{
	static char msg[4096];
	VALUE_PAIR *send, *received;
	UINT4 service;
	char* at, *uri;

	send = received = 0;

	if (parse_sip_msg_uri(_m) < 0) {
		LOG(L_ERR, "radius_does_uri_exist(): Error while parsing URI\n");
		return -1;
	}
	
	uri = (char*)pkg_malloc(_m->parsed_uri.user.len + _m->parsed_uri.host.len + 2);
	if (!uri) {
		LOG(L_ERR, "radius_does_uri_exist(): No memory left\n");
		return -2;
	}

	at = uri;
	memcpy(at, _m->parsed_uri.user.s, _m->parsed_uri.user.len);
	at += _m->parsed_uri.user.len;
	*at = '@';
	at++;
	memcpy(at , _m->parsed_uri.host.s, _m->parsed_uri.host.len);
	at += _m->parsed_uri.host.len;
	*at = '\0';

	if (!rc_avpair_add(&send, PW_USER_NAME, uri, 0)) {
		LOG(L_ERR, "radius_does_uri_exist(): Error adding User-Name\n");
		rc_avpair_free(send);
		pkg_free(uri);
	 	return -3;
	}

	service = PW_CALL_CHECK;
	if (!rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0)) {
		LOG(L_ERR, "radius_does_uri_exist(): Error adding service type\n");
		rc_avpair_free(send);
		pkg_free(uri);
	 	return -4;  	
	}
	
	if (rc_auth(0, send, &received, msg) == OK_RC) {
		DBG("radius_does_uri_exist(): Success\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		pkg_free(uri);
		return 1;
	} else {
		DBG("radius_does_uri_exist(): Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		pkg_free(uri);
		return -5;
	}
}

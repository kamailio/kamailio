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
 */

#include "checks.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../ut.h"
#include <radiusclient.h>
#include "ser_radius.h"
#include <stdlib.h>


/*
 * Check from Radius if request URI belongs to a local user.
 * User-Name is user@host of request Uri and Service-Type is Call-Check.
 */
int radius_does_uri_exist(struct sip_msg* _msg, char* _s1, char* _s2)
{
	char            msg[4096];
	VALUE_PAIR      *send, *received;
	UINT4           service;

	char uri[MAX_URI_SIZE];
	char* at;

	send = NULL;
	received = NULL;

	if (parse_sip_msg_uri(_msg) < 0) {
		LOG(L_ERR, "does_uri_exist(): Error while parsing URI\n");
		return -1;
	}
	
	if (_msg->parsed_uri.user.len + _msg->parsed_uri.host.len + 2 > MAX_URI_SIZE) {
		LOG(L_ERR, "radius_does_uri_exist(): URI user too large\n");
		return -1;
	}

	at = &(uri[0]);
	memcpy(at, _msg->parsed_uri.user.s, _msg->parsed_uri.user.len);
	at = at + _msg->parsed_uri.user.len;
	*at = '@';
	at = at + 1;
	memcpy(at , _msg->parsed_uri.host.s, _msg->parsed_uri.host.len);
	at = at + _msg->parsed_uri.host.len;
	*at = '\0';

	if (rc_avpair_add(&send, PW_USER_NAME, uri, 0) == NULL) {
		LOG(L_ERR, "radius_does_uri_exist(): Error adding User-Name \n");
		rc_avpair_free(send);
	 	return -1;  	
	}

	service = PW_CALL_CHECK;
	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL) {
		LOG(L_ERR, "radius_does_uri_exist(): Error adding service type \n");
		rc_avpair_free(send);
	 	return -1;  	
	}
	
	if (rc_auth(0, send, &received, msg) == OK_RC) {
		DBG("radius_does_uri_exist(): Success \n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return 1;
	} else {
		DBG("radius_does_uri_exist(): Failure \n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -1;
	}
}


/*
 * Check from Radius if a user belongs to a group. User-Name is digest
 * username or digest username@realm, SIP-Group is group, and Service-Type
 * is Group-Check.  SIP-Group is SER specific attribute and Group-Check is
 * SER specific service type value.
 */
int radius_is_in_group (struct sip_msg* _msg, char* _group, char* _s2)
{
	str *grp, user_name;
	struct hdr_field* h;
	dig_cred_t* cred;

	UINT4 service;
	VALUE_PAIR *send, *received;
	char msg[4096];

	grp = (str*)_group; /* via fixup */
	send = NULL;
	received = NULL;

	get_authorized_cred(_msg->authorization, &h);
	if (!h) {
		get_authorized_cred(_msg->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "radius_is_in_group(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	cred = &(((auth_body_t*)(h->parsed))->digest);

	if (q_memchr(cred->username.s, '@', cred->username.len)) {
		if (rc_avpair_add(&send, PW_USER_NAME, cred->username.s, cred->username.len) == NULL) {
			LOG(L_ERR, "radius_is_user_in_group(): Error adding PW_USER_NAME\n");
			rc_avpair_free(send);
			return -1;
		}
	} else {
		user_name.len = cred->username.len + cred->realm.len + 1;
		user_name.s = malloc(user_name.len);
		if (!user_name.s) {
			LOG(L_ERR, "radius_is_user_in_group(): Memory allocation failure\n");
			return -1;
		}
		strncpy(user_name.s, cred->username.s, cred->username.len);
		user_name.s[cred->username.len] = '@';
		strncpy(user_name.s + cred->username.len + 1, cred->realm.s, cred->realm.len);
		if (rc_avpair_add(&send, PW_USER_NAME, user_name.s, user_name.len) == NULL) {
			free(user_name.s);
			LOG(L_ERR, "radius_is_user_in_group(): Error adding PW_USER_NAME\n");
			rc_avpair_free(send);
			return -1;
		}
		free(user_name.s);
	}

	if (rc_avpair_add(&send, PW_SIP_GROUP, grp->s, grp->len) == NULL) {
		LOG(L_ERR, "radius_is_user_in_group(): Error adding PW_SIP_GROUP\n");
	 	return -1;  	
	}

	service = PW_GROUP_CHECK;
	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL) {
		LOG(L_ERR, "radius_is_user_in_group(): Error adding PW_SERVICE_TYPE\n");
		rc_avpair_free(send);
	 	return -1;  	
	}

	if (rc_auth(0, send, &received, msg) == OK_RC) {
		DBG("radius_is_user_in_group(): Success\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return 1;
	} else {
		DBG("radius_is_user_in_group(): Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -1;
	}
}

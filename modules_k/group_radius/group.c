/*
 * $Id$
 *
 * Group membership checking over Radius
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-03-10 - created by janakj
 *
 */

#include <string.h>
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/hf.h"
#include "../../parser/digest/digest.h"
#include "../../radius.h"
#include "group.h"
#include "grouprad_mod.h"


/*
 * Check from Radius if a user belongs to a group. User-Name is digest
 * username or digest username@realm, SIP-Group is group, and Service-Type
 * is Group-Check.  SIP-Group is SER specific attribute and Group-Check is
 * SER specific service type value.
 */
int radius_is_user_in(struct sip_msg* _m, char* _hf, char* _group)
{
	str *grp, user_name, user, domain;
	dig_cred_t* cred = 0;
	int hf_type;
	uint32_t service;
	VALUE_PAIR *send, *received;
	static char msg[4096];
	struct hdr_field* h;
	struct sip_uri *turi;

	grp = (str*)_group; /* via fixup */
	send = received = 0;

	hf_type = (int)(long)_hf;

	turi = 0;

	switch(hf_type) {
		case 1: /* Request-URI */
			if(parse_sip_msg_uri(_m)<0) {
				LM_ERR("failed to get Request-URI\n");
				return -1;
			}
			turi = &_m->parsed_uri;
			break;

		case 2: /* To */
			if((turi=parse_to_uri(_m))==NULL) {
				LM_ERR("failed to get To URI\n");
				return -1;
			}
			break;

		case 3: /* From */
			if((turi=parse_from_uri(_m))==NULL) {
				LM_ERR("failed to get From URI\n");
				return -1;
			}
			break;

		case 4: /* Credentials */
			get_authorized_cred(_m->authorization, &h);
			if (!h) {
				get_authorized_cred(_m->proxy_auth, &h);
				if (!h) {
				LM_ERR("no authorized"
							" credentials found (error in scripts)\n");
					return -4;
				}
			}
			cred = &((auth_body_t*)(h->parsed))->digest;
			break;
	}

	if (hf_type != 4) {
		user = turi->user;
		domain = turi->host;
	} else {
		user = cred->username.user;
		domain = *GET_REALM(cred);
	}

	if (user.s==NULL || user.len==0 ) {
		LM_DBG("no username part\n");
		return -1;
	}

	if (use_domain) {
		user_name.len = user.len + domain.len + 1;
		user_name.s = (char*)pkg_malloc(user_name.len);
		if (!user_name.s) {
			LM_ERR("no pkg memory left\n");
			return -6;
		}
		
		memcpy(user_name.s, user.s, user.len);
		user_name.s[user.len] = '@';
		memcpy(user_name.s + user.len + 1, domain.s, domain.len);
	} else {
		user_name = user;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user_name.s, user_name.len, 0)) {
		LM_ERR("failed to add User-Name attribute\n");
		rc_avpair_free(send);
		if (use_domain) pkg_free(user_name.s);
		return -7;
	}

	if (use_domain) pkg_free(user_name.s);

	if (!rc_avpair_add(rh, &send, attrs[A_SIP_GROUP].v, grp->s, grp->len, 0)) {
		LM_ERR("failed to add Sip-Group attribute\n");
	 	return -8;  	
	}

	service = vals[V_GROUP_CHECK].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
		LM_ERR("failed to add Service-Type attribute\n");
		rc_avpair_free(send);
	 	return -9;  	
	}

	if (rc_auth(rh, 0, send, &received, msg) == OK_RC) {
		LM_DBG("Success\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return 1;
	} else {
		LM_DBG("Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -11;
	}
}

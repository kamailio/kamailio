/* 
 * $Id$
 *
 * Digest Authentication - Radius support
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-03-09: Based on digest.c from radius_auth module (janakj)
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../modules/auth/api.h"
#include "../../rad_dict.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "sterman.h"
#include "authrad_mod.h"

#include <stdlib.h>
#include <string.h>

static int add_cisco_vsa(VALUE_PAIR** send, struct sip_msg* msg)
{
	str callid;

	if (!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) == -1) {
		LOG(L_ERR, "add_cisco_vsa: Cannot parse Call-ID header field\n");
		return -1;
	}

	if (!msg->callid) {
		LOG(L_ERR, "add_cisco_vsa: Call-ID header field not found\n");
		return -1;
	}

	callid.len = msg->callid->body.len + 8;
	callid.s = pkg_malloc(callid.len);
	if (callid.s == NULL) {
		LOG(L_ERR, "add_cisco_vsa: No memory left\n");
		return -1;
	}

	memcpy(callid.s, "call-id=", 8);
	memcpy(callid.s + 8, msg->callid->body.s, msg->callid->body.len);

	if (rc_avpair_add(rh, send, ATTRID(attrs[A_CISCO_AVPAIR].v), callid.s,
			  callid.len, VENDOR(attrs[A_CISCO_AVPAIR].v)) == 0) {
		LOG(L_ERR, "add_cisco_vsa: Unable to add Cisco-AVPair attribute\n");
		pkg_free(callid.s);
		return -1;
	}

	pkg_free(callid.s);
	return 0;
}


/*
 * This function creates and submits radius authentication request as per
 * draft-sterman-aaa-sip-00.txt.  In addition, _user parameter is included
 * in the request as value of a SER specific attribute type SIP-URI-User,
 * which can be be used as a check item in the request.  Service type of
 * the request is Authenticate-Only.
 */
int radius_authorize_sterman(VALUE_PAIR** received, struct sip_msg* _msg, dig_cred_t* _cred, str* _method, str* _user) 
{
	static char msg[4096];
	VALUE_PAIR *send;
	UINT4 service, ser_service_type;
	str method, user, user_name;
	str *ruri;
	int i;
	
	send = 0;

	if (!(_cred && _method && _user)) {
		LOG(L_ERR, "radius_authorize_sterman(): Invalid parameter value\n");
		return -1;
	}

	method = *_method;
	user = *_user;
	
	/*
	 * Add all the user digest parameters according to the qop defined.
	 * Most devices tested only offer support for the simplest digest.
	 */
	if (_cred->username.domain.len) {
	        if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_USER_NAME].v), 
				   _cred->username.whole.s, _cred->username.whole.len, 
			           VENDOR(attrs[A_USER_NAME].v))) {
			LOG(L_ERR, "radius_authorize_sterman(): Unable to add User-Name attribute\n");
			goto err;
		}
	} else {
		user_name.len = _cred->username.user.len + _cred->realm.len + 1;
		user_name.s = pkg_malloc(user_name.len);
		if (!user_name.s) {
			LOG(L_ERR, "radius_authorize_sterman(): No memory left\n");
			return -3;
		}
		memcpy(user_name.s, _cred->username.whole.s, _cred->username.whole.len);
		user_name.s[_cred->username.whole.len] = '@';
		memcpy(user_name.s + _cred->username.whole.len + 1, _cred->realm.s, _cred->realm.len);
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_USER_NAME].v), 
				   user_name.s, user_name.len, 
				   VENDOR(attrs[A_USER_NAME].v))) {
			LOG(L_ERR, "sterman(): Unable to add User-Name attribute\n");
			pkg_free(user_name.s);
			goto err;
		}
		pkg_free(user_name.s);
	}

	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_USER_NAME].v), 
			   _cred->username.whole.s, _cred->username.whole.len, 
			   VENDOR(attrs[A_DIGEST_USER_NAME].v))) {
		LOG(L_ERR, "sterman(): Unable to add Digest-User-Name attribute\n");
		goto err;
	}

	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_REALM].v), 
			   _cred->realm.s, _cred->realm.len, 
			   VENDOR(attrs[A_DIGEST_REALM].v))) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Realm attribute\n");
		goto err;
	}
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_NONCE].v), 
			   _cred->nonce.s, _cred->nonce.len, 
			   VENDOR(attrs[A_DIGEST_NONCE].v))) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Nonce attribute\n");
		goto err;
	}
	
	if (use_ruri_flag < 0 || isflagset(_msg, use_ruri_flag) != 1) {
		ruri = &_cred->uri;
	} else {
		ruri = GET_RURI(_msg);
	}
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_URI].v), 
			   ruri->s, ruri->len, 
			   VENDOR(attrs[A_DIGEST_URI].v))) {
		LOG(L_ERR, "sterman(): Unable to add Digest-URI attribute\n");
		goto err;
	}
		
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_METHOD].v),
			   method.s, method.len, 
			   VENDOR(attrs[A_DIGEST_METHOD].v))) {
	        LOG(L_ERR, "sterman(): Unable to add Digest-Method attribute\n");
		goto err;
	}
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (_cred->qop.qop_parsed == QOP_AUTH) {
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_QOP].v), "auth", 4,
				   VENDOR(attrs[A_DIGEST_QOP].v))) {
			LOG(L_ERR, "sterman(): Unable to add Digest-QOP attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_NONCE_COUNT].v), 
				   _cred->nc.s, _cred->nc.len,
				   VENDOR(attrs[A_DIGEST_NONCE_COUNT].v))) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce-Count attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_CNONCE].v), 
				   _cred->cnonce.s, _cred->cnonce.len,
				   VENDOR(attrs[A_DIGEST_CNONCE].v))) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce attribute\n");
			goto err;
		}
	} else if (_cred->qop.qop_parsed == QOP_AUTHINT) {
	        if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_QOP].v), "auth-int", 8, 
				   VENDOR(attrs[A_DIGEST_QOP].v))) {
		        LOG(L_ERR, "sterman(): Unable to add Digest-QOP attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_NONCE_COUNT].v), 
				   _cred->nc.s, _cred->nc.len, 
				   VENDOR(attrs[A_DIGEST_NONCE_COUNT].v))) {
			LOG(L_ERR, "sterman(): Unable to add Digest-Nonce-Count attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_CNONCE].v), 
				   _cred->cnonce.s, _cred->cnonce.len, 
				   VENDOR(attrs[A_DIGEST_CNONCE].v))) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_BODY_DIGEST].v), 
				   _cred->opaque.s, _cred->opaque.len, 
				   VENDOR(attrs[A_DIGEST_BODY_DIGEST].v))) {
			LOG(L_ERR, "sterman(): Unable to add Digest-Body-Digest attribute\n");
			goto err;
		}
		
	} else  {
		/* send nothing for qop == "" */
	}

	/* Add the response... What to calculate against... */
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_DIGEST_RESPONSE].v), 
			   _cred->response.s, _cred->response.len, 
			   VENDOR(attrs[A_DIGEST_RESPONSE].v))) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Response attribute\n");
		goto err;
	}

	/* Indicate the service type, Authenticate only in our case */
	service = vals[V_SIP_SESSION].v;
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SERVICE_TYPE].v), 
			   &service, -1, 
			   VENDOR(attrs[A_SERVICE_TYPE].v))) {
	        LOG(L_ERR, "sterman(): Unable to add Service-Type attribute\n");
		goto err;
	}

	/* Indicate the service type, Authenticate only in our case */
	ser_service_type = vals[V_DIGEST_AUTHENTICATION].v;
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SER_SERVICE_TYPE].v), 
			   &ser_service_type, -1, 
			   VENDOR(attrs[A_SER_SERVICE_TYPE].v))) {
		LOG(L_ERR, "sterman(): Unable to add SER-Service-Type attribute\n");
		goto err;
	}

	/* Add SIP URI as a check item */
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SER_URI_USER].v), 
			   user.s, user.len, 
			   VENDOR(attrs[A_SER_URI_USER].v))) {
		LOG(L_ERR, "sterman(): Unable to add Sip-URI-User attribute\n");
		goto err;
	}

	if (attrs[A_CISCO_AVPAIR].n != NULL) {
		if (add_cisco_vsa(&send, _msg)) {
			goto err;
		}
	}

	/* Send request */
	if ((i = rc_auth(rh, SIP_PORT, send, received, msg)) == OK_RC) {
		DBG("radius_authorize_sterman(): Success\n");
		rc_avpair_free(send);
		send = 0;
		return 1;
	} else {
		DBG("radius_authorize_sterman(): Failure\n");
		goto err;
	}

 err:
	if (send) rc_avpair_free(send);	
	return -1;
}

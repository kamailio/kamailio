/* 
 * $Id$
 *
 * Digest Authentication - Radius support
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * 2003-03-09: Based on digest.c from radius_auth module (janakj)
 */


#include <stdlib.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../auth/api.h"
#include "../../modules/acc/dict.h"
#include "sterman.h"
#include "authrad_mod.h"
#include <radiusclient.h>


/*
 * This function creates and submits radius authentication request as per
 * draft-sterman-aaa-sip-00.txt.  In addition, _user parameter is included
 * in the request as value of a SER specific attribute type SIP-URI-User,
 * which can be be used as a check item in the request.  Service type of
 * the request is Authenticate-Only.
 */
int radius_authorize_sterman(struct sip_msg* _msg, dig_cred_t* _cred, str* _method, str* _user, str* _rpid) 
{
	static char msg[4096];
	VALUE_PAIR *send, *received, *vp;
	UINT4 service;
	str method, user, user_name, callid;
	int i;
	
	send = received = 0;

	if (!(_cred && _method && _user && _rpid)) {
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
		if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, _cred->username.whole.s, _cred->username.whole.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add User-Name attribute\n");
			rc_avpair_free(send);
			return -2;
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
		if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user_name.s, user_name.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add User-Name attribute\n");
			pkg_free(user_name.s);
			rc_avpair_free(send);
			return -4;
		}
		pkg_free(user_name.s);
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_USER_NAME].v, _cred->username.whole.s, _cred->username.whole.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-User-Name attribute\n");
		rc_avpair_free(send);
		return -5;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_REALM].v, _cred->realm.s, _cred->realm.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Realm attribute\n");
		rc_avpair_free(send);
		return -6;
	}
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE].v, _cred->nonce.s, _cred->nonce.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Nonce attribute\n");
		rc_avpair_free(send);
		return -7;
	}
	
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_URI].v, _cred->uri.s, _cred->uri.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-URI attribute\n");
		rc_avpair_free(send);
		return -8;
	}
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_METHOD].v, method.s, method.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Method attribute\n");
		rc_avpair_free(send);
		return -9;
	}
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (_cred->qop.qop_parsed == QOP_AUTH) {
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_QOP].v, "auth", 4, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-QOP attribute\n");
			rc_avpair_free(send);
			return -10;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE_COUNT].v, _cred->nc.s, _cred->nc.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce-Count attribute\n");
			rc_avpair_free(send);
			return -11;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_CNONCE].v, _cred->cnonce.s, _cred->cnonce.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce attribute\n");
			rc_avpair_free(send);
			return -12;
		}
	} else if (_cred->qop.qop_parsed == QOP_AUTHINT) {
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_QOP].v, "auth-int", 8, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-QOP attribute\n");
			rc_avpair_free(send);
			return -13;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE_COUNT].v, _cred->nc.s, _cred->nc.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-Nonce-Count attribute\n");
			rc_avpair_free(send);
			return -14;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_CNONCE].v, _cred->cnonce.s, _cred->cnonce.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce attribute\n");
			rc_avpair_free(send);
			return -15;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_BODY_DIGEST].v, _cred->opaque.s, _cred->opaque.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-Body-Digest attribute\n");
			rc_avpair_free(send);
			return -16;
		}
		
	} else  {
		/* send nothing for qop == "" */
	}

	/* Add the response... What to calculate against... */
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_RESPONSE].v, _cred->response.s, _cred->response.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Response attribute\n");
		rc_avpair_free(send);
		return -17;
	}

	/* Indicate the service type, Authenticate only in our case */
	service = vals[V_SIP_SESSION].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, 0, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Service-Type attribute\n");
		rc_avpair_free(send);
	 	return -18;
	}

	/* Add SIP URI as a check item */
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_USER].v, user.s, user.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Sip-URI-User attribute\n");
		rc_avpair_free(send);
	 	return -19;  	
	}

	if (ciscopec != -1) {
		/* Add SIP Call-ID as a Cisco VSA, like IOS does */
		if (_msg->callid == NULL || _msg->callid->body.s == NULL) {
			LOG(L_ERR, "sterman(): Call-ID is missed\n");
			rc_avpair_free(send);
			return -20;
		}
		callid.len = _msg->callid->body.len + 8;
		callid.s = alloca(callid.len);
		if (callid.s == NULL) {
			LOG(L_ERR, "sterman(): No memory left\n");
			rc_avpair_free(send);
			return -21;
		}
		memcpy(callid.s, "call-id=", 8);
		memcpy(callid.s + 8, _msg->callid->body.s, _msg->callid->body.len);
		if (rc_avpair_add(rh, &send, attrs[A_CISCO_AVPAIR].v, callid.s,
		    callid.len, ciscopec) == 0) {
			LOG(L_ERR, "sterman(): Unable to add Cisco-AVPair attribute\n");
			rc_avpair_free(send);
			return -22;
 		}
	}

	/* Send request */
	if ((i = rc_auth(rh, SIP_PORT, send, &received, msg)) == OK_RC) {
		DBG("radius_authorize_sterman(): Success\n");
		rc_avpair_free(send);

		     /* Make a copy of rpid if available */
		if ((vp = rc_avpair_get(received, attrs[A_SIP_RPID].v, 0))) {
			if (MAX_RPID_LEN < vp->lvalue) {
				LOG(L_ERR, "radius_authorize_sterman(): rpid buffer too small\n");
				return -23;
			}
			memcpy(_rpid->s, vp->strvalue, vp->lvalue);
			_rpid->len = vp->lvalue;
		}

		rc_avpair_free(received);
		return 1;
	} else {
		DBG("res: %d\n", i);
		DBG("radius_authorize_sterman(): Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -24;
	}
}

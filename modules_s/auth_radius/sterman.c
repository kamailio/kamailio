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


#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../auth/api.h"
#include "ser_radius.h"
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
int radius_authorize_sterman(dig_cred_t* _cred, str* _method, str* _user, str* _rpid) 
{
	static char msg[4096];
	VALUE_PAIR *send, *received, *vp;
	UINT4 service;
	str method, user, user_name;

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
		if (!rc_avpair_add(&send, PW_USER_NAME, _cred->username.whole.s, _cred->username.whole.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_USER_NAME attribute\n");
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
		if (!rc_avpair_add(&send, PW_USER_NAME, user_name.s, user_name.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_USER_NAME attribute\n");
			pkg_free(user_name.s);
			rc_avpair_free(send);
			return -4;
		}
		pkg_free(user_name.s);
	}

	if (!rc_avpair_add(&send, PW_DIGEST_USER_NAME, _cred->username.whole.s, _cred->username.whole.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_USER_NAME attribute\n");
		rc_avpair_free(send);
		return -5;
	}

	if (!rc_avpair_add(&send, PW_DIGEST_REALM, _cred->realm.s, _cred->realm.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_REALM attribute\n");
		rc_avpair_free(send);
		return -6;
	}
	if (!rc_avpair_add(&send, PW_DIGEST_NONCE, _cred->nonce.s, _cred->nonce.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_NONCE attribute\n");
		rc_avpair_free(send);
		return -7;
	}
	
	if (!rc_avpair_add(&send, PW_DIGEST_URI, _cred->uri.s, _cred->uri.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_URI attribute\n");
		rc_avpair_free(send);
		return -8;
	}
	if (!rc_avpair_add(&send, PW_DIGEST_METHOD, method.s, method.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_METHOD attribute\n");
		rc_avpair_free(send);
		return -9;
	}
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (_cred->qop.qop_parsed == QOP_AUTH) {
		if (!rc_avpair_add(&send, PW_DIGEST_QOP, "auth", 4)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_QOP attribute\n");
			rc_avpair_free(send);
			return -10;
		}
		if (!rc_avpair_add(&send, PW_DIGEST_NONCE_COUNT, _cred->nc.s, _cred->nc.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_NONCE_COUNT attribute\n");
			rc_avpair_free(send);
			return -11;
		}
		if (!rc_avpair_add(&send, PW_DIGEST_CNONCE, _cred->cnonce.s, _cred->cnonce.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_CNONCE attribute\n");
			rc_avpair_free(send);
			return -12;
		}
	} else if (_cred->qop.qop_parsed == QOP_AUTHINT) {
		if (!rc_avpair_add(&send, PW_DIGEST_QOP, "auth-int", 8)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_QOP attribute\n");
			rc_avpair_free(send);
			return -13;
		}
		if (!rc_avpair_add(&send, PW_DIGEST_NONCE_COUNT, _cred->nc.s, _cred->nc.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_NONCE_COUNT attribute\n");
			rc_avpair_free(send);
			return -14;
		}
		if (!rc_avpair_add(&send, PW_DIGEST_CNONCE, _cred->cnonce.s, _cred->cnonce.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_CNONCE attribute\n");
			rc_avpair_free(send);
			return -15;
		}
		if (!rc_avpair_add(&send, PW_DIGEST_BODY_DIGEST, _cred->opaque.s, _cred->opaque.len)) {
			LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_BODY_DIGEST attribute\n");
			rc_avpair_free(send);
			return -16;
		}
		
	} else  {
		/* send nothing for qop == "" */
	}

	/*
	 * Now put everything place all the previous attributes into the
	 * PW_DIGEST_ATTRIBUTES
	 */
	
	/*
	 *  Fix up Digest-Attributes issues see draft-sterman-aaa-sip-00
	 */
	for (vp = send; vp; vp = vp->next) {
		switch (vp->attribute) {
  		default:
			break;

			/* Fall thru the know values */
		case PW_DIGEST_REALM:
		case PW_DIGEST_NONCE:
		case PW_DIGEST_METHOD:
		case PW_DIGEST_URI:
		case PW_DIGEST_QOP:
		case PW_DIGEST_ALGORITHM:
		case PW_DIGEST_BODY_DIGEST:
		case PW_DIGEST_CNONCE:
		case PW_DIGEST_NONCE_COUNT:
		case PW_DIGEST_USER_NAME:
	
			/* overlapping! */
			memmove(&vp->strvalue[2], &vp->strvalue[0], vp->lvalue);
			vp->strvalue[0] = vp->attribute - PW_DIGEST_REALM + 1;
			vp->lvalue += 2;
			vp->strvalue[1] = vp->lvalue;
			vp->attribute = PW_DIGEST_ATTRIBUTES;
			break;
		}
	}

	/* Add the response... What to calculate against... */
	if (!rc_avpair_add(&send, PW_DIGEST_RESPONSE, _cred->response.s, _cred->response.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_DIGEST_RESPONSE attribute\n");
		rc_avpair_free(send);
		return -17;
	}

	/* Indicate the service type, Authenticate only in our case */
	service = service_type;
	if (!rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0)) {
		LOG(L_ERR, "sterman(): Unable to add PW_SERVICE_TYPE attribute\n");
		rc_avpair_free(send);
	 	return -18;
	}

	/* Add SIP URI as a check item */
	if (!rc_avpair_add(&send, PW_SIP_URI_USER, user.s, user.len)) {
		LOG(L_ERR, "sterman(): Unable to add PW_SIP_URI_USER attribute\n");
		rc_avpair_free(send);
	 	return -19;  	
	}
       
	/* Send request */
	if (rc_auth(SIP_PORT, send, &received, msg) == OK_RC) {
		DBG("radius_authorize_sterman(): Success\n");
		rc_avpair_free(send);

		     /* Make a copy of rpid if available */
		if ((vp = rc_avpair_get(received, PW_SIP_RPID))) {
			if (MAX_RPID_LEN < vp->lvalue) {
				LOG(L_ERR, "radius_authorize_sterman(): rpid buffer too small\n");
				return -20;
			}
			memcpy(_rpid->s, vp->strvalue, vp->lvalue);
			_rpid->len = vp->lvalue;
		}

		rc_avpair_free(received);
		return 1;
	} else {
		DBG("radius_authorize_sterman(): Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -21;
	}
}

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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-09: Based on digest.c from radius_auth module (janakj)
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../auth/api.h"
#include "../../modules/acc/dict.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "sterman.h"
#include "authrad_mod.h"

#include <stdlib.h>
#include <string.h>
#include <radiusclient.h>


static void attr_name_value(VALUE_PAIR* vp, str* name, str* value)
{
	int i;
	
	for (i = 0; i < vp->lvalue; i++) {
		if (vp->strvalue[i] == ':') {
			name->s = vp->strvalue;
			name->len = i;

			if (i == (vp->lvalue - 1)) {
				value->s = (char*)0;
				value->len = 0;
			} else {
				value->s = vp->strvalue + i + 1;
				value->len = vp->lvalue - i - 1;
			}
			return;
		}
	}

	name->len = value->len = 0;
	name->s = value->s = (char*)0;
}


/*
 * Generate AVPs from the database result
 */
static int generate_avps(VALUE_PAIR* received)
{
	str name_str, val_str;
	int_str name, val;
	VALUE_PAIR *vp;

	vp = received;
	name.s = &name_str;
	val.s = &val_str;

	while ((vp = rc_avpair_get(vp, attrs[A_SIP_AVP].v, 0))) {
		attr_name_value(vp, &name_str, &val_str);
		
		if (add_avp(AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) {
			LOG(L_ERR, "generate_avps: Unable to create a new AVP\n");
		} else {
			DBG("generate_avps: AVP '%.*s'='%.*s' has been added\n",
			    name_str.len, ZSW(name_str.s), 
			    val_str.len, ZSW(val_str.s));
		}
		vp = vp->next;
	}
	
	return 0;
}


static int add_cisco_vsa(VALUE_PAIR** send, struct sip_msg* msg)
{
	str callid;

	if (!msg->callid && parse_headers(msg, HDR_CALLID, 0) == -1) {
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

	if (rc_avpair_add(rh, send, attrs[A_CISCO_AVPAIR].v, callid.s,
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
int radius_authorize_sterman(struct sip_msg* _msg, dig_cred_t* _cred, str* _method, str* _user) 
{
	static char msg[4096];
	VALUE_PAIR *send, *received;
	UINT4 service;
	str method, user, user_name;
	int i;
	
	send = received = 0;

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
		if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, _cred->username.whole.s, _cred->username.whole.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add User-Name attribute\n");
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
		if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user_name.s, user_name.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add User-Name attribute\n");
			pkg_free(user_name.s);
			goto err;
		}
		pkg_free(user_name.s);
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_USER_NAME].v, _cred->username.whole.s, _cred->username.whole.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-User-Name attribute\n");
		goto err;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_REALM].v, _cred->realm.s, _cred->realm.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Realm attribute\n");
		goto err;
	}
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE].v, _cred->nonce.s, _cred->nonce.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Nonce attribute\n");
		goto err;
	}
	
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_URI].v, _cred->uri.s, _cred->uri.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-URI attribute\n");
		goto err;
	}
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_METHOD].v, method.s, method.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Method attribute\n");
		goto err;
	}
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (_cred->qop.qop_parsed == QOP_AUTH) {
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_QOP].v, "auth", 4, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-QOP attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE_COUNT].v, _cred->nc.s, _cred->nc.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce-Count attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_CNONCE].v, _cred->cnonce.s, _cred->cnonce.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce attribute\n");
			goto err;
		}
	} else if (_cred->qop.qop_parsed == QOP_AUTHINT) {
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_QOP].v, "auth-int", 8, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-QOP attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE_COUNT].v, _cred->nc.s, _cred->nc.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-Nonce-Count attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_CNONCE].v, _cred->cnonce.s, _cred->cnonce.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-CNonce attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_BODY_DIGEST].v, _cred->opaque.s, _cred->opaque.len, 0)) {
			LOG(L_ERR, "sterman(): Unable to add Digest-Body-Digest attribute\n");
			goto err;
		}
		
	} else  {
		/* send nothing for qop == "" */
	}

	/* Add the response... What to calculate against... */
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_RESPONSE].v, _cred->response.s, _cred->response.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Digest-Response attribute\n");
		goto err;
	}

	/* Indicate the service type, Authenticate only in our case */
	service = vals[V_SIP_SESSION].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Service-Type attribute\n");
		goto err;
	}

	/* Add SIP URI as a check item */
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_USER].v, user.s, user.len, 0)) {
		LOG(L_ERR, "sterman(): Unable to add Sip-URI-User attribute\n");
		goto err;
	}

	if (attrs[A_CISCO_AVPAIR].n != NULL) {
		if (add_cisco_vsa(&send, _msg)) {
			goto err;
		}
	}

	/* Send request */
	if ((i = rc_auth(rh, SIP_PORT, send, &received, msg)) == OK_RC) {
		DBG("radius_authorize_sterman(): Success\n");
		rc_avpair_free(send);
		send = 0;

		if (generate_avps(received)) {
			goto err;
		}

		rc_avpair_free(received);
		return 1;
	} else {
		DBG("radius_authorize_sterman(): Failure\n");
		goto err;
	}

 err:
	if (send) rc_avpair_free(send);
	if (received) rc_avpair_free(received);
	return -1;
}

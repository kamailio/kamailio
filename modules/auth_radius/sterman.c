/* 
 * $Id$
 *
 * Digest Authentication - Radius support
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-03-09: Based on digest.c from radius_auth module (janakj)
 * 2005-07-08: Radius AVP may contain any kind of Kamailio AVP - ID/name or
 *             int/str value (bogdan)
 * 2005-07-08: old RPID RADIUS AVP compatibility droped (bogdan)
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../lib/kcore/radius.h"
#include "../../ut.h"
#include "../../modules/auth/api.h"
#include "sterman.h"
#include "authrad_mod.h"
#include "extra.h"

#include <stdlib.h>
#include <string.h>


/* Array for extra attribute values */
static str val_arr[MAX_EXTRA];


/* Macro to add extra attribute */
#define ADD_EXTRA_AVPAIR(_attrs, _attr, _val, _len)			\
    do {								\
	if ((_len) != 0) {						\
	    if ((_len) == -1) {						\
		if (_attrs[_attr].t != PW_TYPE_INTEGER) {		\
		    LM_ERR("attribute %d is not of type integer\n",	\
			   _attrs[_attr].v);				\
		    goto err;						\
		}							\
	    }								\
	    if (!rc_avpair_add( rh, &send, _attrs[_attr].v, _val, _len, 0)) { \
		LM_ERR("failed to add %s, %d\n", _attrs[_attr].n, _attr); \
		goto err;						\
	    }								\
	}								\
    }while(0)


static inline int extract_avp(VALUE_PAIR* vp, unsigned short *flags,
			      int_str *name, int_str *value)
{
	char *p, *q, *r;

	LM_DBG("vp->name '%.*s'\n",(int)strlen(vp->name),vp->name);
	LM_DBG("vp->attribute '%d'\n",vp->attribute);
	LM_DBG("vp->type '%d'\n",vp->type);
	LM_DBG("vp->lvalue '%d'\n",vp->lvalue);
	if (vp->type == PW_TYPE_STRING) 
		LM_DBG("vp->strvalue '%.*s'\n",(int)strlen(vp->strvalue),vp->strvalue);

	if ( vp->attribute == attrs[A_SIP_AVP].v && vp->type == PW_TYPE_STRING ) {
		p = strchr(vp->strvalue,'#');
		q = strchr(vp->strvalue,':');
		if (!q && p == vp->strvalue && vp->strvalue+sizeof(char) != '\0' ) {
				r = p;
				r = strchr(r++,'#');
		} else 
			r = NULL;
		errno = 0;
		if ( p == vp->strvalue && q ) {
			/* int name and str value */
			*flags |= AVP_VAL_STR;
			name->n = strtol(++p,&q,10);
			value->s.s = ++q;
			value->s.len = strlen(q);
		} else if ( p && !r && p > vp->strvalue && !q ) {
			/* str name and int value */
			*flags |= AVP_NAME_STR;
			name->s.len = p - vp->strvalue;
			name->s.s = vp->strvalue;
			value->n = strtol(++p,&r,10);
		} else if ( p && p != r && !q ) {
			/* int name and int vale */
			name->n = strtol(++p,&q,10);
			value->n = strtol(++r,&q,10);
		} else if ( (!p || p > q) && q ) {
			/* str name and str value */
			*flags |= AVP_VAL_STR|AVP_NAME_STR;
			name->s.len = q - vp->strvalue;
			name->s.s = vp->strvalue;
			value->s.len = strlen(++q);
			value->s.s = q;
		} else /* error - unknown */
			return 0;
	    if ( errno != 0 )
			return 0;
	} else if (vp->type == PW_TYPE_STRING) {
			*flags |= AVP_VAL_STR|AVP_NAME_STR;
			/* if start of value is the name of value */
			if (vp->strvalue == strstr(vp->strvalue,vp->name))
				/* then get value after name + one char delimiter */
				p = vp->strvalue + strlen(vp->name) + sizeof(char);
			else
				p = vp->strvalue;
			value->s.len = vp->lvalue - (p - vp->strvalue);
			value->s.s = p;
			name->s.len = strlen(vp->name); 
			name->s.s = vp->name;
	} else if ((vp->type == PW_TYPE_INTEGER) || (vp->type == PW_TYPE_IPADDR) || (vp->type == PW_TYPE_DATE)) {
			*flags |= AVP_NAME_STR;
			value->n = vp->lvalue;
			name->s.len = strlen(vp->name); 
			name->s.s = vp->name;
	} else {
			LM_ERR("Unknown AVP type '%d'!\n",vp->type);
			return 0;
	}
	if ( ! name->s.len )
	   return 0;
	return 1;
}

/*
 * Generate AVPs from the database result
 */
static int generate_avps(VALUE_PAIR* received)
{
	int_str name, val;
	unsigned short flags;
	VALUE_PAIR *vp;

	LM_DBG("getting AVPs from RADIUS Reply\n");
	vp = received;
	if ( ! ar_radius_avps_mode )
		vp=rc_avpair_get(vp,attrs[A_SIP_AVP].v,0);
	for( ; vp; vp=((ar_radius_avps_mode)?vp->next:rc_avpair_get(vp->next,attrs[A_SIP_AVP].v,0)) ) {
		flags = 0;
		if (!extract_avp( vp, &flags, &name, &val)){
			LM_ERR("error while extracting AVP '%.*s'\n",(int)strlen(vp->name),vp->name);
			continue;
		}
		if (add_avp( flags, name, val) < 0) {
			LM_ERR("unable to create a new AVP\n");
		} else {
			LM_DBG("AVP '%.*s'/%d='%.*s'/%d has been added\n",
				(flags&AVP_NAME_STR)?name.s.len:4,
				(flags&AVP_NAME_STR)?name.s.s:"null",
				(flags&AVP_NAME_STR)?0:name.n,
				(flags&AVP_VAL_STR)?val.s.len:4,
				(flags&AVP_VAL_STR)?val.s.s:"null",
				(flags&AVP_VAL_STR)?0:val.n );
		}
	}

	return 0;
}


static int add_cisco_vsa(VALUE_PAIR** send, struct sip_msg* msg)
{
	str callid;

	if (!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) == -1) {
		LM_ERR("cannot parse Call-ID header field\n");
		return -1;
	}

	if (!msg->callid) {
		LM_ERR("call-ID header field not found\n");
		return -1;
	}

	callid.len = msg->callid->body.len + 8;
	callid.s = pkg_malloc(callid.len);
	if (callid.s == NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	memcpy(callid.s, "call-id=", 8);
	memcpy(callid.s + 8, msg->callid->body.s, msg->callid->body.len);

	if (rc_avpair_add(rh, send, attrs[A_CISCO_AVPAIR].v, callid.s,
			callid.len, VENDOR(attrs[A_CISCO_AVPAIR].v)) == 0) {
		LM_ERR("unable to add Cisco-AVPair attribute\n");
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
	uint32_t service;
	str method, user, user_name;
	str *ruri;
	int extra_cnt, offset, i;
		
	send = received = 0;

	if (!(_cred && _method && _user)) {
		LM_ERR("invalid parameter value\n");
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
			LM_ERR("unable to add User-Name attribute\n");
			goto err;
		}
	} else {
		user_name.len = _cred->username.user.len + _cred->realm.len + 1;
		user_name.s = pkg_malloc(user_name.len);
		if (!user_name.s) {
			LM_ERR("no pkg memory left\n");
			return -3;
		}
		memcpy(user_name.s, _cred->username.whole.s, _cred->username.whole.len);
		user_name.s[_cred->username.whole.len] = '@';
		memcpy(user_name.s + _cred->username.whole.len + 1, _cred->realm.s,
			_cred->realm.len);
		if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user_name.s,
		user_name.len, 0)) {
			LM_ERR("unable to add User-Name attribute\n");
			pkg_free(user_name.s);
			goto err;
		}
		pkg_free(user_name.s);
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_USER_NAME].v, 
	_cred->username.whole.s, _cred->username.whole.len, 0)) {
		LM_ERR("unable to add Digest-User-Name attribute\n");
		goto err;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_REALM].v, _cred->realm.s,
	_cred->realm.len, 0)) {
		LM_ERR("unable to add Digest-Realm attribute\n");
		goto err;
	}
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE].v, _cred->nonce.s,
	_cred->nonce.len, 0)) {
		LM_ERR("unable to add Digest-Nonce attribute\n");
		goto err;
	}

	if (use_ruri_flag < 0 || isflagset(_msg, use_ruri_flag) != 1) {
		ruri = &_cred->uri;
	} else {
		ruri = GET_RURI(_msg);
	}
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_URI].v, ruri->s,
	ruri->len, 0)) {
		LM_ERR("unable to add Digest-URI attribute\n");
		goto err;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_METHOD].v, method.s,
	method.len, 0)) {
		LM_ERR("unable to add Digest-Method attribute\n");
		goto err;
	}
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (_cred->qop.qop_parsed == QOP_AUTH) {
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_QOP].v, "auth", 4, 0)) {
			LM_ERR("unable to add Digest-QOP attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE_COUNT].v, 
		_cred->nc.s, _cred->nc.len, 0)) {
			LM_ERR("unable to add Digest-CNonce-Count attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_CNONCE].v, 
		_cred->cnonce.s, _cred->cnonce.len, 0)) {
			LM_ERR("unable to add Digest-CNonce attribute\n");
			goto err;
		}
	} else if (_cred->qop.qop_parsed == QOP_AUTHINT) {
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_QOP].v,
		"auth-int", 8, 0)) {
			LM_ERR("unable to add Digest-QOP attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_NONCE_COUNT].v,
		_cred->nc.s, _cred->nc.len, 0)) {
			LM_ERR("unable to add Digest-Nonce-Count attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_CNONCE].v,
		_cred->cnonce.s, _cred->cnonce.len, 0)) {
			LM_ERR("unable to add Digest-CNonce attribute\n");
			goto err;
		}
		if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_BODY_DIGEST].v, 
		_cred->opaque.s, _cred->opaque.len, 0)) {
			LM_ERR("unable to add Digest-Body-Digest attribute\n");
			goto err;
		}
		
	} else  {
		/* send nothing for qop == "" */
	}

	/* Add the response... What to calculate against... */
	if (!rc_avpair_add(rh, &send, attrs[A_DIGEST_RESPONSE].v, 
	_cred->response.s, _cred->response.len, 0)) {
		LM_ERR("unable to add Digest-Response attribute\n");
		goto err;
	}

	/* Indicate the service type, Authenticate only in our case */
	service = vals[V_SIP_SESSION].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
		LM_ERR("unable to add Service-Type attribute\n");
		goto err;
	}

	/* Add SIP URI as a check item */
	if (!rc_avpair_add(rh,&send,attrs[A_SIP_URI_USER].v,user.s,user.len,0)) {
		LM_ERR("unable to add Sip-URI-User attribute\n");
		goto err;
	}

	if (attrs[A_CISCO_AVPAIR].n != NULL) {
		if (add_cisco_vsa(&send, _msg)) {
			goto err;
		}
	}

	/* Add extra attributes */
	extra_cnt = extra2strar(auth_extra, _msg, val_arr);
	if (extra_cnt == -1) {
	    LM_ERR("in getting values of extra attributes\n");
	    goto err;
	}
	offset = A_MAX;
	for (i = 0; i < extra_cnt; i++) {
	    if (val_arr[i].len == -1) {
		/* Add integer attribute */
		ADD_EXTRA_AVPAIR(attrs, offset+i,
				 &(val_arr[i].s), val_arr[i].len );
	    } else {
		/* Add string attribute */
		ADD_EXTRA_AVPAIR(attrs, offset+i,
				 val_arr[i].s, val_arr[i].len );
	    }
	}

	/* Send request */
	if ((i = rc_auth(rh, SIP_PORT, send, &received, msg)) == OK_RC) {
		LM_DBG("Success\n");
		rc_avpair_free(send);
		send = 0;

		generate_avps(received);

		rc_avpair_free(received);
		return 1;
	} else {
#ifdef REJECT_RC
                if (i == REJECT_RC) {
                        LM_DBG("Failure\n");
                        goto err;
                }
#endif 
		LM_ERR("authorization failed. RC auth returned %d\n", i);
	}

 err:
	if (send) rc_avpair_free(send);
	if (received) rc_avpair_free(received);
	return -1;
}

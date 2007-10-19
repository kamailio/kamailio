/* checks.c v 0.1 2003/1/20
 *
 * Radius based checks
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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


#include <string.h>
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "../../radius.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "checks.h"
#include "urirad_mod.h"


 /*
 * Split name:value into string name and string value
 */
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

	while ((vp = rc_avpair_get(vp, attrs[A_SIP_AVP].v, 0))) {
		attr_name_value(vp, &name_str, &val_str);
		
		name.s = name_str;
		val.s = val_str;
		if (add_avp(AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) {
			LM_ERR("failed to create a new AVP\n");
		} else {
			LM_DBG("AVP '%.*s'='%.*s' has been added\n",
			    name_str.len, ZSW(name_str.s), 
			    val_str.len, ZSW(val_str.s));
		}
		vp = vp->next;
	}
	
	return 0;
}


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
		LM_ERR("parsing URI failed\n");
		return -1;
	}
	
	uri = (char*)pkg_malloc(_m->parsed_uri.user.len + _m->parsed_uri.host.len + 2);
	if (!uri) {
		LM_ERR("no more pkg memory\n");
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

	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, uri, -1, 0)) {
		LM_ERR("adding User-Name failed\n");
		rc_avpair_free(send);
		pkg_free(uri);
	 	return -3;
	}

	service = vals[V_CALL_CHECK].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
		LM_ERR("adding service type failed\n");
		rc_avpair_free(send);
		pkg_free(uri);
	 	return -4;  	
	}
	
	if (rc_auth(rh, 0, send, &received, msg) == OK_RC) {
		LM_DBG("success\n");
		rc_avpair_free(send);
		generate_avps(received);
		rc_avpair_free(received);
		pkg_free(uri);
		return 1;
	} else {
		LM_DBG("failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		pkg_free(uri);
		return -5;
	}
}

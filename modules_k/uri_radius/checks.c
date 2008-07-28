/* 
 * Radius based checks
 *
 * Copyright (C) 2002-2008 Juha Heinanen
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
 * -------
 * 2003-03-11: Code cleanup (janakj)
 * 2008-04-17: Added functions that accept pvar arguments (juhe)
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
#include "../../pvar.h"


/* Extract from SIP-AVP value flags/name/value of an AVP */
static inline int extract_avp(VALUE_PAIR* vp, unsigned short *flags,
			      int_str *name, int_str *value)
{
    static str names, values;
    unsigned int r;
    char *p;
    char *end;

    /* empty? */
    if (vp->lvalue==0 || vp->strvalue==0)
	goto error;

    p = vp->strvalue;
    end = vp->strvalue + vp->lvalue;
    
    /* get name */
    if (*p!='#') {
	/* name AVP */
	*flags |= AVP_NAME_STR;
	names.s = p;
    } else {
	names.s = ++p;
    }

    names.len = 0;
    while( p<end && *p!=':' && *p!='#')
	p++;
    if (names.s==p || p==end) {
	LM_ERR("Empty AVP name\n");
	goto error;
    }
    names.len = p - names.s;
    
    /* get value */
    if (*p!='#') {
	/* string value */
	*flags |= AVP_VAL_STR;
    }
    values.s = ++p;
    values.len = end-values.s;
    if (values.len==0) {
	LM_ERR("Empty AVP value\n");
	goto error;
    }

    if ( !((*flags)&AVP_NAME_STR) ) {
	/* convert name to id*/
	if (str2int(&names,&r)!=0 ) {
	    LM_ERR("Invalid AVP ID '%.*s'\n", names.len,names.s);
	    goto error;
	}
	name->n = (int)r;
    } else {
	name->s = names;
    }

    if ( !((*flags)&AVP_VAL_STR) ) {
	/* convert value to integer */
	if (str2int(&values,&r)!=0 ) {
	    LM_ERR("Invalid AVP numerical value '%.*s'\n",
		   values.len,values.s);
	    goto error;
	}
	value->n = (int)r;
    } else {
	value->s = values;
    }

    return 0;
 error:
    return -1;
}


/* Generate AVPs from Radius reply items */
static int generate_avps(VALUE_PAIR* received)
{
    int_str name, val;
    unsigned short flags;
    VALUE_PAIR *vp;
    
    vp = received;

    for( ; (vp=rc_avpair_get(vp,attrs[A_SIP_AVP].v,0)) ; vp=vp->next) {
	flags = 0;
	if (extract_avp( vp, &flags, &name, &val)!=0 )
	    continue;
	if (add_avp( flags, name, val) < 0) {
	    LM_ERR("Unable to create a new AVP\n");
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


/*
 * Check from Radius if URI, whose user an host parts are given as
 * arguments, exists.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_user_host_exist(str user, str host)
{
    static char msg[4096];
    VALUE_PAIR *send, *received;
    uint32_t service;
    char* at, *uri = 0;
    int res;

    send = received = 0;

    if (!use_sip_uri_host) {

	/* Send userpart@hostpart of Request-URI in A_USER_NAME attr */
	uri = (char*)pkg_malloc(user.len + host.len + 2);
	if (!uri) {
	    LM_ERR("no more pkg memory\n");
	    return -1;
	}
	at = uri;
	memcpy(at, user.s, user.len);
	at += user.len;
	*at = '@';
	at++;
	memcpy(at , host.s, host.len);
	at += host.len;
	*at = '\0';
	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, uri, -1, 0)) {
	    LM_ERR("adding User-Name failed\n");
	    rc_avpair_free(send);
	    pkg_free(uri);
	    return -1;
	}

    } else {

	/* Send userpart of Request-URI in A_USER_NAME attribute and
	   hostpart in A_SIP_URI_HOST attribute */
	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
			   user.s, user.len, 0)) {
	    LM_ERR("adding User-Name failed\n");
	    rc_avpair_free(send);
	    return -1;
	}
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_HOST].v,
			   host.s, host.len, 0)) {
	    LM_ERR("adding SIP-URI-Host failed\n");
	    rc_avpair_free(send);
	    return -1;
	}
    }
	    
    service = vals[V_CALL_CHECK].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
	LM_ERR("adding service type failed\n");
	rc_avpair_free(send);
	if (uri) pkg_free(uri);
	return -1;
    }
	
    if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	generate_avps(received);
	rc_avpair_free(received);
	if (uri) pkg_free(uri);
	return 1;
    } else {
	rc_avpair_free(send);
	rc_avpair_free(received);
	if (uri) pkg_free(uri);
#ifdef REJECT_RC
	if (res == REJECT_RC) {
	    LM_DBG("rejected\n");
	    return -1;
	} else {
	    LM_ERR("failure\n");
	    return -2;
	}
#else
	LM_DBG("failure\n");
	return -1;
#endif
    }
}


/*
 * Check from Radius if Request URI belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_exist_0(struct sip_msg* _m, char* _s1, char* _s2)
{

    if (parse_sip_msg_uri(_m) < 0) {
	LM_ERR("parsing URI failed\n");
	return -1;
    }

    return radius_does_uri_user_host_exist(_m->parsed_uri.user,
					   _m->parsed_uri.host);
}


/*
 * Check from Radius if URI giving in pvar argument belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_exist_1(struct sip_msg* _m, char* _sp, char* _s2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    struct sip_uri parsed_uri;

    sp = (pv_spec_t *)_sp;

    if (sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		LM_ERR("pvar argument is empty\n");
		return -1;
	    }
	} else {
	    LM_ERR("pvar value is not string\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get pvar value\n");
	return -1;
    }

    if (parse_uri(pv_val.rs.s, pv_val.rs.len, &parsed_uri) < 0) {
	LM_ERR("parsing of URI in pvar failed\n");
	return -1;
    }

    return radius_does_uri_user_host_exist(parsed_uri.user, parsed_uri.host);
}


/*
 * Check from Radius if URI user given as argument belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_user_exist(str user)
{
    static char msg[4096];
    VALUE_PAIR *send, *received;
    UINT4 service;
    int res;
    
    send = received = 0;
    
    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user.s, user.len, 0)) {
	LM_ERR("error adding User-Name\n");
	rc_avpair_free(send);
	return -1;
    }
    
    service = vals[V_CALL_CHECK].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
	LM_ERR("error adding service type\n");
	rc_avpair_free(send);
	return -1;
    }
	
    if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	generate_avps(received);
	rc_avpair_free(received);
	return 1;
    } else {
	rc_avpair_free(send);
	rc_avpair_free(received);
#ifdef REJECT_RC
	if (res == REJECT_RC) {
	    LM_DBG("rejected\n");
	    return -1;
	} else {
	    LM_ERR("failure\n");
	    return -2;
	}
#else
	LM_DBG("failure\n");
	return -1;
#endif
    }
}


/*
 * Check from Radius if Request URI user belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_user_exist_0(struct sip_msg* _m, char* _s1, char* _s2)
{

    if (parse_sip_msg_uri(_m) < 0) {
	LM_ERR("parsing URI failed\n");
	return -1;
    }

    return radius_does_uri_user_exist(_m->parsed_uri.user);
}


/*
 * Check from Radius if URI user giving in pvar argument belongs
 * to a local user. If so, loads AVPs based on reply items returned
 * from Radius. 
 */
int radius_does_uri_user_exist_1(struct sip_msg* _m, char* _sp, char* _s2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;

    sp = (pv_spec_t *)_sp;

    if (sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		LM_ERR("pvar argument is empty\n");
		return -1;
	    }
	} else {
	    LM_ERR("pvar value is not string\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get pvar value\n");
	return -1;
    }

    return radius_does_uri_user_exist(pv_val.rs);
}


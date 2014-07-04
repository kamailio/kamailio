/*
 * Verification functions
 *
 * Copyright (C) 2008 Juha Heinanen
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 *
 */

/*! \file
 * \ingroup peering
 * \brief Verification functions
 *
 * - Module: \ref peering
 */



#include "../../str.h"
#include "../../lib/kcore/radius.h"
#include "../../usr_avp.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/kcore/radius.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "peering.h"


/*
 * Extract name and value of an AVP from VALUE_PAIR
 */
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
	LM_ERR("empty AVP name\n");
	goto error;
    }
    names.len = p - names.s;
    LM_DBG("AVP name is <%.*s>\n", names.len, names.s);

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
    LM_DBG("AVP val is <%.*s>\n", values.len, values.s);

    if ( !((*flags)&AVP_NAME_STR) ) {
	/* convert name to id*/
	if (str2int(&names,&r)!=0 ) {
	    LM_ERR("invalid AVP ID '%.*s'\n", names.len,names.s);
	    goto error;
	}
	name->n = (int)r;
    } else {
	name->s = names;
    }

    if ( !((*flags)&AVP_VAL_STR) ) {
	/* convert value to integer */
	if (str2int(&values,&r)!=0 ) {
	    LM_ERR("invalid AVP numerical value '%.*s'\n",
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


/*
 * Generate AVPs from result of Radius query
 */
static int generate_avps(VALUE_PAIR* received)
{
    int_str name, val;
    unsigned short flags;
    VALUE_PAIR *vp;

    vp = received;

    LM_DBG("getting SIP AVPs from avpair %d\n",	attrs[A_SIP_AVP].v);

    for(; (vp=rc_avpair_get(vp,attrs[A_SIP_AVP].v,0)); vp=vp->next) {
	flags = 0;
	if (extract_avp(vp, &flags, &name, &val) != 0)
	    continue;
	if (add_avp( flags, name, val) < 0) {
	    LM_ERR("unable to add a new AVP\n");
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
 * Send Radius request to verify destination and generate AVPs from
 * reply items of positive response.
 */
int verify_destination(struct sip_msg* _msg, char* s1, char* s2)
{
    VALUE_PAIR* send, *received;
    uint32_t service;
    static char rad_msg[4096];
    int i;

    send = received = 0;

    /* Add Request-URI host A_USER_NAME and user as A_SIP_URI_USER */
    if (parse_sip_msg_uri(_msg) < 0) {
        LM_ERR("error while parsing Request-URI\n");
	return -1;
    }
    
    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
		       _msg->parsed_uri.host.s,
		       _msg->parsed_uri.host.len, 0)) {
	LM_ERR("error adding PW_USER_NAME\n");
	goto err;
    }
   
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_USER].v,
		       _msg->parsed_uri.user.s,
		       _msg->parsed_uri.user.len, 0)) {
	LM_ERR("error adding PW_SIP_URI_USER\n");
	goto err;
    }

    /* Add From Tag */
    if (parse_from_header(_msg) < 0) {
	LM_ERR("error while parsing From header field\n");
	goto err;
    }

    if ((_msg->from==NULL) || (get_from(_msg) == NULL) ||
	(get_from(_msg)->tag_value.s == NULL) ||
	(get_from(_msg)->tag_value.len <= 0)) {
	LM_ERR("error while accessing From header tag\n");
	goto err;
    }
    
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_FROM_TAG].v,
		       get_from(_msg)->tag_value.s,
		       get_from(_msg)->tag_value.len, 0)) {
	LM_ERR("error adding PW_SIP_FROM_TAG\n");
	goto err;
    }

    /* Add Call-Id */
    if ((parse_headers(_msg, HDR_CALLID_F, 0) == -1) ||
	(_msg->callid == NULL) || (_msg->callid->body.s == NULL) ||
	(_msg->callid->body.len <= 0)) {
	LM_ERR("error while accessing Call-Id\n");
	goto err;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_SIP_CALL_ID].v,
		       _msg->callid->body.s,
		       _msg->callid->body.len, 0)) {
	LM_ERR("error adding PW_SIP_CALL_ID\n");
	goto err;
    }
    
    /* Add Service-Type */
    service = vals[V_SIP_VERIFY_DESTINATION].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v,
		       &service, -1, 0)) {
	LM_ERR("error adding PW_SERVICE_TYPE\n");
	goto err;
    }
    
    /* Send Request and generate AVPs of positive reply */
    if ((i = rc_auth(rh, SIP_PORT, send, &received, rad_msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	generate_avps(received);
	rc_avpair_free(received);
	return 1;
    } else {
#ifdef REJECT_RC
	if (i == REJECT_RC) {
	    LM_DBG("rejected\n");
	} else {
	    LM_ERR("failure\n");
	}
	goto err;
#else
	LM_DBG("failure\n");
	goto err;
#endif
    }

err:
    if (send) rc_avpair_free(send);
    if (received) rc_avpair_free(received);
    return -1;
}


/* 
 * Send Radius request to verify source.
 */
int verify_source(struct sip_msg* _msg, char* s1, char* s2)
{
    VALUE_PAIR* send, *received;
    struct hdr_field *hf;
    uint32_t service;
    static char rad_msg[4096];
    int i;

    send = received = 0;
 
    /* Add Request-URI host A_USER_NAME and user as A_SIP_URI_USER */
    if (parse_sip_msg_uri(_msg) < 0) {
        LM_ERR("error while parsing Request-URI\n");
	return -1;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
		       _msg->parsed_uri.host.s,
		       _msg->parsed_uri.host.len, 0)) {
	LM_ERR("error adding PW_USER_NAME\n");
	goto err;
    }
   
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_USER].v,
		       _msg->parsed_uri.user.s,
		       _msg->parsed_uri.user.len, 0)) {
	LM_ERR("error adding PW_SIP_URI_USER\n");
	goto err;
    }

    /* Add From Tag */
    if (parse_from_header(_msg) < 0) {
	LM_ERR("error while parsing From header field\n");
	goto err;
    }

    if ((_msg->from==NULL) || (get_from(_msg) == NULL) ||
	(get_from(_msg)->tag_value.s == NULL) ||
	(get_from(_msg)->tag_value.len <= 0)) {
	LM_ERR("error while accessing From header tag\n");
	goto err;
    }
    
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_FROM_TAG].v,
		       get_from(_msg)->tag_value.s,
		       get_from(_msg)->tag_value.len, 0)) {
	LM_ERR("error adding PW_SIP_FROM_TAG\n");
	goto err;
    }

    /* Add Call-Id */
    if ((parse_headers(_msg, HDR_CALLID_F, 0) == -1) ||
	(_msg->callid == NULL) || (_msg->callid->body.s == NULL) ||
	(_msg->callid->body.len <= 0)) {
	LM_ERR("error while accessing Call-Id\n");
	goto err;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_SIP_CALL_ID].v,
		       _msg->callid->body.s,
		       _msg->callid->body.len, 0)) {
	LM_ERR("error adding PW_SIP_CALL_ID\n");
	goto err;
    }

    /* Add P-Request-Hash header body */
    parse_headers(_msg, HDR_EOH_F, 0);
    for (hf = _msg->headers; hf; hf = hf->next) {
		if(cmp_hdrname_strzn(&hf->name, "P-Request-Hash",
			sizeof("P-Request-Hash") - 1) == 0)
	    break;
    }
    if (!hf) {
	LM_ERR("no P-Request-Hash header field\n");
	goto err;
    }
    if ((hf->body.s == NULL) || (hf->body.len <= 0)) {
	LM_ERR("error while accessing P-Request-Hash body\n");
	goto err;
    }
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_REQUEST_HASH].v,
		       hf->body.s, hf->body.len, 0)) {
	LM_ERR("error adding PW_SIP_REQUEST_HASH\n");
	goto err;
    }
    
    /* Add Service-Type */
    service = vals[V_SIP_VERIFY_SOURCE].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v,
		       &service, -1, 0)) {
	LM_ERR("error adding PW_SERVICE_TYPE\n");
	goto err;
    }
    
    /* Send Request and generate AVPs of positive reply */
    if ((i = rc_auth(rh, SIP_PORT, send, &received, rad_msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	rc_avpair_free(received);
	return 1;
    } else {
#ifdef REJECT_RC
	if (i == REJECT_RC) {
	    LM_DBG("rejected\n");
	} else {
	    LM_ERR("failure\n");
	}
	goto err;
#else
	LM_DBG("failure\n");
	goto err;
#endif
    }

err:
    if (send) rc_avpair_free(send);
    if (received) rc_avpair_free(received);
    return -1;
}

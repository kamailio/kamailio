/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 */


#include "utils.h"

/*
 * Find credentials with given realm in a SIP message header
 */
inline int ims_find_credentials(struct sip_msg* _m, str* _realm,
		hdr_types_t _hftype, struct hdr_field** _h) {
	struct hdr_field** hook, *ptr, *prev;
	hdr_flags_t hdr_flags;
	int res;
	str* r;

	LM_DBG("Searching credentials in realm [%.*s]\n", _realm->len, _realm->s);

	/*
	 * Determine if we should use WWW-Authorization or
	 * Proxy-Authorization header fields, this parameter
	 * is set in www_authorize and proxy_authorize
	 */
	switch (_hftype) {
	case HDR_AUTHORIZATION_T:
		hook = &(_m->authorization);
		hdr_flags = HDR_AUTHORIZATION_F;
		break;
	case HDR_PROXYAUTH_T:
		hook = &(_m->proxy_auth);
		hdr_flags = HDR_PROXYAUTH_F;
		break;
	default:
		hook = &(_m->authorization);
		hdr_flags = HDR_T2F(_hftype);
		break;
	}

	/*
	 * If the credentials haven't been parsed yet, do it now
	 */
	if (*hook == 0) {
		/* No credentials parsed yet */
		LM_DBG("*hook == 0, No credentials parsed yet\n");
		if (parse_headers(_m, hdr_flags, 0) == -1) {
			LM_ERR("Error while parsing headers\n");
			return -1;
		}
	}

	ptr = *hook;
	LM_DBG("*hook = %p\n", ptr);
	/*
	 * Iterate through the credentials in the message and
	 * find credentials with given realm
	 */
	while (ptr) {
		res = parse_credentials(ptr);
		if (res < 0) {
			LM_ERR("Error while parsing credentials\n");
			return (res == -1) ? -2 : -3;
		} else if (res == 0) {
			LM_DBG("Credential parsed successfully\n");
			if (_realm->len) {
				r = &(((auth_body_t*) (ptr->parsed))->digest.realm);
				LM_DBG("Comparing realm <%.*s> and <%.*s>\n", _realm->len, _realm->s, r->len, r->s);
				if (r->len == _realm->len) {
					if (!strncasecmp(_realm->s, r->s, r->len)) {
						*_h = ptr;
						return 0;
					}
				}
			} else {
				*_h = ptr;
				return 0;
			}

		}

		prev = ptr;
		if (parse_headers(_m, hdr_flags, 1) == -1) {
			LM_ERR("Error while parsing headers\n");
			return -4;
		} else {
			if (prev != _m->last_header) {
				if (_m->last_header->type == _hftype)
					ptr = _m->last_header;
				else
					break;
			} else
				break;
		}
	}

	/*
	 * Credentials with given realm not found
	 */
	LM_DBG("Credentials with given realm not found\n");
	return 1;
}

/**
 * Looks for the nonce and response parameters in the Authorization header and returns them
 * @param msg - the SIP message
 * @param realm - realm to match the right Authorization header
 * @param nonce - param to fill with the nonce found
 * @param response - param to fill with the response
 * @returns 1 if found, 0 if not
 */
int get_nonce_response(struct sip_msg *msg, str realm,str *nonce,str *response,
	enum qop_type *qop,str *qop_str,str *nc,str *cnonce,str *uri, int is_proxy_auth)
{
	struct hdr_field* h = 0;
	int ret;


	ret = parse_headers(msg, is_proxy_auth ? HDR_PROXYAUTH_F : HDR_AUTHORIZATION_F, 0);

	if (ret != 0) {
		return 0;
	}

	if ((!is_proxy_auth && !msg->authorization)
		|| (is_proxy_auth && !msg->proxy_auth)) {
		return 0;
	}

	LM_DBG("Calling find_credentials with realm [%.*s]\n", realm.len, realm.s);
	ret = ims_find_credentials(msg, &realm, is_proxy_auth ? HDR_PROXYAUTH_T : HDR_AUTHORIZATION_T, &h);
	if (ret < 0) {
		return 0;
	} else if (ret > 0) {
		LM_DBG("ret > 0");
		return 0;
	}

	if (h && h->parsed) {
		if (nonce)
			*nonce = ((auth_body_t*) h->parsed)->digest.nonce;
		if (response)
			*response = ((auth_body_t*) h->parsed)->digest.response;
		if (qop)
			*qop = ((auth_body_t*) h->parsed)->digest.qop.qop_parsed;
		if (qop_str)
			*qop_str = ((auth_body_t*) h->parsed)->digest.qop.qop_str;
		if (nc)
			*nc = ((auth_body_t*) h->parsed)->digest.nc;
		if (cnonce)
			*cnonce = ((auth_body_t*) h->parsed)->digest.cnonce;
		if (uri)
			*uri = ((auth_body_t*) h->parsed)->digest.uri;
	}
	LM_DBG("Found nonce response\n");
	return 1;
}

str ims_get_body(struct sip_msg * msg)
{		
	str x={0,0};
	
	if (parse_headers(msg,HDR_CONTENTLENGTH_F,0)!=0) {
		LM_DBG("Error parsing until header Content-Length: \n");
		return x;
	}
	x.len = (int)(long)msg->content_length->parsed;
        
        if (x.len>0) 
            x.s = get_body(msg);	
	
        return x;
}


/**
 * Looks for the auts parameter in the Authorization header and returns its value.
 * @param msg - the SIP message
 * @param realm - realm to match the right Authorization header
 * @returns the auts value or an empty string if not found
 */
str ims_get_auts(struct sip_msg *msg, str realm, int is_proxy_auth)
{
	str name={"auts=\"",6};
	struct hdr_field* h=0;
	int i,ret;
	str auts={0,0};

	if (parse_headers(msg, is_proxy_auth ? HDR_PROXYAUTH_F : HDR_AUTHORIZATION_F,0)!=0) {
		LM_ERR("Error parsing until header Authorization: \n");
		return auts;
	}

	if ((!is_proxy_auth && !msg->authorization)
			|| (is_proxy_auth && !msg->proxy_auth)){
		LM_ERR("Message does not contain Authorization nor Proxy-Authorization header.\n");
		return auts;
	}

	ret = find_credentials(msg, &realm, is_proxy_auth ? HDR_PROXYAUTH_F : HDR_AUTHORIZATION_F, &h);
	if (ret < 0) {
		LM_ERR("Error while looking for credentials.\n");
		return auts;
	} else 
		if (ret > 0) {
			LM_ERR("No credentials for this realm found.\n");
			return auts;
		}
	
	if (h) {
		for(i=0;i<h->body.len-name.len;i++)
			if (strncasecmp(h->body.s+i,name.s,name.len)==0){
				auts.s = h->body.s+i+name.len;
				while(i+auts.len<h->body.len && auts.s[auts.len]!='\"')
					auts.len++;
			}
	}
	
	return auts;	
}

/**
 * Looks for the nonce parameter in the Authorization header and returns its value.
 * @param msg - the SIP message
 * @param realm - realm to match the right Authorization header
 * @returns the nonce or an empty string if none found
 */
str ims_get_nonce(struct sip_msg *msg, str realm)
{
	struct hdr_field* h=0;
	int ret;
	str nonce={0,0};

	if (parse_headers(msg,HDR_AUTHORIZATION_F,0)!=0) {
		LM_ERR("Error parsing until header Authorization: \n");
		return nonce;
	}

	if (!msg->authorization){
		LM_ERR("Message does not contain Authorization header.\n");
		return nonce;
	}

	ret = find_credentials(msg, &realm, HDR_AUTHORIZATION_F, &h);
	if (ret < 0) {
		LM_ERR("Error while looking for credentials.\n");
		return nonce;
	} else 
		if (ret > 0) {
			LM_ERR("No credentials for this realm found.\n");
			return nonce;
		}
	
	if (h&&h->parsed) {
		nonce = ((auth_body_t*)h->parsed)->digest.nonce;
	}
	
	return nonce;	
}

/**
 * Adds a header to the reply message
 * @param msg - the request to add a header to its reply
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int ims_add_header_rpl(struct sip_msg *msg, str *hdr)
{
	if (add_lump_rpl( msg, hdr->s, hdr->len, LUMP_RPL_HDR)==0) {
		LM_ERR("Can't add header <%.*s>\n",
			hdr->len,hdr->s);
 		return 0;
 	}
 	return 1;
}



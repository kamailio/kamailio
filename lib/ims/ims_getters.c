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

#include "../../parser/msg_parser.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_expires.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_nameaddr.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "ims_getters.h"
#include "../../parser/parse_ppi_pai.h"


/**
 *	Delete parameters and stuff from uri.
 * @param uri - the string to operate on 
 */
static inline void cscf_strip_uri(str *uri)
{
	int i;
	/* Strip the ending */
	i=0;
	while(i<uri->len&&uri->s[i]!='@')
		i++;
	while(i<uri->len&&
			uri->s[i]!=':'&&
			uri->s[i]!='/'&&
			uri->s[i]!='&')
		i++;
	uri->len=i;
}

/**
 * Parses all the contact headers.
 * @param msg - the SIP message
 * @returns the first contact_body
 */
contact_body_t *cscf_parse_contacts(struct sip_msg *msg)
{
	struct hdr_field* ptr;
	if (!msg) return 0;

	if (parse_headers(msg, HDR_EOH_F, 0)<0){
		LM_ERR("Error parsing headers \n");
		return 0;
	}
	if (msg->contact) {
		ptr = msg->contact;
		while(ptr) {
			if (ptr->type == HDR_CONTACT_T) {
				if (msg->contact->parsed==0){					
					if (parse_contact(ptr)<0){
						LM_DBG("error parsing contacts [%.*s]\n",
								ptr->body.len,ptr->body.s);
					}
				}
			}
			ptr = ptr->next;
		}
	}
	if (!msg->contact) return 0;
	return msg->contact->parsed;
}

/**
 * Returns the Private Identity extracted from the Authorization header.
 * If none found there takes the SIP URI in To without the "sip:" prefix
 * \todo - remove the fallback case to the To header
 * @param msg - the SIP message
 * @param realm - the realm to match in an Authorization header
 * @returns the str containing the private id, no mem dup
 */
str cscf_get_private_identity(struct sip_msg *msg, str realm)
{
	str pi={0,0};
	struct hdr_field* h=0;
	int ret,i,res;

	if (parse_headers(msg,HDR_AUTHORIZATION_F,0)!=0) {
		return pi;
	}
        
        h = msg->authorization;
	if (!msg->authorization){
		goto fallback;
	}
        
        if (realm.len && realm.s) {
            ret = find_credentials(msg, &realm, HDR_AUTHORIZATION_F, &h);
            if (ret < 0) {
                    goto fallback;
            } else 
                    if (ret > 0) {
                            goto fallback;
                    }
        }

	res = parse_credentials(h);
        if (res != 0) {
                LOG(L_ERR, "Error while parsing credentials\n");
                return pi;
        }

	if (h) pi=((auth_body_t*)h->parsed)->digest.username.whole;

	goto done;

	fallback:
	pi = cscf_get_public_identity(msg);
	if (pi.len>4&&strncasecmp(pi.s,"sip:",4)==0) {pi.s+=4;pi.len-=4;}
	for(i=0;i<pi.len;i++)
		if (pi.s[i]==';') {
			pi.len=i;
			break;
		}
	done:
	return pi;	
}

/**
 * Returns the Private Identity extracted from the Authorization header.
 * If none found there takes the SIP URI in To without the "sip:" prefix
 * \todo - remove the fallback case to the To header
 * @param msg - the SIP message
 * @param realm - the realm to match in an Authorization header
 * @returns the str containing the private id, no mem dup
 */
str cscf_get_private_identity_no_realm(struct sip_msg *msg, str realm)
{
	str pi={0,0};
	struct hdr_field* h=0;
	int i;

	if (parse_headers(msg,HDR_AUTHORIZATION_F,0)!=0) {
		return pi;
	}

	if (!msg->authorization){
		goto fallback;
	}

        h = msg->authorization;
        if (h) pi=((auth_body_t*)h->parsed)->digest.username.whole;

	goto done;

	fallback:
	pi = cscf_get_public_identity(msg);
	if (pi.len>4&&strncasecmp(pi.s,"sip:",4)==0) {pi.s+=4;pi.len-=4;}
	for(i=0;i<pi.len;i++)
		if (pi.s[i]==';') {
			pi.len=i;
			break;
		}
	done:
	return pi;	
}

/**
 * Returns the Public Identity extracted from the To header
 * @param msg - the SIP message
 * @returns the str containing the public id, no mem dup
 */
str cscf_get_public_identity(struct sip_msg *msg)
{
	str pu={0,0};
	struct to_body *to;
	int i;

	if (parse_headers(msg,HDR_TO_F,0)!=0) {
		return pu;
	}

	if ( get_to(msg) == NULL ) {
		to = (struct to_body*) pkg_malloc(sizeof(struct to_body));
		parse_to( msg->to->body.s, msg->to->body.s + msg->to->body.len, to );
		msg->to->parsed = to;
	}
	else to=(struct to_body *) msg->to->parsed;

	pu = to->uri;

	/* truncate to sip:username@host or tel:number */
	for(i=4;i<pu.len;i++)
		if (pu.s[i]==';' || pu.s[i]=='?' ||pu.s[i]==':'){
			pu.len = i;
		}

	return pu;
}



/**
 * Returns the expires value from the Expires header in the message.
 * It searches into the Expires header and if not found returns -1
 * @param msg - the SIP message, if available
 * @is_shm - msg from from shared memory 
 * @returns the value of the expire or -1 if not found
 */
int cscf_get_expires_hdr(struct sip_msg *msg, int is_shm)
{
	exp_body_t *exp;
	int expires;
	if (!msg) return -1;
	/*first search in Expires header */
	if (parse_headers(msg,HDR_EXPIRES_F,0)!=0) {
		return -1;
	}
	if (msg->expires){		
		if (!msg->expires->parsed) {
			parse_expires(msg->expires);
		}
		if (msg->expires->parsed) {
			exp = (exp_body_t*) msg->expires->parsed;
			if (exp->valid) {
				expires = exp->val;
				if(is_shm) {
					free_expires((exp_body_t**)&exp);
					msg->expires->parsed = 0;	
				}
				return expires;
			}
		}
	}

	return -1;
}

/**
 * Returns the expires value from the message.
 * First it searches into the Expires header and if not found it also looks 
 * into the expires parameter in the contact header
 * @param msg - the SIP message
 * @param is_shm - msg from shared memory
 * @returns the value of the expire or the default 3600 if none found
 */
int cscf_get_max_expires(struct sip_msg *msg, int is_shm)
{
	unsigned int exp;
	int max_expires = -1;
	struct hdr_field *h;
	contact_t *c;
	/*first search in Expires header */
	max_expires = cscf_get_expires_hdr(msg, is_shm);

	cscf_parse_contacts(msg);
	for(h=msg->contact;h;h=h->next){
		if (h->type==HDR_CONTACT_T && h->parsed) {
			for(c=((contact_body_t *) h->parsed)->contacts;c;c=c->next){
				if(c->expires){
					if (!str2int(&(c->expires->body), (unsigned int*)&exp) && (int)exp>max_expires) max_expires = exp;
				}
			}
		}	
	}

	if(is_shm){
		for(h=msg->contact;h;h=h->next){
			if (h->type==HDR_CONTACT_T && h->parsed) {
				free_contact((contact_body_t**)&(h->parsed));
				h->parsed = 0;
			}
		}
	}

	return max_expires;
}

/**
 * Get the Public Identity from the Request URI of the message
 * NB: free returned result str when done from shm
 * @param msg - the SIP message
 * @returns the public identity (don't forget to free from shm)
 */
str cscf_get_public_identity_from_requri(struct sip_msg *msg)
{
	str pu={0,0};

	if (msg->first_line.type!=SIP_REQUEST) {
		return pu;
	}
	if (parse_sip_msg_uri(msg)<0){
		return pu;
	}

	if(msg->parsed_uri.type==TEL_URI_T){
		pu.len = 4 + msg->parsed_uri.user.len ;
		pu.s = shm_malloc(pu.len+1);
		if (!pu.s){
                        LM_ERR("cscf_get_public_identity_from_requri: Error allocating %d bytes\n", pu.len + 1);
                        pu.len = 0;
			goto done;
                }
		sprintf(pu.s,"tel:%.*s",
				msg->parsed_uri.user.len,
				msg->parsed_uri.user.s);
	}else{
		pu.len = 4 + msg->parsed_uri.user.len + 1 + msg->parsed_uri.host.len;
		pu.s = shm_malloc(pu.len+1);
		if (!pu.s){
                        LM_ERR("cscf_get_public_identity_from_requri: Error allocating %d bytes\n", pu.len + 1);
                        pu.len = 0;
			goto done;
                }
		sprintf(pu.s,"sip:%.*s@%.*s",
				msg->parsed_uri.user.len,
				msg->parsed_uri.user.s,
				msg->parsed_uri.host.len,
				msg->parsed_uri.host.s);
	}

	done:
	return pu;
}

/**
 * Get the contact from the Request URI of the message
 * NB: free returned result str when done from shm
 * @param msg - the SIP message
 * @returns the contact (don't forget to free from shm)
 * 
 * NOTE: should only be called when REQ URI has been converted sip:user@IP_ADDRESS:PORT or tel:IP_ADDRESS:PORT
 */
str cscf_get_contact_from_requri(struct sip_msg *msg)
{
	str pu={0,0};

	if (msg->first_line.type!=SIP_REQUEST) {
		return pu;
	}
	if (parse_sip_msg_uri(msg)<0){
		return pu;
	}
	if(!msg->parsed_uri.port.len){
	    return pu;
	}

	if(msg->parsed_uri.type==TEL_URI_T){
		pu.len = 4 + msg->parsed_uri.user.len + msg->parsed_uri.port.len + 1 /*for colon before port*/;
		pu.s = shm_malloc(pu.len+1);
		if (!pu.s){
                        LM_ERR("cscf_get_public_identity_from_requri: Error allocating %d bytes\n", pu.len + 1);
                        pu.len = 0;
			goto done;
                }
		sprintf(pu.s,"tel:%.*s:%.*s",
				msg->parsed_uri.user.len,
				msg->parsed_uri.user.s,
				msg->parsed_uri.port.len,
				msg->parsed_uri.port.s);
	}else{
		pu.len = 4 + msg->parsed_uri.user.len + 1/*for @*/ + msg->parsed_uri.host.len + msg->parsed_uri.port.len + 1 /*for colon before port*/;
		pu.s = shm_malloc(pu.len+1);
		if (!pu.s){
                        LM_ERR("cscf_get_public_identity_from_requri: Error allocating %d bytes\n", pu.len + 1);
                        pu.len = 0;
			goto done;
                }
		sprintf(pu.s,"sip:%.*s@%.*s:%.*s",
				msg->parsed_uri.user.len,
				msg->parsed_uri.user.s,
				msg->parsed_uri.host.len,
				msg->parsed_uri.host.s,
				msg->parsed_uri.port.len,
				msg->parsed_uri.port.s);
	}

	done:
	return pu;
}

/**
 * Finds if the message contains the orig parameter in the first Route header
 * @param msg - the SIP message
 * @param str1 - not used
 * @param str2 - not used
 * @returns #CSCF_RETURN_TRUE if yes, else #CSCF_RETURN_FALSE
 */
int cscf_has_originating(struct sip_msg *msg,char *str1,char *str2)
{
	//int ret=CSCF_RETURN_FALSE;
	struct hdr_field *h;
	str* uri;
	rr_t *r;

	if (parse_headers(msg, HDR_ROUTE_F, 0)<0){
		LM_DBG("I_originating: error parsing headers\n");
		return CSCF_RETURN_FALSE;
	}
	h = msg->route;
	if (!h){
		LM_DBG("I_originating: Header Route not found\n");
		return CSCF_RETURN_FALSE;
	}
	if (parse_rr(h)<0){
		LM_DBG("I_originating: Error parsing as Route header\n");
		return CSCF_RETURN_FALSE;
	}
	r = (rr_t*)h->parsed;

	uri = &r->nameaddr.uri;
	struct sip_uri puri;
	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LM_DBG( "I_originating: Error while parsing the first route URI\n");
		return -1;
	}
	if (puri.params.len < 4) return CSCF_RETURN_FALSE;
	int c = 0;
	int state = 0; 
	while (c < puri.params.len) {
		switch (puri.params.s[c]) {
		case 'o': if (state==0) state=1;
		break;
		case 'r': if (state==1) state=2;
		break;
		case 'i': if (state==2) state=3;
		break;
		case 'g': if (state==3) state=4;
		break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case ',':
		case ';':
			if (state==4) return CSCF_RETURN_TRUE;
			state=0;
			break;
		case '=': if (state==4) return CSCF_RETURN_TRUE;
		state=-1;
		break;
		default: state=-1;
		}
		c++;
	}

	return state==4 ? CSCF_RETURN_TRUE : CSCF_RETURN_FALSE;
}

str s_asserted_identity={"P-Asserted-Identity",19};
/**
 * Looks for the P-Asserted-Identity header and extracts its content
 * @param msg - the sip message
 * @returns the asserted identity
 */
str cscf_get_asserted_identity(struct sip_msg *msg, int is_shm) {
	int len;
	str uri = { 0, 0 };

	if (!msg || !msg->pai)
		return uri;

	if ((parse_pai_header(msg) == 0) && (msg->pai) && (msg->pai->parsed)) {
		to_body_t *pai = get_pai(msg)->id;
		if (!is_shm)
			return pai->uri;

		//make a pkg malloc str to return to consuming function
		len = pai->uri.len + 1;
		uri.s = (char*) pkg_malloc(pai->uri.len + 1);
		if (!uri.s) {
			LM_ERR("no more pkg mem\n");
			return uri;
		}
		memset(uri.s, 0, len);
		memcpy(uri.s, pai->uri.s, pai->uri.len);
		uri.len = pai->uri.len;

		p_id_body_t* ptr = (p_id_body_t*) msg->pai->parsed;
		msg->pai->parsed = 0;
		free_pai_ppi_body(ptr);
	}
	return uri;
}

static str phone_context_s={";phone-context=",15};
/**
 * Extracts the realm from a SIP/TEL URI. 
 * - SIP - the hostname
 * - TEL - the phone-context parameter
 * @param msg - the SIP message
 * @returns the realm
 */
str cscf_get_realm_from_uri(str uri)
{
	str realm={0,0};
	int i;

	if (uri.len<5) {
		LM_DBG( "cscf_get_realm_from_uri: Error trying to extra realm from too short URI <%.*s>.\n",uri.len,uri.s);
		return realm;
	}
	if (strncasecmp(uri.s,"sip:",4)==0||
			strncasecmp(uri.s,"sips:",5)==0) {
		/* SIP URI */
		realm = uri;
		for(i=0;i<realm.len;i++)
			if (realm.s[i]=='@'){
				realm.s = realm.s + i + 1;
				realm.len = realm.len - i - 1;
				break;
			}
		if (!realm.len) realm = uri;
		for(i=0;i<realm.len;i++)
			if (realm.s[i]==';'||realm.s[i]=='&'||realm.s[i]==':') {
				realm.len = i;
				break;
			}		
	}else
		if (strncasecmp(uri.s,"tel:",4)==0) {
			/* TEL URI */
			realm = uri;
			while(realm.s[0]!=';' && realm.len>0){
				realm.s++;
				realm.len--;
			}		
			if (realm.len<1) {realm.len=0;return realm;}
			else{
				while(realm.len>phone_context_s.len){
					if (strncasecmp(realm.s,phone_context_s.s,phone_context_s.len)==0){
						realm.s+=phone_context_s.len;
						realm.len-=phone_context_s.len;
						for(i=0;i<realm.len;i++)
							if (realm.s[i]==';' || realm.s[i]=='&'){
								realm.len = i;
								break;
							}
						break;
					}
					realm.s++;
					realm.len--;
				}
			}
		}else{
			/* unknown... just extract between @ and ;? */
			realm = uri;
			for(i=0;i<realm.len;i++)
				if (realm.s[i]=='@'){
					realm.s = realm.s + i + 1;
					realm.len = realm.len - i - 1;
					break;
				}
			if (!realm.len) realm = uri;
			for(i=0;i<realm.len;i++)
				if (realm.s[i]==';'||realm.s[i]=='&'||realm.s[i]==':') {
					realm.len = i;
					break;
				}
		}

	LM_DBG( "cscf_get_realm_from_uri: realm <%.*s>.\n",realm.len,realm.s);
	return realm;	
}

/** 
 * Delivers the Realm from request URI
 * @param msg sip message 
 * @returns realm as String on success 0 on fail
 */
str cscf_get_realm_from_ruri(struct sip_msg *msg)
{
	str realm={0,0};
	if (!msg || msg->first_line.type!=SIP_REQUEST){
		LM_DBG("cscf_get_realm_from_ruri: This is not a request!!!\n");
		return realm;
	}
	if (!msg->parsed_orig_ruri_ok)
		if (parse_orig_ruri(msg) < 0) 
			return realm;

	realm = msg->parsed_orig_ruri.host;
	return realm;	
}

/**
 * Looks for the Call-ID header
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field 
 * @returns the callid value
 */
str cscf_get_call_id(struct sip_msg *msg,struct hdr_field **hr)
{
	struct hdr_field *h;
	str call_id={0,0};
	if (hr) *hr = 0;	
	if (!msg) return call_id;
	if (parse_headers(msg, HDR_CALLID_F, 0)<0){
		LM_DBG("cscf_get_call_id: error parsing headers\n");
		return call_id;
	}
	h = msg->callid;
	if (!h){
		LM_DBG("cscf_get_call_id: Header Call-ID not found\n");
		return call_id;
	}
	if (hr) *hr = h;
	call_id = h->body;	
	return call_id;
}

static str sos_uri_par={"sos", 3};
/**
 * Check if the contact has an URI parameter with the value "sos",
 * used for detecting an Emergency Registration
 * http://tools.ietf.org/html/draft-patel-ecrit-sos-parameter-0x
 * @param uri - contact uri to be checked
 * @return 1 if found, 0 if not, -1 on error
 */
int cscf_get_sos_uri_param(str uri)
{
	struct sip_uri puri;
	param_hooks_t h;
	param_t *p=0, *crt;
	enum pclass p_class = CLASS_URI;
	int ret;

	ret = 0;
	p = NULL;

	if(parse_uri(uri.s, uri.len, &puri)<0){
		LM_DBG("cscf_get_sos_uri_param: failed to parse %.*s\n",
				uri.len, uri.s);
		return -1;
	}
	if(puri.params.len <= 0)
		return 0;

	LM_DBG( "cscf_get_sos_uri_param: searching through the uri parameters:%.*s\n", 
			puri.params.len, puri.params.s);        

	if(parse_params(&(puri.params), p_class, &h, &p)){
		LM_DBG( "cscf_get_sos_uri_param:error while parsing uri parameters\n");
		ret = -1;
		goto end;
	}

	for(crt = p ; crt ; crt=crt->next){
		LM_DBG( "cscf_get_sos_uri_param:name: %.*s body: %.*s\n",
				crt->name.len, crt->name.s,
				crt->body.len, crt->body.s);
		if((crt->name.len == sos_uri_par.len) &&
				(strncmp(crt->name.s, sos_uri_par.s, sos_uri_par.len) == 0)){
			ret =1;
			goto end;
		}	
	}

	end:
	if(p) free_params(p);
	return ret;
}

str cscf_p_visited_network_id={"P-Visited-Network-ID",20};
/**
 * Return the P-Visited-Network-ID header
 * @param msg - the SIP message
 * @returns the str with the header's body
 */
str cscf_get_visited_network_id(struct sip_msg *msg, struct hdr_field **h)
{
	str vnid={0,0};
	struct hdr_field *hdr;

	if (h) *h=0;
	if (parse_headers(msg,HDR_EOH_F,0)!=0) {
		LM_DBG("cscf_get_visited_network_id: Error parsing until header EOH: \n");
		return vnid;
	}
	hdr = msg->headers;
	while(hdr){
		if (hdr->name.len==cscf_p_visited_network_id.len &&
				strncasecmp(hdr->name.s,cscf_p_visited_network_id.s,hdr->name.len)==0)
		{
			if (h) *h = hdr;
			vnid = hdr->body;
			goto done;
		}
		hdr = hdr->next;
	}
	LM_DBG("cscf_get_visited_network_id: P-Visited-Network-ID header not found \n");

	done:
	LM_DBG("cscf_get_visited_network_id: <%.*s> \n",
			vnid.len,vnid.s);
	return vnid;
}

/**
 * Adds a header to the message as the first one in the message
 * @param msg - the message to add a header to
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int cscf_add_header_first(struct sip_msg *msg, str *hdr,int type)
{
	struct hdr_field *first;
	struct lump* anchor,*l;

	first = msg->headers;
	anchor = anchor_lump(msg, first->name.s - msg->buf, 0 , 0 );

	if (anchor == NULL) {
		LM_DBG( "cscf_add_header_first: anchor_lump failed\n");
		return 0;
	}

	if (!(l=insert_new_lump_before(anchor, hdr->s,hdr->len,type))){
		LM_ERR( "cscf_add_header_first: error creating lump for header\n" );
		return 0;
	}	
	return 1;
}

/**
 * Returns the next header structure for a given header name.
 * @param msg - the SIP message to look into
 * @param header_name - the name of the header to search for
 * @param last_header - last header to ignore in the search, or NULL if to start from the first one
 * @returns the hdr_field on success or NULL if not found  
 */
struct hdr_field* cscf_get_next_header(struct sip_msg * msg ,
		str header_name,struct hdr_field* last_header)
{	
	struct hdr_field *h;
	if (parse_headers(msg, HDR_EOH_F, 0)<0){
		LM_ERR("cscf_get_next_header_field: error parsing headers\n");
		return NULL;
	}
	if (last_header) h = last_header->next;
	else h = msg->headers;
	while(h){
		if (h->name.len==header_name.len &&strncasecmp(h->name.s,header_name.s,header_name.len)==0)
			break;
		h = h->next;
	}
	return h;
}

/**
 * Looks for the First Via header and returns its body.
 * @param msg - the SIP message
 * @param h - the hdr_field to fill with the result
 * @returns the first via_body
 */
struct via_body* cscf_get_first_via(struct sip_msg *msg,struct hdr_field **h)
{
	if (h) *h = 0;

	if (!msg->h_via1 && parse_headers(msg,HDR_VIA_F,0)!=0) {
		LM_ERR("cscf_get_first_via: Error parsing until header Via: \n");
		return msg->h_via1->parsed;
	}

	if (!msg->via1){
		LM_ERR( "cscf_get_first_via: Message does not contain Via header.\n");
		return msg->h_via1->parsed;
	}

	return msg->h_via1->parsed;	
}

/**
 * Looks for the UE Via in First Via header if its a request
 * or in the last if its a response and returns its body
 * @param msg - the SIP message
 * @returns the via of the UE
 */
struct via_body* cscf_get_ue_via(struct sip_msg *msg)
{
	struct via_body *vb=0;

	if (msg->first_line.type==SIP_REQUEST) vb = cscf_get_first_via(msg,0);
	else vb = cscf_get_last_via(msg);

	if (!vb) return 0;

	if (vb->port == 0) vb->port=5060;
	return vb;	
}

/**
 * Looks for the Last Via header and returns it.
 * @param msg - the SIP message
 * @returns the last via body body
 */
struct via_body* cscf_get_last_via(struct sip_msg *msg)
{
	struct hdr_field *h=0,*i;
	struct via_body *vb;
	if (parse_headers(msg,HDR_EOH_F,0)!=0) {
		LM_ERR("cscf_get_last_via: Error parsing until last header\n");
		return 0;
	}

	i = msg->headers;
	while(i){
		if (i->type == HDR_VIA_T){
			h = i;
		}
		i = i->next;
	}
	if (!h) return 0;
	if (!h->parsed){
		vb = pkg_malloc(sizeof(struct via_body));
		if (!vb){
			LM_ERR("cscf_get_last_via: Error allocating %lx bytes\n",sizeof(struct via_body));
			return 0;
		}
		parse_via(h->body.s,h->body.s+h->body.len,vb);
		h->parsed = vb;
	}
	vb = h->parsed;
	while(vb->next)
		vb = vb->next;
	return vb;	
}

/**
 * Looks for the WWW-Authenticate header and returns its body.
 * @param msg - the SIP message
 * @param h - the hdr_field to fill with the result
 * @returns the www-authenticate body
 */
str cscf_get_authenticate(struct sip_msg *msg,struct hdr_field **h)
{
	str auth={0,0};
	struct hdr_field *hdr;
	*h = 0;
	if (parse_headers(msg,HDR_EOH_F,0)!=0) {
		LM_ERR("cscf_get_authorization: Error parsing until header WWW-Authenticate: \n");
		return auth;
	}
	hdr = msg->headers;
	while(hdr){
		if (hdr->name.len ==16  &&
				strncasecmp(hdr->name.s,"WWW-Authenticate",16)==0)
		{
			*h = hdr;
			auth = hdr->body;
			break;
		}
		hdr = hdr->next;
	}
	if (!hdr){
		LM_DBG( "cscf_get_authorization: Message does not contain WWW-Authenticate header.\n");
		return auth;
	}

	return auth;	
}

/**
 * Adds a header to the message
 * @param msg - the message to add a header to
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int cscf_add_header(struct sip_msg *msg, str *hdr,int type)
{
	struct hdr_field *last;
	struct lump* anchor;
	if (parse_headers(msg,HDR_EOH_F,0)!=0) {
		LM_ERR("cscf_add_header: Error parsing until end of headers: \n");
		return 0;
	}
	last = msg->headers;
	while(last->next) 
		last = last->next;
	anchor = anchor_lump(msg, last->name.s + last->len - msg->buf, 0 , 0);
	if (anchor == NULL) {
		LM_ERR( "cscf_add_header_first: anchor_lump failed\n");
		return 0;
	}

	if (!insert_new_lump_after(anchor, hdr->s,hdr->len,type)){
		LM_ERR( "cscf_add_header_first: error creating lump for header\n" );
		return 0;
	}	
	return 1;
}

/**
 *	Get the expires header value from a message. 
 * @param msg - the SIP message
 * @returns the expires value or -1 if not found
 */
int cscf_get_expires(struct sip_msg *msg)
{	
	if (msg->expires) {
		if (parse_expires(msg->expires) < 0) {
			LM_INFO("ifc_get_expires:Error while parsing Expires header\n");
			return -1;
		}
		return ((exp_body_t*) msg->expires->parsed)->val;
	} else {
		return -1;
	}
}


static str bye_s={"BYE",3};
static str ack_s={"ACK",3};
static str prack_s={"PRACK",5};
static str update_s={"UPDATE",6};
static str notify_s={"NOTIFY",6};
/**
 * Check if the message is an initial request for a dialog. 
 *		- BYE, PRACK, UPDATE, NOTIFY belong to an already existing dialog
 * @param msg - the message to check
 * @returns 1 if initial, 0 if not
 */
int cscf_is_initial_request(struct sip_msg *msg)
{
	if (msg->first_line.type != SIP_REQUEST ) return 0;
	if (strncasecmp(msg->first_line.u.request.method.s,bye_s.s,bye_s.len)==0) return 0;
	if (strncasecmp(msg->first_line.u.request.method.s,ack_s.s,ack_s.len)==0) return 0;
	if (strncasecmp(msg->first_line.u.request.method.s,prack_s.s,prack_s.len)==0) return 0;
	if (strncasecmp(msg->first_line.u.request.method.s,update_s.s,update_s.len)==0) return 0;
	if (strncasecmp(msg->first_line.u.request.method.s,notify_s.s,notify_s.len)==0) return 0;
	return 1;
}

/**
 *	Get the public identity from P-Asserted-Identity, or From if asserted not found.
 * @param msg - the SIP message
 * @param uri - uri to fill into
 * @returns 1 if found, 0 if not
 */
int cscf_get_originating_user( struct sip_msg * msg, str *uri )
{
	struct to_body * from;
	*uri = cscf_get_asserted_identity(msg, 0);
	if (!uri->len) {		
		/* Fallback to From header */
		if ( parse_from_header( msg ) == -1 ) {
			LM_ERR("ERROR:cscf_get_originating_user: unable to extract URI from FROM header\n" );
			return 0;
		}
		if (!msg->from) return 0;
		from = (struct to_body*) msg->from->parsed;
		*uri = from->uri;
		cscf_strip_uri(uri);
	}
	DBG("DEBUG:cscf_get_originating_user: From %.*s\n", uri->len,uri->s );
	return 1;
}

/**
 *	Get public identity from Request-URI for terminating.
 * returns in uri the freshly pkg allocated uri - don't forget to free
 * @param msg - the SIP message
 * @param uri - uri to fill into
 * @returns 1 if found, else 0 
 */
int cscf_get_terminating_user( struct sip_msg * msg, str *uri )
{
	*uri = cscf_get_public_identity_from_requri(msg);
	if (!uri->len) return 0;
	return 1;
}

str cscf_p_access_network_info={"P-Access-Network-Info",21};

/**
 * Return the P-Access-Network-Info header
 * @param msg - the SIP message
 * @returns the str with the header's body
 */

str cscf_get_access_network_info(struct sip_msg *msg, struct hdr_field **h)
{
	str ani={0,0};
	struct hdr_field *hdr;

	*h=0;
	if (parse_headers(msg,HDR_EOH_F,0)!=0) {
		LM_DBG("cscf_get_access_network_info: Error parsing until header EOH: \n");
		return ani;
	}
	hdr = msg->headers;
	while(hdr){
		if (hdr->name.len==cscf_p_access_network_info.len &&
				strncasecmp(hdr->name.s,cscf_p_access_network_info.s,hdr->name.len)==0)
		{
			*h = hdr;
			ani = hdr->body;
			goto done;
		}                 
		hdr = hdr->next;
	}
	LM_DBG("cscf_get_access_network_info: P-Access-Network-Info header not found \n");

	done:
	LM_DBG("cscf_get_access_network_info: <%.*s> \n",
			ani.len,ani.s);
	return ani;
}

str cscf_p_charging_vector={"P-Charging-Vector",17};

/**
 * Return the P-Charging-Vector header
 * @param msg - the SIP message
 * @returns the str with the header's body
 */

str cscf_get_charging_vector(struct sip_msg *msg, struct hdr_field **h)
{
	str cv={0,0};
	struct hdr_field *hdr;

	*h=0;
	if (parse_headers(msg,HDR_EOH_F,0)!=0) {
		LM_DBG("cscf_get_charging_vector: Error parsing until header EOH: \n");
		return cv;
	}
	hdr = msg->headers;
	while(hdr){
		if (hdr->name.len==cscf_p_charging_vector.len &&
				strncasecmp(hdr->name.s,cscf_p_charging_vector.s,hdr->name.len)==0)
		{
			*h = hdr;
			cv = hdr->body;
			goto done;
		}
		hdr = hdr->next;
	}
	LM_DBG("cscf_get_charging_vector: P-Charging-Vector header not found \n");

	done:
	LM_DBG("cscf_get_charging_vector: <%.*s> \n",
			cv.len,cv.s);
	return cv;
}

int cscf_get_p_charging_vector(struct sip_msg *msg, str * icid, str * orig_ioi,
		str * term_ioi) {
	struct hdr_field* header = 0;
	str header_body = { 0, 0 };
	char * p;
	int index;
	str temp = { 0, 0 };

	if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("cscf_get_p_charging_vector: error parsing headers\n");
		return 0;
	}
	header = msg->headers;
	while (header) {
		if (header->name.len == cscf_p_charging_vector.len
				&& strncasecmp(header->name.s, cscf_p_charging_vector.s, cscf_p_charging_vector.len) == 0)
			break;
		header = header->next;
	}
	if (!header) {
		LM_DBG("no header %.*s was found\n", cscf_p_charging_vector.len, cscf_p_charging_vector.s);
		return 0;
	}
	if (!header->body.s || !header->body.len)
		return 0;

	str_dup(header_body, header->body, pkg);

	LM_DBG("p_charging_vector body is %.*s\n", header_body.len, header_body.s);

	p = strtok(header_body.s, " ;:\r\t\n\"=");
	loop: if (p > (header_body.s + header_body.len))
		return 1;

	if (strncmp(p, "icid-value", 10) == 0) {
		p = strtok(NULL, " ;:\r\t\n\"=");
		if (p > (header_body.s + header_body.len)) {
			LM_ERR("cscf_get_p_charging_vector: no value for icid\n");
			return 0;
		}
		temp.s = p;
		temp.len = 0;
		while (*p != '\"') {
			temp.len = temp.len + 1;
			p++;
		}
		icid->len = temp.len;
		index = temp.s - header_body.s;
		LM_DBG("icid len %i, index %i\n", temp.len, index);
		icid->s = header->body.s + index;
		LM_DBG("icid is %.*s\n", icid->len, icid->s);
		p = strtok(NULL, " ;:\r\t\n\"=");
		goto loop;
	} else if (strncmp(p, "orig-ioi", 8) == 0) {

		p = strtok(NULL, " ;:\r\t\n\"=");
		if (p > (header_body.s + header_body.len)) {
			LM_ERR("cscf_get_p_charging_vector: no value for icid\n");
			return 0;
		}
		temp.s = p;
		temp.len = 0;
		while (*p != '\"') {
			temp.len = temp.len + 1;
			p++;
		}
		orig_ioi->len = temp.len;
		index = temp.s - header_body.s;
		LM_DBG("orig ioi len %i, index %i\n", temp.len, index);
		orig_ioi->s = header->body.s + index;
		LM_DBG("orig_ioi is %.*s\n", orig_ioi->len, orig_ioi->s);
		p = strtok(NULL, " ;:\r\t\n\"=");
		goto loop;
	} else if (strncmp(p, "term-ioi", 8) == 0) {

		p = strtok(NULL, " ;:\r\t\n\"=");
		if (p > (header_body.s + header_body.len)) {
			LM_ERR("cscf_get_p_charging_vector: no value for icid\n");
			return 0;
		}
		temp.s = p;
		temp.len = 0;
		while (*p != '\"') {
			temp.len = temp.len + 1;
			p++;
		}
		term_ioi->len = temp.len;
		term_ioi->s = header->body.s + (temp.s - header_body.s);
		p = strtok(NULL, " ;:\r\t\n\"=");
		goto loop;
	} else {
		p = strtok(NULL, " ;:\r\t\n\"=");
		goto loop;
	}

	LM_DBG("end\n");
	str_free(header_body, pkg);
	return 1;
	out_of_memory:
	LM_ERR("cscf_get_p_charging_vector:out of pkg memory\n");
	return 0;
}

/**
 * Get the from tag
 * @param msg - the SIP message to look into
 * @param tag - the pointer to the tag to write to
 * @returns 0 on error or 1 on success
 */
int cscf_get_from_tag(struct sip_msg* msg, str* tag)
{
	struct to_body* from;

	if (!msg || parse_from_header(msg)<0||!msg->from||!msg->from->parsed){
		LM_DBG("cscf_get_from_tag: error parsing From header\n");
		if (tag) {tag->s = 0;tag->len = 0;}
		return 0;
	}
	from = msg->from->parsed;	
	if (tag) *tag = from->tag_value;	
	return 1;	
}

/**
 * Get the to tag
 * @param msg  - the SIP Message to look into
 * @param tag - the pointer to the tag to write to
 * @returns 0 on error or 1 on success
 */
int cscf_get_to_tag(struct sip_msg* msg, str* tag)
{	
	if (!msg || !msg->to) {
		LM_DBG("cscf_get_to_tag(): To header field missing\n");
		if (tag) {tag->s = 0;tag->len = 0;}
		return 0;
	}

	if (tag) *tag = get_to(msg)->tag_value;		
	return 1;
}

/**
 * Get the local uri from the From header.
 * @param msg - the message to look into
 * @param local_uri - ptr to fill with the value
 * @returns 1 on success or 0 on error
 */  
int cscf_get_from_uri(struct sip_msg* msg,str *local_uri)
{	
	struct to_body* from;

	if (!msg || parse_from_header(msg)<0 || !msg->from || !msg->from->parsed){
		LM_DBG("cscf_get_from_uri: error parsing From header\n");
		if (local_uri) {local_uri->s = 0;local_uri->len = 0;}
		return 0;
	}
	from = msg->from->parsed;		
	if (local_uri) *local_uri = from->uri;
	return 1;

}

/**
 * Get the local uri from the To header.
 * @param msg - the message to look into
 * @param local_uri - ptr to fill with the value
 * @returns 1 on success or 0 on error
 */  
int cscf_get_to_uri(struct sip_msg* msg,str *local_uri)
{	
	struct to_body* to=	NULL;

	if (!msg || !msg->to || !msg->to->parsed || parse_headers(msg,HDR_TO_F,0)==-1 ){
		LM_DBG("cscf_get_to_uri: error parsing TO header\n");
		if (local_uri) {local_uri->s = 0;local_uri->len = 0;}
		return 0;
	}
	to = msg->to->parsed;		
	if (local_uri) *local_uri = to->uri;
	return 1;

}

/**
 * Looks for the Event header and extracts its content.
 * @param msg - the sip message
 * @returns the string event value or an empty string if none found
 */
str cscf_get_event(struct sip_msg *msg)
{
	str e={0,0};
	if (!msg) return e;
	if (parse_headers(msg, HDR_EVENT_F, 0) != -1 && msg->event &&
			msg->event->body.len > 0)
	{
		e.len = msg->event->body.len;
		e.s = msg->event->body.s;
	}
	return e;
}

/**
 * Returns the content of the P-Associated-URI header
 * Public_id is pkg_alloced and should be later freed.
 * Inside values are not duplicated.
 * @param msg - the SIP message to look into
 * @param public_id - array to be allocated and filled with the result
 * @param public_id_cnt - the size of the public_id array
 * @param is_shm - msg from shared memory
 * @returns 1 on success or 0 on error
 */
int cscf_get_p_associated_uri(struct sip_msg *msg, str **public_id,
		int *public_id_cnt, int is_shm) {
	struct hdr_field *h;
	rr_t *r, *r2;
	*public_id = 0;
	*public_id_cnt = 0;

	if (!msg)
		return 0;
	if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return 0;
	}
	h = msg->headers;
	while (h) {
		if (h->name.len == 16
				&& strncasecmp(h->name.s, "P-Associated-URI", 16) == 0) {
			break;
		}
		h = h->next;
	}
	if (!h) {
		LM_DBG("Header P-Associated-URI not found\n");
		return 0;
	}
	if (parse_rr(h) < 0) {
		LM_DBG("Error parsing as Route header\n");
		return 0;
	}
	r = (rr_t*) h->parsed;
	h->type = HDR_ROUTE_T;
	*public_id_cnt = 0;
	r2 = r;
	while (r2) {
		(*public_id_cnt) = (*public_id_cnt) + 1;
		r2 = r2->next;
	}
	*public_id = pkg_malloc(sizeof(str)*(*public_id_cnt));
	if (!public_id) {
		LM_ERR("Error out of pkg memory");
		return 0;
	}
	r2 = r;
	*public_id_cnt = 0;
	while (r2) {
		(*public_id)[(*public_id_cnt)] = r2->nameaddr.uri;
		(*public_id_cnt) = (*public_id_cnt) + 1;
		r2 = r2->next;
	}

	if (is_shm) {
		r = (rr_t*) h->parsed;
		h->parsed = 0;
		free_rr(&r);
	}

	return 1;
}

static str realm_p={"realm=\"",7};
/**
 * Looks for the realm parameter in the Authorization header and returns its value.
 * @param msg - the SIP message
 * @returns the realm
 */
str cscf_get_realm(struct sip_msg *msg)
{
        str realm={0,0};
        int i,k;

        if (parse_headers(msg,HDR_AUTHORIZATION_F,0)!=0) {
                LM_DBG("Error parsing until header Authorization: \n");
                return realm;
        }

        if (!msg->authorization){
                LM_DBG("Message does not contain Authorization header.\n");
                return realm;
        }

        k = msg->authorization->body.len - realm_p.len;
        for(i=0;i<k;i++)
         if (strncasecmp(msg->authorization->body.s+i,realm_p.s,realm_p.len)==0){
                realm.s = msg->authorization->body.s+ i + realm_p.len;
                i+=realm_p.len;
                while(i<msg->authorization->body.len && msg->authorization->body.s[i]!='\"'){
                        i++;
                        realm.len++;
                }
                break;
         }

        if (!realm.len){
                LM_DBG("Realm parameter not found.\n");
                return realm;
        }
        LM_DBG("realm <%.*s>.\n",realm.len,realm.s);
        return realm;
}

/**
 * Returns the content of the Service-Route header.
 * data vector is pkg_alloced and should be later freed
 * inside values are not duplicated
 * @param msg - the SIP message
 * @param size - size of the returned vector, filled with the result
 * @param is_shm - msg from shared memory
 * @returns - the str vector of uris
 */
str* cscf_get_service_route(struct sip_msg *msg, int *size, int is_shm) {
	struct hdr_field *h;
	rr_t *r, *r2;
	str *x = 0;
	int k;
	if (!size)
		return 0;

	*size = 0;

	if (!msg)
		return 0;
	if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return 0;
	}
	h = msg->headers;
	while (h) {
		if (h->name.len == 13
				&& strncasecmp(h->name.s, "Service-Route", 13) == 0) {
			if (parse_rr(h) < 0) {
				LM_ERR("Error parsing as Route header\n");
				continue;
			}
			r = (rr_t*) h->parsed;
			h->type = HDR_ROUTE_T;
			r2 = r;
			k = 0;
			while (r2) {
				k++;
				r2 = r2->next;
			}
			if (!k) {
				LM_DBG("No items in this Service-Route\n");
				continue;
			}
			x = pkg_realloc(x,(*size+k)*sizeof(str));
			if (!x) {
				LM_ERR("Error our of pkg memory");
				return 0;
			}
			r2 = r;
			while (r2) {
				x[*size] = r2->nameaddr.uri;
				(*size) = (*size) + 1;
				r2 = r2->next;
			}
		}
		h = h->next;
	}
	if (is_shm) {
		h = msg->headers;
		while (h) {
			if (h->name.len == 13
					&& strncasecmp(h->name.s, "Service-Route", 13) == 0) {
				r = (rr_t*) h->parsed;
				h->parsed = 0;
				free_rr(&r);
			}
			h = h->next;
		}
	}

	return x;
}

/**
 * Returns the s_dialog_direction from the direction string.
 * @param direction - "orig" or "term"
 * @returns the s_dialog_direction if ok or #DLG_MOBILE_UNKNOWN if not found
 */
enum cscf_dialog_direction cscf_get_dialog_direction(char *direction)
{
	switch(direction[0]){
		case 'o':
		case 'O':
		case '0':
			return CSCF_MOBILE_ORIGINATING;
		case 't':
		case 'T':
		case '1':
			return CSCF_MOBILE_TERMINATING;
		default:
			LM_WARN("Unknown direction %s",direction);
			return CSCF_MOBILE_UNKNOWN;
	}
}

long cscf_get_content_length (struct sip_msg* msg)
{
	int cl = 0;
	if (!msg)
		return 0;
	if (parse_headers(msg, HDR_CONTENTLENGTH_F, 0) != -1 && msg->content_length
			&& msg->content_length->parsed)
		cl = get_content_length(msg);
	return cl;

}

/**
 * Looks for the Contact header and extracts its content
 * @param msg - the sip message
 * @returns the first contact in the message
 */
str cscf_get_contact(struct sip_msg *msg)
{
	str id={0,0};
	struct hdr_field *h;
	struct contact_body *cb;
	
	if (!msg) return id;
	if (parse_headers(msg, HDR_CONTACT_F, 0)<0) {
		LM_ERR("ERR:cscf_get_contact: Error parsing headers until Contact.\n");
		return id;
	}

	h = msg->contact;
	if (!h) {
		LM_ERR("ERR:cscf_get_contact: Contact header not found.\n");
		return id;
	}
	if (h->parsed==0 &&
		parse_contact(h)<0){
		LM_ERR("ERR:cscf_get_contact: Error parsing contacts.\n");
		return id;
	}
	
	cb = (struct contact_body *)h->parsed;
	if (!cb || !cb->contacts){
		LM_ERR("ERR:cscf_get_contact: No contacts in header.\n");
		return id;
	}
	id = cb->contacts->uri;
	
	return id;
}

/**
 * Adds a header to the reply message
 * @param msg - the request to add a header to its reply
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int cscf_add_header_rpl(struct sip_msg *msg, str *hdr)
{
	if (add_lump_rpl( msg, hdr->s, hdr->len, LUMP_RPL_HDR)==0) {
		LM_ERR("ERR:cscf_add_header_rpl: Can't add header <%.*s>\n",
			hdr->len,hdr->s);
 		return 0;
 	}
 	return 1;
}


/**
 * Looks for the Call-ID header
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field 
 * @returns the callid value
 */
int cscf_get_cseq(struct sip_msg *msg,struct hdr_field **hr)
{
	struct hdr_field *h;
	struct cseq_body *cseq;
	int nr = 0,i;
	
	if (hr) *hr = 0;	
	if (!msg) return 0;
	if (parse_headers(msg, HDR_CSEQ_F, 0)<0){
		LM_ERR("ERR:cscf_get_cseq: error parsing headers\n");
		return 0;
	}
	h = msg->cseq;
	if (!h){
		LM_ERR("ERR:cscf_get_cseq: Header CSeq not found\n");
		return 0;
	}
	if (hr) *hr = h;
	if (!h->parsed){
		cseq = pkg_malloc(sizeof(struct cseq_body));
		if (!cseq){
			LM_ERR("ERR:cscf_get_cseq: Header CSeq not found\n");
			return 0;
		}
		parse_cseq(h->body.s,h->body.s+h->body.len,cseq);
		h->parsed = cseq;
	}else
		cseq = (struct cseq_body*) h->parsed;		
	for(i=0;i<cseq->number.len;i++)
		nr = (nr*10)+(cseq->number.s[i]-'0');
	return nr;
}

static str s_called_party_id={"P-Called-Party-ID",17};
/**
 * Looks for the P-Called-Party-ID header and extracts the public identity from it
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field 
 * @returns the P-Called_Party-ID
 */
str cscf_get_public_identity_from_called_party_id(struct sip_msg *msg,struct hdr_field **hr)
{
	str id={0,0};
	struct hdr_field *h;
	int after_semi_colon=0;
	int len=0;
	int i=0;
	
	if (hr) *hr=0;
	if (!msg) return id;
	if (parse_headers(msg, HDR_EOH_F, 0)<0) {
		return id;
	}
	h = msg->headers;
	while(h)
	{
		if (h->name.len == s_called_party_id.len  &&
			strncasecmp(h->name.s,s_called_party_id.s,s_called_party_id.len)==0)
		{
			id = h->body;
			while(id.len && (id.s[0]==' ' || id.s[0]=='\t' || id.s[0]=='<')){
				id.s = id.s+1;
				id.len --;
			}
			while(id.len && (id.s[id.len-1]==' ' || id.s[id.len-1]=='\t' || id.s[id.len-1]=='>')){
				id.len--;
			}	
			//get only text in front of ';' there might not even be a semi-colon
			//this caters for extra information after the public identity - e.g. phone-context
			len= id.len;
			for(i=0; i<len;i++) {
			    if(id.s[i]==';'){
				//found semi-colon
				after_semi_colon = 1;
			    }
			    if(after_semi_colon){
				id.len--;
			    }
			}
			if (hr) *hr = h;
			return id;
		}
		h = h->next;
	}
	return id;
}


/*
 * Route & Record-Route module
 *
 * $Id$
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
 * --------
 * 2003-02-28 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-01-19 - verification against double record-routing added, 
 *            - option for putting from-tag in record-route added 
 *            - buffer overflow eliminated (jiri)
 */

#include "common.h"
#include <string.h>
#include "../../md5utils.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../action.h"
#include "../../data_lump.h"
#include "../../globals.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_rr.h"
#include "rr_mod.h"


char rr_hash[MD5_LEN];
char rr_s[256];

str rr_suffix;

int append_fromtag;

/*
 * Generate hash string that will be inserted in RR
 */
void generate_hash(void)
{
	str src[3];
	
	     /*some fix string*/
	src[0].s = "SIP Express Router" ;
	src[0].len = 18;

	src[1].s = bind_address->address_str.s ;
	src[1].len = bind_address->address_str.len;

	src[2].s = bind_address->port_no_str.s;
	src[2].len = bind_address->port_no_str.len;
	MDStringArray(rr_hash, src, 3);
}


int generate_rr_suffix(void)
{
	rr_suffix.s = rr_s;
	rr_suffix.len = 0;

	switch(bind_address->address.af) {
	case AF_INET:
		memcpy(rr_suffix.s, bind_address->address_str.s, bind_address->address_str.len);
		rr_suffix.len = bind_address->address_str.len;
		break;
		
	case AF_INET6:
		rr_suffix.s[0] = '[';
		rr_suffix.len = 1;

		memcpy(rr_suffix.s + rr_suffix.len, bind_address->address_str.s, bind_address->address_str.len);
		rr_suffix.len += bind_address->address_str.len;
		rr_suffix.s[rr_suffix.len++] = ']';
		break;
		
	default:
		LOG(L_ERR, "generate_rr_suffix(): Unsupported PF type: %d\n", bind_address->address.af);
		return -1;
	}
	
	
	if (bind_address->port_no != SIP_PORT) {
		memcpy(rr_suffix.s + rr_suffix.len, bind_address->port_no_str.s, bind_address->port_no_str.len);
		rr_suffix.len += bind_address->port_no_str.len;
	}
	
	return 0;
}



/*
 * Parse the message and find first occurence of
 * Route header field. The function returns -1 on
 * an parser error, 0 if there is a Route HF and
 * 1 if there is no Route HF.
 */
int find_first_route(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_ROUTE, 0) == -1) {
		LOG(L_ERR, "find_first_route(): Error while parsing headers\n");
		return -1;
	} else {
		if (_m->route) {
			if (parse_rr(_m->route) < 0) {
				LOG(L_ERR, "find_first_route(): Error while parsing Route HF\n");
				return -1;
			}
			return 0;
		} else {
			DBG("find_first_route(): No Route headers found\n");
			return 1;
		}
	}
}


/*
 * Rewrites Request-URI with string given in _s parameter
 *
 * Reuturn 0 on success, negative number on error
 */
int rewrite_RURI(struct sip_msg* _m, str* _s)
{
       struct action act;
       char* buffer;
       
       buffer = (char*)pkg_malloc(_s->len + 1);
       if (!buffer) {
	       LOG(L_ERR, "rewrite_RURI(): No memory left\n");
	       return -1;
       }
       
       memcpy(buffer, _s->s, _s->len);
       buffer[_s->len] = '\0';
       
       act.type = SET_URI_T;
       act.p1_type = STRING_ST;
       act.p1.string = buffer;
       act.next = 0;
       
       if (do_action(&act, _m) < 0) {
	       LOG(L_ERR, "rewrite_RURI(): Error in do_action\n");
	       pkg_free(buffer);
	       return -2;
       }
       
       pkg_free(buffer);
       
       return 0;
}


/*
 * Remove Top Most Route URI
 * Returns 0 on success, negative number on failure
 */
int remove_first_route(struct sip_msg* _m, struct hdr_field* _route)
{
	int offset, len;

	if (((rr_t*)_route->parsed)->next) {
		DBG("remove_first_route(): Next URI found in the same header found\n");
		offset = _route->body.s - _m->buf;
		len = ((rr_t*)_route->parsed)->next->nameaddr.name.s - _route->body.s;
	} else {
		DBG("remove_first_route(): No next URI found, removing the whole header\n");
		offset = _route->name.s - _m->buf;
		len = _route->len;
	}
	
     	if (del_lump(&_m->add_rm, offset, len, 0) == 0) {
		LOG(L_ERR, "remove_first_route(): Can't remove Route HF\n");
		return -3;
	}

	return 0;
}


/*
 * Builds Record-Route line
 * Returns 0 on success, negative number on a failure
 * if _lr is set to 1, ;lr parameter will be used
 */
static char *build_RR(struct sip_msg* _m, int* _l, int _lr)
{
	str user;
	int len;
	char *rr;
	char *p;
	struct to_body *from;
	struct sip_uri puri;

	from=0; /* fool -Wall */

	/* calculate length first */
	
	len = RR_PREFIX_LEN;
	
     /* first try to look at r-uri for a username */
	if (parse_uri(_m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len, &puri) < 0) {
		LOG(L_ERR, "build_RR: Error while parsing R-URI\n");
		return 0;
	}
	
	user = puri.user;

	/* no username in original uri -- hmm; maybe it is a uri
	 * with just host address and username is in a preloaded route,
	 * which is now no rewritten r-uri (assumed rewriteFromRoute
	 * was called somewhere in script's beginning) 
	 */
	if (user.len==0 && _m->new_uri.s) {
		if (parse_uri(_m->new_uri.s, _m->new_uri.len, &puri) < 0) {
			LOG(L_ERR, "build_RR(): Error while parsing new_uri\n");
			return 0;
		}
		user = puri.user;
	}
	len+=user.len+1 /* '@' */;

	switch(bind_address->address.af) {
	case AF_INET:
		len += bind_address->address_str.len;
		break;

	case AF_INET6:
		len += bind_address->address_str.len + 2;
		break;

	default:
		LOG(L_ERR, "build_RR(): Unsupported PF type: %d\n", bind_address->address.af);
	}
	
	if (_lr && use_fast_cmp) {
		len+=MD5_LEN;
	} 
	len+=rr_suffix.len;
	if (append_fromtag) {
		if (parse_from_header(_m)<0) {
			LOG(L_ERR, "build_RR: From parsing failed\n");
			return 0;
		}
		from = (struct to_body*)_m->from->parsed;
		if (from->tag_value.s) len += RR_FROMTAG_LEN + from->tag_value.len;
	}
	len+=_lr ? RR_LR_TERM_LEN : RR_SR_TERM_LEN;


	rr = (char*)pkg_malloc(len);
	if (!rr) {
		LOG(L_ERR, "build_RR: No memory left\n");
		return 0;
	}

	/* fill the buffer now ... */

	*_l=len;
	p=rr;

	memcpy(p, RR_PREFIX, RR_PREFIX_LEN);
	p+=RR_PREFIX_LEN;
	
	if (_lr && use_fast_cmp) {
		memcpy(p, rr_hash, MD5_LEN);
		p+=MD5_LEN;
	}
	if (user.len) {
		memcpy(p, user.s, user.len);
		p+=user.len;
	}
	*p='@';p++;

	switch(bind_address->address.af) {
	case AF_INET:
		memcpy(p, bind_address->address_str.s, bind_address->address_str.len);
		printf("!!!! '%.*s'\n", bind_address->address_str.len, bind_address->address_str.s);
		p += bind_address->address_str.len;
		break;

	case AF_INET6:
		*p = '[';
		p++;
		memcpy(p, bind_address->address_str.s, bind_address->address_str.len);
		p += bind_address->address_str.len;
		*p = ']';
		p++;
		break;

	default:
		LOG(L_ERR, "build_RR(): Unsupported PF type: %d\n", bind_address->address.af);
	}

	memcpy(p, rr_suffix.s, rr_suffix.len);
	p += rr_suffix.len;
	
	if (append_fromtag && from->tag_value.s) {
		memcpy(p, RR_FROMTAG, RR_FROMTAG_LEN); 
		p += RR_FROMTAG_LEN;
		memcpy(p, from->tag_value.s, from->tag_value.len);
		p += from->tag_value.len;
	}

	if (_lr) {
		memcpy(p, RR_LR_TERM,  RR_LR_TERM_LEN); p+= RR_LR_TERM_LEN;
	} else {
		memcpy(p, RR_SR_TERM,  RR_SR_TERM_LEN); p+= RR_SR_TERM_LEN;
	}
	
	DBG("build_RR(): '%.*s'", len, rr);
	
	return rr;
}


/*
 * Insert a new Record-Route Header Field
 * into a SIP message
 */
int insert_RR(struct sip_msg* _m, str* _l)
{
	struct lump* anchor;

	anchor = anchor_lump(&_m->add_rm, _m->headers->name.s - _m->buf, 0 , 0);
	if (anchor == NULL) {
		LOG(L_ERR, "insert_RR(): Can't get anchor\n");
		return -1;
	}
	
	if (insert_new_lump_before(anchor, _l->s, _l->len, 0) == 0) {
		
		LOG(L_ERR, "insert_RR(): Can't insert Record-Route\n");
		return -2;
	}
	return 0;
}


/*
 * Insert a new Record-Route header field
 */
int record_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	str b;
	static unsigned int last_rr_msg;

	if (_m->id==last_rr_msg) {
			LOG(L_ERR, "ERROR: record_route: double attempt to record-route\n");
			return -1;
	}
	
	b.s = build_RR(_m, &b.len, 0);	
	if (!b.s) {
		LOG(L_ERR, "add_rr(): Error while building Record-Route line\n");
		return -2;
	}

	if (insert_RR(_m, &b) < 0) {
		LOG(L_ERR, "add_rr(): Error while inserting Record-Route line\n");
		pkg_free(b.s);
		return -3;
	}

	last_rr_msg=_m->id;	
	return 1;
}

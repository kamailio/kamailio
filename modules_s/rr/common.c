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
 */

#include "common.h"
#include <string.h>
#include "../../md5utils.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../action.h"
#include "../../data_lump.h"
#include "../../globals.h"
#include "utils.h"


char rr_hash[MD5_LEN];
char rr_s[256];

str rr_suffix;

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
			return 0;
		} else {
			DBG("find_first_route(): No Route headers found\n");
			return 1;
		}
	}
}


/*
 * Extracts URI from the topmost Route header field.
 *
 * Returns 0 on success, negative number on an error.
 */
int parse_first_route(struct hdr_field* _route, str* _s)
{
        str r;
	char* uri_end;
	
	r.s = _route->body.s;
	r.len = _route->body.len;

	_s->s = find_not_quoted(&r, '<'); 
	if (_s->s) {
		_s->s++; /* We will skip < character */
	} else {
		LOG(L_ERR, "parse_first_route(): Malformed Route HF (no < found)\n");
		return -1;
	}
	
	r.len -= _s->s - r.s;
	r.s = _s->s;

	uri_end = find_not_quoted(&r, '>');

	if (!uri_end) {
		LOG(L_ERR, "parse_first_route(): Malformed Route HF (no > found)\n");
		return -2;
	}

	_s->len = uri_end - _s->s;
	return 0;
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
 * If there is another URI in the Route header field, it will
 * be stored in _uri parameter
 */
int remove_TMRoute(struct sip_msg* _m, struct hdr_field* _route, str* _uri)
{
	int offset, len;
	char* next;
	str rest;

	rest.s = _uri->s + _uri->len; /* Just after > */
	rest.len = _route->body.s + _route->body.len - rest.s;

	next = find_not_quoted(&rest, ',');
	
	if (next) {
		rest.len -= next - rest.s;
		rest.s = next;

		next = find_not_quoted(&rest, '<');
		if (next == 0) {
			LOG(L_ERR, "remove_TMRoute(): Found \',\' but no \'<\' after the comma\n");
			return -1;
		}

		DBG("remove_TMRoute(): next URI found: \'%.*s\'\n", rest.len - (next - rest.s), next);
		offset = _route->body.s - _m->buf + 1; /* + 1 - keep the first white char */
		len = next - _route->body.s - 1;
		
		     /* Extract next URI */
		_uri->s = next + 1;

		     /* Shift rest just behind the < */
		rest.len -= next - rest.s;
		rest.s = next;
		next = find_not_quoted(&rest, '>');  /* Find closing > */
		if (!next) {
			LOG(L_ERR, "remove_TMRoute(): No > found\n");
			return -2;
		}

		_uri->len = next - _uri->s;  /* Update length */
	} else {
		DBG("remove_TMRoute(): No next URI in the same Route found\n");
		offset = _route->name.s - _m->buf;
		len = _route->name.len + _route->body.len + 2;
		if (_route->body.s[_route->body.len] != '\0') len++;  /* FIXME: Is this necessary ? */

		_uri->s = 0;
		_uri->len = 0;
	}
	
     	if (del_lump(&_m->add_rm, offset, len, 0) == 0) {
		LOG(L_ERR, "remove_TMRoute(): Can't remove Route HF\n");
		return -3;
	}

	return 0;
}


/*
 * Builds Record-Route line
 * Returns 0 on success, negative number on a failure
 * if _lr is set to 1, ;lr parameter will be used
 */
int build_RR(struct sip_msg* _m, str* _l, int _lr)
{
	str user;
	
	_l->len = RR_PREFIX_LEN;
	memcpy(_l->s, RR_PREFIX, _l->len);
	
	     /* first try to look at r-uri for a username */
	user.s = _m->first_line.u.request.uri.s;
	user.len = _m->first_line.u.request.uri.len;
	get_username(&user);
	
	     /* no username in original uri -- hmm; maybe it is a uri
		with just host address and username is in a preloaded route,
		which is now no rewritten r-uri (assumed rewriteFromRoute
		was called somewhere in script's beginning) 
	     */
	if (user.len==0 && _m->new_uri.s) {
		user.s = _m->new_uri.s;
		user.len = _m->new_uri.len;
		get_username(&user);
	}
	
	if (_lr) {
		memcpy(_l->s + _l->len, rr_hash, MD5_LEN);
		_l->len += MD5_LEN;

		if (user.len) {
			memcpy(_l->s + _l->len, user.s, user.len);
			_l->len += user.len;
		}
		
		*(_l->s + _l->len++) = '@';
	} else {
		if (user.len) {
			memcpy(_l->s + _l->len, user.s, user.len);
			_l->len += user.len;
			*(_l->s + _l->len++) = '@';
		}
	}

	memcpy(_l->s + _l->len, rr_suffix.s, rr_suffix.len);
	_l->len += rr_suffix.len;

	if (_lr) {
		memcpy(_l->s + _l->len, ";lr>\r\n",  7);
		_l->len += 6;
	} else {
		memcpy(_l->s + _l->len, ">\r\n", 4);
		_l->len += 3;
	}
	
	DBG("build_RR(): \'%.*s\'", _l->len, _l->s);
	
	return 0;
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
int record_route(struct sip_msg* _m, char* _lr, char* _s)
{
	str b;
	
	b.s = (char*)pkg_malloc(MAX_RR_LEN);
	if (!b.s) {
		LOG(L_ERR, "add_rr(): No memory left\n");
		return -1;
	}
	
	if (build_RR(_m, &b, (int)_lr) < 0) {
		LOG(L_ERR, "add_rr(): Error while building Record-Route line\n");
		pkg_free(b.s);
		return -2;
	}

	if (insert_RR(_m, &b) < 0) {
		LOG(L_ERR, "add_rr(): Error while inserting Record-Route line\n");
		pkg_free(b.s);
		return -3;
	}
	
	return 1;
}

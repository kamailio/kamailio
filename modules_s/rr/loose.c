/*
 * Route & Record-Route module, loose routing support
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
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */


#include "loose.h"
#include <string.h>
#include <stdlib.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../parser/hf.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../globals.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_rr.h"
#include "common.h"
#include "rr_mod.h"


#define ROUTE_PREFIX "Route: <"
#define ROUTE_PREFIX_LEN (sizeof(ROUTE_PREFIX)-1)

#define ROUTE_SUFFIX ">\r\n"
#define ROUTE_SUFFIX_LEN (sizeof(ROUTE_SUFFIX)-1)


/*
 * Find out if a URI contains r2 parameter which indicates
 * that we put 2 record routes
 */
static inline int is_2rr(str* _uri)
{
	str s;
	int i, state = 0;
	struct sip_uri puri;

	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		LOG(L_ERR, "is_2rr(): Error while parsing Request URI\n");
		return 0;
	}

	if (puri.params.len == 0) return 0;

	s = puri.params;

	s.len--;
	s.s++;

	for(i = 0; i < s.len; i++) {
		switch(state) {
		case 0:
			switch(s.s[i]) {
			case ' ':
			case '\r':
			case '\n':
			case '\t':           break;
			case 'r':
			case 'R': state = 1; break;
			default:  state = 4; break;
			}
			break;

		case 1:
			switch(s.s[i]) {
			case '2': state = 2; break;
			default:  state = 4; break;
			}
			break;

		case 2:
			switch(s.s[i]) {
			case ';':  return 1;
			case ' ':
			case '\r':
			case '\n':
			case '\t': state = 3; break;
			default:   state = 4; break;
			}
			break;

		case 3:
			switch(s.s[i]) {
			case ';':  return 1;
			case ' ':
			case '\r':
			case '\n':
			case '\t': break;
			default:   state = 4; break;
			}
			break;

		case 4:
			switch(s.s[i]) {
			case '\"': state = 5; break;
			case ';':  state = 0; break;
			default:              break;
			}
			break;
			
		case 5:
			switch(s.s[i]) {
			case '\\': state = 6; break;
			case '\"': state = 4; break;
			default:              break;
			}
			break;

		case 6: state = 5; break;
		}
	}
	
	if ((state == 2) || (state == 3)) return 1;
	else return 0;
}


/*
 * Ordinary URI comparison
 */
/*
 * NOTE: We do not compare transport. This is because usualy both
 * transports (UDP and TCP) are handled by the same server and the spec
 * mandates that the request must reach the same element which is true
 * in this case.
 * 
 * A configuration where on UDP transport is listening a completely different
 * server that has nothing in common with the server listening on TCP socket
 * is highly unprobable and generaly makes no sense so we ignore it. (janakj)
 */
static inline int is_myself(struct receive_info* rcv, str* _uri)
{
	struct sip_uri uri;
	int port;

	if (parse_uri(_uri->s, _uri->len, &uri) != 0) {
		LOG(L_ERR, "is_myself(): Error while parsing URI\n");
		return 0;
	}

	port = (uri.port.s) ? (uri.port_no) : SIP_PORT;

	if (rcv->bind_address->port_no == port) {
		if (uri.host.len == rcv->bind_address->address_str.len) {
			if (!memcmp(uri.host.s, rcv->bind_address->address_str.s, uri.host.len)) {
				DBG("is_myself(): equal\n");
				return 1;
			} else DBG("is_myself(): hosts differ\n");
		} else DBG("is_myself(): Host lengths differ\n");
	} else DBG("is_myself(): Ports differ\n");
	
	return 0;
}


/*
 * Find and parse next Route header field
 */
static inline int find_next_route(struct sip_msg* _m, struct hdr_field** _hdr, rr_t** _r, rr_t** _p)
{
	struct hdr_field* ptr;

	if ((*_r)->next) {
		*_p = *_r;
		*_r = (*_r)->next;
		return 0;
	}

	ptr = (*_hdr)->next;

	     /* Try to find already parsed Route headers */
	while(ptr) {
		if (ptr->type == HDR_ROUTE) goto found;
		ptr = ptr->next;
	}

	     /* There are no already parsed Route headers, try to find next
	      * occurence of Route header
	      */
	if (parse_headers(_m, HDR_ROUTE, 1) == -1) {
		LOG(L_ERR, "fnr(): Error while parsing headers\n");
		return -1;
	}

	if (_m->last_header->type != HDR_ROUTE) {
		DBG("fnr(): No next Route HF found\n");
		return 1;
	}

	ptr = _m->last_header;

 found:
	if (parse_rr(ptr) < 0) {
		LOG(L_ERR, "fnr(): Error while parsing Route body\n");
		return -2;
	}

	*_hdr = ptr;
	*_r = ptr->parsed;
	*_p = 0;
	return 0;
}


/*
 * Check if the given uri contains lr parameter which marks loose routers
 */
static inline int is_strict(str* _params)
{
	str s;
	int i, state = 0;

	if (_params->len == 0) return 1;

	s.s = _params->s;
	s.len = _params->len;

	s.len--;
	s.s++;

	for(i = 0; i < s.len; i++) {
		switch(state) {
		case 0:
			switch(s.s[i]) {
			case ' ':
			case '\r':
			case '\n':
			case '\t':           break;
			case 'l':
			case 'L': state = 1; break;
			default:  state = 4; break;
			}
			break;

		case 1:
			switch(s.s[i]) {
			case 'r':
			case 'R': state = 2; break;
			default:  state = 4; break;
			}
			break;

		case 2:
			switch(s.s[i]) {
			case ';':  return 0;
			case ' ':
			case '\r':
			case '\n':
			case '\t': state = 3; break;
			default:   state = 4; break;
			}
			break;

		case 3:
			switch(s.s[i]) {
			case ';':  return 0;
			case ' ':
			case '\r':
			case '\n':
			case '\t': break;
			default:   state = 4; break;
			}
			break;

		case 4:
			switch(s.s[i]) {
			case '\"': state = 5; break;
			case ';':  state = 0; break;
			default:              break;
			}
			break;
			
		case 5:
			switch(s.s[i]) {
			case '\\': state = 6; break;
			case '\"': state = 4; break;
			default:              break;
			}
			break;

		case 6: state = 5; break;
		}
	}
	
	if ((state == 2) || (state == 3)) return 0;
	else return 1;
}


/*
 * Put Request-URI as last Route header of a SIP
 * message, this is necessary when forwarding to
 * a strict router
 */
static inline int save_ruri(struct sip_msg* _m)
{
	struct lump* anchor;
	char *s;
	int len;

	     /* We must parse the whole message header here,
	      * because Request-URI must be saved in last
	      * Route HF in the message
	      */
	if (parse_headers(_m, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "save_ruri(): Error while parsing message\n");
		return -1;
	}

	     /* Create an anchor */
	anchor = anchor_lump(&_m->add_rm, _m->unparsed - _m->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "save_ruri(): Can't get anchor\n");
		return -2;
	}

	     /* Create buffer for new lump */
	len = ROUTE_PREFIX_LEN + _m->first_line.u.request.uri.len + ROUTE_SUFFIX_LEN + 1; /* Terminating zero */
	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "save_ruri(): No memory left\n");
		return -3;
	}

	     /* Create new header field */
	memcpy(s, ROUTE_PREFIX, ROUTE_PREFIX_LEN);
	memcpy(s + ROUTE_PREFIX_LEN, _m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len);
	memcpy(s + ROUTE_PREFIX_LEN + _m->first_line.u.request.uri.len, ROUTE_SUFFIX, ROUTE_SUFFIX_LEN + 1);

	DBG("save_ruri(): New header: '%s'\n", s);

	     /* Insert it */
	if (insert_new_lump_before(anchor, s, len, 0) == 0) {
		pkg_free(s);
		LOG(L_ERR, "save_ruri(): Can't insert lump\n");
		return -4;
	}

	return 0;
}


/*
 * Logic necessary to forward request to strict routers
 *
 * Returns 0 on success, negative number on an error
 */
static inline int handle_strict_router(struct sip_msg* _m, struct hdr_field* _hdr, rr_t* _r, rr_t* _p)
{
	str* uri;

	uri = &_r->nameaddr.uri;

	     /* Next hop is strict router, save R-URI here */
	if (save_ruri(_m) < 0) {
		LOG(L_ERR, "hsr(): Error while saving Request-URI\n");
		return -1;
	}
	
	     /* Put the first Route in Request-URI */
	if (rewrite_RURI(_m, uri) < 0) {
		LOG(L_ERR, "hsr(): Error while rewriting request URI\n");
		return -2;
	}
	
	if (remove_route(_m, _hdr, _r, _p) < 0) {
		LOG(L_ERR, "hsr(): Error while removing next Route URI\n");
		return -3;
	}
	
	return 0;
}


/*
 * Find last route in the last Route header field,
 * if there was a previous route in the last Route header
 * field, it will be saved in _p parameter
 */
static inline int find_last_route(struct sip_msg* _m, struct hdr_field** _h, rr_t** _l, rr_t** _p)
{
	struct hdr_field* ptr, *last;

	if (parse_headers(_m, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "find_last_route(): Error while parsing message header\n");
		return -1;
	}

	ptr = _m->route->next;
	last = 0;

	while(ptr) {
		if (ptr->type == HDR_ROUTE) last = ptr;
		ptr = ptr->next;
	}

	if (last) {
		if (parse_rr(last) < 0) {
			LOG(L_ERR, "find_last_route(): Error while parsing last Route HF\n");
			return -2;
		}

		*_p = 0;
		*_l = (rr_t*)last->parsed;
		while ((*_l)->next) {
			*_p = *_l;
			*_l = (*_l)->next;
		}
		return 0;
	} else {
		LOG(L_ERR, "find_last_route(): Can't find last Route HF\n");
		return 1;
	}
}


/*
 * Remove the 2nd route inserted by me.
 * The function returns position of the next route
 * in _r and _h and previous route in the same header field if any in _p
 * 
 * The function returns 1 if there is no route left and a negative
 * number on error. 0 is returned if everything went OK.
 */
static inline int remove_2nd_route(struct sip_msg* _m, struct hdr_field** _h, rr_t** _r, rr_t** _p)
{
	int res;

	if ((*_r)->next) {
		*_p = *_r;
		*_r = (*_r)->next;
	} else {
		res = find_next_route(_m, _h, _r, _p);
		if (res < 0) {
			LOG(L_ERR, "remove_2nd_route): Error while parsing headers\n");
			return -2;
		} else if (res > 0) return 1;  /* No next route header field found */
	}
	
	if (remove_route(_m, *_h, *_r, 0) < 0) {
		LOG(L_ERR, "remove_2nd_route(): Error while removing my 2nd route\n");
		return -3;
	}
	return 0;
}


/*
 * Previous hop was a strict router, handle this case
 */
static inline int route_after_strict(struct sip_msg* _m)
{
	int res;
	str* uri;
	rr_t* rt, *prev;
	struct hdr_field* hdr;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;

	if (enable_double_rr && is_2rr(&_m->first_line.u.request.uri)) {
		res = remove_2nd_route(_m, &hdr, &rt, &prev);
		if (res < 0) return -2;  /* Error */
		if (res > 0) return 0;   /* No next route found */
	}

	uri = &rt->nameaddr.uri;
	DBG("ras(): First URI '%.*s'\n", uri->len, uri->s);

	if (is_strict(uri)) {
		DBG("ras(): Next hop is a strict router\n");

		     /* Previous hop was a strict router and the next hop is strict
		      * router too. There is no need to save R-URI again because it
		      * is saved already. In fact, in this case we will behave exactly
		      * like a strict router.
		      */

		     /* Note: when there is only one Route URI left (endpoint), it will
		      * always be a strict router because endpoints don't use ;lr parameter
		      * In this case we will simply put the URI in R-URI and forward it, which
		      * will work perfectly
		      */
		if (rewrite_RURI(_m, uri) < 0) {
			LOG(L_ERR, "ras(): Error while rewriting request URI\n");
			return -4;
		}
		if (remove_route(_m, hdr, rt, prev) < 0) {
			LOG(L_ERR, "ras(): Error while removing the topmost Route URI\n");
			return -5;
		}
	} else {
		DBG("ras(): Next hop is a loose router\n");

		_m->dst_uri.s = uri->s;
		_m->dst_uri.len = uri->len;

		     /* Next hop is a loose router - Which means that is is not endpoint yet
		      * In This case we have to recover from previous strict routing, that means we have
		      * to find the last Route URI and put in in R-URI and remove the last Route URI.
		      */
		if (find_last_route(_m, &hdr, &rt, &prev) < 0) {
			LOG(L_ERR, "ras(): Error while looking for last Route URI\n");
			return -6;
		}
		
		     /* The first character if uri will be either '<' when it is the only URI in a
		      * Route header field or ',' if there is more than one URI in the header field
		      */
		DBG("ras(): last: '%.*s'\n", rt->nameaddr.uri.len, rt->nameaddr.uri.s);

		if (rewrite_RURI(_m, uri) < 0) {
			LOG(L_ERR, "ras(): Can't rewrite R-URI\n");
			return -7;
		}

		if (remove_route(_m, hdr, rt, prev) < 0) {
			LOG(L_ERR, "ras(): Error while removing last route\n");
			return -8;
		}
	}
	
	return 0;
}


static inline int route_after_loose(struct sip_msg* _m)
{
	struct hdr_field* hdr;
	int res;	
	str* uri;
	rr_t* rt, *prev;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = &rt->nameaddr.uri;

	     /* IF the URI was added by me, remove it */
	if (is_myself(&_m->rcv, uri)) {
		DBG("ral(): Topmost URI is myself\n");

		if (remove_route(_m, _m->route, rt, 0) < 0) {
			LOG(L_ERR, "ral(): Error while removing the topmost Route URI\n");
			return -2;
		}

		res = find_next_route(_m, &hdr, &rt, &prev);
		if (res < 0) {
			LOG(L_ERR, "ral(): Error while trying to find next Route header field\n");
			return -3;
		}
		     /* No next Route URI found */
		if (res != 0) {
			DBG("ral(): No next URI found\n");
			return 0;
		}

		uri = &rt->nameaddr.uri;
		if (enable_double_rr && is_2rr(uri)) {
			res = remove_2nd_route(_m, &hdr, &rt, &prev);
			if (res < 0) return -2;  /* Error */
			if (res > 0) return 0;   /* No next route found */
		}
		uri = &rt->nameaddr.uri;
	} else {
		DBG("ral(): Topmost URI is NOT myself\n");
	}

	DBG("ral(): URI to be processed: '%.*s'\n", uri->len, uri->s);
	if (is_strict(uri)) {
		DBG("ral(): Next URI is a strict router\n");
		if (handle_strict_router(_m, hdr, rt, prev) < 0) {
			LOG(L_ERR, "ral(): Error while handling strict router\n");
			return -5;
		}
	} else {
		     /* Next hop is loose router */
		DBG("ral(): Next URI is a loose router\n");
		_m->dst_uri.s = uri->s;
		_m->dst_uri.len = uri->len;
	}

	return 0;
}


/*
 * Do loose routing as defined in RFC3621
 */
int loose_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	if (find_first_route(_m) != 0) {
		DBG("loose_route(): There is no Route HF\n");
		return 1;
	}
		
	if (is_myself(&_m->rcv, &(_m->first_line.u.request.uri))) {
		if (route_after_strict(_m) < 0) {
			LOG(L_ERR, "loose_route(): Error in route_after_strict\n");
			return -1;
		}
	} else {
		if (route_after_loose(_m) < 0) {
			LOG(L_ERR, "loose_route(): Error in route_after_loose\n");
			return -1;
		}
	}

	return 1;
}

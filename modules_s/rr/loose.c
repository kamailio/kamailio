/*
 * Route & Record-Route module, loose routing support
 *
 * $Id$
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
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */


#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../action.h"
#include "../../dprint.h"
#include "../../forward.h"
#include "../../data_lump.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "../../mem/mem.h"
#include "loose.h"
#include "rr_mod.h"


#define ROUTE_PREFIX "Route: <"
#define ROUTE_PREFIX_LEN (sizeof(ROUTE_PREFIX)-1)

#define ROUTE_SUFFIX ">\r\n"
#define ROUTE_SUFFIX_LEN (sizeof(ROUTE_SUFFIX)-1)


/*
 * Parse the message and find first occurrence of
 * Route header field. The function returns -1 or -2 
 * on a parser error, 0 if there is a Route HF and
 * 1 if there is no Route HF.
 */
static inline int find_first_route(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_ROUTE, 0) == -1) {
		LOG(L_ERR, "find_first_route(): Error while parsing headers\n");
		return -1;
	} else {
		if (_m->route) {
			if (parse_rr(_m->route) < 0) {
				LOG(L_ERR, "find_first_route(): Error while parsing Route HF\n");
				return -2;
			}
			return 0;
		} else {
			DBG("find_first_route(): No Route headers found\n");
			return 1;
		}
	}
}


/*
 * Remove route field given by _hdr and _r, if the route
 * field is not first in it's header field, previous route
 * URI in the same header must be given in _p
 * Returns 0 on success, negative number on failure
 */
static inline int rewrite_RURI(struct sip_msg* _m, str* _s)
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
 * Find out if a URI contains r2 parameter which indicates
 * that we put 2 record routes
 */
static inline int is_2rr(str* _params)
{
	str s;
	int i, state = 0;

	if (_params->len == 0) return 0;
	s = *_params;

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
			case '=':  return 1;
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
			case '=':  return 1;
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
 * Check if URI is myself
 */
#ifdef ENABLE_USER_CHECK
static inline int is_myself(str *_user, str* _host, unsigned short _port)
#else
static inline int is_myself(str* _host, unsigned short _port)
#endif
{
	int ret;
	
	ret = check_self(_host, _port ? _port : SIP_PORT, 0);/* match all protos*/
	if (ret < 0) return 0;

#ifdef ENABLE_USER_CHECK
	if(i_user.len && i_user.len==_user->len
			&& !strncmp(i_user.s, _user->s, _user->len))
	{
		DBG("RR:is_myself: this URI isn't mine\n");
		return -1;
	}
#endif
	
	return ret;
}


/*
 * Find and parse next Route header field
 */
static inline int find_next_route(struct sip_msg* _m, struct hdr_field** _hdr)
{
	struct hdr_field* ptr;

	ptr = (*_hdr)->next;

	     /* Try to find already parsed Route headers */
	while(ptr) {
		if (ptr->type == HDR_ROUTE) goto found;
		ptr = ptr->next;
	}

	     /* There are no already parsed Route headers, try to find next
	      * occurrence of Route header
	      */
	if (parse_headers(_m, HDR_ROUTE, 1) == -1) {
		LOG(L_ERR, "fnr(): Error while parsing headers\n");
		return -1;
	}

	if ((_m->last_header->type != HDR_ROUTE) || (_m->last_header == *_hdr)) {
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
			case '=':  return 0;
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
			case '=':  return 0;
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
	anchor = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "save_ruri(): Can't get anchor\n");
		return -2;
	}

	     /* Create buffer for new lump */
	len = ROUTE_PREFIX_LEN + _m->first_line.u.request.uri.len + ROUTE_SUFFIX_LEN;
	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "save_ruri(): No memory left\n");
		return -3;
	}

	     /* Create new header field */
	memcpy(s, ROUTE_PREFIX, ROUTE_PREFIX_LEN);
	memcpy(s + ROUTE_PREFIX_LEN, _m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len);
	memcpy(s + ROUTE_PREFIX_LEN + _m->first_line.u.request.uri.len, ROUTE_SUFFIX, ROUTE_SUFFIX_LEN);

	DBG("save_ruri(): New header: '%.*s'\n", len, ZSW(s));

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
static inline int handle_strict_router(struct sip_msg* _m, struct hdr_field* _hdr, rr_t* _r)
{
	str* uri;
	char* rem_off;
	int rem_len;

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

	if (!_r->next) {
		rem_off = _hdr->name.s;
		rem_len = _hdr->len;
	} else {
		rem_off = _hdr->body.s;
		rem_len = _r->next->nameaddr.name.s - _hdr->body.s;
	}

	if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
		LOG(L_ERR, "hsr(): Can't remove Route HF\n");
		return -9;
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

	ptr = _m->route;
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
 * Previous hop was a strict router, handle this case
 */
static inline int route_after_strict(struct sip_msg* _m, struct sip_uri* _ruri)
{
	int res, rem_len;
	struct hdr_field* hdr;
	struct sip_uri puri;
	rr_t* rt, *prev;
	char* rem_off;
	str* uri;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = &rt->nameaddr.uri;

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "ral(): Error while parsing the first route URI\n");
		return -1;
	}

#ifdef ENABLE_USER_CHECK
	if (is_myself(&puri.user, &puri.host, puri.port_no))
#else
	if (is_myself(&puri.host, puri.port_no))
#endif
	{
		     /*	if (enable_double_rr && is_2rr(&_ruri->params)) { */ 
	      /* DBG("ras(): Removing 2nd URI of mine: '%.*s'\n", rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s)); */
 		if (!rt->next) {
			     /* No next route in the same header, remove the whole header
			      * field immediately
			      */
			if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
				LOG(L_ERR, "ras(): Can't remove Route HF\n");
				return -1;
			}
			res = find_next_route(_m, &hdr);
			if (res < 0) {
				LOG(L_ERR, "ras(): Error while finding next route\n");
				return -2;
			}
			if (res > 0) { /* No next route found */
				DBG("ras(): No next URI found\n");
				return 1;
			}
			rt = (rr_t*)hdr->parsed;
		} else rt = rt->next;
	}

	uri = &rt->nameaddr.uri;
	if (parse_uri(uri->s, uri->len, &puri) == -1) {
		LOG(L_ERR, "ras(): Error while parsing URI\n");
		return -1;
	}

	if (is_strict(&puri.params)) {
		DBG("ras(): Next hop: '%.*s' is strict router\n", uri->len, ZSW(uri->s));
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
		
		if (rt->next) {
			rem_off = hdr->body.s;
			rem_len = rt->next->nameaddr.name.s - hdr->body.s;
		} else {
			rem_off = hdr->name.s;
			rem_len = hdr->len;
		}
		if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
			LOG(L_ERR, "ras(): Can't remove Route HF\n");
			return -5;
		}
	} else {
		DBG("ras(): Next hop: '%.*s' is loose router\n", uri->len, ZSW(uri->s));

		if (set_dst_uri(_m, uri) < 0) {
			LOG(L_ERR, "ras(): Error while setting dst_uri\n");
			return -5;
		}

		     /* Next hop is a loose router - Which means that is is not endpoint yet
		      * In This case we have to recover from previous strict routing, that means we have
		      * to find the last Route URI and put in in R-URI and remove the last Route URI.
		      */
		if (rt != hdr->parsed) {
			     /* There is a previous route uri which was 2nd uri of mine
			      * and must be removed here
			      */
			rem_off = hdr->body.s;
			rem_len = rt->nameaddr.name.s - hdr->body.s;
			if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
				LOG(L_ERR, "ras(): Can't remove Route HF\n");
				return -6;
			}			
		}


		res = find_last_route(_m, &hdr, &rt, &prev);
		if (res < 0) {
			LOG(L_ERR, "ras(): Error while looking for last Route URI\n");
			return -7;
		} else if (res > 0) {
			return 0;
		}

		uri = &rt->nameaddr.uri;
		if (rewrite_RURI(_m, uri) < 0) {
			LOG(L_ERR, "ras(): Can't rewrite R-URI\n");
			return -8;
		}
		
		     /* The first character if uri will be either '<' when it is the only URI in a
		      * Route header field or ',' if there is more than one URI in the header field
		      */
		DBG("ras(): The last route URI: '%.*s'\n", rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));

		if (prev) {
			rem_off = prev->nameaddr.name.s + prev->len;
			rem_len = rt->nameaddr.name.s + rt->len - rem_off;
		} else {
			rem_off = hdr->name.s;
			rem_len = hdr->len;
		}
		if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
			LOG(L_ERR, "ras(): Can't remove Route HF\n");
			return -9;
		}
	}
	
	return 0;
}


static inline int route_after_loose(struct sip_msg* _m)
{
	struct hdr_field* hdr;
	struct sip_uri puri;
	rr_t* rt;
	int res;	
#ifdef ENABLE_USER_CHECK
	int ret;
#endif
	str* uri;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = &rt->nameaddr.uri;

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "ral(): Error while parsing the first route URI\n");
		return -1;
	}

	     /* IF the URI was added by me, remove it */
#ifdef ENABLE_USER_CHECK
	ret=is_myself(&puri.user, &puri.host, puri.port_no);
	if (ret>0)
#else
	if (is_myself(&puri.host, puri.port_no))
#endif
	{
		DBG("ral(): Topmost route URI: '%.*s' is me\n", uri->len, ZSW(uri->s));
		if (!rt->next) {
			     /* No next route in the same header, remove the whole header
			      * field immediately
			      */
			if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
				LOG(L_ERR, "ral(): Can't remove Route HF\n");
				return -2;
			}
			res = find_next_route(_m, &hdr);
			if (res < 0) {
				LOG(L_ERR, "ral(): Error while finding next route\n");
				return -3;
			}
			if (res > 0) { /* No next route found */
				DBG("ral(): No next URI found\n");
				return 1;
			}
			rt = (rr_t*)hdr->parsed;
		} else rt = rt->next;
		
		if (enable_double_rr && is_2rr(&puri.params)) {
			if (!rt->next) {
				     /* No next route in the same header, remove the whole header
				      * field immediately
				      */
				if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
					LOG(L_ERR, "ral(): Can't remove Route HF\n");
					return -4;
				}
				res = find_next_route(_m, &hdr);
				if (res < 0) {
					LOG(L_ERR, "ral(): Error while finding next route\n");
					return -5;
				}
				if (res > 0) { /* No next route found */
					DBG("ral(): No next URI found\n");
					return 1;
				}
				rt = (rr_t*)hdr->parsed;
			} else rt = rt->next;
		}
		
		uri = &rt->nameaddr.uri;
		if (parse_uri(uri->s, uri->len, &puri) < 0) {
			LOG(L_ERR, "ral(): Error while parsing the first route URI\n");
			return -6;
		}
	} else {
#ifdef ENABLE_USER_CHECK
		/* check if it the ignored user */
		if(ret < 0)
			return 0;
#endif	
		DBG("ral(): Topmost URI is NOT myself\n");
	}
	
	DBG("ral(): URI to be processed: '%.*s'\n", uri->len, ZSW(uri->s));
	if (is_strict(&puri.params)) {
		DBG("ral(): Next URI is a strict router\n");
		if (handle_strict_router(_m, hdr, rt) < 0) {
			LOG(L_ERR, "ral(): Error while handling strict router\n");
			return -7;
		}
	} else {
		     /* Next hop is loose router */
		DBG("ral(): Next URI is a loose router\n");

		if (set_dst_uri(_m, uri) < 0) {
			LOG(L_ERR, "ral(): Error while setting dst_uri\n");
			return -7;
		}

		     /* There is a previous route uri which was 2nd uri of mine
		      * and must be removed here
		      */
		if (rt != hdr->parsed) {
			if (!del_lump(_m, hdr->body.s - _m->buf, rt->nameaddr.name.s - hdr->body.s, 0)) {
				LOG(L_ERR, "ral(): Can't remove Route HF\n");
				return -8;
			}			
		}
		
	}

	return 0;
}


/*
 * Do loose routing as defined in RFC3621
 */
int loose_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct sip_uri puri;
	int ret;

	if (find_first_route(_m) != 0) {
		DBG("loose_route(): There is no Route HF\n");
		return -1;
	}
		
	if (parse_uri(_m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len, &puri) < 0) {
		LOG(L_ERR, "loose_route(): Error while parsing Request URI\n");
		return -1;
	}

#ifdef ENABLE_USER_CHECK
	if (is_myself(&puri.user, &puri.host, puri.port_no))
#else
	if (is_myself(&puri.host, puri.port_no))
#endif
	{
		ret = route_after_strict(_m, &puri);
		if (ret < 0) {
			LOG(L_ERR, "loose_route(): Error in route_after_strict\n");
			return -1;
		}
	} else {
		ret = route_after_loose(_m);
		if (ret < 0) {
			LOG(L_ERR, "loose_route(): Error in route_after_loose\n");
			return -1;
		}
	}

	if (ret) return -1;
	else return 1;
}

/*
 * Route & Record-Route module, loose routing support
 *
 * $Id$
 *
 * Copyright (C) 2001-2004 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2006-01-09 store user part of Route URI in AVP (if required) (mma)
 * 2009-05-26 Force the send socket if two RR headers were added (Miklos)
 */


#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../forward.h"
#include "../../data_lump.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../dset.h"
#include "../../socket_info.h"
#include "loose.h"
#include "rr_mod.h"
#include "avp_cookie.h"

#define RR_ERROR -1       /* An error occured while processing route set */
#define RR_DRIVEN 1       /* The next hop is determined from the route set */
#define NOT_RR_DRIVEN -1  /* The next hop is not determined from the route set */

#define MY_RT_PREFIX ROUTE_PREFIX "<"
#define MY_RT_PREFIX_LEN (sizeof(MY_RT_PREFIX)-1)

#define MY_RT_SUFFIX ">\r\n"
#define MY_RT_SUFFIX_LEN (sizeof(MY_RT_SUFFIX)-1)


/*
 * Check if the To-tag is set in the message
 */
static int has_to_tag(struct sip_msg* msg)
{
	str tag;

	if (!msg->to && parse_headers(msg, HDR_TO_F, 0) == -1) {
		LOG(L_ERR, "has_to_tag: Cannot parse To header field\n");
		return -1;
	}

	if (!msg->to) {
		LOG(L_ERR, "has_to_tag: To header field not found\n");
		return -1;
	}

	tag = get_to(msg)->tag_value;
	if (tag.s == 0 || tag.len == 0) {
		DBG("has_to_tag: No\n");
		return 0;
	}

	DBG("has_to_tag: Yes\n");
	return 1;
}


/*
 * Parse the message and find first occurrence of
 * Route header field. The function returns -1 or -2
 * on a parser error, 0 if there is a Route HF and
 * 1 if there is no Route HF.
 */
static inline int find_first_route(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_ROUTE_F, 0) == -1) {
		LOG(L_ERR, "find_first_route: Error while parsing headers\n");
		return -1;
	} else {
		if (_m->route) {
			if (parse_rr(_m->route) < 0) {
				LOG(L_ERR, "find_first_route: Error while parsing Route HF\n");
				return -2;
			}
			return 0;
		} else {
			DBG("find_first_route: No Route headers found\n");
			return 1;
		}
	}
}


/*
 * Check if URI is myself
 */
static inline int is_myself(str* _host, unsigned short _port)
{
    str did;
    int ret;
    
    ret = check_self(_host, _port ? _port : SIP_PORT, 0);/* match all protos*/
    if (ret < 0) return 0;
    
    if (ret == 0 && dm_get_did) {
	ret = dm_get_did(&did, _host);
	if (ret < 0) return 0;
	else if (ret > 0) {
	    /* as the domain module does not know anything about ports
	       lets check if the port matches any of our listening ports */
	    ret = check_self_port(_port ? _port : SIP_PORT, 0);
	}
    }

    return ret;
}

static inline void store_user_in_avps(str* user)
{
	avp_value_t val;

	if (user_part_avp_ident.name.s.s && user_part_avp_ident.name.s.len && user && user->s && user->len) {
		val.s = *user;
		add_avp(user_part_avp_ident.flags | AVP_NAME_STR | AVP_VAL_STR, user_part_avp_ident.name, val);
	}	
	
}


static inline void store_next_route_in_avps(str* uri)
{
	avp_value_t val;

	if (next_route_avp_ident.name.s.s && next_route_avp_ident.name.s.len && uri && uri->s && uri->len) {
		val.s = *uri;
		add_avp(next_route_avp_ident.flags | AVP_NAME_STR | AVP_VAL_STR, next_route_avp_ident.name, val);
	}	
	
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
		if (ptr->type == HDR_ROUTE_T) goto found;
		ptr = ptr->next;
	}

	     /* There are no already parsed Route headers, try to find next
	      * occurrence of Route header
	      */
	if (parse_headers(_m, HDR_ROUTE_F, 1) == -1) {
		LOG(L_ERR, "find_next_route: Error while parsing headers\n");
		return -1;
	}

	if ((_m->last_header->type != HDR_ROUTE_T) || (_m->last_header == *_hdr)) {
		DBG("find_next_route: No next Route HF found\n");
		return 1;
	}

	ptr = _m->last_header;

 found:
	if (parse_rr(ptr) < 0) {
		LOG(L_ERR, "find_next_route: Error while parsing Route body\n");
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

/* check direction using ftag */
static int get_direction(struct sip_msg* _m, str* _params) {
	str s, ftag;
	int i, state = 0;
	
	if (_params->len == 0) return 0;

	ftag.len = 0;
	ftag.s = 0;
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
					case 'f':
					case 'F': state = 1; break;
					default:  state = 8; break;
				}
				break;

			case 1:
				switch(s.s[i]) {
					case 't':
					case 'T': state = 2; break;
					default:  state = 8; break;
				}
				break;

			case 2:
				switch(s.s[i]) {
					case 'a':
					case 'A': state = 3; break;
					default:  state = 8; break;
				}
				break;

			case 3:
				switch(s.s[i]) {
					case 'g':
					case 'G': state = 4; break;
					default:  state = 8; break;
				}
				break;
			case 4:
				switch(s.s[i]) {
					case ';':  return 0;
					case '=':  state = 6; break;
					case ' ':
					case '\r':
					case '\n':
					case '\t': state = 5; break;
					default:   state = 8; break;
				}
				break;
			case 5:
				switch(s.s[i]) {
					case ';':  return 0;
					case '=':  state = 6; break;
					case ' ':
					case '\r':
					case '\n':
					case '\t': break;
					default:   state = 8; break;
				}
				break;

			case 6:
				switch(s.s[i]) {
					case '\"': state=101; break;
					case ';':  return 0;
					case ' ':
					case '\r':
					case '\n':
					case '\t': break;
					default:   state = 100; ftag.s = s.s+i; break;
				}
				break;

			case 8:
				switch(s.s[i]) {
					case '\"': state = 9; break;
					case ';':  state = 0; break;
					default:              break;
				}
				break;

			case 9:
				switch(s.s[i]) {
					case '\\': state = 10; break;
					case '\"': state = 8; break;
					default:              break;
				}
				break;

			case 10: state = 9;
				break;

			case 100:
				switch(s.s[i]) {
					case '\"':  return 0;
					case ';':
					case ' ':
					case '\r':
					case '\n':
					case '\t': ftag.len = s.s+i - ftag.s; 
						    i = s.len;
						    state = 0;
						    break;
				}
				break;

			case 101:	/* no escape chars supported in base64 algorithm */
				switch(s.s[i]) {
					case '\"': ftag.len = s.s+i - ftag.s; break;
					case '\\': state = 102; break;
					default:                break;
				}
				break;

			case 102:
				state = 101;
				break;
		}
	}

	if (state == 100)
		ftag.len = s.s+i - ftag.s;

	if (ftag.len) {
		if ((parse_from_header(_m)==0) && get_from(_m)) {
			/* compare if from.tag == ftag */
			if (ftag.len!=get_from(_m)->tag_value.len || strncmp(ftag.s, get_from(_m)->tag_value.s, ftag.len)) return 1;
		}
	}
	return 0;
}

static void get_avp_cookie_from_uri(str* _params, str *_avp_cookie) {
	str s;
	int i, state = 0;

	_avp_cookie->len = 0;
	_avp_cookie->s = 0;
	if (_params->len == 0) return;

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
					case 'a':
					case 'A': state = 1; break;
					default:  state = 7; break;
				}
				break;

			case 1:
				switch(s.s[i]) {
					case 'v':
					case 'V': state = 2; break;
					default:  state = 7; break;
				}
				break;

			case 2:
				switch(s.s[i]) {
					case 'p':
					case 'P': state = 3; break;
					default:  state = 7; break;
				}
				break;

			case 3:
				switch(s.s[i]) {
					case ';':  return;
					case '=':  state = 5; break;
					case ' ':
					case '\r':
					case '\n':
					case '\t': state = 4; break;
					default:   state = 7; break;
				}
				break;
			case 4:
				switch(s.s[i]) {
					case ';':  return;
					case '=':  state = 5; break;
					case ' ':
					case '\r':
					case '\n':
					case '\t': break;
					default:   state = 7; break;
				}
				break;

			case 5:
				switch(s.s[i]) {
					case '\"': state=101; break;
					case ';':  return;
					case ' ':
					case '\r':
					case '\n':
					case '\t': break;
					default:   state = 100; _avp_cookie->s = s.s+i; break;
				}
				break;

			case 7:
				switch(s.s[i]) {
					case '\"': state = 8; break;
					case ';':  state = 0; break;
					default:              break;
				}
				break;

			case 8:
				switch(s.s[i]) {
					case '\\': state = 9; break;
					case '\"': state = 7; break;
					default:              break;
				}
				break;

			case 9: state = 8;
				break;

			case 100:
				switch(s.s[i]) {
					case '\"':  return;
					case ';':
					case ' ':
					case '\r':
					case '\n':
					case '\t': _avp_cookie->len = s.s+i - _avp_cookie->s; 
						    i = s.len;
						    state = 0;
						    break;
				}
				break;

			case 101:	/* no escape chars supported in base64 algorithm */
				switch(s.s[i]) {
					case '\"': _avp_cookie->len = s.s+i - _avp_cookie->s; break;
					case '\\':  state = 102; break;
					default:                 break;
				}
				break;

			case 102:
				state = 101;
				break;
		}
	}

	if (state == 100)
		_avp_cookie->len = s.s+i - _avp_cookie->s;
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
	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR, "save_ruri: Error while parsing message\n");
		return -1;
	}

	     /* Create an anchor */
	anchor = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "save_ruri: Can't get anchor\n");
		return -2;
	}

	     /* Create buffer for new lump */
	len = MY_RT_PREFIX_LEN + _m->first_line.u.request.uri.len + MY_RT_SUFFIX_LEN;
	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "save_ruri: No memory left\n");
		return -3;
	}

	     /* Create new header field */
	memcpy(s, MY_RT_PREFIX, MY_RT_PREFIX_LEN);
	memcpy(s + MY_RT_PREFIX_LEN, _m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len);
	memcpy(s + MY_RT_PREFIX_LEN + _m->first_line.u.request.uri.len, MY_RT_SUFFIX, MY_RT_SUFFIX_LEN);

	DBG("save_ruri: New header: '%.*s'\n", len, ZSW(s));

	     /* Insert it */
	if (insert_new_lump_before(anchor, s, len, 0) == 0) {
		pkg_free(s);
		LOG(L_ERR, "save_ruri: Cannot insert lump\n");
		return -4;
	}

	return 0;
}


/*
 * Logic necessary to forward request to strict routers
 *
 * Returns 0 on success, negative number on an error
 */
static inline int handle_sr(struct sip_msg* _m, struct hdr_field* _hdr, rr_t* _r)
{
	str* uri;
	char* rem_off;
	int rem_len;

	uri = &_r->nameaddr.uri;

	     /* Next hop is strict router, save R-URI here */
	if (save_ruri(_m) < 0) {
		LOG(L_ERR, "handle_sr: Error while saving Request-URI\n");
		return -1;
	}

	     /* Put the first Route in Request-URI */
	if (rewrite_uri(_m, uri) < 0) {
		LOG(L_ERR, "handle_sr: Error while rewriting request URI\n");
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
		LOG(L_ERR, "handle_sr: Can't remove Route HF\n");
		return -9;
	}

	return 0;
}


/*
 * Find last route in the last Route header field,
 * if there was a previous route in the last Route header
 * field, it will be saved in _p parameter
 */
static inline int find_rem_target(struct sip_msg* _m, struct hdr_field** _h, rr_t** _l, rr_t** _p)
{
	struct hdr_field* ptr, *last;

	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR, "find_rem_target: Error while parsing message header\n");
		return -1;
	}

	ptr = _m->route;
	last = 0;

	while(ptr) {
		if (ptr->type == HDR_ROUTE_T) last = ptr;
		ptr = ptr->next;
	}

	if (last) {
		if (parse_rr(last) < 0) {
			LOG(L_ERR, "find_rem_target: Error while parsing last Route HF\n");
			return -2;
		}

		*_h = last;
		*_p = 0;
		*_l = (rr_t*)last->parsed;
		while ((*_l)->next) {
			*_p = *_l;
			*_l = (*_l)->next;
		}
		return 0;
	} else {
		LOG(L_ERR, "find_rem_target: Can't find last Route HF\n");
		return 1;
	}
}


/*
 * Previous hop was a strict router, handle this case
 */
static inline int after_strict(struct sip_msg* _m, struct sip_uri* _pru, int _route_myself)
{
	int res, rem_len;
	struct hdr_field* hdr;
	rr_t* rt, *prev;
	char* rem_off;
	str* uri;
	str avp_cookie;

	get_avp_cookie_from_uri(&_m->parsed_uri.params, &avp_cookie);
	if (avp_cookie.len > 0)
		rr_set_avp_cookies(&avp_cookie, get_direction(_m, &_m->parsed_uri.params));

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;

	if (_route_myself == 1) {
		store_user_in_avps(&(_pru->user));

		if (!rt->next) {
			     /* No next route in the same header, remove the whole header
			      * field immediately
			      */
			if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
				LOG(L_ERR, "after_strict: Cannot remove Route HF\n");
				return RR_ERROR;
			}
			res = find_next_route(_m, &hdr);
			if (res < 0) {
				LOG(L_ERR, "after_strict: Error while searching next route\n");
				return RR_ERROR;
			}
			if (res > 0) { /* No next route found */
				DBG("after_strict: No next URI found\n");
				return NOT_RR_DRIVEN;
			}
			rt = (rr_t*)hdr->parsed;
		} else rt = rt->next;
	}

	if (rt != _m->route->parsed) {
		uri = &rt->nameaddr.uri;
		if (parse_uri(uri->s, uri->len, _pru) == -1) {
			LOG(L_ERR, "after_strict: Error while parsing URI\n");
			return RR_ERROR;
		}
	} else {
		uri = &rt->nameaddr.uri;
	}

	store_next_route_in_avps(uri);
	if (is_strict(&(_pru->params))) {
		DBG("after_strict: Next hop: '%.*s' is strict router\n", uri->len, ZSW(uri->s));
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
		if (rewrite_uri(_m, uri) < 0) {
			LOG(L_ERR, "after_strict: Error while rewriting request URI\n");
			return RR_ERROR;
		}

		if (rt->next) {
			rem_off = hdr->body.s;
			rem_len = rt->next->nameaddr.name.s - hdr->body.s;
		} else {
			rem_off = hdr->name.s;
			rem_len = hdr->len;
		}
		if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
			LOG(L_ERR, "after_strict: Cannot remove Route HF\n");
			return RR_ERROR;
		}
	} else {
		DBG("after_strict: Next hop: '%.*s' is loose router\n", uri->len, ZSW(uri->s));

		if (set_dst_uri(_m, uri) < 0) {
			LOG(L_ERR, "after_strict: Error while setting dst_uri\n");
			return RR_ERROR;
		}

		     /* Next hop is a loose router - Which means that is is not endpoint yet
		      * In This case we have to recover from previous strict routing, that
		      * means we have to find the last Route URI and put in in R-URI and
		      * remove the last Route URI.
		      */
		if (rt != hdr->parsed) {
			     /* There is a previous route uri which was 2nd uri of mine
			      * and must be removed here
			      */
			rem_off = hdr->body.s;
			rem_len = rt->nameaddr.name.s - hdr->body.s;
			if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
				LOG(L_ERR, "after_strict: Can't remove Route HF\n");
				return RR_ERROR;
			}
		}


		res = find_rem_target(_m, &hdr, &rt, &prev);
		if (res < 0) {
			LOG(L_ERR, "after_strict: Error while looking for last Route URI\n");
			return RR_ERROR;
		} else if (res > 0) {
			     /* No remote target is an error */
			return RR_ERROR;
		}

		uri = &rt->nameaddr.uri;
		if (rewrite_uri(_m, uri) < 0) {
			LOG(L_ERR, "after_strict: Can't rewrite R-URI\n");
			return RR_ERROR;
		}

		     /* The first character if uri will be either '<' when it is the only URI in a
		      * Route header field or ',' if there is more than one URI in the header field
		      */
		DBG("after_strict: The last route URI: '%.*s'\n",
		    rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));

		if (prev) {
			rem_off = prev->nameaddr.name.s + prev->len;
			rem_len = rt->nameaddr.name.s + rt->len - rem_off;
		} else {
			rem_off = hdr->name.s;
			rem_len = hdr->len;
		}
		if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
			LOG(L_ERR, "after_strict: Can't remove Route HF\n");
			return RR_ERROR;
		}
	}

	return RR_DRIVEN;
}


static inline int after_loose(struct sip_msg* _m, struct sip_uri* _pru, int _route_myself, int _ruri_myself)
{
	struct hdr_field* hdr;
	rr_t* rt;
	int res;
#ifdef ENABLE_USER_CHECK
	int ret;
#endif
	str* uri;
	str avp_cookie;
	struct sip_uri parsed_uri;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = &rt->nameaddr.uri;

	     /* IF the URI was added by me, remove it */
	if (_route_myself == 1) {
		store_user_in_avps(&(_pru->user));

		DBG("after_loose: Topmost route URI: '%.*s' is me\n", uri->len, ZSW(uri->s));
		get_avp_cookie_from_uri(&(_pru->params), &avp_cookie);
		if (avp_cookie.len > 0)
			rr_set_avp_cookies(&avp_cookie, get_direction(_m, &(_pru->params)));
		if (!rt->next) {
			     /* No next route in the same header, remove the whole header
			      * field immediately
			      */
			if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
				LOG(L_ERR, "after_loose: Can't remove Route HF\n");
				return RR_ERROR;
			}
			res = find_next_route(_m, &hdr);
			if (res < 0) {
				LOG(L_ERR, "after_loose: Error while finding next route\n");
				return RR_ERROR;
			}
			if (res > 0) { /* No next route found */
				DBG("after_loose: No next URI found\n");
				if (_ruri_myself == 1) {
					/* this a preloaded request. we do not check for the To-tag
					 * because the ACK for a negative reply will contain such
					 * a tag but the original INVITE not */
					return NOT_RR_DRIVEN;
				} else {
					return RR_DRIVEN;
				}
			}
			rt = (rr_t*)hdr->parsed;
		} else rt = rt->next;

		if (enable_double_rr && is_2rr(&(_pru->params))) {
			if (rr_force_send_socket) {
				if (parse_uri(rt->nameaddr.uri.s, rt->nameaddr.uri.len,
						&parsed_uri) < 0
				) {
					LOG(L_ERR, "after_loose: Error while parsing the second route header\n");
					return RR_ERROR;
				}
				set_force_socket(_m, grep_sock_info(&parsed_uri.host,
													parsed_uri.port_no,
													parsed_uri.proto) );
				if (_m->force_send_socket == 0)
					LOG(L_WARN, "after_loose: send socket cannot be set"
						" based on the second route header\n");
					/* Do not return error because there is still a chance
					 * that the outgoing socket will be correct, especially
					 * if mhomed is turned on. It can happen that the Route HF
					 * contains a domain name as opposed to ip address therefore
					 * the outgoing socket cannot be determined (easily) from the URI.
					 * (Miklos)
					 */
			}
			if (!rt->next) {
				     /* No next route in the same header, remove the whole header
				      * field immediately
				      */
				if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
					LOG(L_ERR, "after_loose: Can't remove Route HF\n");
					return RR_ERROR;
				}
				res = find_next_route(_m, &hdr);
				if (res < 0) {
					LOG(L_ERR, "after_loose: Error while finding next route\n");
					return RR_ERROR;
				}
				if (res > 0) { /* No next route found */
					DBG("after_loose: No next URI found\n");
					/* preloaded routes can not happen with double_rr, so
					 * we were just the last hop with double_rr */
					return RR_DRIVEN;
				}
				rt = (rr_t*)hdr->parsed;
			} else rt = rt->next;
		}

		if(rt != _m->route->parsed) {
			uri = &rt->nameaddr.uri;
			if (parse_uri(uri->s, uri->len, _pru) < 0) {
				LOG(L_ERR, "after_loose: Error while parsing the next route URI\n");
				return RR_ERROR;
			}
		}
	} else {
#ifdef ENABLE_USER_CHECK
		/* check if it the ignored user */
		if(ret < 0)
			return NOT_RR_DRIVEN;
#endif
		DBG("after_loose: Topmost URI is NOT myself\n");
	}
	
	store_next_route_in_avps(uri);
	DBG("after_loose: URI to be processed: '%.*s'\n", uri->len, ZSW(uri->s));
	if (is_strict(&(_pru->params))) {
		DBG("after_loose: Next URI is a strict router\n");
		if (handle_sr(_m, hdr, rt) < 0) {
			LOG(L_ERR, "after_loose: Error while handling strict router\n");
			return RR_ERROR;
		}
	} else {
		     /* Next hop is loose router */
		DBG("after_loose: Next URI is a loose router\n");

		if (set_dst_uri(_m, uri) < 0) {
			LOG(L_ERR, "after_loose: Error while setting dst_uri\n");
			return RR_ERROR;
		}
		/* dst_uri changed, so it makes sense to re-use the current uri for
			forking */
		ruri_mark_new(); /* re-use uri for serial forking */

		     /* There is a previous route uri which was 2nd uri of mine
		      * and must be removed here
		      */
		if (rt != hdr->parsed) {
			if (!del_lump(_m, hdr->body.s - _m->buf, rt->nameaddr.name.s - hdr->body.s, 0)) {
				LOG(L_ERR, "after_loose: Can't remove Route HF\n");
				return RR_ERROR;
			}
		}
	}

	return RR_DRIVEN;
}


/*
 * Do loose routing as defined in RFC3621
 */
int loose_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct hdr_field* hdr;
	struct sip_uri puri;
	rr_t* rt;
	int ret;
	str* uri;

	if (find_first_route(_m) != 0) {
		DBG("loose_route: There is no Route HF\n");
		return -1;
	}

	if (parse_sip_msg_uri(_m) == -1) {
		LOG(L_ERR, "loose_route: Error while parsing Request URI\n");
		return -1;
	}

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = &rt->nameaddr.uri;

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "loose_route: Error while parsing the first route URI\n");
		return -1;
	}

	if (is_myself(&_m->parsed_uri.host, _m->parsed_uri.port_no)) {
		DBG("loose_route: RURI is myself\n");
		if ((ret = is_myself(&puri.host, puri.port_no)) == 1 &&
			!(enable_double_rr && is_2rr(&puri.params))) {
			DBG("loose_route: found preloaded loose route\n");
			return after_loose(_m, &puri, ret, 1);
		} else {
			if (has_to_tag(_m) == 1) {
				return after_strict(_m, &puri, ret);
			} else {
				LOG(L_WARN, "loose_route: pre-loaded strict routing?!\n");
				return -1;
			}
		}
	} else {
		DBG("loose_route: RURI is NOT myself\n");
		if (is_myself(&puri.host, puri.port_no)) {
			return after_loose(_m, &puri, 1, 0);
		} else {
			store_next_route_in_avps(uri);
			LOG(L_WARN, "loose_route: no routing target is local\n");
			return -1;
		}
	}
}

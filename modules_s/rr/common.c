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
 * 2003-04-02 Changed to use substituting lumps (janakj)
 */

#include "common.h"
#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../action.h"
#include "../../data_lump.h"
#include "../../globals.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_rr.h"
#include "rr_mod.h"


#define RR_PREFIX "Record-Route: <sip:"
#define RR_PREFIX_LEN (sizeof(RR_PREFIX)-1)
#define RR_LR_TERM ";lr>\r\n"
#define RR_LR_TERM_LEN (sizeof(RR_LR_TERM)-1)
#define RR_SR_TERM ">\r\n"
#define RR_SR_TERM_LEN (sizeof(RR_SR_TERM)-1)
#define RR_FROMTAG ";ftag="
#define RR_FROMTAG_LEN (sizeof(RR_FROMTAG)-1)
#define RR_TRANSPORT ";transport="
#define RR_TRANSPORT_LEN (sizeof(RR_TRANSPORT)-1)


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
 * Extract username from the Request URI
 * First try to look at the original Request URI and if there
 * is no username use the new Request URI
 */
static inline int get_username(struct sip_msg* _m, str* _user)
{
	struct sip_uri puri;

	     /* first try to look at r-uri for a username */
	if (parse_uri(_m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len, &puri) < 0) {
		LOG(L_ERR, "get_username(): Error while parsing R-URI\n");
		return -1;
	}

	/* no username in original uri -- hmm; maybe it is a uri
	 * with just host address and username is in a preloaded route,
	 * which is now no rewritten r-uri (assumed rewriteFromRoute
	 * was called somewhere in script's beginning) 
	 */
	if (!puri.user.len && _m->new_uri.s) {
		if (parse_uri(_m->new_uri.s, _m->new_uri.len, &puri) < 0) {
			LOG(L_ERR, "get_username(): Error while parsing new_uri\n");
			return -2;
		}

	}

	_user->s = puri.user.s;
	_user->len = puri.user.len;
	return 0;
}


/*
 * Insert inbound Record-Route
 */
static inline int ins_in_rr(struct sip_msg* _m, int _lr, str* user, str* tag)
{
	char* prefix, *suffix;
	int suffix_len;
	struct lump* l;

	l = anchor_lump(&_m->add_rm, _m->headers->name.s - _m->buf, 0, 0);
	if (!l) {
		LOG(L_ERR, "ins_in_rr(): Error while creating an anchor\n");
		return -2;
	}

	prefix = pkg_malloc(RR_PREFIX_LEN + user->len + 1);
	suffix_len = _lr ? RR_LR_TERM_LEN : RR_SR_TERM_LEN + (tag->len ? (RR_FROMTAG_LEN + tag->len) : 0);
	suffix = pkg_malloc(suffix_len);

	if (!prefix && !suffix) {
		LOG(L_ERR, "ins_in_rr(): No memory left\n");
		if (suffix) pkg_free(suffix);
		if (prefix) pkg_free(prefix);
		return -3;
	}

	memcpy(prefix, RR_PREFIX, RR_PREFIX_LEN);
	if (user->len) {
		memcpy(prefix + RR_PREFIX_LEN, user->s, user->len);
		prefix[RR_PREFIX_LEN + user->len] = '@';
	}

	if (tag->len) {
		memcpy(suffix, RR_FROMTAG, RR_FROMTAG_LEN);
		memcpy(suffix + RR_FROMTAG_LEN, tag->s, tag->len);
		memcpy(suffix + RR_FROMTAG_LEN + tag->len, _lr ? RR_LR_TERM : RR_SR_TERM, _lr ? RR_LR_TERM_LEN : RR_SR_TERM_LEN);
	} else {
		memcpy(suffix, _lr ? RR_LR_TERM : RR_SR_TERM, _lr ? RR_LR_TERM_LEN : RR_SR_TERM_LEN);
	}

	if (!(l = insert_new_lump_after(l, prefix, RR_PREFIX_LEN + (user->len ? (user->len + 1) : 0), 0))) goto lump_err;
	prefix = 0;
	if (!(l = insert_subst_lump_after(l, SUBST_RCV_ALL, 0))) goto lump_err;
	if (!(l = insert_new_lump_after(l, suffix, suffix_len, 0))) goto lump_err;

	return 0;

 lump_err:
	LOG(L_ERR, "insert_RR(): Error while inserting lumps\n");
	if (prefix) pkg_free(prefix);
	if (suffix) pkg_free(suffix);
	return -4;

}


/*
 * Insert outbound Record-Route if necessarry
 */
static inline int ins_out_rr(struct sip_msg* _m, int _lr)
{

	return 0;
}


/*
 * Insert a new Record-Route header field
 */
static inline int insert_RR(struct sip_msg* _m, int _lr)
{

	str user;
	struct to_body* from;
	
	from = 0; /* Makes gcc happy */
	user.len = 0;

	if (get_username(_m, &user) < 0) {
		LOG(L_ERR, "insert_RR(): Error while extracting username\n");
		return -1;
	}

	if (append_fromtag) {
		if (parse_from_header(_m) < 0) {
			LOG(L_ERR, "insert_RR: From parsing failed\n");
			return -1;
		}
		from = (struct to_body*)_m->from->parsed;
	}

	return ins_in_rr(_m, _lr, &user, &from->tag_value);

}


static inline int do_RR(struct sip_msg* _m, int _lr)
{
	static unsigned int last_rr_msg;

	if (_m->id == last_rr_msg) {
			LOG(L_ERR, "record_route(): Double attempt to record-route\n");
			return -1;
	}
	
	if (insert_RR(_m, _lr) < 0) {
		LOG(L_ERR, "record_route(): Error while inserting Record-Route line\n");
		return -3;
	}

	last_rr_msg=_m->id;	
	return 1;
}


/*
 * Insert new Record-Route header field with lr parameter
 */
int record_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	return do_RR(_m, 1);
}


/*
 * Insert a new Record_route header field with given IP address
 */
int record_route_ip(struct sip_msg* _m, char* _ip, char* _s2)
{
	return 1;
}


/*
 * Insert new Record-Route header field without lr parameter
 */
int record_route_strict(struct sip_msg* _m, char* _s1, char* _s2)
{
	return do_RR(_m, 0);
}

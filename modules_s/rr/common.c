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


#include <string.h>
#include "common.h"
#include "../../action.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"


/*
 * Parse the message and find first occurence of
 * Route header field. The function returns -1 or -2 
 * on a parser error, 0 if there is a Route HF and
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
 * Remove a Route URI
 * Returns 0 on success, negative number on failure
 */
int remove_route(struct sip_msg* _m, struct hdr_field* _hf, rr_t* _r, rr_t* _p)
{
	char* s, *e;

	if (!_p) {
		if (!_r->next) s = _hf->name.s;
		else s = _hf->body.s;
	} else {
		if (_p->params) s = _p->params->name.s + _p->params->len;
		else s = _p->nameaddr.name.s + _p->nameaddr.len;
	}

	if (_r->next) {
		if (_p) {
			if (_r->params) e = _r->params->name.s + _r->params->len;
			else e = _r->nameaddr.name.s + _r->nameaddr.len;
		} else e = _r->next->nameaddr.name.s;
	} else e = _hf->body.s + _hf->body.len;

     	if (del_lump(&_m->add_rm, s - _m->buf, e - s, 0) == 0) {
		LOG(L_ERR, "remove_route(): Can't remove Route HF\n");
		return -1;
	}
	return 0;
}

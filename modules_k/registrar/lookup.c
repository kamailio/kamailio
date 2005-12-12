/*
 * $Id$
 *
 * Lookup contacts in usrloc
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 * 2003-03-12 added support for zombie state (nils)
 */


#include <string.h>
#include "../../ut.h"
#include "../../dset.h"
#include "../../str.h"
#include "../../config.h"
#include "../../action.h"
#include "../usrloc/usrloc.h"
#include "common.h"
#include "regtime.h"
#include "reg_mod.h"
#include "lookup.h"


#define supported_method(_msg, _c) \
	( !method_filtering || ((_msg)->REQ_METHOD)&((_c)->methods) )


/*
 * Lookup contact in the database and rewrite Request-URI
 * Returns: -1 : not found
 *          -2 : found but method not supported
 *          -3 : error
 */
int lookup(struct sip_msg* _m, char* _t, char* _s)
{
	urecord_t* r;
	str aor, uri;
	ucontact_t* ptr;
	int res;
	int bflags;
	int ret;

	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
	
	if (extract_aor(&uri, &aor) < 0) {
		LOG(L_ERR, "lookup(): Error while extracting address of record\n");
		return -3;
	}
	
	get_act_time();

	ul.lock_udomain((udomain_t*)_t);
	res = ul.get_urecord((udomain_t*)_t, &aor, &r);
	if (res < 0) {
		LOG(L_ERR, "lookup(): Error while querying usrloc\n");
		ul.unlock_udomain((udomain_t*)_t);
		return -3;
	}
	
	if (res > 0) {
		DBG("lookup(): '%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
		ul.unlock_udomain((udomain_t*)_t);
		return -1;
	}

	ptr = r->contacts;
	ret = -1;
	/* look first first un-expired and suported contact */
	while ( (ptr) &&
	!(VALID_CONTACT(ptr,act_time) && (ret=-2) && supported_method(_m,ptr)))
		ptr = ptr->next;
	if (ptr==0) {
		/* nothing found */
		ul.unlock_udomain((udomain_t*)_t);
		return ret;
	}

	if (ptr) {
		if (rewrite_uri(_m, &ptr->c) < 0) {
			LOG(L_ERR, "lookup(): Unable to rewrite Request-URI\n");
			ul.unlock_udomain((udomain_t*)_t);
			return -3;
		}

		if (ptr->received.s && ptr->received.len) {
			if (set_dst_uri(_m, &ptr->received) < 0) {
				ul.unlock_udomain((udomain_t*)_t);
				return -3;
			}
		}

		set_ruri_q(ptr->q);

		/* for RURI branch, the nat flag goes into msg */
		if ( ptr->flags&FL_NAT )
			_m->flags |= nat_flag;

		if (ptr->sock)
			_m->force_send_socket = ptr->sock;

		ptr = ptr->next;
	}

	/* Append branches if enabled */
	if (!append_branches) goto skip;

	for( ; ptr ; ptr = ptr->next ) {
		if (VALID_CONTACT(ptr, act_time) && supported_method(_m, ptr)) {
			/* for additional branches, the nat flag goes into dset */
			bflags = (use_branch_flags && (ptr->flags & FL_NAT))?nat_flag:0;
			if (append_branch(_m, &ptr->c, &ptr->received, ptr->q,
			bflags, ptr->sock) == -1) {
				LOG(L_ERR, "lookup(): Error while appending a branch\n");
				/* Return 1 here so the function succeeds even if
				 * appending of a branch failed */
				/* Also give a chance to the next branches*/
				continue;
			}
			if (!use_branch_flags && (ptr->flags & FL_NAT))
				_m->flags |= nat_flag;
		}
	}

skip:
	ul.unlock_udomain((udomain_t*)_t);
	return 1;
}


/*
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered(struct sip_msg* _m, char* _t, char* _s)
{
	str uri, aor;
	urecord_t* r;
	ucontact_t* ptr;
	int res;

	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
	
	if (extract_aor(&uri, &aor) < 0) {
		LOG(L_ERR, "registered(): Error while extracting address of record\n");
		return -1;
	}
	
	ul.lock_udomain((udomain_t*)_t);
	res = ul.get_urecord((udomain_t*)_t, &aor, &r);

	if (res < 0) {
		ul.unlock_udomain((udomain_t*)_t);
		LOG(L_ERR, "registered(): Error while querying usrloc\n");
		return -1;
	}

	if (res == 0) {
		ptr = r->contacts;
		while (ptr && !VALID_CONTACT(ptr, act_time)) {
			ptr = ptr->next;
		}

		if (ptr) {
			ul.unlock_udomain((udomain_t*)_t);
			DBG("registered(): '%.*s' found in usrloc\n", aor.len, ZSW(aor.s));
			return 1;
		}
	}

	ul.unlock_udomain((udomain_t*)_t);
	DBG("registered(): '%.*s' not found in usrloc\n", aor.len, ZSW(aor.s));
	return -1;
}

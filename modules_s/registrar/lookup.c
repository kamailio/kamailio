/*
 * $Id$
 *
 * Lookup contacts in usrloc
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


/*
 * Rewrite Request-URI
 */
static inline int rewrite(struct sip_msg* _m, str* _s)
{
	char* buf;

	buf = (char*)pkg_malloc(_s->len + 1);
	if (!buf) {
		LOG(L_ERR, "rewrite(): No memory left\n");
		return -1;
	}

	memcpy(buf, _s->s, _s->len);
	_s->s[_s->len] = '\0';

	_m->parsed_uri_ok = 0;
	if (_m->new_uri.s) {
		pkg_free(_m->new_uri.s);
	}

	_m->new_uri.s = buf;
	_m->new_uri.len = _s->len;

	DBG("rewrite(): Rewriting Request-URI with '%.*s'\n", _s->len, buf);
	return 0;
}


/*
 * Lookup contact in the database and rewrite Request-URI
 */
int lookup(struct sip_msg* _m, char* _t, char* _s)
{
	urecord_t* r;
	str aor, uri;
	ucontact_t* ptr;
	int res;
	unsigned int nat;

	nat = 0;
	
	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
	
	if (extract_aor(&uri, &aor) < 0) {
		LOG(L_ERR, "lookup(): Error while extracting address of record\n");
		return -1;
	}
	
	get_act_time();

	ul.lock_udomain((udomain_t*)_t);
	res = ul.get_urecord((udomain_t*)_t, &aor, &r);
	if (res < 0) {
		LOG(L_ERR, "lookup(): Error while querying usrloc\n");
		ul.unlock_udomain((udomain_t*)_t);
		return -2;
	}
	
	if (res > 0) {
		DBG("lookup(): '%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
		ul.unlock_udomain((udomain_t*)_t);
		return -3;
	}

	ptr = r->contacts;
	while ((ptr) && ((ptr->expires <= act_time) || 
			(ptr->state >= CS_ZOMBIE_N)))
		ptr = ptr->next;
	
	if (ptr) {
		if (rewrite(_m, &ptr->c) < 0) {
			LOG(L_ERR, "lookup(): Unable to rewrite Request-URI\n");
			ul.unlock_udomain((udomain_t*)_t);
			return -4;
		}
		nat |= ptr->flags & FL_NAT;
		ptr = ptr->next;
	} else {
		     /* All contacts expired */
		ul.unlock_udomain((udomain_t*)_t);
		return -5;
	}
	
	     /* Append branches if enabled */
	if (!append_branches) goto skip;

	while(ptr) {
		if (ptr->expires > act_time && (ptr->state < CS_ZOMBIE_N)) {
			if (append_branch(_m, ptr->c.s, ptr->c.len) == -1) {
				LOG(L_ERR, "lookup(): Error while appending a branch\n");
				     /* Return 1 here so the function succeeds even if appending of
				      * a branch failed
				      */
				goto skip; 
			} 
			
			nat |= ptr->flags & FL_NAT; 
		} 
		ptr = ptr->next; 
	}
	
 skip:
	ul.unlock_udomain((udomain_t*)_t);
	if (nat) setflag(_m, nat_flag);
	return 1;
}

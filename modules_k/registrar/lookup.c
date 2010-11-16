/*
 * $Id$
 *
 * Lookup contacts in usrloc
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 * 2003-03-12 added support for zombie state (nils)
 */
/*!
 * \file
 * \brief SIP registrar module - lookup contacts in usrloc
 * \ingroup registrar  
 */  


#include <string.h>
#include "../../ut.h"
#include "../../dset.h"
#include "../../str.h"
#include "../../config.h"
#include "../../action.h"
#include "../../parser/parse_rr.h"
#include "../usrloc/usrloc.h"
#include "../../lib/kcore/parser_helpers.h"
#include "common.h"
#include "regtime.h"
#include "reg_mod.h"
#include "lookup.h"
#include "config.h"

#define allowed_method(_msg, _c) \
	( !method_filtering || ((_msg)->REQ_METHOD)&((_c)->methods) )


/*! \brief
 * Lookup contact in the database and rewrite Request-URI
 * \return: -1 : not found
 *          -2 : found but method not allowed
 *          -3 : error
 */
int lookup(struct sip_msg* _m, udomain_t* _d)
{
	urecord_t* r;
	str aor, uri;
	ucontact_t* ptr;
	int res;
	int ret;
	str path_dst;
	flag_t old_bflags;


	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
	
	if (extract_aor(&uri, &aor) < 0) {
		LM_ERR("failed to extract address of record\n");
		return -3;
	}
	
	get_act_time();

	ul.lock_udomain(_d, &aor);
	res = ul.get_urecord(_d, &aor, &r);
	if (res > 0) {
		LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
		ul.unlock_udomain(_d, &aor);
		return -1;
	}

	ptr = r->contacts;
	ret = -1;
	/* look first for an un-expired and suported contact */
	while ( (ptr) &&
	!(VALID_CONTACT(ptr,act_time) && (ret=-2) && allowed_method(_m,ptr)))
		ptr = ptr->next;
	if (ptr==0) {
		/* nothing found */
		goto done;
	}

	ret = 1;
	if (ptr) {
		if (rewrite_uri(_m, &ptr->c) < 0) {
			LM_ERR("unable to rewrite Request-URI\n");
			ret = -3;
			goto done;
		}

		/* reset next hop address */
		reset_dst_uri(_m);

		/* If a Path is present, use first path-uri in favour of
		 * received-uri because in that case the last hop towards the uac
		 * has to handle NAT. - agranig */
		if (ptr->path.s && ptr->path.len) {
			if (get_path_dst_uri(&ptr->path, &path_dst) < 0) {
				LM_ERR("failed to get dst_uri for Path\n");
				ret = -3;
				goto done;
			}
			if (set_path_vector(_m, &ptr->path) < 0) {
				LM_ERR("failed to set path vector\n");
				ret = -3;
				goto done;
			}
			if (set_dst_uri(_m, &path_dst) < 0) {
				LM_ERR("failed to set dst_uri of Path\n");
				ret = -3;
				goto done;
			}
		} else if (ptr->received.s && ptr->received.len) {
			if (set_dst_uri(_m, &ptr->received) < 0) {
				ret = -3;
				goto done;
			}
		}

		set_ruri_q(ptr->q);

		old_bflags = 0;
		getbflagsval(0, &old_bflags);
		setbflagsval(0, old_bflags|ptr->cflags);

		if (ptr->sock)
			set_force_socket(_m, ptr->sock);

		ptr = ptr->next;
	}

	/* Append branches if enabled */
	if (!cfg_get(registrar, registrar_cfg, append_branches)) goto done;

	for( ; ptr ; ptr = ptr->next ) {
		if (VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
			path_dst.len = 0;
			if(ptr->path.s && ptr->path.len 
			&& get_path_dst_uri(&ptr->path, &path_dst) < 0) {
				LM_ERR("failed to get dst_uri for Path\n");
				continue;
			}

			/* The same as for the first contact applies for branches 
			 * regarding path vs. received. */
			if (km_append_branch(_m,&ptr->c,path_dst.len?&path_dst:&ptr->received,
			&ptr->path, ptr->q, ptr->cflags, ptr->sock) == -1) {
				LM_ERR("failed to append a branch\n");
				/* Also give a chance to the next branches*/
				continue;
			}
		}
	}

done:
	ul.release_urecord(r);
	ul.unlock_udomain(_d, &aor);
	return ret;
}


/*! \brief the is_registered() function
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
	int_str match_callid=(int_str)0;

	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
	
	if (extract_aor(&uri, &aor) < 0) {
		LM_ERR("failed to extract address of record\n");
		return -1;
	}
	
	ul.lock_udomain((udomain_t*)_t, &aor);
	res = ul.get_urecord((udomain_t*)_t, &aor, &r);

	if (res < 0) {
		ul.unlock_udomain((udomain_t*)_t, &aor);
		LM_ERR("failed to query usrloc\n");
		return -1;
	}

	if (res == 0) {
		
		if (reg_callid_avp_name.n) {
			struct usr_avp *avp =
				search_first_avp( reg_callid_avp_type, reg_callid_avp_name, &match_callid, 0);
			if (!(avp && is_avp_str_val(avp)))
				match_callid.n = 0;
				match_callid.s.s = NULL;
		} else {
			match_callid.n = 0;
			match_callid.s.s = NULL;
		}

		for (ptr = r->contacts; ptr; ptr = ptr->next) {
			if(!VALID_CONTACT(ptr, act_time)) continue;
			if (match_callid.s.s && /* optionally enforce tighter matching w/ Call-ID */
				memcmp(match_callid.s.s,ptr->callid.s,match_callid.s.len))
				continue;
			ul.release_urecord(r);
			ul.unlock_udomain((udomain_t*)_t, &aor);
			LM_DBG("'%.*s' found in usrloc\n", aor.len, ZSW(aor.s));
			return 1;
		}
	}

	ul.unlock_udomain((udomain_t*)_t, &aor);
	LM_DBG("'%.*s' not found in usrloc\n", aor.len, ZSW(aor.s));
	return -1;
}

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../ims_usrloc_scscf/usrloc.h"
#include "../../lib/ims/ims_getters.h"
#include "common.h"
#include "regtime.h"
#include "reg_mod.h"
#include "lookup.h"
#include "config.h"

#include "save.h"

#define allowed_method(_msg, _c) \
	( !method_filtering || ((_msg)->REQ_METHOD)&((_c)->methods) )

/*! \brief
 * Lookup contact in the database and rewrite Request-URI
 * \return: -1 : not found
 *          -2 : found but method not allowed
 *          -3 : error
 */
int lookup(struct sip_msg* _m, udomain_t* _d) {
    impurecord_t* r;
    str aor;
    ucontact_t* ptr;
    int res;
    int ret;
    str path_dst;
    flag_t old_bflags;
    int i = 0;

	if (!_m){
		LM_ERR("NULL message!!!\n");
		return -1;
	}

	if (_m->new_uri.s) aor = _m->new_uri;
	else aor = _m->first_line.u.request.uri;
	
	for(i=0;i<aor.len;i++)
		if (aor.s[i]==';' || aor.s[i]=='?') {
			aor.len = i;
			break;
		}

	LM_DBG("Looking for <%.*s>\n",aor.len,aor.s);

    get_act_time();

    ul.lock_udomain(_d, &aor);
    res = ul.get_impurecord(_d, &aor, &r);
    if (res > 0) {
	LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
	ul.unlock_udomain(_d, &aor);
	return -1;
    }
    ret = -1;
    i = 0;

    while (i < MAX_CONTACTS_PER_IMPU && (ptr = r->newcontacts[i])) {
	if (VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
	    LM_DBG("Found a valid contact [%.*s]\n", ptr->c.len, ptr->c.s);
	    i++;
	    break;
	}
	i++;
    }

    /* look first for an un-expired and supported contact */
    if (ptr == 0) {
	LM_INFO("No contacts founds for IMPU <%.*s>\n",aor.len,aor.s);
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
		return -1;
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
	setbflagsval(0, old_bflags | ptr->cflags);

	if (ptr->sock)
	    set_force_socket(_m, ptr->sock);

	ptr = ptr->next;
    }

    /* Append branches if enabled */
    if (!cfg_get(registrar, registrar_cfg, append_branches)) goto done;

    //the last i was the first valid contact we found - let's go through the rest of valid contacts and append the branches.
    while (i < MAX_CONTACTS_PER_IMPU && (ptr = r->newcontacts[i])) {
	if (VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
	    path_dst.len = 0;
	    if (ptr->path.s && ptr->path.len
		    && get_path_dst_uri(&ptr->path, &path_dst) < 0) {
		LM_ERR("failed to get dst_uri for Path\n");
		continue;
	    }

	    /* The same as for the first contact applies for branches
	     * regarding path vs. received. */
	    if (km_append_branch(_m, &ptr->c, path_dst.len ? &path_dst : &ptr->received,
		    &ptr->path, ptr->q, ptr->cflags, ptr->sock) == -1) {
		LM_ERR("failed to append a branch\n");
		/* Also give a chance to the next branches*/
		continue;
	    }
	}
	i++;
    }

done:
    ul.unlock_udomain(_d, &aor);
    return ret;
}

int lookup_path_to_contact(struct sip_msg* _m, char* contact_uri) {
    ucontact_t* contact;
    str s_contact_uri;
    str path_dst;
    
    if (get_str_fparam(&s_contact_uri, _m, (fparam_t*) contact_uri) < 0) {
	    LM_ERR("failed to get RURI\n");
	    return -1;
    }
    LM_DBG("Looking up contact [%.*s]\n", s_contact_uri.len, s_contact_uri.s);

    if (ul.get_ucontact(NULL, &s_contact_uri, 0, 0, 0, &contact) == 0) { //get_contact returns with lock
	LM_DBG("CONTACT FOUND and path is [%.*s]\n", contact->path.len, contact->path.s);
	
	if (get_path_dst_uri(&contact->path, &path_dst) < 0) {
	    LM_ERR("failed to get dst_uri for Path\n");
	    ul.release_ucontact(contact);
	    return -1;
	}
	if (set_path_vector(_m, &contact->path) < 0) {
	    LM_ERR("failed to set path vector\n");
	    ul.release_ucontact(contact);
	    return -1;
	}
	if (set_dst_uri(_m, &path_dst) < 0) {
	    LM_ERR("failed to set dst_uri of Path\n");
	    ul.release_ucontact(contact);
	    return -1;
	}
	
	ul.release_ucontact(contact);
	return 1;
    }

    LM_DBG("no contact found for [%.*s]\n", s_contact_uri.len, s_contact_uri.s);
    return -1;
}

/*! \brief the impu_registered() function
 * Return true if the AOR in the To Header is registered
 */
int impu_registered(struct sip_msg* _m, char* _t, char* _s)
{
	impurecord_t* r;
	int res, ret=-1;

	str impu;
	impu = cscf_get_public_identity(_m);

	LM_DBG("Looking for IMPU <%.*s>\n", impu.len, impu.s);

	ul.lock_udomain((udomain_t*)_t, &impu);
	res = ul.get_impurecord((udomain_t*)_t, &impu, &r);

	if (res < 0) {
		ul.unlock_udomain((udomain_t*)_t, &impu);
		LM_ERR("failed to query usrloc for IMPU <%.*s>\n", impu.len, impu.s);
		return ret;
	}

	if (res == 0) {
		if (r->reg_state == IMPU_REGISTERED ) ret = 1;
		ul.unlock_udomain((udomain_t*) _t, &impu);
		LM_DBG("'%.*s' found in usrloc\n", impu.len, ZSW(impu.s));
		return ret;
	}

	ul.unlock_udomain((udomain_t*)_t, &impu);
	LM_DBG("'%.*s' not found in usrloc\n", impu.len, ZSW(impu.s));
	return ret;
}

/**
 * Check that the IMPU at the Term S has at least one valid contact...
 * @param _m - msg
 * @param _t - t
 * @param _s - s
 * @return true if there is at least one valid contact. false if not
 */
int term_impu_has_contact(struct sip_msg* _m, udomain_t* _d, char* _s) 
{
    impurecord_t* r;
    str aor, uri;
    ucontact_t* ptr;
    int res;
    int ret;
    int i = 0;

    if (_m->new_uri.s) uri = _m->new_uri;
    else uri = _m->first_line.u.request.uri;

    if (extract_aor(&uri, &aor) < 0) {
	LM_ERR("failed to extract address of record\n");
	return -3;
    }

    get_act_time();

    ul.lock_udomain(_d, &aor);
    res = ul.get_impurecord(_d, &aor, &r);
    if (res > 0) {
	LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
	ul.unlock_udomain(_d, &aor);
	return -1;
    }

    while (i < MAX_CONTACTS_PER_IMPU && (ptr = r->newcontacts[i])) {
	if (VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
	    LM_DBG("Found a valid contact [%.*s]\n", ptr->c.len, ptr->c.s);
	    i++;
	    ret = 1;
	    break;
	}
	i++;
    }

    /* look first for an un-expired and supported contact */
    if (ptr == 0) {
	/* nothing found */
	ret = -1;
    } 

    ul.unlock_udomain(_d, &aor);

    return ret;
}
/*! \brief the term_impu_registered() function
 * Return true if the AOR in the Request-URI  for the terminating user is registered
 */
int term_impu_registered(struct sip_msg* _m, char* _t, char* _s)
{
	//str uri, aor;
	struct sip_msg *req;	
	int i;
	str uri;
	impurecord_t* r;
	int res;

//	if (_m->new_uri.s) uri = _m->new_uri;
//	else uri = _m->first_line.u.request.uri;
//
//	if (extract_aor(&uri, &aor) < 0) {
//		LM_ERR("failed to extract address of record\n");
//		return -1;
//	}
	
	req = _m;	
	if (!req){
		LM_ERR(":term_impu_registered: NULL message!!!\n");
		return -1;
	}
 	if (req->first_line.type!=SIP_REQUEST){
 		req = get_request_from_reply(req);
 	}
	
	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
		
	for(i=0;i<uri.len;i++)
		if (uri.s[i]==';' || uri.s[i]=='?' || (i>3 /*sip:*/ && uri.s[i]==':' /*strip port*/)) {
			uri.len = i;
			break;
		}
	LM_DBG("term_impu_registered: Looking for <%.*s>\n",uri.len,uri.s);

	ul.lock_udomain((udomain_t*)_t, &uri);
	res = ul.get_impurecord((udomain_t*)_t, &uri, &r);

	if (res < 0) {
		ul.unlock_udomain((udomain_t*)_t, &uri);
		LM_ERR("failed to query for terminating IMPU <%.*s>\n", uri.len, uri.s);
		return -1;
	}

	if (res == 0) {
		//ul.release_impurecord(r);
		ul.unlock_udomain((udomain_t*) _t, &uri);
		LM_DBG("'%.*s' found in usrloc\n", uri.len, ZSW(uri.s));
		return 1;
	}

	ul.unlock_udomain((udomain_t*)_t, &uri);
	LM_DBG("'%.*s' not found in usrloc\n", uri.len, ZSW(uri.s));
	return -1;
}

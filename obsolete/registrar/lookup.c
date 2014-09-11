/*
 * $Id$
 *
 * Lookup contacts in usrloc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2005-08-01 fix: "received=" is used also for the branches (andrei)
 * 2005-04-22 added received_to_uri option support (andrei)
 * 2003-03-12 added support for zombie state (nils)
 * 2005-02-25 lookup() forces the socket stored in USRLOC (bogdan)
 */


#include <string.h>
#include "../../ut.h"
#include "../../dset.h"
#include "../../str.h"
#include "../../config.h"
#include "../../action.h"
#include "../usrloc/usrloc.h"
#include "../../error.h"
#include "../../id.h"
#include "../../sr_module.h"
#include "common.h"
#include "regtime.h"
#include "reg_mod.h"
#include "lookup.h"
#include "../../usr_avp.h"



/* new_uri= uri "; received=" received
 * returns 0 on success, < 0 on error (out of memory)
 * new_uri.s is pkg_malloc'ed (so you must pkg_free it when you are done
 *  with it)
 */
static inline int add_received(str* new_uri, str* uri, str* received)
{
	/* copy received into an uri param */
	new_uri->len=uri->len+RECEIVED_LEN+1+received->len+1;
	new_uri->s=pkg_malloc(new_uri->len+1);
	if (new_uri->s==0){
		LOG(L_ERR, "ERROR: add_received(): out of memory\n");
		return -1;
	}
	memcpy(new_uri->s, uri->s, uri->len);
	memcpy(new_uri->s+uri->len, RECEIVED, RECEIVED_LEN);
	new_uri->s[uri->len+RECEIVED_LEN]='"';
	memcpy(new_uri->s+uri->len+RECEIVED_LEN+1, received->s, received->len);
	new_uri->s[new_uri->len-1]='"';
	new_uri->s[new_uri->len]=0; /* for debugging */
	return 0;
}

	

/*
 * Lookup contact in the database and rewrite Request-URI
 */
int lookup(struct sip_msg* _m, char* _t, char* _s)
{
	urecord_t* r;
	str uid;
	ucontact_t* ptr;
	int res;
	unsigned int nat;
	str new_uri; 	

	nat = 0;
	
	if (get_to_uid(&uid, _m) < 0) return -1;

	get_act_time();

	ul.lock_udomain((udomain_t*)_t);
	res = ul.get_urecord((udomain_t*)_t, &uid, &r);
	if (res < 0) {
		LOG(L_ERR, "lookup(): Error while querying usrloc\n");
		ul.unlock_udomain((udomain_t*)_t);
		return -2;
	}
	
	if (res > 0) {
		DBG("lookup(): '%.*s' Not found in usrloc\n", uid.len, ZSW(uid.s));
		ul.unlock_udomain((udomain_t*)_t);
		return -3;
	}

	ptr = r->contacts;
	while ((ptr) && !VALID_CONTACT(ptr, act_time))
		ptr = ptr->next;
	
	if (ptr) {
		if (ptr->received.s && ptr->received.len) {
			if (received_to_uri){
				if (add_received(&new_uri, &ptr->c, &ptr->received)<0){
					LOG(L_ERR, "ERROR: lookup(): out of memory\n");
					return -4;
				}
			/* replace the msg uri */
			if (_m->new_uri.s)      pkg_free(_m->new_uri.s);
			_m->new_uri=new_uri;
			_m->parsed_uri_ok=0;
			ruri_mark_new();
			goto skip_rewrite_uri;
			}else if (set_dst_uri(_m, &ptr->received) < 0) {
				ul.unlock_udomain((udomain_t*)_t);
				return -4;
			}
		}
		
		if (rewrite_uri(_m, &ptr->c) < 0) {
			LOG(L_ERR, "lookup(): Unable to rewrite Request-URI\n");
			ul.unlock_udomain((udomain_t*)_t);
			return -4;
		}

		if (ptr->sock) {
			set_force_socket(_m, ptr->sock);
		}

skip_rewrite_uri:

		set_ruri_q(ptr->q);

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
		if (VALID_CONTACT(ptr, act_time)) {
			if (received_to_uri && ptr->received.s && ptr->received.len){
				if (add_received(&new_uri, &ptr->c, &ptr->received)<0){
					LOG(L_ERR, "ERROR: lookup(): branch: out of memory\n");
					goto cont; /* try to continue */
				}
				if (append_branch(_m, &new_uri, 0, 0, ptr->q,
						  0, 0, 0, 0) == -1) {
					LOG(L_ERR, "lookup(): Error while appending a branch\n");
					pkg_free(new_uri.s);
					if (ser_error==E_TOO_MANY_BRANCHES) goto skip;
					else goto cont; /* try to continue, maybe we have an
							                   oversized contact */
				}
				pkg_free(new_uri.s); /* append_branch doesn't free it */
			}else{
				if (append_branch(_m, &ptr->c, &ptr->received,
						  0 /* path */,
						  ptr->q, 0 /* brflags*/,
						  ptr->sock, 0, 0) == -1) {
					LOG(L_ERR, "lookup(): Error while appending a branch\n");
					goto skip; /* Return OK here so the function succeeds */
				}
			}
			
			nat |= ptr->flags & FL_NAT; 
		} 
cont:
		ptr = ptr->next; 
	}
	
 skip:
	ul.unlock_udomain((udomain_t*)_t);
	if (nat) setflag(_m, load_nat_flag);
	return 1;
}


/*
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered(struct sip_msg* _m, char* _t, char* _s)
{
	str uid;
	urecord_t* r;
        ucontact_t* ptr;
	int res;

	if (get_to_uid(&uid, _m) < 0) return -1;

	ul.lock_udomain((udomain_t*)_t);
	res = ul.get_urecord((udomain_t*)_t, &uid, &r);

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
			DBG("registered(): '%.*s' found in usrloc\n", uid.len, ZSW(uid.s));
			return 1;
		}
	}

	ul.unlock_udomain((udomain_t*)_t);
	DBG("registered(): '%.*s' not found in usrloc\n", uid.len, ZSW(uid.s));
	return -1;
}

/*
 * Lookup contact in the database and rewrite Request-URI,
 * and filter them by aor
 */
int lookup2(struct sip_msg* msg, char* table, char* p2)
{
	urecord_t* r;
	str uid;
	ucontact_t* ptr;
	int res;
	unsigned int nat;
	str new_uri, aor;

	nat = 0;
	
	if (get_str_fparam(&aor, msg, (fparam_t*)p2) != 0) {
	    ERR("Unable to get the AOR value\n");
	    return -1;
	}

	if (get_to_uid(&uid, msg) < 0) return -1;
	get_act_time();

	ul.lock_udomain((udomain_t*)table);
	res = ul.get_urecord((udomain_t*)table, &uid, &r);
	if (res < 0) {
		ERR("Error while querying usrloc\n");
		ul.unlock_udomain((udomain_t*)table);
		return -2;
	}
	
	if (res > 0) {
		DBG("'%.*s' Not found in usrloc\n", uid.len, ZSW(uid.s));
		ul.unlock_udomain((udomain_t*)table);
		return -3;
	}

	ptr = r->contacts;
	while (ptr && (!VALID_CONTACT(ptr, act_time) || !VALID_AOR(ptr, aor)))
	    ptr = ptr->next;
	
	if (ptr) {
	       if (ptr->received.s && ptr->received.len) {
			if (received_to_uri){
				if (add_received(&new_uri, &ptr->c, &ptr->received) < 0) {
					ERR("Out of memory\n");
					return -4;
				}
			/* replace the msg uri */
			if (msg->new_uri.s) pkg_free(msg->new_uri.s);
			msg->new_uri = new_uri;
			msg->parsed_uri_ok = 0;
			ruri_mark_new();
			goto skip_rewrite_uri;
			} else if (set_dst_uri(msg, &ptr->received) < 0) {
			        ul.unlock_udomain((udomain_t*)table);
				return -4;
			}
		}
		
		if (rewrite_uri(msg, &ptr->c) < 0) {
			ERR("Unable to rewrite Request-URI\n");
			ul.unlock_udomain((udomain_t*)table);
			return -4;
		}

		if (ptr->sock) {
			set_force_socket(msg, ptr->sock);
		}

skip_rewrite_uri:
		set_ruri_q(ptr->q);

		nat |= ptr->flags & FL_NAT;
		ptr = ptr->next;
	} else {
		     /* All contacts expired */
		ul.unlock_udomain((udomain_t*)table);
		return -5;
	}
	
	     /* Append branches if enabled */
	if (!append_branches) goto skip;

	while(ptr) {
		if (VALID_CONTACT(ptr, act_time) && VALID_AOR(ptr, aor)) {
			if (received_to_uri && ptr->received.s && ptr->received.len) {
				if (add_received(&new_uri, &ptr->c, &ptr->received) < 0) {
					ERR("branch: out of memory\n");
					goto cont; /* try to continue */
				}
				if (append_branch(msg, &new_uri, 0, 0, ptr->q,
						  0, 0, 0, 0) == -1) {
					ERR("Error while appending a branch\n");
					pkg_free(new_uri.s);
					if (ser_error == E_TOO_MANY_BRANCHES) goto skip;
					else goto cont; /* try to continue, maybe we have an
							                   oversized contact */
				}
				pkg_free(new_uri.s); /* append_branch doesn't free it */
			} else {
				if (append_branch(msg, &ptr->c, &ptr->received,
						  0 /* path */,
						  ptr->q, 0, ptr->sock,
						  0, 0) == -1) {
					ERR("Error while appending a branch\n");
					goto skip; /* Return OK here so the function succeeds */
				}
			}
			
			nat |= ptr->flags & FL_NAT; 
		} 
cont:
		ptr = ptr->next; 
	}
	
 skip:
	ul.unlock_udomain((udomain_t*)table);
	if (nat) setflag(msg, load_nat_flag);
	return 1;
}


/*
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered2(struct sip_msg* _m, char* _t, char* p2)
{
	str uid, aor;
	urecord_t* r;
        ucontact_t* ptr;
	int res;

	if (get_str_fparam(&aor, _m, (fparam_t*)p2) != 0) {
	    ERR("Unable to get the AOR value\n");
	    return -1;
	}

	if (get_to_uid(&uid, _m) < 0) return -1;

	ul.lock_udomain((udomain_t*)_t);
	res = ul.get_urecord((udomain_t*)_t, &uid, &r);

	if (res < 0) {
		ul.unlock_udomain((udomain_t*)_t);
		LOG(L_ERR, "registered(): Error while querying usrloc\n");
		return -1;
	}

	if (res == 0) {
		ptr = r->contacts;
		while (ptr && (!VALID_CONTACT(ptr, act_time) || !VALID_AOR(ptr, aor))) {
			ptr = ptr->next;
		}

		if (ptr) {
			ul.unlock_udomain((udomain_t*)_t);
			DBG("registered(): '%.*s' found in usrloc\n", uid.len, ZSW(uid.s));
			return 1;
		}
	}

	ul.unlock_udomain((udomain_t*)_t);
	DBG("registered(): '%.*s' not found in usrloc\n", uid.len, ZSW(uid.s));
	return -1;
}

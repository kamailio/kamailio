/*
 * $Id$
 *
 * Lookup contacts in usrloc
 */

#include "lookup.h"
#include <string.h>
#include "../../dset.h"
#include "../../str.h"
#include "../../config.h"
#include "../../action.h"
#include "../usrloc/usrloc.h"
#include "common.h"
#include "regtime.h"
#include "reg_mod.h"


/*
 * Rewrite Request-URI
 */
static inline int rwrite(struct sip_msg* _m, str* _s)
{
	char buffer[MAX_URI_SIZE];
	struct action act;
	
	if (_s->len > MAX_URI_SIZE - 1) {
		LOG(L_ERR, "rwrite(): URI too long\n");
		return -1;
	}
	
	memcpy(buffer, _s->s, _s->len);
	buffer[_s->len] = '\0';
	
	DBG("rwrite(): Rewriting Request-URI with \'%s\'\n", buffer);
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = buffer;
	act.next = 0;
	
	if (do_action(&act, _m) < 0) {
		LOG(L_ERR, "rwrite(): Error in do_action\n");
		return -1;
	}
	return 0;
}


/*
 * Lookup contact in the database and rewrite Request-URI
 */
int lookup(struct sip_msg* _m, char* _t, char* _s)
{
	urecord_t* r;
	str user;
	ucontact_t* ptr;
	int res;
	
	if (!_m->to && (parse_headers(_m, HDR_TO, 0) == -1)) {
		LOG(L_ERR, "lookup(): Error while parsing headers\n");
		return -1;
	}

	if (!_m->to) {
	        LOG(L_ERR, "lookup(): Unable to find To HF\n");
		return -2;
	}
	
	if (_m->new_uri.s) str_copy(&user, &_m->new_uri);
	else str_copy(&user, &_m->first_line.u.request.uri);
	
	if ((ul_get_user(&user) < 0) || !user.len) {
		LOG(L_ERR, "lookup(): Error while extracting username\n");
		return -3;
	}
	
	get_act_time();

	ul_lock_udomain((udomain_t*)_t);
	res = ul_get_urecord((udomain_t*)_t, &user, &r);
	if (res < 0) {
		LOG(L_ERR, "lookup(): Error while querying usrloc\n");
		ul_unlock_udomain((udomain_t*)_t);
		return -4;
	}
	
	if (res > 0) {
		DBG("lookup(): \'%.*s\' Not found in usrloc\n", user.len, user.s);
		ul_unlock_udomain((udomain_t*)_t);
		return -5;
	}

	ptr = r->contacts;
	while ((ptr) && (ptr->expires <= act_time)) ptr = ptr->next;
	
	if (ptr) {
		if (rwrite(_m, &ptr->c) < 0) {
			LOG(L_ERR, "lookup(): Unable to rewrite Request-URI\n");
			ul_unlock_udomain((udomain_t*)_t);
			return -6;
		}
		ptr = ptr->next;
	} else {
		     /* All contacts expired */
		return -7;
	}
	
	     /* Append branches if enabled */
	if (!append_branches) goto skip;

	while(ptr) {
		if (ptr->expires > act_time) {
			if (append_branch(_m, ptr->c.s, ptr->c.len) == -1) {
				LOG(L_ERR, "lookup(): Error while appending a branch\n");
				ul_unlock_udomain((udomain_t*)_t);
				return -8;
			}
		} 
		ptr = ptr->next;
	}
	
 skip:
	ul_unlock_udomain((udomain_t*)_t);
	return 1;
}

/*
 * $Id$
 *
 * Process REGISTER request and send reply
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
 * ----------
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-02-28 scrathcpad compatibility abandoned (jiri)
 * 2003-03-21  save_noreply added, patch provided by Maxim Sobolev <sobomax@portaone.com> (janakj)
 */


#include "../../comp_defs.h"
#include "../../str.h"
#include "../../parser/parse_to.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../usrloc/usrloc.h"
#include "../../qvalue.h"
#include "../../usr_avp.h"
#include "common.h"
#include "sip_msg.h"
#include "rerrno.h"
#include "reply.h"
#include "reg_mod.h"
#include "regtime.h"
#include "save.h"

static int mem_only = 0;

void remove_cont(urecord_t* _r, ucontact_t* _c)
{
	if (_c->prev) {
		_c->prev->next = _c->next;
		if (_c->next) {
			_c->next->prev = _c->prev;
		}
	} else {
		_r->contacts = _c->next;
		if (_c->next) {
			_c->next->prev = 0;
		}
	}
}


void move_on_top(urecord_t* _r, ucontact_t* _c)
{
	ucontact_t* prev;

	if (!_r->contacts) return;
	if (_c->prev == 0) return;

	prev = _c->prev;

	remove_cont(_r, _c);
	
	_c->next = _r->contacts;
	_c->prev = 0;

	_r->contacts->prev = _c;
	_r->contacts = _c;
}


/*
 * Process request that contained a star, in that case, 
 * we will remove all bindings with the given username 
 * from the usrloc and return 200 OK response
 */
static inline int star(udomain_t* _d, str* _a)
{
	urecord_t* r;
	ucontact_t* c;
	
	ul.lock_udomain(_d);

	if (!ul.get_urecord(_d, _a, &r)) {
		c = r->contacts;
		while(c) {
			if (mem_only) {
				c->flags |= FL_MEM;
			} else {
				c->flags &= ~FL_MEM;
			}
			c = c->next;
		}
	}

	if (ul.delete_urecord(_d, _a) < 0) {
		LOG(L_ERR, "star(): Error while removing record from usrloc\n");
		
		     /* Delete failed, try to get corresponding
		      * record structure and send back all existing
		      * contacts
		      */
		rerrno = R_UL_DEL_R;
		if (!ul.get_urecord(_d, _a, &r)) {
			build_contact(r->contacts);
		}
		ul.unlock_udomain(_d);
		return -1;
	}
	ul.unlock_udomain(_d);
	return 0;
}


/*
 * Process request that contained no contact header
 * field, it means that we have to send back a response
 * containing a list of all existing bindings for the
 * given username (in To HF)
 */
static inline int no_contacts(udomain_t* _d, str* _a)
{
	urecord_t* r;
	int res;
	
	ul.lock_udomain(_d);
	res = ul.get_urecord(_d, _a, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LOG(L_ERR, "no_contacts(): Error while retrieving record from usrloc\n");
		ul.unlock_udomain(_d);
		return -1;
	}
	
	if (res == 0) {  /* Contacts found */
		build_contact(r->contacts);
	}
	ul.unlock_udomain(_d);
	return 0;
}


/*
 * Message contained some contacts, but record with same address
 * of record was not found so we have to create a new record
 * and insert all contacts from the message that have expires
 * > 0
 */
static inline int insert(struct sip_msg* _m, contact_t* _c, udomain_t* _d, str* _a, str *ua)
{
	urecord_t* r = 0;
	ucontact_t* c;
	int e, cseq;
	qvalue_t q;
	str callid;
	unsigned int flags;
	str* recv;
	int_str val;

	if (isflagset(_m, nat_flag) == 1) flags = FL_NAT;
	else flags = FL_NONE;

	flags |= mem_only;

	while(_c) {
		if (calc_contact_expires(_m, _c->expires, &e) < 0) {
			LOG(L_ERR, "insert(): Error while calculating expires\n");
			return -1;
		}
		     /* Skip contacts with zero expires */
		if (e == 0) goto skip;
		
	        if (r == 0) {
			if (ul.insert_urecord(_d, _a, &r) < 0) {
				rerrno = R_UL_NEW_R;
				LOG(L_ERR, "insert(): Can't insert new record structure\n");
				return -2;
			}
		}
		
		     /* Calculate q value of the contact */
		if (calc_contact_q(_c->q, &q) < 0) {
			LOG(L_ERR, "insert(): Error while calculating q\n");
			ul.delete_urecord(_d, _a);
			return -3;
		}

		     /* Get callid of the message */
		callid = _m->callid->body;	
		trim_trailing(&callid);
		
		     /* Get CSeq number of the message */
		if (str2int(&get_cseq(_m)->number, (unsigned int*)&cseq) < 0) {
			rerrno = R_INV_CSEQ;
			LOG(L_ERR, "insert(): Error while converting cseq number\n");
			ul.delete_urecord(_d, _a);
			return -4;
		}

		if (_c->received) {
			recv = &_c->received->body;
		} else if (search_first_avp(0, rcv_avp, &val)) {
			recv = val.s;
		} else {
			recv = 0;
		}

		if (ul.insert_ucontact(r, &_c->uri, e, q, &callid, cseq, flags, &c, ua, recv) < 0) {
			rerrno = R_UL_INS_C;
			LOG(L_ERR, "insert(): Error while inserting contact\n");
			ul.delete_urecord(_d, _a);
			return -5;
		}
		
	skip:
		_c = get_next_contact(_c);
	}
	
	if (r) {
		if (!r->contacts) {
			ul.delete_urecord(_d, _a);
		} else {
			build_contact(r->contacts);
		}
	}
	
	return 0;
}


/*
 * Message contained some contacts and appropriate
 * record was found, so we have to walk through
 * all contacts and do the following:
 * 1) If contact in usrloc doesn't exists and
 *    expires > 0, insert new contact
 * 2) If contact in usrloc exists and expires
 *    > 0, update the contact
 * 3) If contact in usrloc exists and expires
 *    == 0, delete contact
 */
static inline int update(struct sip_msg* _m, urecord_t* _r, contact_t* _c, str* _ua)
{
	ucontact_t* c, *c2;
	str callid;
	int cseq, e;
	qvalue_t q;
	unsigned int fl;
	str* recv;
	int_str val;

	fl = (isflagset(_m, nat_flag) == 1);

	while(_c) {
		if (calc_contact_expires(_m, _c->expires, &e) < 0) {
			build_contact(_r->contacts);
			LOG(L_ERR, "update(): Error while calculating expires\n");
			return -1;
		}

		if (ul.get_ucontact(_r, &_c->uri, &c) > 0) {
			     /* Contact not found */
			if (e != 0) {
				     /* Calculate q value of the contact */
				if (calc_contact_q(_c->q, &q) < 0) {
					LOG(L_ERR, "update(): Error while calculating q\n");
					return -2;
				}
				
				     /* Get callid of the message */
				callid = _m->callid->body;
				trim_trailing(&callid);
				
				     /* Get CSeq number of the message */
				if (str2int(&(((struct cseq_body*)_m->cseq->parsed)->number), 
								(unsigned int*) &cseq) < 0) {
					rerrno = R_INV_CSEQ;
					LOG(L_ERR, "update(): Error while converting cseq number\n");
					return -3;
				}
				
				if (_c->received) {
					recv = &_c->received->body;
				} else if (search_first_avp(0, rcv_avp, &val)) {
					recv = val.s;
				} else {
					recv = 0;
				}

				if (ul.insert_ucontact(_r, &_c->uri, e, q, &callid, cseq,
						       (fl ? FL_NAT : FL_NONE), &c2, _ua, recv) < 0) {
					rerrno = R_UL_INS_C;
					LOG(L_ERR, "update(): Error while inserting contact\n");
					return -4;
				}

				if (mem_only) {
					c2->flags |= FL_MEM;
				} else {
					c2->flags &= ~FL_MEM;
				}
			}
		} else {
			if (e == 0) {
				if (mem_only) {
					c->flags |= FL_MEM;
				} else {
					c->flags &= ~FL_MEM;
				}

				if (ul.delete_ucontact(_r, c) < 0) {
					rerrno = R_UL_DEL_C;
					LOG(L_ERR, "update(): Error while deleting contact\n");
					return -5;
				}
			} else {
				     /* Calculate q value of the contact */
				if (calc_contact_q(_c->q, &q) < 0) {
					LOG(L_ERR, "update(): Error while calculating q\n");
					return -6;
				}
				
				     /* Get callid of the message */
				callid = _m->callid->body;				
				trim_trailing(&callid);
				
				     /* Get CSeq number of the message */
				if (str2int(&(((struct cseq_body*)_m->cseq->parsed)->number), (unsigned int*)&cseq)
							< 0) {
					rerrno = R_INV_CSEQ;
					LOG(L_ERR, "update(): Error while converting cseq number\n");
					return -7;
				}
				
				if (_c->received) {
					recv = &_c->received->body;
				} else if (search_first_avp(0, rcv_avp, &val)) {
					recv = val.s;
				} else {
					recv = 0;
				}

				if (ul.update_ucontact(c, e, q, &callid, cseq,
						       (fl ? FL_NAT : FL_NONE),
						       (fl ? FL_NONE : FL_NAT), _ua, recv) < 0) {
					rerrno = R_UL_UPD_C;
					LOG(L_ERR, "update(): Error while updating contact\n");
					return -8;
				}
				if (mem_only) {
					c->flags |= FL_MEM;
				} else {
					c->flags &= ~FL_MEM;
				}

				if (desc_time_order) {
					move_on_top(_r, c);
				}
			}
		}
		_c = get_next_contact(_c);
	}

	return 0;
}


/* 
 * This function will process request that
 * contained some contact header fields
 */
static inline int contacts(struct sip_msg* _m, contact_t* _c, udomain_t* _d, str* _a, str* _ua)
{
	int res;
	urecord_t* r;

	ul.lock_udomain(_d);
	res = ul.get_urecord(_d, _a, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LOG(L_ERR, "contacts(): Error while retrieving record from usrloc\n");
		ul.unlock_udomain(_d);
		return -2;
	}

	if (res == 0) { /* Contacts found */
		if (update(_m, r, _c, _ua) < 0) {
			LOG(L_ERR, "contacts(): Error while updating record\n");
			build_contact(r->contacts);
			ul.release_urecord(r);
			ul.unlock_udomain(_d);
			return -3;
		}
		build_contact(r->contacts);
		ul.release_urecord(r);
	} else {
		if (insert(_m, _c, _d, _a, _ua) < 0) {
			LOG(L_ERR, "contacts(): Error while inserting record\n");
			ul.unlock_udomain(_d);
			return -4;
		}
	}
	ul.unlock_udomain(_d);
	return 0;
}

#define UA_DUMMY_STR "Unknown"
#define UA_DUMMY_LEN 7

/*
 * Process REGISTER request and save it's contacts
 */
static inline int save_real(struct sip_msg* _m, udomain_t* _t, str* _key, int doreply)
{
	contact_t* c;
	int st;
	str aor, ua;

	rerrno = R_FINE;

	if (parse_message(_m) < 0) {
		goto error;
	}

	if (check_contacts(_m, &st) > 0) {
		goto error;
	}
	
	get_act_time();
	c = get_first_contact(_m);

	if (!_key) {
		if (extract_aor(&get_to(_m)->uri, &aor) < 0) {
			LOG(L_ERR, "save(): Error while extracting Address Of Record\n");
			goto error;
		}
	} else {
		str = *_key;
	}

	ua.len = 0;
	if (parse_headers(_m, HDR_USERAGENT, 0) != -1 && _m->user_agent &&
	    _m->user_agent->body.len > 0) {
		ua.len = _m->user_agent->body.len;
		ua.s = _m->user_agent->body.s;
	}
	if (ua.len == 0) {
		ua.len = UA_DUMMY_LEN;
		ua.s = UA_DUMMY_STR;
	}

	if (c == 0) {
		if (st) {
			if (star(_t, &aor) < 0) goto error;
		} else {
			if (no_contacts(_t, &aor) < 0) goto error;
		}
	} else {
		if (contacts(_m, c, _t, &aor, &ua) < 0) goto error;
	}

	if (doreply && (send_reply(_m) < 0)) return -1;
	else return 1;
	
 error:
	if (doreply) send_reply(_m);
	return 0;
}


/*
 * Process REGISTER request and save it's contacts
 */
int save(struct sip_msg* _m, char* _t, char* _s)
{
	

	return save_real(_m, (udomain_t*)_t, 0, 1);
}


/*
 * Process REGISTER request and save it's contacts
 */
int save_key(struct sip_msg* _m, char* _t, char* _key)
{
	int_str avp_name;

	avp_name.s = (str*)_key;

	return save_real(_m, (udomain_t*)_t, 0, 1);
}


/*
 * Process REGISTER request and save it's contacts, do not send any replies
 */
int save_noreply(struct sip_msg* _m, char* _t, char* _s)
{
	return save_real(_m, (udomain_t*)_t, 0, 0);
}


/*
 * Update memory cache only
 */
int save_memory(struct sip_msg* _m, char* _t, char* _s)
{
	mem_only = FL_MEM;
	return save_real(_m, (udomain_t*)_t, 0, 1);
}

/*
 * $Id$
 *
 * SIP message related functions
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
 */



#include "sip_msg.h"
#include "../../parser/hf.h"
#include "../../dprint.h"
#include "../../parser/parse_expires.h"  
#include "reg_mod.h"                     /* Module parameters */
#include "convert.h"                     /* atof, atoi */
#include "regtime.h"                     /* act_time */
#include "rerrno.h"


static struct hdr_field* act_contact;


/*
 * Return value of Expires header field
 * if the HF exists converted to absolute
 * time, if the HF doesn't exist, returns
 * default value;
 */
static inline int get_expires_hf(struct sip_msg* _m)
{
	exp_body_t* p;
	
	if (_m->expires) {
		p = (exp_body_t*)_m->expires->parsed;
		if (p->valid) {
			if (p->val != 0) {
				return p->val + act_time;
			} else return 0;
		} else return act_time + default_expires;
	} else {
		return act_time + default_expires;
	}
}


/*
 * Parse the whole messsage and bodies of all header fieds
 * that will be needed by registrar
 */
int parse_message(struct sip_msg* _m)
{
	struct hdr_field* ptr;
	
	if (parse_headers(_m, HDR_EOH, 0) == -1) {
		rerrno = R_PARSE;
		LOG(L_ERR, "parse_message(): Error while parsing headers\n");
		return -1;
	}
	
	if (!_m->to) {
		rerrno = R_TO_MISS;
		LOG(L_ERR, "parse_message(): To not found\n");
		return -2;
	}

	if (!_m->callid) {
		rerrno = R_CID_MISS;
		LOG(L_ERR, "parse_message(): Call-ID not found\n");
		return -3;
	}

	if (!_m->cseq) {
		rerrno = R_CS_MISS;
		LOG(L_ERR, "parse_message(): CSeq not found\n");
		return -4;
	}

	if (_m->expires && !_m->expires->parsed && (parse_expires(_m->expires) < 0)) {
		rerrno = R_PARSE_EXP;
		LOG(L_ERR, "parse_message(): Error while parsing expires body\n");
		return -5;
	}
	
	if (_m->contact) {
		ptr = _m->contact;
		while(ptr) {
			if (ptr->type == HDR_CONTACT) {
				if (!ptr->parsed && (parse_contact(ptr) < 0)) {
					rerrno = R_PARSE_CONT;
					LOG(L_ERR, "parse_message(): Error while parsing Contact body\n");
					return -6;
				}
			}
			ptr = ptr->next;
		}
	}
	
	return 0;
}


/*
 * Check if the originating REGISTER message was formed correctly
 * The whole message must be parsed before calling the function
 * _s indicates whether the contact was star
 */
int check_contacts(struct sip_msg* _m, int* _s)
{
	struct hdr_field* p;
	
	*_s = 0;
	     /* Message without contacts is OK */
	if (_m->contact == 0) return 0;
	
	if (((contact_body_t*)_m->contact->parsed)->star == 1) { /* The first Contact HF is star */
		     /* Expires must be zero */
		if (get_expires_hf(_m) > 0) {
			rerrno = R_STAR_EXP;
			return 1;
		}
		
		     /* Message must contain no contacts */
		if (((contact_body_t*)_m->contact->parsed)->contacts) {
			rerrno = R_STAR_CONT;
			return 1;
		}
		
		     /* Message must contain no other Contact HFs */
		p = _m->contact->next;
		while(p) {
			if (p->type == HDR_CONTACT) {
				rerrno = R_STAR_CONT;
				return 1;
			}
			p = p->next;
		}
		
		*_s = 1;
	} else { /* The first Contact HF is not star */
		     /* Message must contain no star Contact HF */
		p = _m->contact->next;
		while(p) {
			if (p->type == HDR_CONTACT) {
				if (((contact_body_t*)p->parsed)->star == 1) {
					rerrno = R_STAR_CONT;
					return 1;
				}
			}
			p = p->next;
		}
	}
	
	return 0;
}


/*
 * Get the first contact in message
 */
contact_t* get_first_contact(struct sip_msg* _m)
{
	if (_m->contact == 0) return 0;
	
	act_contact = _m->contact;
	return (((contact_body_t*)_m->contact->parsed)->contacts);
}


/* 
 * Get next contact in message
 */
contact_t* get_next_contact(contact_t* _c)
{
	struct hdr_field* p;
	if (_c->next == 0) {
		p = act_contact->next;
		while(p) {
			if (p->type == HDR_CONTACT) {
				act_contact = p;
				return (((contact_body_t*)p->parsed)->contacts);
			}
			p = p->next;
		}
		return 0;
	} else {
		return _c->next;
	}
}


/*
 * Calculate absolute expires value per contact as follows:
 * 1) If the contact has expires value, use the value. If it
 *    is not zero, add actual time to it
 * 2) If the contact has no expires parameter, use expires
 *    header field in the same way
 * 3) If the message contained no expires header field, use
 *    the default value
 */
int calc_contact_expires(struct sip_msg* _m, cparam_t* _ep, int* _e)
{
	if (!_ep || (_ep->body.len == 0)) {
		*_e = get_expires_hf(_m);
	} else {
		if (atoi(&_ep->body, _e) < 0) {
			*_e = 3600;
			     /*
			       rerrno = R_INV_EXP;
			       LOG(L_ERR, "calc_contact_expires(): Invalid expires parameter\n");
			       return -1;
			     */
		}
		     /* Convert to absolute value */
		if (*_e != 0) *_e += act_time;
	}		
	return 0;
}


/*
 * Calculate contact q value as follows:
 * 1) If q parameter exists, use it
 * 2) If the parameter doesn't exist, use default value
 */
int calc_contact_q(cparam_t* _q, float* _r)
{
	if (!_q || (_q->body.len == 0)) {
		*_r = def_q;
	} else {
		if (atof(&_q->body, _r) < 0) {
			rerrno = R_INV_Q; /* Invalid q parameter */
			LOG(L_ERR, "calc_contact_q(): Invalid q parameter\n");
			return -1;
		}
	}
	return 0;
}

/*
 * $Id$
 *
 * SIP message related functions
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
 */

/*!
 * \file
 * \brief SIP registrar module - SIP message related functions
 * \ingroup registrar   
 */

#include "../../parser/hf.h"
#include "../../dprint.h"
#include "../../parser/parse_expires.h"  
#include "../../ut.h"
#include "../../qvalue.h"
#include "reg_mod.h"                     /* Module parameters */
#include "regtime.h"                     /* act_time */
#include "rerrno.h"
#include "sip_msg.h"
#include "config.h"
#include "../../lib/ims/ims_getters.h"

static struct hdr_field* act_contact;

/*! \brief
 *  Return an expire value in the range [ default_expires - range%, default_expires + range% ]
 */
static inline int get_expire_val(void) {
	int expires = cfg_get(registrar, registrar_cfg, default_expires);
	int range = cfg_get(registrar, registrar_cfg, default_expires_range);
	/* if no range is given just return default_expires */
	if (range == 0)
		return expires;
	/* select a random value in the range */
	return expires - (float) range / 100 * expires
			+ (float) (rand() % 100) / 100 * 2 * (float) range / 100 * expires;
}

/*! \brief
 * Return value of Expires header field
 * if the HF exists converted to absolute
 * time, if the HF doesn't exist, returns
 * default value;
 */
static inline int get_expires_hf(struct sip_msg* _m) {
	exp_body_t* p;
	if (_m->expires) {
		p = (exp_body_t*) _m->expires->parsed;
		if (p->valid) {
			if (p->val != 0) {
				return p->val + act_time;
			} else
				return 0;
		} else {
			return act_time + get_expire_val();
		}
	} else
		return act_time + get_expire_val();
}

/*! \brief
 * Get the first contact in message
 */
contact_t* get_first_contact(struct sip_msg* _m) {
	if (_m->contact == 0)
		return 0;

	act_contact = _m->contact;
	return (((contact_body_t*) _m->contact->parsed)->contacts);
}

/*! \brief
 * Get next contact in message
 */
contact_t* get_next_contact(contact_t* _c) {
	struct hdr_field* p;
	if (_c->next == 0) {
		p = act_contact->next;
		while (p) {
			if (p->type == HDR_CONTACT_T) {
				act_contact = p;
				return (((contact_body_t*) p->parsed)->contacts);
			}
			p = p->next;
		}
		return 0;
	} else {
		return _c->next;
	}
}

/*! \brief
 * Calculate absolute expires value per contact as follows:
 * 1) If the contact has expires value, use the value. If it
 *    is not zero, add actual time to it
 * 2) If the contact has no expires parameter, use expires
 *    header field in the same way
 * 3) If the message contained no expires header field, use
 *    the default value
 */
//void calc_contact_expires(struct sip_msg* _m, param_t* _ep, int* _e) {
//	if (!_ep || !_ep->body.len) {
//		*_e = get_expires_hf(_m);
//	} else {
//		if (str2int(&_ep->body, (unsigned int*) _e) < 0) {
//			*_e = get_expire_val();
//		}
//		/* Convert to absolute value */
//		if (*_e != 0)
//			*_e += act_time;
//	}
//
//	if ((*_e != 0)
//			&& ((*_e - act_time)
//					< cfg_get(registrar, registrar_cfg, min_expires))) {
//		*_e = cfg_get(registrar, registrar_cfg, min_expires) + act_time;
//	}
//
//	if ((*_e != 0) && cfg_get(registrar, registrar_cfg, max_expires)
//			&& ((*_e - act_time)
//					> cfg_get(registrar, registrar_cfg, max_expires))) {
//		*_e = cfg_get(registrar, registrar_cfg, max_expires) + act_time;
//	}
//}

/*! \brief
 * Calculate contact q value as follows:
 * 1) If q parameter exists, use it
 * 2) If the parameter doesn't exist, use the default value
 */
int calc_contact_q(param_t* _q, qvalue_t* _r) {
	if (!_q || (_q->body.len == 0)) {
		*_r = cfg_get(registrar, registrar_cfg, default_q);
	} else {
		if (str2q(_r, _q->body.s, _q->body.len) < 0) {
			rerrno = R_INV_Q; /* Invalid q parameter */
			LM_ERR("invalid q parameter\n");
			return -1;
		}
	}
	return 0;
}

/*! \brief
 * Check if the originating REGISTER message was formed correctly
 * The whole message must be parsed before calling the function
 * _s indicates whether the contact was star
 */
int check_contacts(struct sip_msg* _m, int* _s) {
	struct hdr_field* p;
	contact_t* c;

	*_s = 0;
	/* Message without contacts is OK */
	if (_m->contact == 0)
		return 0;

	if (((contact_body_t*) _m->contact->parsed)->star == 1) {
		/* The first Contact HF is star */
		/* Expires must be zero */

		if (cscf_get_expires(_m) != 0) {
			rerrno = R_STAR_EXP;
			return 1;
		}

		/* Message must contain no contacts */
		if (((contact_body_t*) _m->contact->parsed)->contacts) {
			rerrno = R_STAR_CONT;
			return 1;
		}

		/* Message must contain no other Contact HFs */
		p = _m->contact->next;
		while (p) {
			if (p->type == HDR_CONTACT_T) {
				rerrno = R_STAR_CONT;
				return 1;
			}
			p = p->next;
		}

		*_s = 1;
	} else { /* The first Contact HF is not star */
		/* Message must contain no star Contact HF */
		p = _m->contact->next;
		while (p) {
			if (p->type == HDR_CONTACT_T) {
				if (((contact_body_t*) p->parsed)->star == 1) {
					rerrno = R_STAR_CONT;
					return 1;
				}
				/* check also the lenght of all contacts */
				for (c = ((contact_body_t*) p->parsed)->contacts; c;
						c = c->next) {
					if (c->uri.len > CONTACT_MAX_SIZE
							|| (c->received
									&& c->received->len > RECEIVED_MAX_SIZE)) {
						rerrno = R_CONTACT_LEN;
						return 1;
					}
				}
			}
			p = p->next;
		}
	}

	return 0;
}

/*! \brief
 * Parse the whole message and bodies of all header fields
 * that will be needed by registrar
 */
int parse_message_for_register(struct sip_msg* _m) {
	struct hdr_field* ptr;

	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		rerrno = R_PARSE;
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if (!_m->to) {
		rerrno = R_TO_MISS;
		LM_ERR("To not found\n");
		return -2;
	}

	if (!_m->callid) {
		rerrno = R_CID_MISS;
		LM_ERR("Call-ID not found\n");
		return -3;
	}

	if (!_m->cseq) {
		rerrno = R_CS_MISS;
		LM_ERR("CSeq not found\n");
		return -4;
	}

	if (_m->expires && !_m->expires->parsed
			&& (parse_expires(_m->expires) < 0)) {
		rerrno = R_PARSE_EXP;
		LM_ERR("failed to parse expires body\n");
		return -5;
	}

	if (_m->contact) {
		ptr = _m->contact;
		while (ptr) {
			if (ptr->type == HDR_CONTACT_T) {
				if (!ptr->parsed && (parse_contact(ptr) < 0)) {
					rerrno = R_PARSE_CONT;
					LM_ERR("failed to parse Contact body\n");
					return -6;
				}
			}
			ptr = ptr->next;
		}
	}

	return 0;
}

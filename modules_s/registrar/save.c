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
 * 2003-03-21 save_noreply added, patch provided by 
 *            Maxim Sobolev <sobomax@sippysoft.com> (janakj)
 * 2005-02-25 incoming socket is saved in USRLOC (bogdan)
 */


#include "../../comp_defs.h"
#include "../../str.h"
#include "../../parser/parse_to.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../usrloc/usrloc.h"
#include "../../qvalue.h"
#include "../../id.h"
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


/*
 * Process request that contained a star, in that case, 
 * we will remove all bindings with the given username 
 * from the usrloc and return 200 OK response
 */
static inline int star(udomain_t* _d, str* _u, str* aor_filter)
{
	urecord_t* r;
	ucontact_t* c;
	
	ul.lock_udomain(_d);

	if (!ul.get_urecord(_d, _u, &r)) {
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

	if (ul.delete_urecord(_d, _u) < 0) {
		LOG(L_ERR, "star(): Error while removing record from usrloc\n");
		
		     /* Delete failed, try to get corresponding
		      * record structure and send back all existing
		      * contacts
		      */
		rerrno = R_UL_DEL_R;
		if (!ul.get_urecord(_d, _u, &r)) {
			build_contact(r->contacts, aor_filter);
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
 * given aor
 */
static inline int no_contacts(udomain_t* _d, str* _u, str* aor_filter)
{
	urecord_t* r;
	int res;
	
	ul.lock_udomain(_d);
	res = ul.get_urecord(_d, _u, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LOG(L_ERR, "no_contacts(): Error while retrieving record from usrloc\n");
		ul.unlock_udomain(_d);
		return -1;
	}
	
	if (res == 0) {  /* Contacts found */
		build_contact(r->contacts, aor_filter);
	}
	ul.unlock_udomain(_d);
	return 0;
}


#define DSTIP_PARAM ";dstip="
#define DSTIP_PARAM_LEN (sizeof(DSTIP_PARAM) - 1)

#define DSTPORT_PARAM ";dstport="
#define DSTPORT_PARAM_LEN (sizeof(DSTPORT_PARAM) - 1)


/*
 * Create received SIP uri that will be either
 * passed to registrar in an AVP or apended
 * to Contact header field as a parameter
 */
static int create_rcv_uri(str** uri, struct sip_msg* m)
{
	static str res;
	static char buf[MAX_URI_SIZE];
	char* p;
	str src_ip, src_port, dst_ip, dst_port;
	int len;
	str proto;

	if (!uri || !m) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	src_ip.s = ip_addr2a(&m->rcv.src_ip);
	src_ip.len = strlen(src_ip.s);
	src_port.s = int2str(m->rcv.src_port, &src_port.len);

	dst_ip = m->rcv.bind_address->address_str;
	dst_port = m->rcv.bind_address->port_no_str;

	switch(m->rcv.proto) {
	case PROTO_NONE:
	case PROTO_UDP:
		proto.s = 0; /* Do not add transport parameter, UDP is default */
		proto.len = 0;
		break;

	case PROTO_TCP:
		proto.s = "TCP";
		proto.len = 3;
		break;

	case PROTO_TLS:
		proto.s = "TLS";
		proto.len = 3;
		break;

	case PROTO_SCTP:
		proto.s = "SCTP";
		proto.len = 4;
		break;

	default:
		ERR("BUG: Unknown transport protocol\n");
		return -1;
	}

	len = 4 + src_ip.len + 1 + src_port.len;
	if (proto.s) {
		len += TRANSPORT_PARAM_LEN;
		len += proto.len;
	}

	len += DSTIP_PARAM_LEN + dst_ip.len;
	len += DSTPORT_PARAM_LEN + dst_port.len;

	if (len > MAX_URI_SIZE) {
		ERR("Buffer too small\n");
		return -1;
	}

	p = buf;
	memcpy(p, "sip:", 4);
	p += 4;

	memcpy(p, src_ip.s, src_ip.len);
	p += src_ip.len;

	*p++ = ':';

	memcpy(p, src_port.s, src_port.len);
	p += src_port.len;

	if (proto.s) {
		memcpy(p, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
		p += TRANSPORT_PARAM_LEN;

		memcpy(p, proto.s, proto.len);
		p += proto.len;
	}

	memcpy(p, DSTIP_PARAM, DSTIP_PARAM_LEN);
	p += DSTIP_PARAM_LEN;
	memcpy(p, dst_ip.s, dst_ip.len);
	p += dst_ip.len;

	memcpy(p, DSTPORT_PARAM, DSTPORT_PARAM_LEN);
	p += DSTPORT_PARAM_LEN;
	memcpy(p, dst_port.s, dst_port.len);
	p += dst_port.len;

	res.s = buf;
	res.len = len;
	*uri = &res;

	return 0;
}


/*
 * Message contained some contacts, but record with same address
 * of record was not found so we have to create a new record
 * and insert all contacts from the message that have expires
 * > 0
 */
static inline int insert(struct sip_msg* _m, str* aor, contact_t* _c, udomain_t* _d, str* _u, str *ua, str* aor_filter)
{
	urecord_t* r = 0;
	ucontact_t* c;
	int e, cseq;
	qvalue_t q;
	str callid;
	unsigned int flags;
	str *recv, *inst;
	int num;

	if (isflagset(_m, save_nat_flag) == 1) flags = FL_NAT;
	else flags = FL_NONE;

	flags |= mem_only;

	num = 0;
	while(_c) {
		if (calc_contact_expires(_m, _c->expires, &e) < 0) {
			LOG(L_ERR, "insert(): Error while calculating expires\n");
			return -1;
		}
		     /* Skip contacts with zero expires */
		if (e == 0) goto skip;

		if (max_contacts && (num >= max_contacts)) {
			rerrno = R_TOO_MANY;
			ul.delete_urecord(_d, _u);
			return -1;
		}
		num++;
		
	        if (r == 0) {
			if (ul.insert_urecord(_d, _u, &r) < 0) {
				rerrno = R_UL_NEW_R;
				LOG(L_ERR, "insert(): Can't insert new record structure\n");
				return -2;
			}
		}
		
		     /* Calculate q value of the contact */
		if (calc_contact_q(_c->q, &q) < 0) {
			LOG(L_ERR, "insert(): Error while calculating q\n");
			ul.delete_urecord(_d, _u);
			return -3;
		}

		     /* Get callid of the message */
		callid = _m->callid->body;	
		trim_trailing(&callid);
		
		     /* Get CSeq number of the message */
		if (str2int(&get_cseq(_m)->number, (unsigned int*)&cseq) < 0) {
			rerrno = R_INV_CSEQ;
			LOG(L_ERR, "insert(): Error while converting cseq number\n");
			ul.delete_urecord(_d, _u);
			return -4;
		}

		if (_c->received) {
			recv = &_c->received->body;
		} else if (flags & FL_NAT) {
			if (create_rcv_uri(&recv, _m) < 0) {
				ERR("Error while creating rcv URI\n");
				ul.delete_urecord(_d, _u);
				return -4;
			}
		} else {
			recv = 0;
		}

		if(_c->instance) {
			inst = &_c->instance->body;
		} else {
			inst = 0;
		}

		if (ul.insert_ucontact(r, aor, &_c->uri, e, q, &callid, cseq, flags, &c, ua, recv, 
				       _m->rcv.bind_address, inst) < 0) {
			rerrno = R_UL_INS_C;
			LOG(L_ERR, "insert(): Error while inserting contact\n");
			ul.delete_urecord(_d, _u);
			return -5;
		}
		
	skip:
		_c = get_next_contact(_c);
	}
	
	if (r) {
		if (!r->contacts) {
			ul.delete_urecord(_d, _u);
		} else {
			build_contact(r->contacts, aor_filter);
		}
	}
	
	return 0;
}


static int test_max_contacts(struct sip_msg* _m, urecord_t* _r, contact_t* _c)
{
	int num;
	int e;
	ucontact_t* ptr, *cont;
	
	num = 0;
	ptr = _r->contacts;
	while(ptr) {
		if (VALID_CONTACT(ptr, act_time)) {
			num++;
		}
		ptr = ptr->next;
	}
	DBG("test_max_contacts: %d valid contacts\n", num);
	
	while(_c) {
		if (calc_contact_expires(_m, _c->expires, &e) < 0) {
			LOG(L_ERR, "test_max_contacts: Error while calculating expires\n");
			return -1;
		}
		
		if (ul.get_ucontact(_r, &_c->uri, &cont) > 0) {
			     /* Contact not found */
			if (e != 0) num++;
		} else {
			if (e == 0) num--;
		}
		
		_c = get_next_contact(_c);
	}
	
	DBG("test_max_contacts: %d contacts after commit\n", num);
	if (num > max_contacts) {
		rerrno = R_TOO_MANY;
		return 1;
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
static inline int update(struct sip_msg* _m, urecord_t* _r, str* aor, contact_t* _c, str* _ua, str* aor_filter)
{
	ucontact_t* c, *c2;
	str callid;
	int cseq, e, ret;
	int set, reset;
	qvalue_t q;
	unsigned int nated;
	str* recv, *inst;
	
	if (isflagset(_m, save_nat_flag) == 1) {
		nated = FL_NAT;
	} else {
		nated = FL_NONE;
	}

	if (max_contacts) {
		ret = test_max_contacts(_m, _r, _c);
		if (ret != 0) {
			build_contact(_r->contacts, aor_filter);
			return -1;
		}
	}

	_c = get_first_contact(_m);

	while(_c) {
		if (calc_contact_expires(_m, _c->expires, &e) < 0) {
			build_contact(_r->contacts, aor_filter);
			LOG(L_ERR, "update(): Error while calculating expires\n");
			return -1;
		}

		if(_c->instance) {
			inst = &_c->instance->body;
		} else {
			inst = 0;
		}

		if (ul.get_ucontact_by_instance(_r, &_c->uri, inst, &c) > 0) {
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
				} else if (nated & FL_NAT) {
					if (create_rcv_uri(&recv, _m) < 0) {
						ERR("Error while creating rcv URI\n");
						rerrno = R_UL_INS_C;
						return -4;
					}
				} else {
					recv = 0;
				}

				if (ul.insert_ucontact(_r, aor, &_c->uri, e, q, &callid, cseq, 
						       nated | mem_only,
						       &c2, _ua, recv, 
						       _m->rcv.bind_address,
							   inst) < 0) {
					rerrno = R_UL_INS_C;
					LOG(L_ERR, "update(): Error while inserting contact\n");
					return -4;
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
				} else if (nated & FL_NAT) {
					if (create_rcv_uri(&recv, _m) < 0) {
						ERR("Error while creating rcv URI\n");
					        rerrno = R_UL_UPD_C;
						return -4;
					}
				} else {
					recv = 0;
				}

				set = nated | mem_only;
				reset = ~(nated | mem_only) & (FL_NAT | FL_MEM);
				if (ul.update_ucontact(c, &_c->uri, aor, e, q, &callid, cseq, set, reset, _ua, recv, _m->rcv.bind_address, inst) < 0) {
					rerrno = R_UL_UPD_C;
					LOG(L_ERR, "update(): Error while updating contact\n");
					return -8;
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
static inline int contacts(struct sip_msg* _m, contact_t* _c, udomain_t* _d, str* _u, str* _ua, str* aor_filter)
{
	int res;
	urecord_t* r;
	str* aor;
	int_str name, val;

	name.s.s = aor_attr.s + 1; /* Skip the 1st char which is $ */
	name.s.len = aor_attr.len - 1;
	if (search_first_avp(AVP_TRACK_TO | AVP_NAME_STR, name, &val, 0)) {
	    aor = &val.s;
	} else {
	    aor = &get_to(_m)->uri;
	}

	ul.lock_udomain(_d);
	res = ul.get_urecord(_d, _u, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LOG(L_ERR, "contacts(): Error while retrieving record from usrloc\n");
		ul.unlock_udomain(_d);
		return -2;
	}

	if (res == 0) { /* Contacts found */
		if (update(_m, r, aor, _c, _ua, aor_filter) < 0) {
			LOG(L_ERR, "contacts(): Error while updating record\n");
			build_contact(r->contacts, aor_filter);
			ul.release_urecord(r);
			ul.unlock_udomain(_d);
			return -3;
		}
		build_contact(r->contacts, aor_filter);
		ul.release_urecord(r);
	} else {
		if (insert(_m, aor, _c, _d, _u, _ua, aor_filter) < 0) {
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
static inline int save_real(struct sip_msg* _m, udomain_t* _t, char* aor_filt, int doreply)
{
	contact_t* c;
	int st;
	str uid, ua, aor_filter;

	rerrno = R_FINE;

	if (parse_message(_m) < 0) {
		goto error;
	}

	if (check_contacts(_m, &st) > 0) {
		goto error;
	}
	
	if (aor_filt) {
	    if (get_str_fparam(&aor_filter, _m, (fparam_t*)aor_filt) != 0) {
		ERR("registrar:save: Unable to get the AOR value\n");
		return -1;
	    }
	} else {
	    aor_filter.s = 0;
	    aor_filter.len = 0;
	}

	get_act_time();
	c = get_first_contact(_m);

	if (get_to_uid(&uid, _m) < 0) goto error;

	ua.len = 0;
	if (parse_headers(_m, HDR_USERAGENT_F, 0) != -1 && _m->user_agent &&
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
			if (star(_t, &uid, &aor_filter) < 0) goto error;
		} else {
			if (no_contacts(_t, &uid, &aor_filter) < 0) goto error;
		}
	} else {
		if (contacts(_m, c, _t, &uid, &ua, &aor_filter) < 0) goto error;
	}

	if (doreply) {
		if (send_reply(_m) < 0) return -1;
	} else {
		     /* No reply sent, create attributes with values
		      * of reply code, reason text, and contacts
		      */
		if (setup_attrs(_m) < 0) return -1;
	}
	return 1;

 error:
	if (doreply) {
		send_reply(_m);
		return 0;
	}
	return -2;
}


/*
 * Process REGISTER request and save it's contacts
 */
int save(struct sip_msg* _m, char* table, char* aor_filter)
{
	mem_only = FL_NONE;
	return save_real(_m, (udomain_t*)table, aor_filter, 1);
}


/*
 * Process REGISTER request and save it's contacts, do not send any replies
 */
int save_noreply(struct sip_msg* _m, char* table, char* aor_filter)
{
	mem_only = FL_NONE;
	return save_real(_m, (udomain_t*)table, aor_filter, 0);
}


/*
 * Update memory cache only
 */
int save_memory(struct sip_msg* _m, char* table, char* aor_filter)
{
	mem_only = FL_MEM;
	return save_real(_m, (udomain_t*)table, aor_filter, 1);
}

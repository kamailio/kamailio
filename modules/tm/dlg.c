/*
 * $Id$
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
 *   info@iptel.org
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
 * -------
 * 2003-03-29 Created by janakj
 * 2003-07-08 added wrapper to calculate_hooks, needed by b2bua (dcm)
 */


#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../config.h"
#include "dlg.h"
#include "t_reply.h"
#include "../../parser/parser_f.h"


#define NORMAL_ORDER 0  /* Create route set in normal order - UAS */
#define REVERSE_ORDER 1 /* Create route set in reverse order - UAC */

#define ROUTE_PREFIX "Route: "
#define ROUTE_PREFIX_LEN (sizeof(ROUTE_PREFIX) - 1)

#define ROUTE_SEPARATOR "," CRLF "       "
#define ROUTE_SEPARATOR_LEN (sizeof(ROUTE_SEPARATOR) - 1)


/*** Temporary hack ! */
/*
 * This function skips name part
 * uri parsed by parse_contact must be used
 * (the uri must not contain any leading or
 *  trailing part and if angle bracket were
 *  used, right angle bracket must be the
 *  last character in the string)
 *
 * _s will be modified so it should be a tmp
 * copy
 */
void get_raw_uri(str* _s)
{
        char* aq;
        
        if (_s->s[_s->len - 1] == '>') {
                aq = find_not_quoted(_s, '<');
                _s->len -= aq - _s->s + 2;
                _s->s = aq + 1;
        }
}



/*
 * Make a copy of a str structure using shm_malloc
 */
static inline int str_duplicate(str* _d, str* _s)
{
	_d->s = shm_malloc(_s->len);
	if (!_d->s) {
		LOG(L_ERR, "str_duplicate(): No memory left\n");
		return -1;
	}
	
	memcpy(_d->s, _s->s, _s->len);
	_d->len = _s->len;
	return 0;
}


/*
 * Calculate dialog hooks
 */
static inline int calculate_hooks(dlg_t* _d)
{
	str* uri;
	struct sip_uri puri;
	param_hooks_t hooks;
	param_t* params;

	if (_d->route_set) {
		uri = &_d->route_set->nameaddr.uri;
		if (parse_uri(uri->s, uri->len, &puri) < 0) {
			LOG(L_ERR, "calculate_hooks(): Error while parsing URI\n");
			return -1;
		}

		if (parse_params(&puri.params, CLASS_URI, &hooks, &params) < 0) {
			LOG(L_ERR, "calculate_hooks(): Error while parsing parameters\n");
			return -2;
		}
		free_params(params);
		
		if (hooks.uri.lr) {
			if (_d->rem_target.s) _d->hooks.request_uri = &_d->rem_target;
			else _d->hooks.request_uri = &_d->rem_uri;
			_d->hooks.next_hop = &_d->route_set->nameaddr.uri;
			_d->hooks.first_route = _d->route_set;
		} else {
			_d->hooks.request_uri = &_d->route_set->nameaddr.uri;
			_d->hooks.next_hop = _d->hooks.request_uri;
			_d->hooks.first_route = _d->route_set->next;
			_d->hooks.last_route = &_d->rem_target;
		}
	} else {
		if (_d->rem_target.s) _d->hooks.request_uri = &_d->rem_target;
		else _d->hooks.request_uri = &_d->rem_uri;
		_d->hooks.next_hop = _d->hooks.request_uri;
	}

	if ((_d->hooks.request_uri) && (_d->hooks.request_uri->s) && (_d->hooks.request_uri->len)) {
		_d->hooks.ru.s = _d->hooks.request_uri->s;
		_d->hooks.ru.len = _d->hooks.request_uri->len;
		_d->hooks.request_uri = &_d->hooks.ru;
		get_raw_uri(_d->hooks.request_uri);
	}
	if ((_d->hooks.next_hop) && (_d->hooks.next_hop->s) && (_d->hooks.next_hop->len)) {
		_d->hooks.nh.s = _d->hooks.next_hop->s;
		_d->hooks.nh.len = _d->hooks.next_hop->len;
		_d->hooks.next_hop = &_d->hooks.nh;
		get_raw_uri(_d->hooks.next_hop);
	}

	return 0;
}

/*
 * wrapper to calculate_hooks
 * added by dcm
 */
int w_calculate_hooks(dlg_t* _d)
{
	return calculate_hooks(_d);
}

/*
 * Create a new dialog
 */
int new_dlg_uac(str* _cid, str* _ltag, unsigned int _lseq, str* _luri, str* _ruri, dlg_t** _d)
{
	dlg_t* res;

	if (!_cid || !_ltag || !_luri || !_ruri || !_d) {
		LOG(L_ERR, "new_dlg_uac(): Invalid parameter value\n");
		return -1;
	}

	res = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (res == 0) {
		LOG(L_ERR, "new_dlg_uac(): No memory left\n");
		return -2;
	}

	     /* Clear everything */	
	memset(res, 0, sizeof(dlg_t));
	
	     /* Make a copy of Call-ID */
	if (str_duplicate(&res->id.call_id, _cid) < 0) return -3;
	     /* Make a copy of local tag (usually From tag) */
	if (str_duplicate(&res->id.loc_tag, _ltag) < 0) return -4;
	     /* Make a copy of local URI (usually From) */
	if (str_duplicate(&res->loc_uri, _luri) < 0) return -5;
	     /* Make a copy of remote URI (usually To) */
	if (str_duplicate(&res->rem_uri, _ruri) < 0) return -6;
	     /* Make a copy of local sequence (usually CSeq) */
	res->loc_seq.value = _lseq;
	     /* And mark it as set */
	res->loc_seq.is_set = 1;

	*_d = res;

	if (calculate_hooks(*_d) < 0) {
		LOG(L_ERR, "new_dlg_uac(): Error while calculating hooks\n");
		/* FIXME: free everything here */
		shm_free(res);
		return -2;
	}
	
	return 0;
}


/*
 * Parse Contact header field body and extract URI
 * Does not parse headers !!
 */
static inline int get_contact_uri(struct sip_msg* _m, str* _uri)
{
	contact_t* c;

	_uri->len = 0;

	if (!_m->contact) return 1;

	if (parse_contact(_m->contact) < 0) {
		LOG(L_ERR, "get_contact_uri(): Error while parsing Contact body\n");
		return -2;
	}

	c = ((contact_body_t*)_m->contact->parsed)->contacts;

	if (!c) {
		LOG(L_ERR, "get_contact_uri(): Empty body or * contact\n");
		return -3;
	}

	_uri->s = c->uri.s;
	_uri->len = c->uri.len;
	return 0;
}


/*
 * Extract tag from To header field of a response
 * Doesn't parse message headers !!
 */
static inline int get_to_tag(struct sip_msg* _m, str* _tag)
{
	if (!_m->to) {
		LOG(L_ERR, "get_to_tag(): To header field missing\n");
		return -1;
	}

	if (get_to(_m)->tag_value.len) {
		_tag->s = get_to(_m)->tag_value.s;
		_tag->len = get_to(_m)->tag_value.len;
	} else {
		_tag->len = 0;
	}

	return 0;
}


/*
 * Extract tag from From header field of a request
 */
static inline int get_from_tag(struct sip_msg* _m, str* _tag)
{
	if (parse_from_header(_m) == -1) {
		LOG(L_ERR, "get_from_tag(): Error while parsing From header\n");
		return -1;
	}

	if (get_from(_m)->tag_value.len) {
		_tag->s = get_from(_m)->tag_value.s;
		_tag->len = get_from(_m)->tag_value.len;
	} else {
		_tag->len = 0;
	}

	return 0;
}


/*
 * Extract Call-ID value
 * Doesn't parse headers !!
 */
static inline int get_callid(struct sip_msg* _m, str* _cid)
{
	if (_m->callid == 0) {
		LOG(L_ERR, "get_callid(): Call-ID not found\n");
		return -1;
	}

	_cid->s = _m->callid->body.s;
	_cid->len = _m->callid->body.len;
	trim(_cid);
	return 0;
}


/*
 * Create a copy of route set either in normal or reverse order
 */
static inline int get_route_set(struct sip_msg* _m, rr_t** _rs, unsigned char _order)
{
	struct hdr_field* ptr;
	rr_t* last, *p, *t;
	
	last = 0;

	ptr = _m->record_route;
	while(ptr) {
		if (ptr->type == HDR_RECORDROUTE) {
			if (parse_rr(ptr) < 0) {
				LOG(L_ERR, "get_route_set(): Error while parsing Record-Route body\n");
				goto error;
			}

			p = (rr_t*)ptr->parsed;
			while(p) {
				if (shm_duplicate_rr(&t, p) < 0) {
					LOG(L_ERR, "get_route_set(): Error while duplicating rr_t\n");
					goto error;
				}
				if (_order == NORMAL_ORDER) {
					if (!*_rs) *_rs = t;
					if (last) last->next = t;
					last = t;
				} else {
					t->next = *_rs;
					*_rs = t;
				}

				p = p->next;
			}
			
		}
		ptr = ptr->next;
	}
	
	return 0;

 error:
        shm_free_rr(_rs);
	return -1;
}


/*
 * Extract all necessary information from a response and put it
 * in a dialog structure
 */
static inline int response2dlg(struct sip_msg* _m, dlg_t* _d)
{
	str contact, rtag;

	     /* Parse the whole message, we will need all Record-Route headers */
	if (parse_headers(_m, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "response2dlg(): Error while parsing headers\n");
		return -1;
	}
	
	if (get_contact_uri(_m, &contact) < 0) return -2;
	if (contact.len && str_duplicate(&_d->rem_target, &contact) < 0) return -3;
	
	if (get_to_tag(_m, &rtag) < 0) goto err1;
	if (rtag.len && str_duplicate(&_d->id.rem_tag, &rtag) < 0) goto err1;
	
	if (get_route_set(_m, &_d->route_set, REVERSE_ORDER) < 0) goto err2;

	return 0;
 err2:
	if (_d->id.rem_tag.s) shm_free(_d->id.rem_tag.s);
	_d->id.rem_tag.s = 0;
	_d->id.rem_tag.len = 0;

 err1:
	if (_d->rem_target.s) shm_free(_d->rem_target.s);
	_d->rem_target.s = 0;
	_d->rem_target.len = 0;
	return -4;
}


/*
 * Handle dialog in DLG_NEW state, we will be processing the
 * first response
 */
static inline int dlg_new_resp_uac(dlg_t* _d, struct sip_msg* _m)
{
	int code;
	     /*
	      * Dialog is in DLG_NEW state, we will copy remote
	      * target URI, remote tag if present, and route-set 
	      * if present. And we will transit into DLG_CONFIRMED 
	      * if the response was 2xx and to DLG_DESTROYED if the 
	      * request was a negative final response.
	      */

	code = _m->first_line.u.reply.statuscode;

	if (code < 200) {
		     /* A provisional response, do nothing, we could
		      * update remote tag and route set but we will do that
		      * for a positive final response anyway and I don't want
		      * bet on presence of these fields in provisional responses
		      *
		      * Send a request to jan@iptel.org if you need to update
		      * the structures here
		      */
	} else if ((code >= 200) && (code < 299)) {
		     /* A final response, update the structures and transit
		      * into DLG_CONFIRMED
		      */
		if (response2dlg(_m, _d) < 0) return -1;
		_d->state = DLG_CONFIRMED;

		if (calculate_hooks(_d) < 0) {
			LOG(L_ERR, "dlg_new_resp_uac(): Error while calculating hooks\n");
			return -2;
		}
	} else {
		     /* 
		      * A negative final response, mark the dialog as destroyed
		      * Again, I do not update the structures here because it
		      * makes no sense to me, a dialog shouldn't be used after
		      * it is destroyed
		      */
		_d->state = DLG_DESTROYED;
		     /* Signalize the termination with positive return value */
		return 1;
	}

	return 0;
}


/*
 * Handle dialog in DLG_EARLY state, we will be processing either
 * next provisional response or a final response
 */
static inline int dlg_early_resp_uac(dlg_t* _d, struct sip_msg* _m)
{
	int code;
	code = _m->first_line.u.reply.statuscode;	

	if (code < 200) {
		     /* We are in early state already, do nothing
		      */
	} else if ((code >= 200) && (code <= 299)) {
		     /* Same as in dlg_new_resp_uac */
		     /* A final response, update the structures and transit
		      * into DLG_CONFIRMED
		      */
		if (response2dlg(_m, _d) < 0) return -1;
		_d->state = DLG_CONFIRMED;

		if (calculate_hooks(_d) < 0) {
			LOG(L_ERR, "dlg_early_resp_uac(): Error while calculating hooks\n");
			return -2;
		}
	} else {
		     /* Else terminate the dialog */
		_d->state = DLG_DESTROYED;
		     /* Signalize the termination with positive return value */
		return 1;
	}

	return 0;
}


/*
 * Extract method from CSeq header field
 */
static inline int get_cseq_method(struct sip_msg* _m, str* _method)
{
	if (!_m->cseq && ((parse_headers(_m, HDR_CSEQ, 0) == -1) || !_m->cseq)) {
		LOG(L_ERR, "get_cseq_method(): Error while parsing CSeq\n");
		return -1;
	}

	_method->s = get_cseq(_m)->method.s;
	_method->len = get_cseq(_m)->method.len;
	return 0;
}


/*
 * Handle dialog in DLG_CONFIRMED state, we will be processing
 * a response to a request sent within a dialog
 */
static inline int dlg_confirmed_resp_uac(dlg_t* _d, struct sip_msg* _m)
{
	int code;
	str method, contact;

	code = _m->first_line.u.reply.statuscode;	

	     /* Dialog has been already confirmed, that means we received
	      * a response to a request sent within the dialog. We will
	      * update remote target URI if and only if the message sent was
	      * a target refresher. 
	      */

	     /* FIXME: Currently we support only INVITEs as target refreshers,
	      * this should be generalized
	      */

	     /* IF we receive a 481 response, terminate the dialog because
	      * the remote peer indicated that it didn't have the dialog
	      * state anymore, signal this termination with a positive return
	      * value
	      */
	if (code == 481) {
		_d->state = DLG_DESTROYED;
		return 1;
	}

	     /* Do nothing if not 2xx */
	if ((code < 200) || (code >= 300)) return 0;
	
	if (get_cseq_method(_m, &method) < 0) return -1;
	if ((method.len == 6) && !memcmp("INVITE", method.s, 6)) {
		     /* Get contact if any and update remote target */
		if (parse_headers(_m, HDR_CONTACT, 0) == -1) {
			LOG(L_ERR, "dlg_confirmed_resp_uac(): Error while parsing headers\n");
			return -2;
		}

		     /* Try to extract contact URI */
		if (get_contact_uri(_m, &contact) < 0) return -3;
		     /* If there is a contact URI */
		if (contact.len) {
			     /* Free old remote target if any */
			if (_d->rem_target.s) shm_free(_d->rem_target.s);
			     /* Duplicate new remote target */
			if (str_duplicate(&_d->rem_target, &contact) < 0) return -4;
		}
	}

	return 0;
}


/*
 * A response arrived, update dialog
 */
int dlg_response_uac(dlg_t* _d, struct sip_msg* _m)
{
	if (!_d || !_m) {
		LOG(L_ERR, "dlg_response_uac(): Invalid parameter value\n");
		return -1;
	}

	     /* The main dispatcher */
	switch(_d->state) {
	case DLG_NEW:       
		return dlg_new_resp_uac(_d, _m);

	case DLG_EARLY:     
		return dlg_early_resp_uac(_d, _m);

	case DLG_CONFIRMED: 
		return dlg_confirmed_resp_uac(_d, _m);

	case DLG_DESTROYED:
		LOG(L_ERR, "dlg_response_uac(): Cannot handle destroyed dialog\n");
		return -2;
	}

	LOG(L_ERR, "dlg_response_uac(): Error in switch statement\n");
	return -3;
}


/*
 * Get CSeq number
 * Does not parse headers !!
 */
static inline int get_cseq_value(struct sip_msg* _m, unsigned int* _cs)
{
	str num;

	if (_m->cseq == 0) {
		LOG(L_ERR, "get_cseq_value(): CSeq header not found\n");
		return -1;
	}

	num.s = get_cseq(_m)->number.s;
	num.len = get_cseq(_m)->number.len;

	trim_leading(&num);
	if (str2int(&num, _cs) < 0) {
		LOG(L_ERR, "get_cseq_value(): Error while converting cseq number\n");
		return -2;
	}
	return 0;
}


/*
 * Copy To or From URI without tag parameter
 */
static inline int get_dlg_uri(struct hdr_field* _h, str* _s)
{
	struct to_param* ptr, *prev;
	struct to_body* body;
	char* tag = 0; /* Makes gcc happy */
	int tag_len = 0, len;

	if (!_h) {
		LOG(L_ERR, "get_dlg_uri(): Header field not found\n");
		return -1;
	}

	     /* From was already parsed when extracting tag
	      * and To is parsed by default
	      */
	
	body = (struct to_body*)_h->parsed;

	ptr = body->param_lst;
	prev = 0;
	while(ptr) {
		if (ptr->type == TAG_PARAM) break;
		prev = ptr;
		ptr = ptr->next;
	}

	if (ptr) {
		     /* Tag param found */
		if (prev) {
			tag = prev->value.s + prev->value.len;
		} else {
			tag = body->body.s + body->body.len;
		}
		
		if (ptr->next) {
			tag_len = ptr->value.s + ptr->value.len - tag;
		} else {
			tag_len = _h->body.s + _h->body.len - tag;
		}
	}

	_s->s = shm_malloc(_h->body.len - tag_len);
	if (!_s->s) {
		LOG(L_ERR, "get_dlg_uri(): No memory left\n");
		return -1;
	}

	if (tag_len) {
		len = tag - _h->body.s;
		memcpy(_s->s, _h->body.s, len);
		memcpy(_s->s + len, tag + tag_len, _h->body.len - len - tag_len);
		_s->len = _h->body.len - tag_len;
	} else {
		memcpy(_s->s, _h->body.s, _h->body.len);
		_s->len = _h->body.len;
	}

	return 0;
}


/*
 * Extract all information from a request 
 * and update a dialog structure
 */
static inline int request2dlg(struct sip_msg* _m, dlg_t* _d)
{
	str contact, rtag, callid;

	if (parse_headers(_m, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "request2dlg(): Error while parsing headers");
		return -1;
	}

	if (get_contact_uri(_m, &contact) < 0) return -2;
	if (contact.len && str_duplicate(&_d->rem_target, &contact) < 0) return -3;
	
	if (get_from_tag(_m, &rtag) < 0) goto err1;
	if (rtag.len && str_duplicate(&_d->id.rem_tag, &rtag) < 0) goto err1;

	if (get_callid(_m, &callid) < 0) goto err2;
	if (callid.len && str_duplicate(&_d->id.call_id, &callid) < 0) goto err2;

	if (get_cseq_value(_m, &_d->rem_seq.value) < 0) goto err3;
	_d->rem_seq.is_set = 1;

	if (get_dlg_uri(_m->from, &_d->rem_uri) < 0) goto err3;
	if (get_dlg_uri(_m->to, &_d->loc_uri) < 0) goto err4;

	if (get_route_set(_m, &_d->route_set, NORMAL_ORDER) < 0) goto err5;	

	return 0;
 err5:
	if (_d->loc_uri.s) shm_free(_d->loc_uri.s);
	_d->loc_uri.s = 0;
	_d->loc_uri.len = 0;
 err4:
	if (_d->rem_uri.s) shm_free(_d->rem_uri.s);
	_d->rem_uri.s = 0;
	_d->rem_uri.len = 0;
 err3:
	if (_d->id.call_id.s) shm_free(_d->id.call_id.s);
	_d->id.call_id.s = 0;
	_d->id.call_id.len = 0;
 err2:
	if (_d->id.rem_tag.s) shm_free(_d->id.rem_tag.s);
	_d->id.rem_tag.s = 0;
	_d->id.rem_tag.len = 0;
 err1:
	if (_d->rem_target.s) shm_free(_d->rem_target.s);
	_d->rem_target.s = 0;
	_d->rem_target.len = 0;
	return -4;
}


/*
 * Establishing a new dialog, UAS side
 */
int new_dlg_uas(struct sip_msg* _req, int _code, /*str* _tag,*/ dlg_t** _d)
{
	dlg_t* res;
	str tag;

	if (!_req || /*!_tag ||*/ !_d) {
		LOG(L_ERR, "new_dlg_uas(): Invalid parameter value\n");
		return -1;
	}

	if ((_code < 200) || (_code > 299)) {
		DBG("new_dlg_uas(): Not a 2xx, no dialog created\n");
		return -2;
	}

	res = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (res == 0) {
		LOG(L_ERR, "new_dlg_uac(): No memory left\n");
		return -3;
	}
	     /* Clear everything */
	memset(res, 0, sizeof(dlg_t));	

	if (request2dlg(_req, res) < 0) {
		LOG(L_ERR, "new_dlg_uas(): Error while converting request to dialog\n");
		return -4;
	}

	tag.s = tm_tags;
	tag.len = TOTAG_VALUE_LEN;
	calc_crc_suffix(_req, tm_tag_suffix);
	if (str_duplicate(&res->id.loc_tag, &tag) < 0) {
		free_dlg(res);
		return -5;
	}
	
	*_d = res;

	(*_d)->state = DLG_CONFIRMED;
	if (calculate_hooks(*_d) < 0) {
		LOG(L_ERR, "new_dlg_uas(): Error while calculating hooks\n");
		shm_free(*_d);
		return -6;
	}

	return 0;
}


/*
 * UAS side - update a dialog from a request
 */
int dlg_request_uas(dlg_t* _d, struct sip_msg* _m)
{
	str contact;
	int cseq;

	if (!_d || !_m) {
		LOG(L_ERR, "dlg_request_uas(): Invalid parameter value\n");
		return -1;
	}

	     /* We must check if the request is not out of order or retransmission
	      * first, if so then we will not update anything
	      */
	if (parse_headers(_m, HDR_CSEQ, 0) == -1) {
		LOG(L_ERR, "dlg_request_uas(): Error while parsing headers\n");
		return -2;
	}
	if (get_cseq_value(_m, (unsigned int*)&cseq) < 0) return -3;
	if (_d->rem_seq.is_set && (cseq <= _d->rem_seq.value)) return 0;

	     /* Neither out of order nor retransmission -> update */
	_d->rem_seq.value = cseq;
	_d->rem_seq.is_set = 1;
	
	     /* We will als update remote target URI if the message 
	      * is target refresher
	      */
	if (_m->first_line.u.request.method_value == METHOD_INVITE) {
		     /* target refresher */
		if (parse_headers(_m, HDR_CONTACT, 0) == -1) {
			LOG(L_ERR, "dlg_request_uas(): Error while parsing headers\n");
			return -4;
		}
		
		if (get_contact_uri(_m, &contact) < 0) return -5;
		if (contact.len) {
			if (_d->rem_target.s) shm_free(_d->rem_target.s);
			if (str_duplicate(&_d->rem_target, &contact) < 0) return -6;
		}
	}

	return 0;
}


/*
 * Calculate length of the route set
 */
int calculate_routeset_length(dlg_t* _d)
{
	int len;
	rr_t* ptr;

	len = 0;
	ptr = _d->hooks.first_route;

	if (ptr) {
		len = ROUTE_PREFIX_LEN;
		len += CRLF_LEN;
	}

	while(ptr) {
		len += ptr->len;
		ptr = ptr->next;
		if (ptr) len += ROUTE_SEPARATOR_LEN;
	} 

	if (_d->hooks.last_route) {
		len += ROUTE_SEPARATOR_LEN + 2; /* < > */
		len += _d->hooks.last_route->len;
	}

	return len;
}


/*
 *
 * Print the route set
 */
char* print_routeset(char* buf, dlg_t* _d)
{
	rr_t* ptr;

	ptr = _d->hooks.first_route;

	if (ptr || _d->hooks.last_route) {
		memcpy(buf, ROUTE_PREFIX, ROUTE_PREFIX_LEN);
		buf += ROUTE_PREFIX_LEN;
	}

	while(ptr) {
		memcpy(buf, ptr->nameaddr.name.s, ptr->len);
		buf += ptr->len;

		ptr = ptr->next;
		if (ptr) {
			memcpy(buf, ROUTE_SEPARATOR, ROUTE_SEPARATOR_LEN);
			buf += ROUTE_SEPARATOR_LEN;
		}
	} 

	if (_d->hooks.last_route) {
		if (_d->hooks.first_route) {
			memcpy(buf, ROUTE_SEPARATOR, ROUTE_SEPARATOR_LEN);
			buf += ROUTE_SEPARATOR_LEN;
		}
		memcpy(buf, "<", 1);
		buf++;
		memcpy(buf, _d->hooks.last_route->s, _d->hooks.last_route->len);
		buf += _d->hooks.last_route->len;
		*buf = '>';
		buf++;
	}

	if (_d->hooks.first_route || _d->hooks.last_route) {
		memcpy(buf, CRLF, CRLF_LEN);
		buf += CRLF_LEN;
	}

	return buf;
}


/*
 * Destroy a dialog state
 */
void free_dlg(dlg_t* _d)
{
        if (!_d) return;

	if (_d->id.call_id.s) shm_free(_d->id.call_id.s);
	if (_d->id.rem_tag.s) shm_free(_d->id.rem_tag.s);
	if (_d->id.loc_tag.s) shm_free(_d->id.loc_tag.s);

	if (_d->loc_uri.s) shm_free(_d->loc_uri.s);
	if (_d->rem_uri.s) shm_free(_d->rem_uri.s);
	if (_d->rem_target.s) shm_free(_d->rem_target.s);

	     /* Free all routes in the route set */
	shm_free_rr(&_d->route_set);
	shm_free(_d);
}


/*
 * Print a dialog structure, just for debugging
 */
void print_dlg(FILE* out, dlg_t* _d)
{
	fprintf(out, "====dlg_t===\n");
	fprintf(out, "id.call_id    : '%.*s'\n", _d->id.call_id.len, _d->id.call_id.s);
	fprintf(out, "id.rem_tag    : '%.*s'\n", _d->id.rem_tag.len, _d->id.rem_tag.s);
	fprintf(out, "id.loc_tag    : '%.*s'\n", _d->id.loc_tag.len, _d->id.loc_tag.s);
	fprintf(out, "loc_seq.value : %d\n", _d->loc_seq.value);
	fprintf(out, "loc_seq.is_set: %s\n", _d->loc_seq.is_set ? "YES" : "NO");
	fprintf(out, "rem_seq.value : %d\n", _d->rem_seq.value);
	fprintf(out, "rem_seq.is_set: %s\n", _d->rem_seq.is_set ? "YES" : "NO");
	fprintf(out, "loc_uri       : '%.*s'\n", _d->loc_uri.len, _d->loc_uri.s);
	fprintf(out, "rem_uri       : '%.*s'\n", _d->rem_uri.len, _d->rem_uri.s);
	fprintf(out, "rem_target    : '%.*s'\n", _d->rem_target.len, _d->rem_target.s);
	fprintf(out, "secure:       : %d\n", _d->secure);
	fprintf(out, "state         : ");
	switch(_d->state) {
	case DLG_NEW:       fprintf(out, "DLG_NEW\n");       break;
	case DLG_EARLY:     fprintf(out, "DLG_EARLY\n");     break;
	case DLG_CONFIRMED: fprintf(out, "DLG_CONFIRMED\n"); break;
	case DLG_DESTROYED: fprintf(out, "DLG_DESTROYED\n"); break;
	}
	print_rr(out, _d->route_set);
	if (_d->hooks.request_uri) 
		fprintf(out, "hooks.request_uri: '%.*s'\n", _d->hooks.request_uri->len, _d->hooks.request_uri->s);
	if (_d->hooks.next_hop) 
		fprintf(out, "hooks.next_hop   : '%.*s'\n", _d->hooks.next_hop->len, _d->hooks.next_hop->s);
	if (_d->hooks.first_route) 
		fprintf(out, "hooks.first_route: '%.*s'\n", _d->hooks.first_route->len, _d->hooks.first_route->nameaddr.name.s);
	if (_d->hooks.last_route)
		fprintf(out, "hooks.last_route : '%.*s'\n", _d->hooks.last_route->len, _d->hooks.last_route->s);
	
	fprintf(out, "====dlg_t====\n");
}

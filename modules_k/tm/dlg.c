/*
 * $Id$
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
 * -------
 * 2003-03-29 Created by janakj
 * 2003-07-08 added wrapper to calculate_hooks, needed by b2bua (dcm)
 * 2008-04-04 added support for local and remote dispaly name in TM dialogs
 *            (by Andrei Pisau <andrei.pisau at voice-system dot ro> )
 */

/*! \file
 * \brief TM :: Dialog handling
 *
 * \ingroup tm
 * - Module: \ref tm
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


#define NORMAL_ORDER 0  /*!< Create route set in normal order - UAS */
#define REVERSE_ORDER 1 /*!< Create route set in reverse order - UAC */

#define ROUTE_PREFIX "Route: "
#define ROUTE_PREFIX_LEN (sizeof(ROUTE_PREFIX) - 1)

#define ROUTE_SEPARATOR "," CRLF "       "
#define ROUTE_SEPARATOR_LEN (sizeof(ROUTE_SEPARATOR) - 1)



/*!
 * \brief This function skips a name part in a URI
 *
 * This function skips a name part in a URI.
 * The URI parsed by parse_contact must be used (the URI
 * must not contain any leading or trailing part and if
 * angle bracket were used, right angle bracket must be the
 * last character in the string)
 * \note Temporary hack!
 * \param _s URI, will be modified so it should be a temporary copy
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


/*!
 * \brief Calculate dialog hooks
 * \param _d dialog state
 * \return 0 on success, -1 on error
 */
static inline int calculate_hooks(dlg_t* _d)
{
	str* uri;
	struct sip_uri puri;

	if (_d->route_set) {
		uri = &_d->route_set->nameaddr.uri;
		if (parse_uri(uri->s, uri->len, &puri) < 0) {
			LM_ERR("failed parse to URI\n");
			return -1;
		}

		if (puri.lr.s) {
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
		if(_d->hooks.next_hop==NULL)
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


/*!
 * \brief Small wrapper to calculate_hooks
 * \param _d dialog state
 * \return 0 on success, -1 on error
 */
int w_calculate_hooks(dlg_t* _d)
{
	return calculate_hooks(_d);
}


/*!
 * \brief Create a new dialog
 * \param _cid Callid
 * \param _ltag local tag (usually From tag)
 * \param _lseq local sequence (usually CSeq)
 * \param _luri local URI (usually From)
 * \param _ruri remote URI (usually To)
 * \param _d dialog state
 * \return 0 on success, negative on errors
 */
int new_dlg_uac(str* _cid, str* _ltag, unsigned int _lseq, str* _luri, str* _ruri, dlg_t** _d)
{
	dlg_t* res;

	if (!_cid || !_ltag || !_luri || !_ruri || !_d) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	res = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (res == 0) {
		LM_ERR("No memory left\n");
		return -2;
	}

	     /* Clear everything */	
	memset(res, 0, sizeof(dlg_t));
	
	     /* Make a copy of Call-ID */
	if (shm_str_dup(&res->id.call_id, _cid) < 0) return -3;
	     /* Make a copy of local tag (usually From tag) */
	if (shm_str_dup(&res->id.loc_tag, _ltag) < 0) return -4;
	     /* Make a copy of local URI (usually From) */
	if (shm_str_dup(&res->loc_uri, _luri) < 0) return -5;
	     /* Make a copy of remote URI (usually To) */
	if (shm_str_dup(&res->rem_uri, _ruri) < 0) return -6;
	     /* Make a copy of local sequence (usually CSeq) */
	res->loc_seq.value = _lseq;
	     /* And mark it as set */
	res->loc_seq.is_set = 1;

	*_d = res;

	if (calculate_hooks(*_d) < 0) {
		LM_ERR("failed to calculate hooks\n");
		/* FIXME: free everything here */
		shm_free(res);
		return -2;
	}
	
	return 0;
}


/*!
 * \brief Store display names into a dialog
 * \param _d dialog state
 * \param _ldname local display name
 * \param _rdname remote display name
 * \return 0 on success, negative on error
 */
int dlg_add_extra(dlg_t* _d, str* _ldname, str* _rdname)
{
	if(!_d || !_ldname || !_rdname)
	{
		LM_ERR("Invalid parameters\n");
		return -1;
	}

 	/* Make a copy of local Display Name */
	if(shm_str_dup(&_d->loc_dname, _ldname) < 0) return -2;
	/* Make a copy of remote Display Name */
	if(shm_str_dup(&_d->rem_dname, _rdname) < 0) return -3;

	return 0;
}


/*!
 * \brief Parse Contact header field body and extract URI
 * \note Does not parse headers!
 * \param _m SIP message
 * \param _uri SIP URI
 * \return 0 on success, negative on error
 */
static inline int get_contact_uri(struct sip_msg* _m, str* _uri)
{
	contact_t* c;

	_uri->len = 0;

	if (!_m->contact) return 1;

	if (parse_contact(_m->contact) < 0) {
		LM_ERR("failed to parse Contact body\n");
		return -2;
	}

	c = ((contact_body_t*)_m->contact->parsed)->contacts;

	if (!c) {
		LM_ERR("Empty body or * contact\n");
		return -3;
	}

	_uri->s = c->uri.s;
	_uri->len = c->uri.len;
	return 0;
}


/*!
 * \brief Extract tag from To header field of a response
 * \note Doesn't parse message headers!
 * \param _m SIP message
 * \param _tag tag
 * \return 0 on success, -1 on error
 */
static inline int get_to_tag(struct sip_msg* _m, str* _tag)
{
	if (!_m->to) {
		LM_ERR("To header field missing\n");
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


/*!
 * \brief Extract tag from From header field of a request
 * \param _m SIP message
 * \param _tag tag
 * \return 0 on success, -1 on error
 */
static inline int get_from_tag(struct sip_msg* _m, str* _tag)
{
	if (parse_from_header(_m)<0) {
		LM_ERR("failed to parse From header\n");
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


/*!
 * \brief Extract Call-ID value
 * \note Doesn't parse headers!
 * \param _m SIP message
 * \param _cid Callid
 * \return 0 on success, -1 on error
 */
static inline int get_callid(struct sip_msg* _m, str* _cid)
{
	if (_m->callid == 0) {
		LM_ERR("Call-ID not found\n");
		return -1;
	}

	_cid->s = _m->callid->body.s;
	_cid->len = _m->callid->body.len;
	trim(_cid);
	return 0;
}


/*!
 * \brief Create a copy of route set either in normal or reverse order
 * \param _m SIP message
 * \param rr_t Route set
 * \param _order how to order the copy, set to NORMAL_ORDER for normal copy
 * \param 0 on success, -1 on error
 */
static inline int get_route_set(struct sip_msg* _m, rr_t** _rs, unsigned char _order)
{
	struct hdr_field* ptr;
	rr_t* last, *p, *t;
	
	last = 0;
	*_rs = 0;

	ptr = _m->record_route;
	while(ptr) {
		if (ptr->type == HDR_RECORDROUTE_T) {
			if (parse_rr(ptr) < 0) {
				LM_ERR("failed to parse Record-Route body\n");
				goto error;
			}

			p = (rr_t*)ptr->parsed;
			while(p) {
				if (shm_duplicate_rr(&t, p, 1/*only first*/) < 0) {
					LM_ERR("duplicating rr_t\n");
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


/*!
 * \brief Extract necessary information from response and insert it into dialog
 *
 * Extract all necessary information from a response and put it in a dialog structure
 * \param _m SIP message
 * \param _d dialog state
 * \return 0 on success, negative on error
 */
static inline int response2dlg(struct sip_msg* _m, dlg_t* _d)
{
	str contact, rtag;

	     /* Parse the whole message, we will need all Record-Route headers */
	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}
	
	if (get_contact_uri(_m, &contact) < 0) return -2;
	if (contact.len && shm_str_dup(&_d->rem_target, &contact) < 0) return -3;
	
	if (get_to_tag(_m, &rtag) < 0) goto err1;
	if (rtag.len && shm_str_dup(&_d->id.rem_tag, &rtag) < 0) goto err1;
	
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


/*!
 * \brief Handle dialog in DLG_NEW state
 *
 * Handle dialog in DLG_NEW state, we will be processing the first response
 * \param _d dialog state
 * \param _m SIP message
 * \return 1 if the dialog was destroyed, negative on errors, 0 otherwise
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
		      */
	} else if ((code >= 200) && (code < 299)) {
		     /* A final response, update the structures and transit
		      * into DLG_CONFIRMED
		      */
		if (response2dlg(_m, _d) < 0) return -1;
		_d->state = DLG_CONFIRMED;

		if (calculate_hooks(_d) < 0) {
			LM_ERR("failed to calculate hooks\n");
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


/*!
 * \brief Handle dialog in DLG_EARLY state
 *
 * Handle dialog in DLG_EARLY state we will be processing either
 * next provisional response or a final response.
 * \param _d dialog state
 * \param _m SIP message
 * \return 1 if the dialog was destroyed, negative on errors, 0 otherwise
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
			LM_ERR("failed to calculate hooks\n");
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


/*!
 * \brief Extract method from CSeq header field
 * \param _m SIP message
 * \param _method SIP method
 * \return 0 on success, -1 on error
 */
static inline int get_cseq_method(struct sip_msg* _m, str* _method)
{
	if (!_m->cseq && ((parse_headers(_m, HDR_CSEQ_F, 0)==-1) || !_m->cseq)) {
		LM_ERR("failed to parse CSeq\n");
		return -1;
	}

	_method->s = get_cseq(_m)->method.s;
	_method->len = get_cseq(_m)->method.len;
	return 0;
}


/*!
 * \brief Handle dialog in DLG_CONFIRMED state
 *
 * Handle dialog in DLG_CONFIRMED state, we will be processing
 * a response to a request sent within a dialog.
 * \param _d dialog state
 * \param _m SIP message
 * \return 1 if the dialog was destroyed, negative on errors, 0 otherwise
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
		if (parse_headers(_m, HDR_CONTACT_F, 0) == -1) {
			LM_ERR("failed to parse headers\n");
			return -2;
		}

		/* Try to extract contact URI */
		if (get_contact_uri(_m, &contact) < 0) return -3;
		/* If there is a contact URI */
		if (contact.len) {
			/* Free old remote target if any */
			if (_d->rem_target.s) shm_free(_d->rem_target.s);
			/* Duplicate new remote target */
			if (shm_str_dup(&_d->rem_target, &contact) < 0) return -4;
		}
	}

	return 0;
}


/*!
 * \brief A response arrived, update dialog
 * \param _d dialog state
 * \param _m SIP message
 * \return 1 if the dialog was destroyed, negative on errors, 0 otherwise
 */
int dlg_response_uac(dlg_t* _d, struct sip_msg* _m)
{
	if (!_d || !_m) {
		LM_ERR("invalid parameter value\n");
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
		LM_ERR("failed handle destroyed dialog\n");
		return -2;
	}

	LM_ERR("unsuccessful switch statement\n");
	return -3;
}


/*!
 * \brief Get CSeq number
 * \note Does not parse headers!
 * \param _m SIP message
 * \param _cs CSeq number
 * \param 0 on success, negative on error
 */
static inline int get_cseq_value(struct sip_msg* _m, unsigned int* _cs)
{
	str num;

	if (_m->cseq == 0) {
		LM_ERR("CSeq header not found\n");
		return -1;
	}

	num.s = get_cseq(_m)->number.s;
	num.len = get_cseq(_m)->number.len;

	trim_leading(&num);
	if (str2int(&num, _cs) < 0) {
		LM_ERR("converting cseq number failed\n");
		return -2;
	}
	return 0;
}


/*!
 * \brief Copy To or From URI without tag parameter
 * \param _h SIP header
 * \param _s target string
 * \return 0 on success, -1 on error
 */
static inline int get_dlg_uri(struct hdr_field* _h, str* _s)
{
	struct to_param* ptr, *prev;
	struct to_body* body;
	char* tag = 0;
	int tag_len = 0, len;

	if (!_h) {
		LM_ERR("header field not found\n");
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
		LM_ERR("No share memory left\n");
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


/*!
 * \brief Extract all information from request and update dialog structure
 * \param _m SIP message
 * \param _d dialog state
 */
static inline int request2dlg(struct sip_msg* _m, dlg_t* _d)
{
	str contact, rtag, callid;

	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers");
		return -1;
	}

	if (get_contact_uri(_m, &contact) < 0) return -2;
	if (contact.len && shm_str_dup(&_d->rem_target, &contact) < 0) return -3;
	
	if (get_from_tag(_m, &rtag) < 0) goto err1;
	if (rtag.len && shm_str_dup(&_d->id.rem_tag, &rtag) < 0) goto err1;

	if (get_callid(_m, &callid) < 0) goto err2;
	if (callid.len && shm_str_dup(&_d->id.call_id, &callid) < 0) goto err2;

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


/*!
 * \brief Establishing a new dialog from the UAS side
 * \param _req SIP request
 * \param _code request code
 * \param _d dialog state
 * \return 0 on success, negative on error
 */
int new_dlg_uas(struct sip_msg* _req, int _code, dlg_t** _d)
{
	dlg_t* res;
	str tag;

	if (!_req || !_d) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	if ((_code < 200) || (_code > 299)) {
		LM_DBG("not a 2xx, no dialog created\n");
		return -2;
	}

	res = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (res == 0) {
		LM_ERR("no more share memory\n");
		return -3;
	}
	     /* Clear everything */
	memset(res, 0, sizeof(dlg_t));	

	if (request2dlg(_req, res) < 0) {
		LM_ERR("converting request to dialog failed\n");
		return -4;
	}

	tag.s = tm_tags;
	tag.len = TOTAG_VALUE_LEN;
	calc_crc_suffix(_req, tm_tag_suffix);
	if (shm_str_dup(&res->id.loc_tag, &tag) < 0) {
		free_dlg(res);
		return -5;
	}
	
	*_d = res;

	(*_d)->state = DLG_CONFIRMED;
	if (calculate_hooks(*_d) < 0) {
		LM_ERR("calculating hooks failed\n");
		shm_free(*_d);
		return -6;
	}

	return 0;
}


/*!
 * \brief UAS side - update a dialog from a request
 * \param _d dialog state
 * \param _m SIP request
 * \return 0 on success, negative on error
 */
int dlg_request_uas(dlg_t* _d, struct sip_msg* _m)
{
	str contact;
	unsigned int cseq;

	if (!_d || !_m) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	     /* We must check if the request is not out of order or retransmission
	      * first, if so then we will not update anything
	      */
	if (parse_headers(_m, HDR_CSEQ_F, 0) == -1) {
		LM_ERR("parsing headers failed\n");
		return -2;
	}
	if (get_cseq_value(_m, &cseq) < 0) return -3;
	if (_d->rem_seq.is_set && (cseq <= _d->rem_seq.value)) return 0;

	     /* Neither out of order nor retransmission -> update */
	_d->rem_seq.value = cseq;
	_d->rem_seq.is_set = 1;
	
	     /* We will als update remote target URI if the message 
	      * is target refresher
	      */
	if (_m->first_line.u.request.method_value == METHOD_INVITE) {
		     /* target refresher */
		if (parse_headers(_m, HDR_CONTACT_F, 0) == -1) {
			LM_ERR("parsing headers failed\n");
			return -4;
		}
		
		if (get_contact_uri(_m, &contact) < 0) return -5;
		if (contact.len) {
			if (_d->rem_target.s) shm_free(_d->rem_target.s);
			if (shm_str_dup(&_d->rem_target, &contact) < 0) return -6;
		}
	}

	return 0;
}


/*!
 * \brief Calculate length of the route set
 * \param _d dialog set
 * \return the length of the route set, can be 0
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


/*!
 * \brief Print the route set
 * \param buf buffer
 * \param _d dialog state
 * \return pointer to the buffer
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


/*!
 * \brief Destroy a dialog state
 * \param _d dialog state
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

	if (_d->loc_dname.s) shm_free(_d->loc_dname.s);
	if (_d->rem_dname.s) shm_free(_d->rem_dname.s);

	/* Free all routes in the route set */
	shm_free_rr(&_d->route_set);
	shm_free(_d);
}


/*!
 * \brief Print a dialog structure, just for debugging
 * \param out file
 * \param _d dialog state
 * \todo why is this included in the TM API exports
 */
void print_dlg(FILE* out, dlg_t* _d)
{
	fprintf(out, "====dlg_t===\n");
	fprintf(out, "id.call_id    : '%.*s'\n",
			_d->id.call_id.len, _d->id.call_id.s);
	fprintf(out, "id.rem_tag    : '%.*s'\n",
			_d->id.rem_tag.len, _d->id.rem_tag.s);
	fprintf(out, "id.loc_tag    : '%.*s'\n",
			_d->id.loc_tag.len, _d->id.loc_tag.s);
	fprintf(out, "loc_seq.value : %d\n", _d->loc_seq.value);
	fprintf(out, "loc_seq.is_set: %s\n", _d->loc_seq.is_set ? "YES" : "NO");
	fprintf(out, "rem_seq.value : %d\n", _d->rem_seq.value);
	fprintf(out, "rem_seq.is_set: %s\n", _d->rem_seq.is_set ? "YES" : "NO");
	fprintf(out, "loc_uri       : '%.*s'\n",_d->loc_uri.len, _d->loc_uri.s);
	fprintf(out, "rem_uri       : '%.*s'\n",_d->rem_uri.len, _d->rem_uri.s);
	fprintf(out, "loc_dname     : '%.*s'\n",_d->loc_dname.len,_d->loc_dname.s);
	fprintf(out, "rem_dname     : '%.*s'\n",_d->rem_dname.len,_d->rem_dname.s);
	fprintf(out, "rem_target    : '%.*s'\n",
			_d->rem_target.len,_d->rem_target.s);
	fprintf(out, "state         : ");
	switch(_d->state) {
	case DLG_NEW:       fprintf(out, "DLG_NEW\n");       break;
	case DLG_EARLY:     fprintf(out, "DLG_EARLY\n");     break;
	case DLG_CONFIRMED: fprintf(out, "DLG_CONFIRMED\n"); break;
	case DLG_DESTROYED: fprintf(out, "DLG_DESTROYED\n"); break;
	}
	print_rr(out, _d->route_set);
	if (_d->hooks.request_uri) 
		fprintf(out, "hooks.request_uri: '%.*s'\n",
			_d->hooks.request_uri->len, _d->hooks.request_uri->s);
	if (_d->hooks.next_hop) 
		fprintf(out, "hooks.next_hop   : '%.*s'\n",
			_d->hooks.next_hop->len, _d->hooks.next_hop->s);
	if (_d->hooks.first_route) 
		fprintf(out, "hooks.first_route: '%.*s'\n",
			_d->hooks.first_route->len,_d->hooks.first_route->nameaddr.name.s);
	if (_d->hooks.last_route)
		fprintf(out, "hooks.last_route : '%.*s'\n",
			_d->hooks.last_route->len, _d->hooks.last_route->s);
	
	fprintf(out, "====dlg_t====\n");
}

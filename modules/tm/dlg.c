/*
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
 */

/*!
 * \file 
 * \brief TM :: 
 * \ingroup tm
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

/* next added to allow automatical tag generation */
#include "callid.h"
#include "uac.h"

#define NORMAL_ORDER 0  /* Create route set in normal order - UAS */
#define REVERSE_ORDER 1 /* Create route set in reverse order - UAC */


#ifdef DIALOG_CALLBACKS

struct new_dlg_cb{
	int types;
	struct new_dlg_cb* next;
	dialog_cb* callback;
	void* param;
};

static struct new_dlg_cb* new_dlg_cb_list=0;
/* callbacks for new dialogs (called each time a new dialog (uas or uac) is
 * created)
 * params: type - DLG_CB_UAC or DLG_CB_UAS
 *            f - callback function
 *        param - parameter passed to the callback; if allocated it must
 *                  be allocated in shared mem.
 * returns < 0 on error
 * WARNING: this callbacks can be registered only before forking (in mod_init)
 */
int register_new_dlg_cb(int type, dialog_cb f, void* param)
{
	struct new_dlg_cb* dlg_cb;
	
	dlg_cb=shm_malloc(sizeof(struct new_dlg_cb));
	if (dlg_cb==0)
		return E_OUT_OF_MEM;
	dlg_cb->types=type;
	dlg_cb->callback=f;
	dlg_cb->param=param;
	dlg_cb->next=new_dlg_cb_list;
	new_dlg_cb_list=dlg_cb;
	return 0;
}


void destroy_new_dlg_cbs()
{
	struct new_dlg_cb* c;
	struct new_dlg_cb* n;

	c=new_dlg_cb_list;
	while(c){
		n=c->next;
		shm_free(c);
		c=n;
	}
	new_dlg_cb_list=0;
}


static void run_new_dlg_callbacks(int type, dlg_t* dlg, struct sip_msg* msg)
{
	struct new_dlg_cb* c;
	for (c=new_dlg_cb_list; c; c=c->next){
		if (c->types & type)
			c->callback(type, dlg, msg);
	}
}


int register_dlg_tmcb(int types, dlg_t* dlg, transaction_cb f, void* param)
{
	/* only TMCB_DLG bad TMCB_DESTROY allowed */
	if ((types&(TMCB_DLG|TMCB_DESTROY))!=types){
		LOG(L_CRIT, "BUG: tm: register_dlg_tmcb: bad types %d\n", types);
		return E_BUG;
	}
	if (f==0){
		LOG(L_CRIT, "BUG: tm: register_dlg_tmcb: null callback function");
		return E_BUG;
	}
	return insert_tmcb(&dlg->dlg_callbacks, types, f, param, NULL);
}


/* per dialog callbacks receive only the transaction, send buffer, destination
 * and the retr. buff */
void run_trans_dlg_callbacks(dlg_t* dlg, struct cell* trans,
								struct retr_buf* rbuf)
{
	struct tmcb_params params;
	
	if (dlg->dlg_callbacks.first==0)
		return;
	memset(&params, 0, sizeof(params));
	if (rbuf){
		params.t_rbuf=rbuf;
		params.dst=&rbuf->dst;
		params.send_buf.s=rbuf->buffer;
		params.send_buf.len=rbuf->buffer_len;
	}
	
	run_trans_callbacks_internal(&dlg->dlg_callbacks, TMCB_DLG, trans, 
									&params);
}

/* TMCB_DESTROY per dialog callbacks */
static void destroy_trans_dlg_callbacks(dlg_t* dlg)
{
	struct tmcb_params params;
	struct tm_callback* cbp;
	
	if ((dlg->dlg_callbacks.first==0) || 
			((dlg->dlg_callbacks.reg_types&TMCB_DESTROY)==0) )
		return;
	memset(&params, 0, sizeof(params));
	for (cbp=(struct tm_callback*)dlg->dlg_callbacks.first;cbp;cbp=cbp->next){
		if (cbp->types & TMCB_DESTROY){
			params.param=&(cbp->param);
			cbp->callback(0, 0, &params);
		}
	}
}
#endif /* DIALOG_CALLBACKS */

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
 * @return:
 *  negative : error
 *  0 : no routes present
 *  F_RB_NH_LOOSE : routes present, next hop is loose router
 *  F_RB_NH_STRICT: next hop is strict.
 */
static inline int calculate_hooks(dlg_t* _d)
{
	str* uri;
	struct sip_uri puri;
	int nhop;

	/* we might re-calc. some existing hooks =>
	 * reset all the hooks to 0 */
	memset(&_d->hooks, 0, sizeof(_d->hooks));
	if (_d->route_set) {
		uri = &_d->route_set->nameaddr.uri;
		if (parse_uri(uri->s, uri->len, &puri) < 0) {
			LOG(L_ERR, "calculate_hooks(): Error while parsing URI\n");
			return -1;
		}
		
		if (puri.lr.s) {
			if (_d->rem_target.s) _d->hooks.request_uri = &_d->rem_target;
			else _d->hooks.request_uri = &_d->rem_uri;
			_d->hooks.next_hop = &_d->route_set->nameaddr.uri;
			_d->hooks.first_route = _d->route_set;
			nhop = F_RB_NH_LOOSE;
		} else {
			_d->hooks.request_uri = &_d->route_set->nameaddr.uri;
			_d->hooks.next_hop = _d->hooks.request_uri;
			_d->hooks.first_route = _d->route_set->next;
			if (_d->rem_target.len > 0) 
				_d->hooks.last_route = &_d->rem_target;
			else 
				_d->hooks.last_route = NULL; /* ? */
			nhop = F_RB_NH_STRICT;
		}
	} else {
		if (_d->rem_target.s) _d->hooks.request_uri = &_d->rem_target;
		else _d->hooks.request_uri = &_d->rem_uri;
		
		if (_d->dst_uri.s) _d->hooks.next_hop = &_d->dst_uri;
		else _d->hooks.next_hop = _d->hooks.request_uri;

		nhop = 0;
		/*
		 * the routes in the hooks need to be reset because if the route_set 
		 * was dropped somewhere else then these will remain set without the
		 * actual routes existing any more
		 */
		_d->hooks.first_route = 0;
		_d->hooks.last_route = 0;
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

	return nhop;
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
	str generated_cid;
	str generated_ltag;

	if (!_cid) { /* if not given, compute new one */
		generate_callid(&generated_cid);
		_cid = &generated_cid;
	}
	if (_cid && (!_ltag)) { /* if not given, compute new one */
		generate_fromtag(&generated_ltag, _cid);
		_ltag = &generated_ltag;
	}
	if (_lseq == 0) _lseq = DEFAULT_CSEQ;

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
#ifdef DIALOG_CALLBACKS
	run_new_dlg_callbacks(DLG_CB_UAC, res, 0);
#endif
	
	return 0;
}

/**
 * @brief Store display names into a dialog
 * @param _d - dialog structure
 * @param _ldname - local party display name
 * @param _rdname - remote party dispaly name
 * @return 0 on success; negative on error
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

/*
 * Parse Contact header field body and extract URI
 * Does not parse headers !!
 */
static inline int get_contact_uri(struct sip_msg* _m, str* _uri)
{
	contact_t* c;

	_uri->len = 0;
	_uri->s = 0;

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

static rr_t *revert_route(rr_t *r)
{
	rr_t *a, *b;

	a = NULL;

	while (r) {
		b = r->next;
		r->next = a;
		a = r;
		r = b;
	}

	return a;
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
		if (ptr->type == HDR_RECORDROUTE_T) {
			if (parse_rr(ptr) < 0) {
				LOG(L_ERR, "get_route_set(): Error while parsing Record-Route body\n");
				goto error;
			}

			p = (rr_t*)ptr->parsed;
			if (shm_duplicate_rr(&t, p) < 0) {
				LOG(L_ERR, "get_route_set(): Error while duplicating rr_t\n");
				goto error;
			}
			if (!*_rs) *_rs = t;
			if (last) last->next = t;
			last = t;
			while (last->next) last = last->next; /* !!! there may be more routes in one hdr field !!! */

		}
		ptr = ptr->next;
	}
	if ((*_rs) && (_order != NORMAL_ORDER)) {
		/* better to revert the route outside of cycle above */
		*_rs = revert_route(*_rs);
	}
	
	return 0;

 error:
	shm_free_rr(_rs);
	return -1;
}

/*
 * Extract method from CSeq header field
 */
static inline int get_cseq_method(struct sip_msg* _m, str* _method)
{
	if (!_m->cseq && ((parse_headers(_m, HDR_CSEQ_F, 0) == -1) || !_m->cseq)) {
		LOG(L_ERR, "get_cseq_method(): Error while parsing CSeq\n");
		return -1;
	}

	_method->s = get_cseq(_m)->method.s;
	_method->len = get_cseq(_m)->method.len;
	return 0;
}


static inline int refresh_dialog_resp(struct sip_msg* _m, 
		target_refresh_t is_target_refresh)
{
	str method;
	
	switch (is_target_refresh) {
		case IS_NOT_TARGET_REFRESH: 
			return 0;
		case IS_TARGET_REFRESH: 
			return 1;
		case TARGET_REFRESH_UNKNOWN: 
			if (get_cseq_method(_m, &method) < 0) return 0; /* error */
			if ((method.len == 6) && !memcmp("INVITE", method.s, 6)) 
				return 1;
			else return 0;
	}
	return 0;
}
		
static inline int refresh_dialog_req(struct sip_msg* _m, target_refresh_t is_target_refresh)
{
	switch (is_target_refresh) {
		case IS_NOT_TARGET_REFRESH: 
			return 0;
		case IS_TARGET_REFRESH: 
			return 1;
		case TARGET_REFRESH_UNKNOWN: 
			return (_m->first_line.u.request.method_value == METHOD_INVITE);
			break;
	}
	return 0;
}

/*
 * Extract all necessary information from a response and put it
 * in a dialog structure
 */
static inline int response2dlg(struct sip_msg* _m, dlg_t* _d)
{
	str contact, rtag;

	     /* Parse the whole message, we will need all Record-Route headers */
	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR, "response2dlg(): Error while parsing headers\n");
		return -1;
	}
	
	if (get_contact_uri(_m, &contact) < 0) return -2;
	if (_d->rem_target.s) {
		shm_free(_d->rem_target.s);
		_d->rem_target.s = 0; 
		_d->rem_target.len = 0;
	}
	if (_d->dst_uri.s) {
		shm_free(_d->dst_uri.s);
		_d->dst_uri.s = 0; 
		_d->dst_uri.len = 0;
	}
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
		/* Warning - we can handle here response for non-initial request (for
		 * example UPDATE within early INVITE/BYE dialog) and move into
		 * confirmed state may be error! But this depends on dialog type... */
		
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
 * Handle dialog in DLG_CONFIRMED state, we will be processing
 * a response to a request sent within a dialog
 */
static inline int dlg_confirmed_resp_uac(dlg_t* _d, struct sip_msg* _m, 
		target_refresh_t is_target_refresh)
{
	int code;
	str contact;

	code = _m->first_line.u.reply.statuscode;	

	     /* Dialog has been already confirmed, that means we received
	      * a response to a request sent within the dialog. We will
	      * update remote target URI if and only if the message sent was
	      * a target refresher. 
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
	
	if (refresh_dialog_resp(_m, is_target_refresh)) {
		     /* Get contact if any and update remote target */
		if (parse_headers(_m, HDR_CONTACT_F, 0) == -1) {
			LOG(L_ERR, "dlg_confirmed_resp_uac(): Error while parsing headers\n");
			return -2;
		}

		     /* Try to extract contact URI */
		if (get_contact_uri(_m, &contact) < 0) return -3;
		     /* If there is a contact URI */
		if (contact.len) {
			     /* Free old remote target and destination uri if any */
			if (_d->rem_target.s) shm_free(_d->rem_target.s);
			if (_d->dst_uri.s) {
				shm_free(_d->dst_uri.s);
				_d->dst_uri.s = 0;
				_d->dst_uri.len = 0;
			}
				
			     /* Duplicate new remote target */
			if (str_duplicate(&_d->rem_target, &contact) < 0) return -4;
		}

		if (calculate_hooks(_d) < 0)
			return -1;
	}

	return 0;
}


/*
 * A response arrived, update dialog
 */
int dlg_response_uac(dlg_t* _d, struct sip_msg* _m, 
		target_refresh_t is_target_refresh)
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
		return dlg_confirmed_resp_uac(_d, _m, is_target_refresh);

	case DLG_DESTROYED:
		LOG(L_DBG, "dlg_response_uac(): Cannot handle destroyed dialog\n");
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

	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR, "request2dlg(): Error while parsing headers");
		return -1;
	}

	if (get_contact_uri(_m, &contact) < 0) return -2;
	if (contact.len) {
		if (_d->rem_target.s) shm_free(_d->rem_target.s);
		if (_d->dst_uri.s) {
			shm_free(_d->dst_uri.s);
			_d->dst_uri.s = 0;
			_d->dst_uri.len = 0;
		}
		if (str_duplicate(&_d->rem_target, &contact) < 0) return -3;
	}
	
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

	if (_code > 299) {
		DBG("new_dlg_uas(): Status code >= 300, no dialog created\n");
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
		free_dlg(res);
		return -4;
	}

	if (_code > 100) {
		tag.s = tm_tags;
		tag.len = TOTAG_VALUE_LEN;
		calc_crc_suffix(_req, tm_tag_suffix);
		if (str_duplicate(&res->id.loc_tag, &tag) < 0) {
			free_dlg(res);
			return -5;
		}
	}
	
	*_d = res;

	if (_code < 100)
		(*_d)->state = DLG_NEW;
	else if (_code < 200)
		(*_d)->state = DLG_EARLY;
	else
		(*_d)->state = DLG_CONFIRMED;

	if (calculate_hooks(*_d) < 0) {
		LOG(L_ERR, "new_dlg_uas(): Error while calculating hooks\n");
		free_dlg(res);
		return -6;
	}
#ifdef DIALOG_CALLBACKS
	run_new_dlg_callbacks(DLG_CB_UAS, res, _req);
#endif

	return 0;
}

/*
 * UAS side - update dialog state and to tag
 */
int update_dlg_uas(dlg_t *_d, int _code, str* _tag)
{
	if (_d->state == DLG_CONFIRMED) {
		LOG(L_ERR, "update_dlg_uas(): Dialog is already confirmed\n");
		return -1;
	} else if (_d->state == DLG_DESTROYED) {
		LOG(L_ERR, "update_dlg_uas(): Dialog is already destroyed\n");
		return -2;
	}

	if (_tag && _tag->s) {
		if (_d->id.loc_tag.s) {
			if ((_tag->len == _d->id.loc_tag.len)
			&& (!memcmp(_tag->s, _d->id.loc_tag.s, _tag->len))) {
				LOG(L_DBG, "update_dlg_uas(): Local tag is already set\n");
			} else {
				LOG(L_ERR, "update_dlg_uas(): ERROR: trying to rewrite local tag\n");
				return -3;
			}
		} else {
			if (str_duplicate(&_d->id.loc_tag, _tag) < 0) {
				LOG(L_ERR, "update_dlg_uas(): Not enough memory\n");
				return -4;
			}
		}
	}

	if ((100 < _code) && (_code < 200))
		_d->state = DLG_EARLY;
	else if (_code < 300)
		_d->state = DLG_CONFIRMED;
	else
		_d->state = DLG_DESTROYED;

	return 0;
}

/*
 * UAS side - update a dialog from a request
 */
int dlg_request_uas(dlg_t* _d, struct sip_msg* _m, target_refresh_t is_target_refresh)
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
	if (parse_headers(_m, HDR_CSEQ_F, 0) == -1) {
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
	if (refresh_dialog_req(_m, is_target_refresh)) { /* target refresher */
		if (parse_headers(_m, HDR_CONTACT_F, 0) == -1) {
			LOG(L_ERR, "dlg_request_uas(): Error while parsing headers\n");
			return -4;
		}
		
		if (get_contact_uri(_m, &contact) < 0) return -5;
		if (contact.len) {
			if (_d->rem_target.s) shm_free(_d->rem_target.s);
			if (_d->dst_uri.s) {
				shm_free(_d->dst_uri.s);
				_d->dst_uri.s = 0;
				_d->dst_uri.len = 0;
			}
			if (str_duplicate(&_d->rem_target, &contact) < 0) return -6;
		}

		if (calculate_hooks(_d) < 0)
			return -1;
		
	}

	return 0;
}


/*
 * Calculate length of the route set
 */
int calculate_routeset_length(dlg_t* _d)
{
	int len;
	rr_t *ptr;

	if (! _d->route_set)
		return 0;

	len = ROUTE_PREFIX_LEN;

	for (ptr = _d->hooks.first_route; ptr; ptr = ptr->next) {
		len += ptr->len;
		len += ROUTE_SEPARATOR_LEN;
	}
	if (_d->hooks.last_route) {
		if (_d->hooks.first_route)
			len += ROUTE_SEPARATOR_LEN;
		len += _d->hooks.last_route->len + 2; /* < > */
	} else {
		len -= ROUTE_SEPARATOR_LEN;
	}

	len += CRLF_LEN;

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
#ifdef DIALOG_CALLBACKS
	destroy_trans_dlg_callbacks(_d);
#endif
	if (_d->id.call_id.s) shm_free(_d->id.call_id.s);
	if (_d->id.rem_tag.s) shm_free(_d->id.rem_tag.s);
	if (_d->id.loc_tag.s) shm_free(_d->id.loc_tag.s);

	if (_d->loc_uri.s) shm_free(_d->loc_uri.s);
	if (_d->rem_uri.s) shm_free(_d->rem_uri.s);
	if (_d->rem_target.s) shm_free(_d->rem_target.s);
	if (_d->dst_uri.s) shm_free(_d->dst_uri.s);

	if (_d->loc_dname.s) shm_free(_d->loc_dname.s);
	if (_d->rem_dname.s) shm_free(_d->rem_dname.s);

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
	fprintf(out, "dst_uri       : '%.*s'\n", _d->dst_uri.len, _d->dst_uri.s);
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

/*
 * set dialog's request uri and destination uri (optional)
 */
int set_dlg_target(dlg_t* _d, str* _ruri, str* _duri) {

	if (!_d || !_ruri) {
		LOG(L_ERR, "set_dlg_target(): Invalid parameter value\n");
		return -1;
	}

	if (_d->rem_target.s) shm_free(_d->rem_target.s);
	if (_d->dst_uri.s) {
		shm_free(_d->dst_uri.s);
		_d->dst_uri.s = 0;
		_d->dst_uri.len = 0;
	}

	if (str_duplicate(&_d->rem_target, _ruri)) return -1;
	if (_duri && _duri->len) {
		if (str_duplicate(&_d->dst_uri, _duri)) return -1;
	}

	if (calculate_hooks(_d) < 0) {
		LOG(L_ERR, "set_dlg_target(): Error while calculating hooks\n");
		return -1;
	}

	return 0;
}

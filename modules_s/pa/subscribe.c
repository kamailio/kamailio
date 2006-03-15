/*
 * Presence Agent, subscribe handling
 *
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
 * ---------
 * 2003-02-29 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */

#include <string.h>
#include <limits.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "../../parser/parse_content.h"
#include "../../data_lump_rpl.h"
#include "presentity.h"
#include "watcher.h"
#include "pstate.h"
#include "notify.h"
#include "paerrno.h"
#include "pdomain.h"
#include "pa_mod.h"
#include "ptime.h"
#include "reply.h"
#include "subscribe.h"
#include "auth.h"
#include <cds/sstr.h>
#include <cds/msg_queue.h>
#include <cds/logger.h>

#define DOCUMENT_TYPE "application/cpim-pidf+xml"
#define DOCUMENT_TYPE_L (sizeof(DOCUMENT_TYPE) - 1)

typedef struct {
	int event_type;
	int mimes[MAX_MIMES_NR];
} event_mimetypes_t;

static event_mimetypes_t event_package_mimetypes[] = {
	{ EVENT_PRESENCE, {
			MIMETYPE(APPLICATION,PIDFXML), 
/*		    MIMETYPE(APPLICATION,XML_MSRTC_PIDF), */
/*		    MIMETYPE(TEXT,XML_MSRTC_PIDF), */
			MIMETYPE(APPLICATION,CPIM_PIDFXML), 
		    MIMETYPE(APPLICATION,XPIDFXML), 
			MIMETYPE(APPLICATION,LPIDFXML), 
			0 } },
	{ EVENT_PRESENCE_WINFO, { 
			MIMETYPE(APPLICATION,WATCHERINFOXML), 
			0 } },
/*	{ EVENT_SIP_PROFILE, { 
			MIMETYPE(MESSAGE,EXTERNAL_BODY), 
			0 } }, */
/*	{ EVENT_XCAP_CHANGE, { MIMETYPE(APPLICATION,WINFO+XML), 0 } }, */
	{ -1, { 0 }},
};

static void free_tuple_change_info_content(tuple_change_info_t *i)
{
	str_free_content(&i->user);
	str_free_content(&i->contact);
}

/*
 * contact will be NULL if user is offline
 * fixme:locking
 */
void callback(str* _user, str *_contact, int state, void* data)
{
	mq_message_t *msg;
	tuple_change_info_t *info;

	if ((!_user) || (!_contact) || (!data)) {
		ERROR_LOG("callback(): error!\n");
	}
		
	DBG("callback(): user=%.*s, contact=%.*s, %p, state=%d!\n",
			FMT_STR(*_user), FMT_STR(*_contact), data, state);
	
	/* asynchronous processing */
	msg = create_message_ex(sizeof(tuple_change_info_t));
	if (!msg) {
		LOG(L_ERR, "can't create message with tuple status change\n");
		return;
	}
	set_data_destroy_function(msg, (destroy_function_f)free_tuple_change_info_content);
	info = get_message_data(msg);
	if (state == 0) info->state = PS_OFFLINE;
	else info->state = PS_ONLINE;
	str_dup(&info->user, _user);
	str_dup(&info->contact, _contact);
	if (data) push_message(&((struct presentity*)data)->mq, msg);
}

/*
 * Extract plain uri -- return URI without parameters
 * The uri will be in form username@domain
 *
 */
static int extract_plain_uri(str* _uri)
{
	struct sip_uri puri;
	int res = 0;

	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		paerrno = PA_URI_PARSE;
		LOG(L_ERR, "extract_plain_uri(): Error while parsing URI\n");
		return -1;
	}
	
	/* _uri->s = puri.user.s;
	if ((!_uri->s) || (puri.user.len < 1)) {
		_uri->s = puri.host.s;
		_uri->len = puri.host.len;
		return -1;
	}*/
	if (puri.user.len < 1) {
		res = -1; /* it is uri without username ! */
	}
	_uri->len = puri.host.s + puri.host.len - _uri->s;
	return res;
}


/*
 * Get presentity URI, which is stored in R-URI
 */
int get_pres_uri(struct sip_msg* _m, str* _puri)
{
	if (_m->new_uri.s) {
		_puri->s = _m->new_uri.s;
		_puri->len = _m->new_uri.len;
	} else {
		_puri->s = _m->first_line.u.request.uri.s;
		_puri->len = _m->first_line.u.request.uri.len;
	}
	LOG(L_DBG, "get_pres_uri: _puri=%.*s\n", _puri->len, _puri->s);

	if (extract_plain_uri(_puri) < 0) {
		_puri->s = get_to(_m)->uri.s;
		_puri->len = get_to(_m)->uri.len;
		LOG(L_DBG, "get_pres_uri(2): _puri=%.*s\n", _puri->len, _puri->s);
	
		if (extract_plain_uri(_puri) < 0) {
			LOG(L_ERR, "get_pres_uri(): Error while extracting plain URI\n");
			return -1;
		}
	}

	return 0;
}


static int get_watch_uri(struct sip_msg* _m, str* _wuri, str *_dn)
{
	_wuri->s = get_from(_m)->uri.s;
	_wuri->len = get_from(_m)->uri.len;
	_dn->s = get_from(_m)->body.s;
	_dn->len = get_from(_m)->body.len;

	if (extract_plain_uri(_wuri) < 0) {
		LOG(L_ERR, "get_watch_uri(): Error while extracting plain URI\n");
		return -1;
	}
	
	return 0;
}

static int get_dlg_id(struct sip_msg *_m, dlg_id_t *dst)
{
	if (!dst) return -1;
	
	memset(dst, 0, sizeof(*dst));
	if (_m->to) dst->loc_tag = ((struct to_body*)_m->to->parsed)->tag_value;
	if (_m->from) dst->rem_tag = ((struct to_body*)_m->from->parsed)->tag_value;
	if (_m->callid) dst->call_id = _m->callid->body;
	
	return 0;
}

/*
 * Parse all header fields that will be needed
 * to handle a SUBSCRIBE request
 */
static int parse_hfs(struct sip_msg* _m, int accept_header_required)
{
	int rc = 0;
	struct hdr_field *acc;
	
	/* EOH instead HDR_FROM_F | HDR_EVENT_F | HDR_EXPIRES_F | HDR_ACCEPT_F  
	 * because we need all Accept headers */
	if ( (rc = parse_headers(_m, HDR_EOH_F, 0)) == -1) {
		paerrno = PA_PARSE_ERR;
		LOG(L_ERR, "parse_hfs(): Error while parsing headers: rc=%d\n", rc);
		return -1;
	}

	if (!_m->from) {
		ERR("From header missing\n");
		return -1;
	}
	
	if (parse_from_header(_m) < 0) {
		paerrno = PA_FROM_ERR;
		LOG(L_ERR, "parse_hfs(): From malformed or missing\n");
		return -6;
	}

	if (_m->event) {
		if (parse_event(_m->event) < 0) {
			paerrno = PA_EVENT_PARSE;
			LOG(L_ERR, "parse_hfs(): Error while parsing Event header field\n");
			return -8;
		}
	}
	else { 
		paerrno = PA_EVENT_PARSE;
		LOG(L_ERR, "parse_hfs(): Error while parsing Event header field\n");
		return -8;
	}

	if (_m->expires) {
		if (parse_expires(_m->expires) < 0) {
			paerrno = PA_EXPIRES_PARSE;
			LOG(L_ERR, "parse_hfs(): Error while parsing Expires header field\n");
			return -9;
		}
	}

	/* now look for Accept header */
	acc = _m->accept;
	if (accept_header_required && (!acc)) {
		LOG(L_ERR, "no accept header\n");
		return -11;
	}
	
	while (acc) { /* parse all accept headers */
		if (acc->type == HDR_ACCEPT_T) {
			DBG("parsing accept header: %.*s\n", FMT_STR(acc->body));
			if (parse_accept_body(acc) < 0) {
				paerrno = PA_ACCEPT_PARSE;
				LOG(L_ERR, "parse_hfs(): Error while parsing Accept header field\n");
				return -10;
			}
		}
		acc = acc->next;
	}

	return 0;
}

/* returns -1 if m is NOT found in mimes
 * index to the found element otherwise (non-negative value) */
static int find_mime(int *mimes, int m)
{
	int i;
	for (i = 0; mimes[i]; i++) {
		if (mimes[i] == m) return i;
	}
	return -1;
}

static event_mimetypes_t *find_event_mimetypes(int et)
{
	int i;
	event_mimetypes_t *em;
		
	i = 0;
	while (et != event_package_mimetypes[i].event_type) {
		if (event_package_mimetypes[i].event_type == -1) break;
		i++;
	}
	em = &event_package_mimetypes[i]; /* if not found is it the "separator" (-1, 0)*/
	return em;
}

static int check_mime_types(int *accepts_mimes, event_mimetypes_t *em)
{
	/* LOG(L_ERR, "check_message -2- accepts_mimes=%p\n", accepts_mimes); */
	if (accepts_mimes) {
		int j = 0, k;
		int mimetype;
		while ((mimetype = em->mimes[j]) != 0) {
			k = find_mime(accepts_mimes, mimetype);
			if (k >= 0) {
				int am0 = accepts_mimes[0];
				/* we have a match */
				/*LOG(L_ERR, "check_message -4b- eventtype=%#x accepts_mime=%#x\n", eventtype, mimetype); */
				/* move it to front for later */
				accepts_mimes[0] = mimetype;
				accepts_mimes[k] = am0;
				return 0; /* ! this may be useful, but it modifies the parsed content !!! */
			}
			j++;
		}
		
		return -1;
	}
	return 0;
}

/*
 * Check if a message received has been constructed properly
 */
int check_message(struct sip_msg* _m)
{
	event_t *parsed_event;
	int eventtype = 0;
	int *accepts_mimes = NULL;
	event_mimetypes_t *em;
	struct hdr_field *acc;

	if ((!_m->event) || (!_m->event->parsed)) {
		paerrno = PA_EXPIRES_PARSE;
		LOG(L_ERR, "check_message(): Event header field not found\n");
		return -1; /* should be verified in parse_hfs before */
	}

	/* event package verification */
	parsed_event = (event_t*)(_m->event->parsed);
	eventtype = parsed_event->parsed;
	em = find_event_mimetypes(eventtype);
	if (em) 
		if (em->event_type == -1) em = NULL;
	if (!em) {
		paerrno = PA_EVENT_UNSUPP;
		LOG(L_ERR, "check_message(): Unsupported event package\n");
		return -1;
	}

	acc = _m->accept;
	if (!acc) return 0; /* default will be used */
	
	while (acc) { /* go through all Accept headers */
		if (acc->type == HDR_ACCEPT_T) {
			/* it MUST be parsed from parse_hdr !!! */
			accepts_mimes = acc->parsed;
			if (check_mime_types(accepts_mimes, em) == 0) return 0;
			
			/* else, none of the mimetypes accepted are generated for this event package */
			LOG(L_INFO, "check_message(): Accepts %.*s not valid for event package et=%.*s\n",
				acc->body.len, acc->body.s, _m->event->body.len, _m->event->body.s);
		}
		acc = acc->next;
	}
	paerrno = PA_WRONG_ACCEPTS;
	LOG(L_ERR, "no satisfactory document type found\n");
	return -1;
}

/* returns index of mimetype, the lowest index = highest priority */
static int get_accepted_mime_type_idx(int *accepts_mimes, event_mimetypes_t *em)
{
	int i, mt;
	if (accepts_mimes) {
		/* try find "preferred" mime type */
		i = 0;
		while ((mt = em->mimes[i]) != 0) {
			/* TRACE_LOG("searching for %x\n", mt); */
			if (find_mime(accepts_mimes, mt) >= 0) return i;
			i++;
		}
	}
	return -1;
}

int get_preferred_event_mimetype(struct sip_msg *_m, int et)
{
	int idx, tmp, acc = 0;
	int *accepts_mimes;
	struct hdr_field *accept;

	event_mimetypes_t *em = find_event_mimetypes(et);
	if (!em) return 0; /* never happens, but ... */

	accept = _m->accept;
	idx = -1;
	while (accept) { /* go through all Accept headers */
		if (accept->type == HDR_ACCEPT_T) {
			/* it MUST be parsed from parse_hdr !!! */
			accepts_mimes = (int *)accept->parsed;
			tmp = get_accepted_mime_type_idx(accepts_mimes, em);
			if (idx == -1) idx = tmp;
			else
				if ((tmp != -1) && (tmp < idx)) idx = tmp;
			/* TRACE_LOG("%s: found mimetype %x (idx %d), %p\n", __FUNCTION__, (idx >= 0) ? em->mimes[idx]: -1, idx, accepts_mimes); */
			if (idx == 0) break; /* the lowest value */
		}
		accept = accept->next;
	}
	if (idx != -1) return em->mimes[idx]; /* found value with highest priority */

	acc = em->mimes[0];
	DBG("defaulting to mimetype %x for event_type=%d\n", acc, et);
	return acc;
}

int extract_server_contact(struct sip_msg *m, str *dst)
{
	char *tmp = "";
	if (!dst) return -1;

	switch(m->rcv.bind_address->proto){ 
		case PROTO_NONE: break;
		case PROTO_UDP: break;
		case PROTO_TCP: tmp = ";transport=tcp";	break;
		case PROTO_TLS: tmp = ";transport=tls"; break;
		case PROTO_SCTP: tmp = ";transport=sctp"; break;
		default: LOG(L_CRIT, "BUG: extract_server_contact: unknown proto %d\n", m->rcv.bind_address->proto); 
	}
	
	dst->len = 7 + m->rcv.bind_address->name.len + m->rcv.bind_address->port_no_str.len + strlen(tmp);
	dst->s = (char *)mem_alloc(dst->len + 1);
	if (!dst->s) {
		dst->len = 0;
		return -1;
	}
	snprintf(dst->s, dst->len + 1, "<sip:%.*s:%.*s%s>",
			m->rcv.bind_address->name.len, m->rcv.bind_address->name.s,
			m->rcv.bind_address->port_no_str.len, m->rcv.bind_address->port_no_str.s,
			tmp);

	return 0;
}

static int create_watcher(struct sip_msg* _m, struct presentity* _p, struct watcher** _w, int et, time_t expires)
{
	dlg_t* dialog;
	str server_contact = STR_NULL;
	int acc = 0;
	str watch_uri;
	str watch_dn;
	int res;
	
	if (get_watch_uri(_m, &watch_uri, &watch_dn) < 0) {
		paerrno = PA_URI_PARSE;
		LOG(L_ERR, "create_watcher(): Error while extracting watcher URI\n");
		return -1;
	}
	acc = get_preferred_event_mimetype(_m, et);
	if (tmb.new_dlg_uas(_m, 200, &dialog) < 0) {
		paerrno = PA_DIALOG_ERR;
		LOG(L_ERR, "create_watcher(): Error while creating dialog state\n");
		return -4;
	}
	
	if (extract_server_contact(_m, &server_contact) != 0) {
		paerrno = PA_DIALOG_ERR;
		LOG(L_ERR, "create_watcher(): Error while extracting server contact\n");
		return -3;
	}

	if (new_watcher_no_wb(_p, &watch_uri, expires, et, acc, dialog, &watch_dn, &server_contact, _w) < 0) {
		LOG(L_ERR, "create_watcher(): Error while creating watcher\n");
		tmb.free_dlg(dialog);
		if (server_contact.s) mem_free(server_contact.s);
		paerrno = PA_NO_MEMORY;
		return -5;
	}
	if (server_contact.s) mem_free(server_contact.s);
	
	(*_w)->status = authorize_watcher(_p, (*_w));
	if ((*_w)->status == WS_REJECTED) {
		free_watcher((*_w));
		LOG(L_ERR, "create_watcher(): watcher rejected\n");
		paerrno = PA_SUBSCRIPTION_REJECTED;
		return -6;
	}
	if ((*_w)->status == WS_PENDING) {
		paerrno = PA_OK_WAITING_FOR_AUTH;
	}

	res = 0;
	if (et == EVENT_PRESENCE_WINFO) res = add_winfo_watcher(_p, *_w);
	else res = add_watcher(_p, *_w);
	if (res < 0) {
		LOG(L_ERR, "create_watcher(): Error while adding watcher\n");
		free_watcher(*_w);
		paerrno = PA_INTERNAL_ERROR;
		return -5;
	}
	
	res = db_add_watcher(_p, *_w);
	if (res != 0) {
		if (et == EVENT_PRESENCE_WINFO) remove_winfo_watcher(_p, *_w);
		else remove_watcher(_p, *_w);
		LOG(L_ERR, "create_watcher(): Error while adding watcher into DB\n");
		free_watcher(*_w);
		paerrno = PA_INTERNAL_ERROR;
		return -7;
	}
	
	/* actualize watcher's status according to time */
	if ((*_w)->expires <= act_time) {
		LOG(L_DBG, "Created expired watcher %.*s\n", (*_w)->uri.len, (*_w)->uri.s);
		(*_w)->expires = 0;
		set_watcher_terminated_status(*_w);
	}
	
	return 0;
}

static int get_event(struct sip_msg *_m)
{
	int et = 0;
	event_t *event = NULL;
	
	if (_m->event) {
		event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		LOG(L_ERR, "update_presentity defaulting to EVENT_PRESENCE\n");
		et = EVENT_PRESENCE;
	}
	return et;
}

static time_t get_expires(struct sip_msg *_m)
{
	time_t e;
	
	if (_m->expires) {
		e = ((exp_body_t*)_m->expires->parsed)->val;
	} else {
		e = default_expires;
	}
	if (e > max_subscription_expiration) 
		e = max_subscription_expiration;
	return e;
}

/*
 * Create a new presentity and corresponding watcher list
 */
int create_presentity(struct sip_msg* _m, struct pdomain* _d, str* _puri, 
			     str *uid, struct presentity** _p, struct watcher** _w)
{
	time_t e;
	int et = 0;
	int res = 0;

	et = get_event(_m);
	e = get_expires(_m);

	if (verify_event_package(et) != 0) {
		LOG(L_ERR, "create_presentity(): Unsupported event package\n");
		paerrno = PA_EVENT_UNSUPP;
		return -1;
	}

	/* Convert to absolute time if nonzero (zero = polling) */
	if (e > 0) e += act_time;

	if (new_presentity(_d, _puri, uid, _p) < 0) {
		LOG(L_ERR, "create_presentity(): Error while creating presentity\n");
		return -2;
	}

	res = create_watcher(_m, *_p, _w, et, e);
	if (res != 0) {
		/* remove presentity from database !!*/
		release_presentity(*_p);
		return res;
	}

	return 0;
}

/*
 * Update existing presentity and watcher list
 */
static int update_presentity(struct sip_msg* _m, struct pdomain* _d, 
			     struct presentity* _p, struct watcher** _w, int is_renewal)
{
	time_t e;
	str watch_uri;
	str watch_dn;
	int et = 0;
	int res = 0;
	dlg_id_t dlg_id;

	et = get_event(_m);
	e = get_expires(_m);
	
	if (verify_event_package(et) != 0) {
		LOG(L_ERR, "update_presentity(): Unsupported event package\n");
		paerrno = PA_EVENT_UNSUPP;
		return -1;
	}

	if (get_watch_uri(_m, &watch_uri, &watch_dn) < 0) {
		LOG(L_ERR, "update_presentity(): Error while extracting watcher URI\n");
		return -1;
	}

	/* LOG(L_ERR, "update_presentity: after get_watch_uri\n"); */
	
	get_dlg_id(_m, &dlg_id);
	
	/* LOG(L_ERR, "update_presentity: searching watcher\n"); */
	
	if (find_watcher_dlg(_p, &dlg_id, et, _w) == 0) { 
		/* LOG(L_ERR, "update_presentity: watcher found\n"); */
		if (e > 0) e += act_time;
		if (update_watcher(_p, *_w, e) < 0) {
			LOG(L_ERR, "update_presentity(): Error while updating watcher\n");
			return -3;
		}
	} else {
		if (is_renewal) {
			ERR("resubscription for nonexisting watcher\n");
			paerrno = PA_SUBSCRIPTION_NOT_EXISTS;
			return -1;
		}
		/* LOG(L_ERR, "update_presentity: watcher not found\n"); */
		if (e) e += act_time;
		res = create_watcher(_m, _p, _w, et, e);
	}

	return res;
}


/* FIXME: remove */
#if 0

/*
 * Handle a registration request -- make sure aor exists in presentity table
 */
/*
 * Extract Address of Record
 */
#define MAX_AOR_LEN 256

static int pa_extract_aor(str* _uri, str* _a)
{
	static char aor_buf[MAX_AOR_LEN];
	struct sip_uri puri;
	int user_len;

	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		LOG(L_ERR, "pa_extract_aor(): Error while parsing Address of Record\n");
		return -1;
	}
	
	if ((puri.user.len + puri.host.len + 1) > MAX_AOR_LEN) {
		LOG(L_ERR, "pa_extract_aor(): Address Of Record too long\n");
		return -2;
	}

	_a->s = aor_buf;
	_a->len = puri.user.len;

	user_len = _a->len;

	memcpy(aor_buf, puri.user.s, puri.user.len);
	aor_buf[_a->len] = '@';
	memcpy(aor_buf + _a->len + 1, puri.host.s, puri.host.len);
	_a->len += 1 + puri.host.len;

#if 0
	if (case_sensitive) {
		tmp.s = _a->s + user_len + 1;
		tmp.len = puri.host.len;
		strlower(&tmp);
	} else {
		strlower(_a);
	}
#endif

	return 0;
}

int pa_handle_registration(struct sip_msg* _m, char* _domain, char* _s2)
{
     struct pdomain* d = (struct pdomain*)_domain;
     struct presentity *presentity;
     str p_uri;
     struct to_body *from = NULL;
     int e = 0;


     // LOG(L_ERR, "pa_handle_registration() entered\n");
     paerrno = PA_OK;

     d = (struct pdomain*)_domain;

     if (parse_hfs(_m, 0) < 0) {
	  paerrno = PA_PARSE_ERR;
	  LOG(L_ERR, "pa_handle_registration(): Error while parsing headers\n");
	  return -1;
     }

     from = get_from(_m);
     if (!from || (pa_extract_aor(&from->uri, &p_uri) < 0)) {
	  LOG(L_ERR, "pa_handle_registration(): Error while extracting Address Of Record\n");
	  goto error;
     }

     if (_m->expires) {
	  e = ((exp_body_t*)_m->expires->parsed)->val;
     } else {
	  e = default_expires;
     }

     if (from)
       LOG(L_ERR, "pa_handle_registration: from=%.*s p_uri=%.*s expires=%d\n", 
	   from->uri.len, from->uri.s, p_uri.len, p_uri.s, e);

     lock_pdomain(d);
	
     if (find_presentity(d, &p_uri, &presentity) > 0) {
	  LOG(L_ERR, "pa_handle_registration: find_presentity did not find presentity\n");
	  if (e > 0) {
	       if (create_presentity_only(_m, d, &p_uri, &presentity) < 0) {
		    LOG(L_ERR, "pa_handle_registration(): Error while creating new presentity\n");
		    goto error2;
	       }
	  } 
#if 0
	  else {
	       presence_tuple_t *tuple = NULL;
	       if (_m->contact) {
		    struct hdr_field* ptr = _m->contact;
		    while (ptr) {
			 if (ptr->type == HDR_CONTACT_T) {
			      if (!ptr->parsed && (parse_contact(ptr) < 0)) {
				   goto next;
			      }
			 }
			 if (find_presence_tuple(contact, presentity, &tuple) == 0) {
			      tuple->state = PS_OFFLINE;
			 }
		    next:
			 ptr = ptr->next;
		    }
	       }

	       db_update_presentity(presentity);
	  }
#endif
     }

     if (presentity && e > 0) {
	  LOG(L_ERR, "pa_handle_registration about to call d->reg p=%p expires=%d", presentity, e);
	  d->reg(&presentity->uri, &presentity->uri, (void*)callback, presentity);
     }

     /* LOG(L_ERR, "pa_handle_registration about to return 1"); */
     unlock_pdomain(d);
     return 1;
	
 error2:
     LOG(L_DBG, "pa_handle_registration about to return -1\n");
     unlock_pdomain(d);
     return -1;
 error:
     LOG(L_DBG, "pa_handle_registration about to return -2\n");
     return -1;
}
#endif

/**
 * Verifies presence of the To-tag in message. Returns 1 if
 * the tag is present, 0 if not, -1 on error.
 */
static int has_to_tag(struct sip_msg *_m)
{
	struct to_body *to = (struct to_body*)_m->to->parsed;
	if (!to) return 0;
	if (to->tag_value.len > 0) return 1;
	return 0;
}

/*
 * Handle a subscribe Request
 */
int handle_subscription(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity *p;
	struct watcher* w;
	str p_uri = STR_NULL;
	str uid = STR_NULL;
	char tmp[64];
	int i;
	int is_renewal = 0;
	
	get_act_time();
	paerrno = PA_OK;

	if (parse_hfs(_m, 0) < 0) {
		ERR("Error while parsing message header\n");
		goto error;
	}

	if (check_message(_m) < 0) {
		ERR("Error while checking message\n");
		goto error;
	}

	is_renewal = has_to_tag(_m);

	d = (struct pdomain*)_domain;

	if (get_pres_uri(_m, &p_uri) < 0) {
		ERR("Error while extracting presentity URI\n");
		goto error;
	}

	if (get_presentity_uid(&uid, _m) != 0) {
		ERR("Error while extracting presentity UID\n");
		goto error;
	}

	lock_pdomain(d);

	if (find_presentity_uid(d, &uid, &p) > 0) {
		if (is_renewal) {
			ERR("Resubscription to nonexisting presentity\n");
			paerrno = PA_SUBSCRIPTION_NOT_EXISTS;
			goto error2;
		}
		if (create_presentity(_m, d, &p_uri, &uid, &p, &w) < 0) {
			ERR("Error while creating new presentity\n");
			goto error2;
		}
	} else {
		if (update_presentity(_m, d, p, &w, is_renewal) < 0) {
			ERR("Error while updating presentity\n");
			goto error2;
		}
	}
	str_free_content(&uid);

	DBG("generating response\n");
	
	/* add expires header field into response */
	if (w) i = w->expires - act_time;
	else i = 0;
	if (i < 0) i = 0;
	sprintf(tmp, "Expires: %d\r\n", i);
	if (!add_lump_rpl(_m, tmp, strlen(tmp), LUMP_RPL_HDR)) {
		ERR("Can't add Expires header to the response\n");
		/* return -1; */
	}

	if (send_reply(_m) < 0) {
	  LOG(L_ERR, "handle_subscription(): Error while sending reply\n");
	  unlock_pdomain(d);
	  return -3;
	}

	if (p) {
		/* changed at least expiration times of watcher, they should be
		 * present in watcher info notifications => sending notifications
		 * is needed ! */
		p->flags |= PFLAG_WATCHERINFO_CHANGED; 
	}
	if (w) {
		w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
	}

	DBG("handle_subscription about to return 1: w->event_package=%d w->accept=%d p->flags=%x w->flags=%x w=%p\n",
	    (w ? w->event_package : -1), (w ? w->preferred_mimetype : -1), (p ? p->flags : -1), (w ? w->flags : -1), w);

	/* process and change this presentity and notify watchers */
	if (p && w) {
		/* immediately send NOTIFY */
		send_notify(p, w);
		w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED; /* already notified */

		/* remove terminated watcher otherwise he will 
		 * receive another NOTIFY generated from timer_pdomain */
		remove_watcher_if_expired(p, w);
	}
	
	unlock_pdomain(d);
	return 1;
	
 error2:
	str_free_content(&uid);
	ERR("handle_subscription about to return -1\n");
	unlock_pdomain(d);
	send_reply(_m);
	return -1;
 error:
	ERR("handle_subscription about to send_reply and return -2\n");
	send_reply(_m);
	return -1;
}

/* FIXME: remove */
#if 0

/*
 * Returns 1 if subscription exists and -1 if not
 */
int existing_subscription(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity* p;
	struct watcher* w;
	str p_uri, w_uri;
	str w_dn;
	int et = 0;
	dlg_id_t w_dlg_id;
	
	if (_m->event) {
		event_t *event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		LOG(L_ERR, "existing_subscription defaulting to EVENT_PRESENCE\n");
		et = EVENT_PRESENCE;
	}

	paerrno = PA_OK;

	if (parse_from_header(_m) < 0) {
		paerrno = PA_PARSE_ERR;
		LOG(L_ERR, "existing_subscription(): Error while parsing From header field\n");
		goto error;
	}

	d = (struct pdomain*)_domain;

	if (get_pres_uri(_m, &p_uri) < 0) {
		LOG(L_ERR, "existing_subscription(): Error while extracting presentity URI\n");
		goto error;
	}

	if (get_watch_uri(_m, &w_uri, &w_dn) < 0) {
		LOG(L_ERR, "existing_subscription(): Error while extracting watcher URI\n");
		goto error;
	}

	get_dlg_id(_m, &w_dlg_id);
	
	lock_pdomain(d);
	
	if (find_presentity(d, &p_uri, &p) == 0) {
		if (find_watcher_dlg(p, &w_dlg_id, et, &w) == 0) { 
			LOG(L_ERR, "existing_subscription() found watcher\n");
			unlock_pdomain(d);
			return 1;
		}
	}

	unlock_pdomain(d);
	return -1;

 error:
	send_reply(_m);       
	return 0;
}

#endif

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../data_lump_rpl.h"
#include "presentity.h"
#include "watcher.h"
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
#include <cds/sip_utils.h>
#include <presence/utils.h>
#include "offline_winfo.h"
#include "mimetypes.h"

#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../globals.h"
#include "../../md5.h"
#include "../../crc.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "../../modules/tm/ut.h"
#include "../../modules/tm/h_table.h"
#include "../../modules/tm/t_hooks.h"
#include "../../modules/tm/t_funcs.h"
#include "../../modules/tm/t_msgbuilder.h"
#include "../../modules/tm/callid.h"
#include "../../modules/tm/uac.h"

#define DOCUMENT_TYPE "application/cpim-pidf+xml"
#define DOCUMENT_TYPE_L (sizeof(DOCUMENT_TYPE) - 1)

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

static time_t get_expires(struct sip_msg *_m)
{
	time_t e;
	
	if (_m->expires) e = ((exp_body_t*)_m->expires->parsed)->val;
	else e = default_expires;
	if (e > max_subscription_expiration) 
		e = max_subscription_expiration;
	return e;
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
			/* can not parse Event header */
			paerrno = PA_EVENT_PARSE;
			LOG(L_ERR, "parse_hfs(): Error while parsing Event header field\n");
			return -8;
		}
	}
	else { 
		/* no Event header -> bad message */
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

/* returns 0 if package supported by PA */
static inline int verify_event_package(int et)
{
	switch (et) {
		case EVENT_PRESENCE: return 0;
		case EVENT_PRESENCE_WINFO: 
			if (watcherinfo_notify) return 0;
			else return -1;
		default: return -1;
	}
}

/* get_event MUST be parsed when calling get event -> done by parse_hfs! */
#define get_event(_m) ((event_t*)(_m->event->parsed))->parsed

/*
 * Check if a message received has been constructed properly
 */
static int check_message(struct sip_msg* _m)
{
	int eventtype = 0;
	int *accepts_mimes = NULL;
	event_mimetypes_t *em;
	struct hdr_field *acc;

	if ((!_m->event) || (!_m->event->parsed)) {
		paerrno = PA_EXPIRES_PARSE;
		ERR("Event header field not found\n");
		return -1; /* should be verified in parse_hfs before */
	}

	/* event package verification */
	eventtype = get_event(_m);
	
	if (verify_event_package(eventtype) != 0) {
		INFO("Unsupported event package\n");
		paerrno = PA_EVENT_UNSUPP;
		return -1;
	}

	em = find_event_mimetypes(eventtype);
	if (em) 
		if (em->event_type == -1) em = NULL;
	if (!em) {
		paerrno = PA_EVENT_UNSUPP;
		ERR("Unsupported event package\n");
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
			INFO("Accepts %.*s not valid for event package et=%.*s\n",
				acc->body.len, acc->body.s, _m->event->body.len, _m->event->body.s);
		}
		acc = acc->next;
	}
	paerrno = PA_WRONG_ACCEPTS;
	ERR("no satisfactory document type found\n");
	return -1;
}

static int create_watcher(struct sip_msg* _m, struct watcher** _w)
{
	dlg_t* dialog;
	str server_contact = STR_NULL;
	int acc = 0;
	str watch_uri;
	str watch_dn;
	int et;
	time_t expires;
	
	et = get_event(_m);	
	expires = get_expires(_m);
	if (expires) expires += act_time;
	
	if (get_watch_uri(_m, &watch_uri, &watch_dn) < 0) {
		paerrno = PA_URI_PARSE;
		ERR("Error while extracting watcher URI\n");
		return -1;
	}
	acc = get_preferred_event_mimetype(_m, et);
	if (tmb.new_dlg_uas(_m, 200, &dialog) < 0) {
		paerrno = PA_DIALOG_ERR;
		ERR("Error while creating dialog state\n");
		return -4;
	}
	
	if (extract_server_contact(_m, &server_contact, 1) != 0) {
		paerrno = PA_DIALOG_ERR;
		ERR("Error while extracting server contact\n");
		return -3;
	}

	if (new_watcher_no_wb(&watch_uri, expires, et, acc, dialog, 
				&watch_dn, &server_contact, NULL, _w) < 0) {
		ERR("Error while creating watcher\n");
		tmb.free_dlg(dialog);
		if (server_contact.s) mem_free(server_contact.s);
		paerrno = PA_NO_MEMORY;
		return -5;
	}
	if (server_contact.s) mem_free(server_contact.s);
	
	(*_w)->flags |= WFLAG_SUBSCRIPTION_CHANGED;
	
	return 0;
}


static inline int add_rpl_expires(struct sip_msg *_m, watcher_t *w)
{
	int i = 0;
	char tmp[64];
	
	/* add expires header field into response */
	if (w) i = w->expires - act_time;
	if (i < 0) i = 0;
	sprintf(tmp, "Expires: %d\r\n", i);
	if (!add_lump_rpl(_m, tmp, strlen(tmp), LUMP_RPL_HDR)) {
		ERR("Can't add Expires header to the response\n");
		return -1;
	}
	
	return 0;
}

int handle_renewal_subscription(struct sip_msg* _m, struct pdomain *d)
{
	struct presentity *p = NULL;
	struct watcher* w = NULL;
	str uid = STR_NULL;
	dlg_id_t dlg_id;
	int et, e;
	
	if (get_presentity_uid(&uid, _m) < 0) {
		ERR("Error while extracting presentity UID\n");
		paerrno = PA_INTERNAL_ERROR;
		goto err;
	}	

	/* needed to find watcher (better to set outside of crit. sect. due to
	 * better performance) */
	et = get_event(_m);	
	get_dlg_id(_m, &dlg_id);
	
	lock_pdomain(d);
		
	if (find_presentity_uid(d, &uid, &p) != 0) { 
		/* presentity not found */
		INFO("resubscription to nonexisting presentity %.*s\n", FMT_STR(uid));
		paerrno = PA_SUBSCRIPTION_NOT_EXISTS;
		goto err2;
	}
	
	if (find_watcher_dlg(p, &dlg_id, et, &w) != 0) {
		/* watcher not found */
		INFO("resubscription for nonexisting watcher\n");
		paerrno = PA_SUBSCRIPTION_NOT_EXISTS;
		goto err2;
	}
	
	e = get_expires(_m);
	if (e) e += act_time;
	
	update_watcher(p, w, e, _m);
	set_last_subscription_status(w->status);	
	add_rpl_expires(_m, w);
	if (send_reply(_m) >= 0) {
		/* we have successfully sent the response */
		if (send_notify(p, w) >= 0) {		
			w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED; /* notified */
			
			/* remove terminated watcher otherwise he will 
			 * receive another NOTIFY generated from timer_pdomain */
			if (is_watcher_terminated(w)) {
				remove_watcher(p, w);
				free_watcher(w);
			}
		} 
	}
	else {
		ERR("Error while sending reply\n");
	}
	
	unlock_pdomain(d);
	
	return 1;

err2:
	unlock_pdomain(d);
	
err:
	set_last_subscription_status(WS_REJECTED);
	
	if (send_reply(_m) < 0) ERR("Error while sending reply\n");
	return -1;
}

presentity_t *get_presentity(struct sip_msg *_m, struct pdomain *d, int allow_creation)
{
	str p_uri, uid;
	presentity_t *p = NULL;
	xcap_query_params_t xcap_params;
/*	presence_rules_t *auth_rules = NULL;*/

	if (get_presentity_uid(&uid, _m) < 0) {
		ERR("Error while extracting presentity UID\n");
		return NULL;
	}
	
	if (find_presentity_uid(d, &uid, &p) > 0)  {
		if (allow_creation) {
			if (get_pres_uri(_m, &p_uri) < 0) {
				ERR("Error while extracting presentity URI\n");
			}
			else {
				/* presentity not found -> create new presentity */
				memset(&xcap_params, 0, sizeof(xcap_params));
				if (fill_xcap_params) fill_xcap_params(_m, &xcap_params);
				if (new_presentity(d, &p_uri, &uid, &xcap_params, &p) < 0)
					ERR("Error while creating new presentity\n");
			}
		}
	}
	return p;
}

int handle_new_subscription(struct sip_msg* _m, struct pdomain *d)
{
	struct presentity *p;
	struct watcher* w;
	struct retr_buf *req;
	int is_terminated;
	
	if (create_watcher(_m, &w) < 0) {
		ERR("can't create watcher\n");
		goto err;
	}

	lock_pdomain(d);

	p = get_presentity(_m, d, 1);
	if (!p) goto err3;
	
	/* authorize watcher */
	w->status = authorize_watcher(p, w);
										   
	switch (w->status) {
		case WS_REJECTED:
			unlock_pdomain(d);
			free_watcher(w);
			paerrno = PA_SUBSCRIPTION_REJECTED;
			INFO("watcher rejected\n");
			goto err;
		case WS_PENDING:
		case WS_PENDING_TERMINATED:
			paerrno = PA_OK_WAITING_FOR_AUTH;
		default:
			break;
	}
	if (w->expires <= act_time) {
		set_watcher_terminated_status(w);
		is_terminated = 1;
	}
	else {
		is_terminated = 0;	

		if (append_watcher(p, w, 1) < 0) {
			ERR("can't add watcher\n");
			goto err3;
		}
	}
	
	if (prepare_notify(&req, p, w) < 0) {
		ERR("can't send notify\n");
		goto err4;
	}

	set_last_subscription_status(w->status);
	add_rpl_expires(_m, w);
	
	unlock_pdomain(d);

	send_reply(_m); 
	
	if (req) {
		tmb.send_prepared_request(req);
		w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED; /* notified */
		if (is_terminated) {
			free_watcher(w);
			w = NULL;
		} 
	}

	return 1;
	
err4:
	remove_watcher(p, w);

err3:
	unlock_pdomain(d);
	free_watcher(w);
	paerrno = PA_INTERNAL_ERROR;
	
err:
	set_last_subscription_status(WS_REJECTED);
	
	if (paerrno == PA_OK) paerrno = PA_INTERNAL_ERROR;
	if (send_reply(_m) < 0) ERR("Error while sending reply\n");
	return -1;
}

/*
 * Handle a subscribe Request
 */
int handle_subscription(struct sip_msg* _m, char* _domain, char* _s2)
{
	int res;
	PROF_START_DECL(pa_response_generation)

	PROF_START(pa_handle_subscription)

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

	if (has_to_tag(_m)) 
		res = handle_renewal_subscription(_m, (struct pdomain*)_domain);
	else
		res = handle_new_subscription(_m, (struct pdomain*)_domain);

	PROF_STOP(pa_handle_subscription)
	return res;
	
 error:
	INFO("handle_subscription about to send_reply and return -2\n");
	send_reply(_m);
	set_last_subscription_status(WS_REJECTED);
	PROF_STOP(pa_handle_subscription)
	return -1;
}


/*
 * Presence Agent, subscribe handling
 *
 * $Id$
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
 *
 * History:
 * ---------
 * 2003-02-29 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */

#include <string.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
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


#define DOCUMENT_TYPE "application/cpim-pidf+xml"
#define DOCUMENT_TYPE_L (sizeof(DOCUMENT_TYPE) - 1)


/*
 * A static variable holding document type accepted
 * by the watcher's user agent
 */
static doctype_t acc;


/*
 * FIXME: locking
 */
void callback(str* _user, int state, void* data)
{
	struct presentity* ptr;
	int orig;

	ptr = (struct presentity*)data;
	orig = ptr->state;

	if (state == 0) {
		ptr->state = PS_OFFLINE;
	} else {
		ptr->state = PS_ONLINE;
	}

	db_update_presentity(ptr);

	if (orig != state) {
		notify_watchers(ptr);
	}
}

/*
 * Extract plain uri -- return URI without parameters
 * The uri will be in form username@domain
 *
 */
static int extract_plain_uri(str* _uri)
{
	struct sip_uri puri;

	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		paerrno = PA_URI_PARSE;
		LOG(L_ERR, "extract_plain_uri(): Error while parsing URI\n");
		return -1;
	}
	
	_uri->s = puri.user.s;
	_uri->len = puri.host.s + puri.host.len - _uri->s;
	return 0;
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
	
	if (extract_plain_uri(_puri) < 0) {
		LOG(L_ERR, "get_pres_uri(): Error while extracting plain URI\n");
		return -1;
	}

	return 0;
}


static int get_watch_uri(struct sip_msg* _m, str* _wuri)
{
	_wuri->s = get_from(_m)->uri.s;
	_wuri->len = get_from(_m)->uri.len;

	if (extract_plain_uri(_wuri) < 0) {
		LOG(L_ERR, "get_watch_uri(): Error while extracting plain URI\n");
		return -1;
	}
	
	return 0;
}


/*
 * Parse Accept header field body
 * FIXME: This is ugly parser, write something more clean
 */
int parse_accept(struct hdr_field* _h, doctype_t* _a)
{
	if (_h) {
		char* buffer;

		/*
		 * All implementation must support xpidf so make
		 * it the default
		 */
		*_a = DOC_XPIDF;

		buffer = pkg_malloc(_h->body.len + 1);
		if (!buffer) {
			paerrno = PA_NO_MEMORY;
			LOG(L_ERR, "parse_accept(): No memory left\n");
			return -1;
		}

		memcpy(buffer, _h->body.s, _h->body.len);
		buffer[_h->body.len] = '\0';
	
		if (strstr(buffer, "application/pidf+xml")) {
			*_a = DOC_PIDF;
		} else if (strstr(buffer, "application/xpidf+xml")) {
			*_a = DOC_XPIDF;
		} else if (strstr(buffer, "text/lpidf")) {
			*_a = DOC_LPIDF;
		} else {
			*_a = DOC_XPIDF;
		}
	
		pkg_free(buffer);
		return 0;
	} else {
		/* XP messenger is not giving an accept field, so default to lpidf */
		*_a = DOC_XPIDF;
		return 0;
	}
}


/*
 * Parse all header fields that will be needed
 * to handle a SUBSCRIBE request
 */
static int parse_hfs(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_FROM | HDR_EVENT | HDR_EXPIRES | HDR_ACCEPT, 0) == -1) {
		paerrno = PA_PARSE_ERR;
		LOG(L_ERR, "parse_hfs(): Error while parsing headers\n");
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

	if (_m->expires) {
		if (parse_expires(_m->expires) < 0) {
			paerrno = PA_EXPIRES_PARSE;
			LOG(L_ERR, "parse_hfs(): Error while parsing Expires header field\n");
			return -9;
		}
	}

	if (_m->accept) {
		if (parse_accept(_m->accept, &acc) < 0) {
			paerrno = PA_ACCEPT_PARSE;
			LOG(L_ERR, "parse_hfs(): Error while parsing Accept header field\n");
			return -10;
		}
	} else {
		LOG(L_ERR, "no accept header\n");
		acc = DOC_XPIDF;
	}

	return 0;
}


/*
 * Check if a message received has been constructed properly
 */
int check_message(struct sip_msg* _m)
{
	if (_m->event) {
		event_t *event;
		
		if (!_m->event->parsed)
			parse_event(_m->event);
		event = (event_t*)(_m->event->parsed);

		if ((event->parsed != EVENT_PRESENCE)
		    && (event->parsed != EVENT_PRESENCE_WINFO)) {
			paerrno = PA_EVENT_UNSUPP;
			LOG(L_ERR, "check_message(): Unsupported event package event=%p et=%d len=%d\n",
			    event, event->parsed, event->text.len);
			return -1;
		}
	}

	return 0;
}


/*
 * Create a new presentity and corresponding watcher list
 */
static int create_presentity(struct sip_msg* _m, struct pdomain* _d, str* _puri, 
			     struct presentity** _p, struct watcher** _w)
{
	time_t e;
	dlg_t* dialog;
	str watch_uri;
	event_t *event = NULL;
	int et = 0;
	if (_m->event) {
		event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		et = EVENT_PRESENCE;
	}

	if (_m->expires) {
		e = ((exp_body_t*)_m->expires->parsed)->val;
	} else {
		e = default_expires;
	}
	
	if (e == 0) {
		*_p = 0;
		*_w = 0;
		DBG("create_presentity(): expires = 0\n");
		return 0;
	}

	/* Convert to absolute time */
	e += act_time;

	if (get_watch_uri(_m, &watch_uri) < 0) {
		LOG(L_ERR, "create_presentity(): Error while extracting watcher URI\n");
		return -1;
	}

	if (new_presentity(_puri, _p) < 0) {
		LOG(L_ERR, "create_presentity(): Error while creating presentity\n");
		return -2;
	}

	if (tmb.new_dlg_uas(_m, 200, &dialog) < 0) {
		paerrno = PA_DIALOG_ERR;
		LOG(L_ERR, "create_presentity(): Error while creating dialog state\n");
		free_presentity(*_p);
		return -3;
	}

	if (et == EVENT_PRESENCE) {
		if (add_watcher(*_p, &watch_uri, e, acc, dialog, _w) < 0) {
			LOG(L_ERR, "create_presentity(): Error while adding a watcher\n");
			tmb.free_dlg(dialog);
			free_presentity(*_p);
			return -4;
		}
	} else if (et == EVENT_PRESENCE_WINFO) {
		if (add_winfo_watcher(*_p, &watch_uri, e, acc, dialog, _w) < 0) {
			LOG(L_ERR, "create_presentity(): Error while adding a winfo watcher\n");
			tmb.free_dlg(dialog);
			free_presentity(*_p);
			return -5;
		}
	}
	add_presentity(_d, *_p);

	_d->reg(&watch_uri, _puri, (void*)callback, *_p);
	return 0;
}


/*
 * Update existing presentity and watcher list
 */
static int update_presentity(struct sip_msg* _m, struct pdomain* _d, 
			     struct presentity* _p, struct watcher** _w)
{
	time_t e;
	dlg_t* dialog;
	str watch_uri;
	event_t *event = NULL;
	int et = 0;
	if (_m->event) {
		event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		et = EVENT_PRESENCE;
	}

	if (_m->expires) {
		e = ((exp_body_t*)_m->expires->parsed)->val;
	} else {
		e = default_expires;
	}

	if (get_watch_uri(_m, &watch_uri) < 0) {
		LOG(L_ERR, "update_presentity(): Error while extracting watcher URI\n");
		return -1;
	}

	if (find_watcher(_p, &watch_uri, _w) == 0) {
		if (e == 0) {
			if (et == EVENT_PRESENCE) {
				if (remove_watcher(_p, *_w) < 0) {
					LOG(L_ERR, "update_presentity(): Error while deleting winfo watcher\n");
					return -2;
				} 
			} else {
				if (remove_winfo_watcher(_p, *_w) < 0) {
					LOG(L_ERR, "update_presentity(): Error while deleting winfo watcher\n");
					return -2;
				} 
			}
			
			(*_w)->expires = 0;   /* The watcher will be freed after NOTIFY is sent */
			if (!_p->watchers && !_p->winfo_watchers) {
				remove_presentity(_d, _p);
			}
		} else {
			e += act_time;
			if (update_watcher(*_w, e) < 0) {
				LOG(L_ERR, "update_presentity(): Error while updating watcher\n");
				return -3;
			}
		}
	} else {
		if (e) {
			e += act_time;

			if (tmb.new_dlg_uas(_m, 200, &dialog) < 0) {
				paerrno = PA_DIALOG_ERR;
				LOG(L_ERR, "update_presentity(): Error while creating dialog state\n");
				return -4;
			}

			if (et == EVENT_PRESENCE) {
				if (add_watcher(_p, &watch_uri, e, acc, dialog, _w) < 0) {
					LOG(L_ERR, "update_presentity(): Error while creating presentity\n");
					tmb.free_dlg(dialog);
					return -5;
				}
			} else {
				if (add_winfo_watcher(_p, &watch_uri, e, acc, dialog, _w) < 0) {
					LOG(L_ERR, "update_presentity(): Error while creating winfo watcher\n");
					tmb.free_dlg(dialog);
					return -5;
				}			
			}
		} else {
			DBG("update_presentity(): expires = 0 but no watcher found\n");
			*_w = 0;
		}
	}

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
	str p_uri;

	LOG(L_ERR, "handle_subscription() entered\n");
	get_act_time();
	paerrno = PA_OK;

	if (parse_hfs(_m) < 0) {
		LOG(L_ERR, "handle_subscription(): Error while parsing message header\n");
		goto error;
	}

	if (check_message(_m) < 0) {
		LOG(L_ERR, "handle_subscription(): Error while checking message\n");
		goto error;
	}

	d = (struct pdomain*)_domain;

	if (get_pres_uri(_m, &p_uri) < 0) {
		LOG(L_ERR, "handle_subscription(): Error while extracting presentity URI\n");
		goto error;
	}

	lock_pdomain(d);
	
	if (find_presentity(d, &p_uri, &p) > 0) {
		if (create_presentity(_m, d, &p_uri, &p, &w) < 0) {
			LOG(L_ERR, "handle_subscription(): Error while creating new presentity\n");
			goto error2;
		}
	} else {
		if (update_presentity(_m, d, p, &w) < 0) {
			LOG(L_ERR, "handle_subscription(): Error while updating presentity\n");
			goto error2;
		}
	}

	if (send_reply(_m) < 0) {
	  LOG(L_ERR, "handle_subscription(): Error while sending reply\n");
	  goto error2;
	}

	if (p && w) {
		if (send_notify(p, w) < 0) {
			LOG(L_ERR, "handle_subscription(): Error while sending notify\n");
			     /* FIXME: watcher and presentity should be test for removal here
			      * (and possibly in other error cases too
			      */
			goto error2;
		}

		notify_winfo_watchers(p);

		     /* We remove it here because a notify needs to be send first */
		if (w->expires == 0) free_watcher(w);
		if (p->slot == 0) free_presentity(p);
	} else {
		     /* FIXME: We should send a NOTIFY here too. We will end here when the
		      * subscribe contained expires = 0; Either there is no watcher for the
		      * presentity yet - in this case p and w will be set to 0 and we have to
		      * query external module (usrloc, jabber) for presence status or there
		      * are already another watchers - in this case only w will be set to zero
		      * and we have to send a notify. To be implemented later when there is 
		      * full UAS support
		      */
		DBG("handle_subscription(): expires==0 but we sent no NOTIFY - not implemented yet\n");
	}

	LOG(L_ERR, "handle_subscription about to return 1\n");
	unlock_pdomain(d);
	return 1;
	
 error2:
	LOG(L_ERR, "handle_subscription about to return -1\n");
	unlock_pdomain(d);
	return -1;
 error:
	LOG(L_ERR, "handle_subscription about to send_reply and return -2\n");
	send_reply(_m);
	return -1;
}


/*
 * Returns 1 if subscription exists and -1 if not
 */
int existing_subscription(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity* p;
	struct watcher* w;
	str p_uri, w_uri;

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

	if (get_watch_uri(_m, &w_uri) < 0) {
		LOG(L_ERR, "existing_subscription(): Error while extracting watcher URI\n");
		goto error;
	}

	lock_pdomain(d);
	
	if (find_presentity(d, &p_uri, &p) == 0) {
		if (find_watcher(p, &w_uri, &w) == 0) {
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


/*
 * Returns 1 if possibly a user agent can handle SUBSCRIBE
 * itself, 0 if it cannot for sure
 */
int pua_exists(struct sip_msg* _m, char* _domain, char* _s2)
{

	return 0;
}




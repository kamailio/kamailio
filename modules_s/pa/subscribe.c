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
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */

#include "../../comp_defs.h"
#include "subscribe.h"
#include "../../dprint.h"
#include "paerrno.h"
#include "../../parser/parse_event.h"
#include "../../parser/parse_expires.h"
#include "common.h"
#include "pdomain.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "pa_mod.h"
#include "watcher.h"
#include "../../trim.h"
#include "../../parser/parse_uri.h"
#include "reply.h"
#include "notify.h"
#include "../../trim.h"

static doctype_t acc;

/*
 * FIXME: locking
 */
void callback(str* _user, int state, void* data)
{
	struct presentity* ptr;

	ptr = (struct presentity*)data;

	if (state == 0) {
		ptr->state = PS_OFFLINE;
	} else ptr->state = PS_ONLINE;

	notify_watchers(ptr);
}



static inline void parse_accept(struct sip_msg* _m, doctype_t* _a)
{
	char buffer[512];
	struct hdr_field* ptr;
	str b;

	ptr = _m->headers;

	while(ptr) {
	        if (ptr->name.len == 6)
			if (!strncasecmp("Accept", ptr->name.s, 6)) {
				b.s = ptr->body.s;
				b.len = ptr->body.len;
#ifdef PRESERVE_ZT
				trim(&b);
#else
				trim_trailing(&b);
#endif

				memcpy(buffer, b.s, b.len);
				buffer[b.len] = '\0';
				
				if (strstr(buffer, "text/lpidf")) {
					acc = DOC_LPIDF;
				} else {
					acc = DOC_XPIDF;
				}
				return;
			}
		ptr = ptr->next;
	}

	acc = DOC_XPIDF;
}



static inline int parse_message(struct sip_msg* _m)
{

	     /* FIXME: HDR_ACCEPT */
	if (parse_headers(_m, HDR_EOH, 0) == -1) {
    /* if (parse_headers(_m, HDR_CONTACT | HDR_FROM | HDR_EVENT | HDR_EXPIRES | HDR_CALLID | HDR_TO, HDR_ACCEPT, 0) == -1) { */
		paerrno = PA_PARSE_ERR;
		LOG(L_ERR, "parse_message(): Error while parsing headers\n");
		return -1;
	}

	if (_m->contact == 0) {
		paerrno = PA_CONTACT_MISS;
		LOG(L_ERR, "parse_message(): Contact: missing\n");
		return -2;
	} else {
		if (parse_contact(_m->contact) < 0) {
			paerrno = PA_CONT_PARSE;
			LOG(L_ERR, "parse_message(): Error while parsing Contact\n");
			return -3;
		}
	}

	if (((contact_body_t*)(_m->contact->parsed))->star == 1) {
		paerrno = PA_CONT_STAR;
		LOG(L_ERR, "parse_message(): * Contact not allowed in SUBSCRIBE\n");
		return -4;
	}

	if (((contact_body_t*)(_m->contact->parsed))->contacts == 0) {
		paerrno = PA_CONTACT_MISS;
		LOG(L_ERR, "parse_message(): Contact missing\n");
		return -5;
	}

	if (_m->from == 0) {
		paerrno = PA_FROM_MISS;
		LOG(L_ERR, "parse_message(): From: missing\n");
		return -6;
	} else {
		if (parse_from_header(_m) == -1) { 
			paerrno = PA_FROM_ERROR;
			LOG(L_ERR, "parse_message(): Error while parsing From\n");
			return -7;
		}
		     /* FIXME: parse from here */
	}

	if (_m->event) {
		if (parse_event(_m->event) < 0) {
			paerrno = PA_EVENT_PARSE;
			LOG(L_ERR, "parse_message(): Error while parsing Event header field\n");
			return -8;
		}
	}

	if (_m->expires) {
		if (parse_expires(_m->expires) < 0) {
			paerrno = PA_EXPIRES_PARSE;
			LOG(L_ERR, "parse_message(): Error while parsing Expires header field\n");
			return -9;
		}
	}

	parse_accept(_m, &acc);

	return 0;
}


static inline int check_message(struct sip_msg* _m)
{
	if (_m->event) {
		if (((event_t*)(_m->event->parsed))->parsed != EVENT_PRESENCE) {
			paerrno = PA_EVENT_UNSUPP;
			LOG(L_ERR, "check_message(): Unsupported event package\n");
			return -1;
		}
	}

	return 0;
}


/*
 * Create a new presentity and corresponding watcher list
 */
static inline int create_presentity(struct sip_msg* _m, struct pdomain* _d, str* _to, struct presentity** _p, struct watcher** _w)
{
	str* c, cid, to;
	time_t e;

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

	e += time(0);

	if (new_presentity(_to, _p) < 0) {
		LOG(L_ERR, "create_presentity(): Error while creating presentity\n");
		return -1;
	}

	c = &((contact_body_t*)_m->contact->parsed)->contacts->uri;
	get_raw_uri(c);

	cid = _m->callid->body;
	to = _m->to->body;
#ifdef PRESERVE_ZT
	trim(&to);
	trim(&cid);
#else
	trim_trailing(&to);
	trim_trailing(&cid);
#endif
	
	if (add_watcher(*_p, &(get_from(_m)->uri), c, e, acc, &cid, &(get_from(_m)->tag_value), &to, _w) < 0) {
		LOG(L_ERR, "create_presentity(): Error while creating presentity\n");
		return -2;
	}

	add_presentity(_d, *_p);

	_d->reg(&(get_from(_m)->uri), _to, callback, *_p);

	return 0;
}



static inline int find_watcher(struct presentity* _p, str* _c, watcher_t** _w)
{
	watcher_t* ptr;

	ptr = _p->watchers;

	while(ptr) {
		if ((_c->len == ptr->contact.len) &&
		    (!memcmp(_c->s, ptr->contact.s, _c->len))) {
			*_w = ptr;
			return 0;
		}
			
		ptr = ptr->next;
	}
	
	return 1;
}



/*
 * Update existing presentity and watcher list
 */
static inline int update_presentity(struct sip_msg* _m, struct pdomain* _d, struct presentity* _p, struct watcher** _w)
{
	time_t e;
	str* c;

	if (_m->expires) {
		e = ((exp_body_t*)_m->expires->parsed)->val;
	} else {
		e = default_expires;
	}

	c = &((contact_body_t*)_m->contact->parsed)->contacts->uri;
	get_raw_uri(c);

	if (find_watcher(_p, c, _w) == 0) {
		if (e == 0) {
			if (remove_watcher(_p, *_w) < 0) {
				LOG(L_ERR, "update_presentity(): Error while deleting presentity\n");
				return -1;
			}
			
			(*_w)->expires = 0;   /* The watcher will be freed after NOTIFY is sent */
			if (!_p->watchers) {
				remove_presentity(_d, _p);
			}
		} else {
			e += time(0);
			if (update_watcher(*_w, c, e) < 0) {
				LOG(L_ERR, "update_presentity(): Error while updating watcher\n");
				return -2;
			}
		}
	} else {
		if (e) {
			e += time(0);

			if (add_watcher(_p, &(get_from(_m)->uri), c, e, acc, &_m->callid->body, &_m->from->body, &_m->to->body, _w) < 0) {
				LOG(L_ERR, "update_presentity(): Error while creating presentity\n");
				return -2;
			}			
		} else {
			DBG("update_presentity(): expires = 0 but no watcher found\n");
			*_w = 0;
			return 0;
		}
	}


	return 0;
}


int extract_userdomain(str* _ud)
{
	struct sip_uri uri;

	     /* FIXME: Can be password between username and host */
	parse_uri(_ud->s, _ud->len, &uri);
	_ud->s = uri.user.s;
	_ud->len = uri.user.len + uri.host.len + 1;
	return 0;
}


/*
 * Handle a subscribe Request
 */
int subscribe(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct pdomain* d;
	struct presentity *p;
	struct watcher* w;
	str ud;

	paerrno = PA_OK;

	if (parse_message(_m) < 0) {
		LOG(L_ERR, "subscribe(): Error while parsing message header\n");
		goto error;
	}

	if (check_message(_m) < 0) {
		LOG(L_ERR, "subscribe(): Error while checking message\n");
		goto error;
	}

	d = (struct pdomain*)_s1;

	if (_m->new_uri.s) {
		ud = _m->new_uri;
	} else {
		ud = _m->first_line.u.request.uri;
	}
	
	lock_pdomain(d);
	
	if (extract_userdomain(&ud) < 0) {
		LOG(L_ERR, "subscribe(): Error while extracting user@domain\n");
		goto error;
	}

	if (find_presentity(d, &ud, &p) > 0) {
		if (create_presentity(_m, d, &ud, &p, &w) < 0) {
			LOG(L_ERR, "subscribe(): Error while creating new presentity\n");
			unlock_pdomain(d);
			goto error;
		}
	} else {
		if (update_presentity(_m, d, p, &w) < 0) {
			LOG(L_ERR, "subscribe(): Error while updating presentity\n");
			unlock_pdomain(d);
			goto error;
		}
	}

	     /*	print_all_pdomains(stdout); */

	if (send_reply(_m) < 0) return -1;


	if (p && w) {
		if (send_notify(p, w) < 0) {
			LOG(L_ERR, "subscribe(): Error while sending notify\n");
			unlock_pdomain(d);
			     /* FIXME: watcher and presentity should be test for removal here
			      * (and possibly in other error cases too
			      */
			goto error;
		}

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
		DBG("subscribe(): expires==0 but we sent no NOTIFY - not implemented yet\n");
	}

	unlock_pdomain(d);
	
	return 1;
	
 error:
	send_reply(_m);
	return 0;
}

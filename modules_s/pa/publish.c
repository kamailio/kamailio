/*
 * Presence Agent, publish handling
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
 */

#include <string.h>
#include "../../fifo_server.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "dlist.h"
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
#include "publish.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

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

	return 0;
}


/*
 * Check if a message received has been constructed properly
 */
static int check_message(struct sip_msg* _m)
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
 * Create a new presentity but no watcher list
 */
int create_presentity_only(struct sip_msg* _m, struct pdomain* _d, str* _puri, 
			   struct presentity** _p)
{
	time_t e;

	if (_m->expires) {
		e = ((exp_body_t*)_m->expires->parsed)->val;
	} else {
		e = default_expires;
	}
	
	if (e == 0) {
		*_p = 0;
		DBG("create_presentity(): expires = 0\n");
		return 0;
	}

	     /* Convert to absolute time */
	e += act_time;

	if (new_presentity(_puri, _p) < 0) {
		LOG(L_ERR, "create_presentity(): Error while creating presentity\n");
		return -2;
	}

	add_presentity(_d, *_p);

	return 0;
}

/*
 * Update existing presentity and watcher list
 */
static int publish_presentity(struct sip_msg* _m, struct pdomain* _d, 
			     struct presentity* _p, int *pchanged)
{

	/* for now, assume all publishes cause a change */
	if (pchanged)
		*pchanged = 1;
	return 0;
}

/*
 * Handle a publish Request
 */
int handle_publish(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity *p;
	str p_uri;
	int changed;

	get_act_time();
	paerrno = PA_OK;

	if (parse_hfs(_m) < 0) {
		LOG(L_ERR, "handle_publish(): Error while parsing message header\n");
		goto error;
	}

	if (check_message(_m) < 0) {
		LOG(L_ERR, "handle_publish(): Error while checking message\n");
		goto error;
	}

	d = (struct pdomain*)_domain;

	if (get_pres_uri(_m, &p_uri) < 0) {
		LOG(L_ERR, "handle_publish(): Error while extracting presentity URI\n");
		goto error;
	}

	lock_pdomain(d);
	
	if (find_presentity(d, &p_uri, &p) > 0) {
		if (create_presentity_only(_m, d, &p_uri, &p) < 0) {
			LOG(L_ERR, "handle_publish(): Error while creating new presentity\n");
			unlock_pdomain(d);
			goto error;
		}
	}

	/* update presentity event state */
	if (p)
		publish_presentity(_m, d, p, &changed);

	if (send_reply(_m) < 0) return -1;

	if (p && changed) {
		notify_watchers(p);
	}

	unlock_pdomain(d);
	return 1;
	
 error:
	send_reply(_m);
	return 0;
}

/*
 * FIFO function for publishing events
 */
int fifo_pa_publish(FILE *stream, char *response_file)
{
	/* not yet implemented */
	return -1;
}

/*
 * FIFO function for publishing presence
 *
 * :pa_presence:
 * pdomain (registrar or jabber)
 * presentity_uri
 * presentity_presence (civil or geopriv)
 *
 */
#define MAX_P_URI 128
#define MAX_PRESENCE 256
#define MAX_PDOMAIN 256
int fifo_pa_presence(FILE *fifo, char *response_file)
{
	char pdomain_s[MAX_P_URI];
	char p_uri_s[MAX_P_URI];
	char presence_s[MAX_PRESENCE];
	pdomain_t *pdomain = NULL;
	presentity_t *presentity = NULL;
	str pdomain_name, p_uri, presence;
	int origstate, newstate;

	if (!read_line(pdomain_s, MAX_PDOMAIN, fifo, &pdomain_name.len) || pdomain_name.len == 0) {
		fifo_reply(response_file,
			   "400 ul_add: pdomain expected\n");
		LOG(L_ERR, "ERROR: ul_add: pdomain expected\n");
		return 1;
	}
	pdomain_name.s = pdomain_s;

	if (!read_line(p_uri_s, MAX_P_URI, fifo, &p_uri.len) || p_uri.len == 0) {
		fifo_reply(response_file,
			   "400 ul_add: p_uri expected\n");
		LOG(L_ERR, "ERROR: ul_add: p_uri expected\n");
		return 1;
	}
	p_uri.s = p_uri_s;

	if (!read_line(presence_s, MAX_PRESENCE, fifo, &presence.len) || presence.len == 0) {
		fifo_reply(response_file,
			   "400 ul_add: presence expected\n");
		LOG(L_ERR, "ERROR: ul_add: presence expected\n");
		return 1;
	}
	presence.s = presence_s;

	register_pdomain(pdomain_s, &pdomain);
	if (!pdomain) {
		fifo_reply(response_file, "400 could not register pdomain\n");
		LOG(L_ERR, "ERROR: pa_location: could not register pdomain %.*s\n",
		    pdomain_name.len, pdomain_name.s);
		return 1;
	}

	find_presentity(pdomain, &p_uri, &presentity);
	if (!presentity) {
		new_presentity(&p_uri, &presentity);
	}
	if (!presentity) {
		fifo_reply(response_file, "400 could not find presentity %s\n", p_uri_s);
		LOG(L_ERR, "ERROR: pa_location: could not find presentity %.*s\n",
		    p_uri.len, p_uri.s);
		return 1;
	}

	origstate = presentity->state;
	presentity->state = newstate =
		(strcmp(presence_s, "online") == 0) ? PS_ONLINE : PS_OFFLINE;

	if (origstate != newstate) {
		notify_watchers(presentity);
	}

	fifo_reply(response_file, "200 published\n",
		   "(%.*s %.*s)\n",
		   p_uri.len, ZSW(p_uri.s),
		   presence.len, ZSW(presence.s));
	return 1;
}

/*
 * FIFO function for publishing location
 *
 * :pa_location:
 * pdomain (registrar or jabber)
 * presentity_uri
 * presentity_location (civil or geopriv)
 *
 */
#define MAX_P_URI 128
#define MAX_LOCATION 256
#define MAX_PDOMAIN 256
int fifo_pa_location(FILE *fifo, char *response_file)
{
	char pdomain_s[MAX_P_URI];
	char p_uri_s[MAX_P_URI];
	char location_s[MAX_LOCATION];
	pdomain_t *pdomain = NULL;
	presentity_t *presentity = NULL;
	str pdomain_name, p_uri, location;
	int changed = 0;

	if (!read_line(pdomain_s, MAX_PDOMAIN, fifo, &pdomain_name.len) || pdomain_name.len == 0) {
		fifo_reply(response_file,
			   "400 pa_location: pdomain expected\n");
		LOG(L_ERR, "ERROR: pa_location: pdomain expected\n");
		return 1;
	}
	pdomain_name.s = pdomain_s;

	if (!read_line(p_uri_s, MAX_P_URI, fifo, &p_uri.len) || p_uri.len == 0) {
		fifo_reply(response_file,
			   "400 pa_location: p_uri expected\n");
		LOG(L_ERR, "ERROR: pa_location: p_uri expected\n");
		return 1;
	}
	p_uri.s = p_uri_s;

	if (!read_line(location_s, MAX_LOCATION, fifo, &location.len) || location.len == 0) {
		fifo_reply(response_file,
			   "400 pa_location: location expected\n");
		LOG(L_ERR, "ERROR: pa_location: location expected\n");
		return 1;
	}
	location.s = location_s;

	register_pdomain(pdomain_s, &pdomain);
	if (!pdomain) {
		fifo_reply(response_file, "400 could not register pdomain\n");
		LOG(L_ERR, "ERROR: pa_location: could not register pdomain %.*s\n",
		    pdomain_name.len, pdomain_name.s);
		return 1;
	}

	find_presentity(pdomain, &p_uri, &presentity);
	if (!presentity) {
		new_presentity(&p_uri, &presentity);
	}
	if (!presentity) {
		fifo_reply(response_file, "400 could not find presentity\n");
		LOG(L_ERR, "ERROR: pa_location: could not find presentity %.*s\n",
		    p_uri.len, p_uri.s);
		return 1;
	}

	if (presentity->location.len && strcmp(presentity->location.s, location.s) != 0)
		changed = 1;

	if (0)
	if (presentity->location.s)
		free(presentity->location.s);

	presentity->location.len = location.len;
	presentity->location.s = strdup(location_s);

	if (changed) {
		notify_watchers(presentity);
	}

	fifo_reply(response_file, "200 published\n",
		   "(%.*s %.*s)\n",
		   p_uri.len, ZSW(p_uri.s),
		   location.len, ZSW(location.s));
	return 1;
}

/*
 * Presence Agent, publish handling
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 * Copyright (C) 2003-2004 Hewlett-Packard Company
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
#include "pidf.h"
#include "common.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

extern str str_strdup(str string);

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
 * Create a new presentity but no watcher list
 */
int create_presentity_only(struct sip_msg* _m, struct pdomain* _d, str* _puri, 
			   struct presentity** _p)
{
	time_t e;
	event_t *parsed_event;
	int et = EVENT_OTHER;

	if (_m && _m->expires) {
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

	if (_m && _m->event) {
		parsed_event = (event_t *)_m->event->parsed;
		et = parsed_event->parsed;
	}

	if (new_presentity(_d, _puri, et, _p) < 0) {
		LOG(L_ERR, "create_presentity_only(): Error while creating presentity\n");
		return -2;
	}

	add_presentity(_d, *_p);

	return 0;
}

int location_package_location_add_user(pdomain_t *pdomain, str *site, str *floor, str *room, presentity_t *presentity)
{
	str l_uri;
	presentity_t *l_presentity = NULL;
	resource_list_t *users = NULL;
	int changed = 0;
	struct sip_msg *msg = NULL;
	l_uri.len = pa_domain.len + site->len + floor->len + room->len + 4;
	l_uri.s = shm_malloc(l_uri.len);
	if (!l_uri.s)
		return -2;
	sprintf(l_uri.s, "%s.%s.%s@%s", room->s, floor->s, site->s, pa_domain.s);
	if (find_presentity(pdomain, &l_uri, &l_presentity) > 0) {
		changed = 1;
		if (create_presentity_only(msg, pdomain, &l_uri, &l_presentity) < 0) {
			goto error;
		}
	}

	if (!l_presentity) {
		LOG(L_ERR, "location_package_location_add_user: failed to find or create presentity for %s\n", l_uri.s);
		return -2;
	}
	if (!presentity) {
		LOG(L_ERR, "location_package_location_add_user: was passed null presentity\n");
		return -3;
	}

	users = l_presentity->location_package.users;
	l_presentity->location_package.users = 
		resource_list_append_unique(users, &presentity->uri);

 error:
	return -1;
}

int location_package_location_del_user(pdomain_t *pdomain, str *site, str *floor, str *room, presentity_t *presentity)
{
	str l_uri;
	presentity_t *l_presentity = NULL;
	resource_list_t *users;
	struct sip_msg *msg = NULL;
	int changed = 0;
	l_uri.len = pa_domain.len + site->len + floor->len + room->len + 4;
	l_uri.s = shm_malloc(l_uri.len);
	if (!l_uri.s)
		return -2;
	sprintf(l_uri.s, "%s.%s.%s@%s", room->s, floor->s, site->s, pa_domain.s);
	if (find_presentity(pdomain, &l_uri, &l_presentity) > 0) {
		changed = 1;
		if (create_presentity_only(msg, pdomain, &l_uri, &l_presentity) < 0) {
			goto error;
		}
	}

	users = l_presentity->location_package.users;
	l_presentity->location_package.users = 
		resource_list_remove(users, &presentity->uri);

 error:
	return -1;
}

/*
 * Update existing presentity and watcher list
 */
static int publish_presentity_pidf(struct sip_msg* _m, struct pdomain* _d, struct presentity* presentity, int *pchanged)
{
     char *body = get_body(_m);
     presence_tuple_t *tuple = NULL;
     str contact = { NULL, 0 };
     str basic = { NULL, 0 };
     str status = { NULL, 0 };
     str location = { NULL, 0 };
     str site = { NULL, 0 };
     str floor = { NULL, 0 };
     str room = { NULL, 0 };
     str packet_loss = { NULL, 0 };
     double x=0, y=0, radius=0;
     int flags = 0;
     int changed = 0;
     int ret = 0;

     flags = parse_pidf(body, &contact, &basic, &status, &location, &site, &floor, &room, &x, &y, &radius, &packet_loss);
     if (contact.len) {
	  find_presence_tuple(&contact, presentity, &tuple);
	  if (!tuple) {
	       new_presence_tuple(&contact, presentity, &tuple);
	       add_presence_tuple(presentity, tuple);
	  }
     } else {
	  tuple = presentity->tuples;
     }
     if (!tuple) {
	  LOG(L_ERR, "publish_presentity: no tuple for %.*s\n", 
	      presentity->uri.len, presentity->uri.s);
	  return -1;
     }

     LOG(L_INFO, "publish_presentity_pidf: -1-\n");
     if (basic.len && basic.s) {
	  int origstate = tuple->state;
	  tuple->state =
	       (strcmp(basic.s, "online") == 0) ? PS_ONLINE : PS_OFFLINE;
	  if (tuple->state != origstate)
	       changed = 1;
     }
     if (status.len && status.s) {
	  if (tuple->status.len && strcmp(tuple->status.s, status.s) != 0)
	       changed = 1;
	  tuple->status.len = status.len;
	  strncpy(tuple->status.s, status.s, status.len);
	  tuple->status.s[status.len] = 0;
     }
     LOG(L_INFO, "publish_presentity: -2-\n");
     if (location.len && location.s) {
	  if (tuple->location.loc.len && strcmp(tuple->location.loc.s, location.s) != 0)
	       changed = 1;
	  tuple->location.loc.len = location.len;
	  strncpy(tuple->location.loc.s, location.s, location.len);
	  tuple->location.loc.s[location.len] = 0;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.loc.len = 0;
     }
     if (site.len && site.s) {
	  if (tuple->location.site.len && strcmp(tuple->location.site.s, site.s) != 0)
	       changed = 1;
	  tuple->location.site.len = site.len;
	  strncpy(tuple->location.site.s, site.s, site.len);
	  tuple->location.site.s[site.len] = 0;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.site.len = 0;
     }
     if (floor.len && floor.s) {
	  if (tuple->location.floor.len && strcmp(tuple->location.floor.s, floor.s) != 0)
	       changed = 1;
	  tuple->location.floor.len = floor.len;
	  strncpy(tuple->location.floor.s, floor.s, floor.len);
	  tuple->location.floor.s[floor.len] = 0;
     }else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.floor.len = 0;
     }
     if (room.len && room.s) {
	  if (tuple->location.room.len && strcmp(tuple->location.room.s, room.s) != 0)
	       changed = 1;
	  tuple->location.room.len = room.len;
	  strncpy(tuple->location.room.s, room.s, room.len);
	  tuple->location.room.s[room.len] = 0;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.room.len = 0;
     }
     if (packet_loss.len && packet_loss.s) {
	  if (tuple->location.packet_loss.len && strcmp(tuple->location.packet_loss.s, packet_loss.s) != 0)
	       changed = 1;
	  tuple->location.packet_loss.len = packet_loss.len;
	  strncpy(tuple->location.packet_loss.s, packet_loss.s, packet_loss.len);
	  tuple->location.packet_loss.s[packet_loss.len] = 0;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.packet_loss.len = 0;
     }
     if (x) {
	  if (tuple->location.x != x)
	       changed = 1;
	  tuple->location.x = x;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.x = 0;
     }
     if (y) {
	  if (tuple->location.y != y)
	       changed = 1;
	  tuple->location.y = y;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.y = 0;
     }
     if (radius) {
	  if (tuple->location.radius != radius)
	       changed = 1;
	  tuple->location.radius = radius;
     } else if (flags & PARSE_PIDF_LOCATION_MASK) {
	  tuple->location.radius = 0;
     }

     if (use_location_package)
	  if (site.len && floor.len && room.len && changed) {
	       location_package_location_add_user(_d, &site, &floor, &room, presentity);
	  }

     changed = 1;
     if (changed)
	  presentity->flags |= PFLAG_PRESENCE_CHANGED;

     LOG(L_INFO, "publish_presentity: -3-: changed=%d\n", changed);
     if (pchanged && changed) {
	  *pchanged = 1;
     }

     if ((ret = db_update_presentity(presentity)) < 0) {
	  return ret;
     }

     LOG(L_INFO, "publish_presentity: -4-\n");
     return 0;
}

/*
 * If this xcap change is on a watcher list, then reread authorizations
 */
static int publish_presentity_xcap_change(struct sip_msg* _m, struct pdomain* _d, struct presentity* presentity, int *pchanged)
{
	char *body = get_body(_m);
	LOG(L_ERR, "publish_presentity_xcap_change: body=%p\n", body);
	if (body) {
		/* cheesy hack to see if it is presence-lists or watcherinfo that was changed */
		if (strstr(body, "presence-lists"))
			presentity->flags |= PFLAG_PRESENCE_LISTS_CHANGED;
		if (strstr(body, "watcherinfo"))
			presentity->flags |= PFLAG_WATCHERINFO_CHANGED;
		presentity->flags |= PFLAG_XCAP_CHANGED;

		LOG(L_ERR, "publish_presentity_xcap_change: got body, setting flags=%x", 
		    presentity->flags);

		if (pchanged)
			*pchanged = 1;
	}
	return 0;
}

static int publish_presentity(struct sip_msg* _m, struct pdomain* _d, struct presentity* presentity, int *pchanged)
{
	event_t *parsed_event = (event_t *)_m->event->parsed;
	int event_type = parsed_event->parsed;
	if (event_type == EVENT_PRESENCE) {
		publish_presentity_pidf(_m, _d, presentity, pchanged);
	} else if (event_type == EVENT_XCAP_CHANGE) {
		publish_presentity_xcap_change(_m, _d, presentity, pchanged);
	}

	LOG(L_INFO, "publish_presentity: event_type=%d -1-\n", event_type);
	return 0;
}

/*
 * Handle a publish Request
 */
int handle_publish(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity *p;
	str p_uri = { NULL, 0 };
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

	if (get_pres_uri(_m, &p_uri) < 0 || p_uri.s == NULL || p_uri.len == 0) {
		LOG(L_ERR, "handle_publish(): Error while extracting presentity URI\n");
		goto error;
	}

	lock_pdomain(d);
	
	LOG(L_ERR, "handle_publish -4- p_uri=%*.s p_uri.len=%d\n", p_uri.len, p_uri.s, p_uri.len);
	if (find_presentity(d, &p_uri, &p) > 0) {
		changed = 1;
		if (create_presentity_only(_m, d, &p_uri, &p) < 0) {
			goto error2;
		}
	}

	/* update presentity event state */
	LOG(L_ERR, "handle_publish -5- presentity=%p\n", p);
	if (p)
		publish_presentity(_m, d, p, &changed);

	unlock_pdomain(d);

	if (send_reply(_m) < 0) return -1;

	LOG(L_ERR, "handle_publish -8- done\n");
	return 1;
	
 error2:
	unlock_pdomain(d);
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
	// pdomain_t *pdomain = NULL;
	// presentity_t *presentity = NULL;
	str pdomain_name, p_uri, presence;
	// int origstate, newstate;
	// int allocated_presentity = 0;

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

#if 0
	register_pdomain(pdomain_s, &pdomain);
	if (!pdomain) {
		fifo_reply(response_file, "400 could not register pdomain\n");
		LOG(L_ERR, "ERROR: pa_location: could not register pdomain %.*s\n",
		    pdomain_name.len, pdomain_name.s);
		return 1;
	}

	find_presentity(pdomain, &p_uri, &presentity);
	if (!presentity) {
		new_presentity(pdomain, &p_uri, &presentity);
		add_presentity(pdomain, presentity);
		allocated_presentity = 1;
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

	if (origstate != newstate || allocated_presentity) {
		presentity->flags |= PFLAG_PRESENCE_CHANGED;
	}

	db_update_presentity(presentity);

#endif

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
     presence_tuple_t *tuple = NULL;
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

#if 1
     register_pdomain(pdomain_s, &pdomain);
     if (!pdomain) {
	  fifo_reply(response_file, "400 could not register pdomain\n");
	  LOG(L_ERR, "ERROR: pa_location: could not register pdomain %.*s\n",
	      pdomain_name.len, pdomain_name.s);
	  return 1;
     }

     find_presentity(pdomain, &p_uri, &presentity);
     if (!presentity) {
	  new_presentity(pdomain, &p_uri, EVENT_PRESENCE, &presentity);
	  add_presentity(pdomain, presentity);
	  changed = 1;
     }
     if (!presentity) {
	  fifo_reply(response_file, "400 could not find presentity\n");
	  LOG(L_ERR, "ERROR: pa_location: could not find presentity %.*s\n",
	      p_uri.len, p_uri.s);
	  return 1;
     }

     changed = 1;
     for (tuple = presentity->tuples; tuple; tuple = tuple->next) {
	  if (tuple->location.loc.len && strcmp(tuple->location.room.s, location.s) != 0)
	       changed = 1;

	  LOG(L_ERR, "Setting room of contact=%.*s to %.*s\n",
	      tuple->contact.len, tuple->contact.s,
	      tuple->location.room.len, tuple->location.room.s);
	  strncpy(tuple->location.room.s, location.s, location.len);
	  tuple->location.room.len = location.len;

	  strncpy(tuple->location.loc.s, location.s, location.len);
	  tuple->location.loc.len = location.len;
     }

     if (changed) {
	  presentity->flags |= PFLAG_PRESENCE_CHANGED;
     }

     db_update_presentity(presentity);

#endif

     fifo_reply(response_file, "200 published\n",
		"(%.*s %.*s)\n",
		p_uri.len, ZSW(p_uri.s),
		location.len, ZSW(location.s));
     return 1;
}

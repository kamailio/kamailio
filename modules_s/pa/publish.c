/*
 * Presence Agent, publish handling
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
#include <stdlib.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/contact/parse_contact.h"
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
#include "common.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_sipifmatch.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <presence/pidf.h>
#include <cds/logger.h>

/*
 * Parse all header fields that will be needed
 * to handle a PUBLISH request
 */
static int parse_publish_hfs(struct sip_msg* _m)
{
	int rc = 0;
	if ((rc = parse_headers(_m, HDR_FROM_F | HDR_EVENT_F | 
					HDR_EXPIRES_F | HDR_SIPIFMATCH_F | 
					HDR_CONTENTTYPE_F | HDR_CONTENTLENGTH_F, 0))
	    == -1) {
		paerrno = PA_PARSE_ERR;
		LOG(L_ERR, "parse_publish_hfs(): Error while parsing headers\n");
		return -1;
	}

	if (parse_from_header(_m) < 0) {
		paerrno = PA_FROM_ERR;
		LOG(L_ERR, "parse_publish_hfs(): From malformed or missing\n");
		return -6;
	}

	if (_m->event) {
		if (parse_event(_m->event) < 0) {
			paerrno = PA_EVENT_PARSE;
			LOG(L_ERR, "parse_publish_hfs(): Error while parsing Event header field\n");
			return -8;
		}
	} else {
		paerrno = PA_EVENT_PARSE;
		LOG(L_ERR, "parse_publish_hfs(): Missing Event header field\n");
		return -7;
	}

	if (_m->expires) {
		if (parse_expires(_m->expires) < 0) {
			paerrno = PA_EXPIRES_PARSE;
			LOG(L_ERR, "parse_publish_hfs(): Error while parsing Expires header field\n");
			return -9;
		}
	}

	/* patch from PIC-SER */
	if (_m->sipifmatch) {
		if (parse_sipifmatch(_m->sipifmatch) < 0) {
			paerrno = PA_PARSE_ERR;
			LOG(L_ERR, "parse_hfs(): Error while parsing SIP-If-Match header field\n");
			return -10;
		}
	}

	if (_m->content_type) {
		if (parse_content_type_hdr(_m) < 0) {
			LOG(L_ERR, "parse_hfs(): Can't parse Content-Type\n");
			return -12;
		}
	}
	
	return 0;
}


#ifdef HAVE_LOCATION_PACKAGE
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
#endif /* HAVE_LOCATION_PACKAGE */

#if 0

/* FIXME: remove as soon as will be rewritten */

static int basic_status2status(str *s)
{
	if ((strcasecmp(s->s, "online") == 0)
		|| (strcasecmp(s->s, "open") == 0)) return PS_ONLINE;
	if ((strcasecmp(s->s, "closed") == 0)
		|| (strcasecmp(s->s, "offline") == 0)) return PS_OFFLINE;
	/* return basic2status(*s); */
	return PS_OFFLINE;
}
/*
 * Update existing presentity and watcher list
 */
static int publish_presentity_pidf(struct sip_msg* _m, struct presentity* presentity, presence_tuple_t **modified_tuple)
{
	char *body = get_body(_m);
	presence_tuple_t *tuple = NULL;
	str contact = STR_NULL;
	str basic = STR_NULL;
	str status = STR_NULL;
	str location = STR_NULL;
	str site = STR_NULL;
	str floor = STR_NULL;
	str room = STR_NULL;
	str packet_loss = STR_NULL;
	double x=0, y=0, radius=0;
	time_t expires = act_time + default_expires;
	time_t msg_expires = 0;
	double priority = default_priority;
	int prescaps = 0;
	int flags = 0;
	int changed = 0;
	int ret = 0;
	str id = STR_NULL;
	 
	if (modified_tuple) *modified_tuple = 0;
	if (_m->expires) {
		if (_m->expires->parsed) {
			expires = ((exp_body_t*)_m->expires->parsed)->val + act_time;
			msg_expires = expires;
		}
	}
	if (_m->sipifmatch) {
		str *s = (str*)_m->sipifmatch->parsed;
		if (s) id = *s;
	}

	flags = parse_pidf(body, &contact, &basic, &status, &location, &site, &floor, &room, &x, &y, &radius, 
			&packet_loss, &priority, &expires, &prescaps);
	if (msg_expires > 0) {
		/* this has more power than those in PIDF document */
		expires = msg_expires;
	}
	
	/* use etag in SIP-If-Match header if present to find the tuple */
	if (id.len > 0) {
		LOG(L_DBG, "trying to find presetity using SIP-If-Match %.*s\n",
				id.len, ZSW(id.s));
		find_presence_tuple_id(&id, presentity, &tuple);
		if (!tuple) {
			LOG(L_ERR, "publish_presentity: No matching tuple found\n");
			paerrno = PA_NO_MATCHING_TUPLE;
			return -1;
		}
	} else {
		LOG(L_DBG, "NO SIP-If-Match header found\n");
	}
	
	/* try to find tuple using contact from document */
	if ((!tuple) && (contact.len > 0))
		find_registered_presence_tuple(&contact, presentity, &tuple);
	
	/* try to find tuple using contact from message headers */
	if (!tuple) {
		contact_t *sip_contact = NULL;
		/* get contact from SIP Headers*/
		contact_iterator(&sip_contact, _m, NULL);
		if (sip_contact) {
			LOG(L_ERR, "publish_presentity: find tuple for contact %.*s\n", 
					sip_contact->uri.len, sip_contact->uri.s);
			find_registered_presence_tuple(&sip_contact->uri, presentity, &tuple);
		}
	}
	if (!tuple && new_tuple_on_publish) {
		new_presence_tuple(&contact, expires, &tuple, 1);
		add_presence_tuple(presentity, tuple);
		changed = 1;
	}
	if (!tuple) {
		LOG(L_ERR, "publish_presentity: no tuple for %.*s\n", 
				presentity->uri.len, presentity->uri.s);
		return -1;
	}

	if (basic.len && basic.s) {
		int origstate = tuple->state;
		tuple->state = basic_status2status(&basic);
		if (tuple->state != origstate)
			changed = 1;
	}
	if (status.len && status.s) {
		if (tuple->status.len && str_strcasecmp(&tuple->status, &status) != 0)
			changed = 1;
		tuple->status.len = status.len;
		strncpy(tuple->status.s, status.s, status.len);
		tuple->status.s[status.len] = 0;
	}
	if (location.len && location.s) {
		if (tuple->location.loc.len && str_strcasecmp(&tuple->location.loc, &location) != 0)
			changed = 1;
		tuple->location.loc.len = location.len;
		strncpy(tuple->location.loc.s, location.s, location.len);
		tuple->location.loc.s[location.len] = 0;
	} else if (flags & PARSE_PIDF_LOCATION_MASK) {
		tuple->location.loc.len = 0;
	}
	if (site.len && site.s) {
		if (tuple->location.site.len && str_strcasecmp(&tuple->location.site, &site) != 0)
			changed = 1;
		tuple->location.site.len = site.len;
		strncpy(tuple->location.site.s, site.s, site.len);
		tuple->location.site.s[site.len] = 0;
	} else if (flags & PARSE_PIDF_LOCATION_MASK) {
		tuple->location.site.len = 0;
	}
	if (floor.len && floor.s) {
		if (tuple->location.floor.len && str_strcasecmp(&tuple->location.floor, &floor) != 0)
			changed = 1;
		tuple->location.floor.len = floor.len;
		strncpy(tuple->location.floor.s, floor.s, floor.len);
		tuple->location.floor.s[floor.len] = 0;
	}else if (flags & PARSE_PIDF_LOCATION_MASK) {
		tuple->location.floor.len = 0;
	}
	if (room.len && room.s) {
		if (tuple->location.room.len && str_strcasecmp(&tuple->location.room, &room) != 0)
			changed = 1;
		tuple->location.room.len = room.len;
		strncpy(tuple->location.room.s, room.s, room.len);
		tuple->location.room.s[room.len] = 0;
	} else if (flags & PARSE_PIDF_LOCATION_MASK) {
		tuple->location.room.len = 0;
	}
	if (packet_loss.len && packet_loss.s) {
		if (tuple->location.packet_loss.len && str_strcasecmp(&tuple->location.packet_loss, &packet_loss) != 0)
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

	if (tuple->priority != priority) {
		changed = 1;
		tuple->priority = priority;
	}
	if (tuple->expires != expires) {
		changed = 1;
		tuple->expires = expires;
	}
	LOG(L_DBG, "PUBLISH: tuple expires after %d s\n", (int)(tuple->expires - act_time));
#ifdef HAVE_LOCATION_PACKAGE
	if (use_location_package)
		if (site.len && floor.len && room.len && changed) {
			location_package_location_add_user(_d, &site, &floor, &room, presentity);
		}
#endif /* HAVE_LOCATION_PACKAGE */
	if (flags & PARSE_PIDF_PRESCAPS) {
		if (tuple->prescaps != prescaps)
			changed = 1;
		tuple->prescaps = prescaps;
	}

	changed = 1;
	if (changed) presentity->flags |= PFLAG_PRESENCE_CHANGED;

	if ((ret = db_update_presentity(presentity)) < 0) {
		return ret;
	}
	if (!tuple->is_published)
		set_tuple_published(presentity, tuple);
	if (use_db) db_update_presence_tuple(presentity, tuple, 1);
	
	if (modified_tuple) *modified_tuple = tuple;
	return 0;
}

/* FIXME: remove as soon as will be rewritten */

#endif

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

static void add_expires_to_rpl(struct sip_msg *_m, int expires)
{
	char tmp[64];
	
	if (expires < 0) expires = 0;
	
	sprintf(tmp, "Expires: %d\r\n", expires);
	if (!add_lump_rpl(_m, tmp, strlen(tmp), LUMP_RPL_HDR)) {
		LOG(L_ERR, "Can't add expires header to the response\n");
	}
}

static void add_etag_to_rpl(struct sip_msg *_m, str *etag)
{
	char *tmp;

	if (!etag) {
		LOG(L_ERR, "Can't add empty SIP-ETag header to the response\n");
		return;
	}
	
	tmp = (char*)pkg_malloc(32 + etag->len);
	if (!tmp) {
		LOG(L_ERR, "Can't allocate package memory for SIP-ETag header to the response\n");
		return;
	}
	
	sprintf(tmp, "SIP-ETag: %.*s\r\n", etag->len, ZSW(etag->s));
	if (!add_lump_rpl(_m, tmp, strlen(tmp), LUMP_RPL_HDR)) {
		LOG(L_ERR, "Can't add SIP-ETag header to the response\n");
		/* return -1; */
	}
	pkg_free(tmp);
}

static void generate_etag(str *dst, presentity_t *p)
{
	if (!dst) return;
	char tmp[128];

	/* this might not be sufficient !!! */
	sprintf(tmp, "%px%xx%x", p, rand(), (unsigned int)time(NULL));
	str_dup_zt(dst, tmp);
}

static pa_presence_note_t *presence_note2pa(presence_note_t *n, str *etag, time_t expires)
{
	return create_pres_note(etag, &n->value, &n->lang, expires, NULL);
}

static void add_presentity_notes(presentity_t *presentity, presentity_info_t *p, str *etag, time_t expires)
{
	presence_note_t *n;
	pa_presence_note_t *pan;

	if (!p) return;
	
	n = p->first_note;
	while (n) {
		pan = presence_note2pa(n, etag, expires);
		if (pan) add_pres_note(presentity, pan);
		n = n->next;
	}
}

presence_tuple_t *presence_tuple_info2pa(presence_tuple_info_t *i, str *etag, time_t expires)
{
	presence_tuple_t *t = NULL;
	presence_note_t *n, *nn;
	int res;
			
	res = new_presence_tuple(&i->contact, expires, &t, 1);
	if (res != 0) return NULL;
	/* ID for the tuple is newly generated ! */
	t->priority = i->priority;
	switch (i->status) {
		case presence_tuple_open: t->state = PS_ONLINE; break;
		case presence_tuple_closed: t->state = PS_OFFLINE; break;
	}
	str_dup(&t->etag, etag);
	str_dup(&t->published_id, &i->id); /* store published tuple ID - used on update */

	/* add notes for tuple */
	n = i->first_note;
	while (n) {
		nn = create_presence_note(&n->value, &n->lang);
		if (nn) add_tuple_note_no_wb(t, nn);
		n = n->next;
	}
	return t;
}

static void update_tuple(presentity_t *p, presence_tuple_t *t, presence_tuple_info_t *i, time_t expires)
{
	presence_note_t *n, *nn;
	
	t->expires = expires;
	t->priority = i->priority;
	switch (i->status) {
		case presence_tuple_open: t->state = PS_ONLINE; break;
		case presence_tuple_closed: t->state = PS_OFFLINE; break;
	}
	/* FIXME: enable other changes like contact ??? */

	/* remove all old notes for this tuple */
	free_tuple_notes(t);
		
	/* add new notes for tuple */
	n = i->first_note;
	while (n) {
		nn = create_presence_note(&n->value, &n->lang);
		if (nn) add_tuple_note_no_wb(t, nn);
		
		n = n->next;
	}

	if (use_db) db_update_presence_tuple(p, t, 1);
}

static void add_published_tuples(presentity_t *presentity, presentity_info_t *p, str *etag, time_t expires)
{
	presence_tuple_info_t *i;
	presence_tuple_t *t;

	if (!p) return;

	i = p->first_tuple;
	while (i) {
		t = presence_tuple_info2pa(i, etag, expires);
		if (t) add_presence_tuple(presentity, t);
		i = i->next;
	}
}

static presence_tuple_t *find_published_tuple(presentity_t *presentity, str *etag, str *id)
{
	presence_tuple_t *tuple = presentity->tuples;
	while (tuple) {
		if (str_case_equals(&tuple->etag, etag) == 0) {
			if (str_case_equals(&tuple->published_id, id) == 0)
				return tuple;
		}
		tuple = tuple->next;
	}
	return NULL;
}

static int update_published_tuples(presentity_t *presentity, presentity_info_t *p, str *etag, time_t expires)
{
	presence_tuple_info_t *i;
	presence_tuple_t *t, *tt;
	int found = 0;
	double mark = -149.386;

	if (!p) return 0;
	
	/* mark tuples as unprocessed */
	t = presentity->tuples;
	while (t) {
		if (str_case_equals(&t->etag, etag) == 0) {
			t->priority = mark;
			found++;
		}
		t = t->next;
	}
	
	/* add previously not published tuples and update previously published */
	i = p->first_tuple;
	while (i) {
		t = find_published_tuple(presentity, etag, &i->id);
		if (t) {
			/* the tuple was published this way */
			found++;
			update_tuple(presentity, t, i, expires);
		}
		else {
			/* this tuple was not published => add it */
			t = presence_tuple_info2pa(i, etag, expires);
			if (t) add_presence_tuple(presentity, t);
		}
		i = i->next;
	}
	
	/* remove previously published tuples which were not processed (not present now) */
	t = presentity->tuples;
	while (t) {
		tt = t->next;
		if (t->priority == mark) {
			remove_presence_tuple(presentity, t);
			free_presence_tuple(t);
		}
		t = tt;
	}

	return found;
}

static int update_all_published_tuples(presentity_t *p, str *etag, time_t expires)
{
	int found = 0;
	presence_tuple_t *tuple = p->tuples;
	while (tuple) {
		if (str_case_equals(&tuple->etag, etag) == 0) {
			tuple->expires = expires;
			found++;
		}
		tuple = tuple->next;
	}
	return found;
}

static int process_presentity_info(presentity_t *presentity, presentity_info_t *p, str *etag, time_t expires)
{
	if (etag->len < 1) {
		
		if (!p) return -1; /* must be published something */
		
		/* etag is empty -> generate new one and generate published info as new */
		generate_etag(etag, presentity);
		
		/* add all notes for presentity */
		add_presentity_notes(presentity, p, etag, expires);
		
		/* add all tuples */
		add_published_tuples(presentity, p, etag, expires);
	}
	else {
		/* remove all notes for this etag */
		remove_pres_notes(presentity, etag);
		
		if (p) {
			/* add all notes for presentity */
			add_presentity_notes(presentity, p, etag, expires);
			update_published_tuples(presentity, p, etag, expires);
		}
		else update_all_published_tuples(presentity, etag, expires);
	}
	presentity->flags |= PFLAG_PRESENCE_CHANGED;
	return 0;
}

static int publish_presence(struct sip_msg* _m, struct presentity* presentity)
{
	char *body = get_body(_m);
	int body_len = strlen(body);
	int msg_expires = default_expires;
	time_t expires = 0;
	str etag;
	presentity_info_t *p = NULL;
	int content_type = -1;
	
	if (_m->content_type) content_type = get_content_type(_m);
	
	if (_m->expires) {
		if (_m->expires->parsed) {
			msg_expires = ((exp_body_t*)_m->expires->parsed)->val;
		}
	}
	if (msg_expires != 0) expires = msg_expires + act_time;

	if (_m->sipifmatch) str_dup(&etag, (str*)_m->sipifmatch->parsed);
	else str_clear(&etag);
				
	if (body_len > 0) {
		switch (content_type) {
			case MIMETYPE(APPLICATION,PIDFXML):
				if (parse_pidf_document(&p, body, body_len) != 0) {
					LOG(L_ERR, "can't parse PIDF document\n");
					paerrno = PA_UNSUPP_DOC; /* ? PA_PARSE_ERR */
				}
				break;
			case MIMETYPE(APPLICATION,CPIM_PIDFXML):
				if (parse_cpim_pidf_document(&p, body, body_len) != 0) {
					LOG(L_ERR, "can't parse CPIM-PIDF document\n");
					paerrno = PA_UNSUPP_DOC;
				}
				break;
			default:
				LOG(L_ERR, "unsupported Content-Type \'%.*s\' (0x%x) for PUBLISH handling\n", 
						FMT_STR(_m->content_type->body), content_type);
				paerrno = PA_UNSUPP_DOC;
		}
		
		if (paerrno != PA_OK) {
			str_free_content(&etag);
			return -1;
		}
	}
	
	if (process_presentity_info(presentity, p, &etag, expires) == 0) {
		/* add header fields into response */
		add_expires_to_rpl(_m, msg_expires);
		add_etag_to_rpl(_m, &etag);
	}
	str_free_content(&etag);
	if (p) free_presentity_info(p);
	
	return 0;
}

static int publish_presentity(struct sip_msg* _m, struct pdomain* _d, struct presentity* presentity)
{
	event_t *parsed_event = NULL;
	int event_package = EVENT_OTHER;
	str callid = STR_STATIC_INIT("???");
	int changed = 0; /* temporarily */
	int res;
	
	if (_m->event) 
		parsed_event = (event_t *)_m->event->parsed;
	if (parsed_event)
		event_package = parsed_event->parsed;

	LOG(L_DBG, "publish_presentity: event_package=%d -1-\n", event_package);
	switch (event_package) {
		case EVENT_PRESENCE: 
			res = publish_presence(_m, presentity);
			break;
		case EVENT_XCAP_CHANGE:
			/* FIXME: throw it out - it is not presence related, it is XCAP */
			/* FIXME: add headers Expires and SIP-ETag */
			res = publish_presentity_xcap_change(_m, _d, presentity, &changed);
			break;
		default:
			if (_m->callid)	callid = _m->callid->body;
			LOG(L_WARN, "publish_presentity: no handler for event_package=%d"
					" callid=%.*s\n", event_package, callid.len, ZSW(callid.s));
			res = -1;
	}

	return res;
}

/*
 * Handle a publish Request
 */

int handle_publish(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity *p;
	str p_uri = STR_NULL;

	get_act_time();
	paerrno = PA_OK;

	if (parse_publish_hfs(_m) < 0) {
		LOG(L_ERR, "handle_publish(): Error while parsing message header\n");
		goto error;
	}

#if 0
	if (check_message(_m) < 0) {
		LOG(L_ERR, "handle_publish(): Error while checking message\n");
		goto error;
	}
	LOG(L_ERR, "handle_publish -1c-\n");
#endif
	LOG(L_DBG, "handle_publish entered\n");

	d = (struct pdomain*)_domain;

	if (get_pres_uri(_m, &p_uri) < 0 || p_uri.s == NULL || p_uri.len == 0) {
		LOG(L_ERR, "handle_publish(): Error while extracting presentity URI\n");
		goto error;
	}

	lock_pdomain(d);

	if (find_presentity(d, &p_uri, &p) > 0) {
		if (create_presentity_only(_m, d, &p_uri, &p) < 0) {
			LOG(L_ERR, "handle_publish can't create presentity\n");
			goto error2;
		}
	}

	LOG(L_DBG, "handle_publish - publishing status\n");
	
	/* update presentity event state */
	if (p) publish_presentity(_m, d, p);

	unlock_pdomain(d);

	if (send_reply(_m) < 0) return -1;

	LOG(L_DBG, "handle_publish finished\n");
	return 1;

error2:
	unlock_pdomain(d);
error:
	send_reply(_m);
	return 0;
}

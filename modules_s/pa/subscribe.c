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


#define DOCUMENT_TYPE "application/cpim-pidf+xml"
#define DOCUMENT_TYPE_L (sizeof(DOCUMENT_TYPE) - 1)

static struct {
	int event_type;
	int mimes[MAX_MIMES_NR];
} event_package_mimetypes[] = {
	{ EVENT_PRESENCE, { MIMETYPE(APPLICATION,PIDFXML), 
#ifdef SUBTYPE_XML_MSRTC_PIDF
			    MIMETYPE(APPLICATION,XML_MSRTC_PIDF),
#endif
			    MIMETYPE(APPLICATION,XPIDFXML), MIMETYPE(APPLICATION,LPIDFXML), 0 } },
	{ EVENT_PRESENCE_WINFO, { MIMETYPE(APPLICATION,WATCHERINFOXML), 0 }},
#ifdef EVENT_SIP_PROFILE
	{ EVENT_SIP_PROFILE, { MIMETYPE(MESSAGE,EXTERNAL_BODY), 0 }},
#endif
//	{ EVENT_XCAP_CHANGE, { MIMETYPE(APPLICATION,WINFO+XML), 0 } },
	{ -1, { 0 }},
};

/*
 * contact will be NULL if user is offline
 * fixme:locking
 */
void callback(str* _user, str *_contact, int state, void* data)
{
	presentity_t *presentity;

	get_act_time();

	presentity = (struct presentity*)data;

	if (presentity && callback_update_db) {
		presence_tuple_t *tuple = NULL;
		int orig;
		LOG(L_ERR, "callback: uri=%.*s contact=%.*s state=%d presentity=%p\n",
			presentity->uri.len, presentity->uri.s, (_contact ? _contact->len : 0), (_contact ? _contact->s : ""), state,
			presentity);
		if (_contact) {
			if (callback_lock_pdomain) {
				lock_pdomain(presentity->pdomain);
			}

			find_presence_tuple(_contact, presentity, &tuple);
			if (!tuple) {
				new_presence_tuple(_contact, act_time + default_expires, presentity, &tuple);
				add_presence_tuple(presentity, tuple);
			};

			orig = tuple->state;

			if (state == 0) {
				tuple->state = PS_OFFLINE;
				tuple->expires = act_time + 2 * timer_interval;
			}
			else {
				tuple->state = PS_ONLINE;
				tuple->expires = act_time + default_expires;
			}

			db_update_presentity(presentity);

			if (orig != state) {
				presentity->flags |= PFLAG_PRESENCE_CHANGED;
			}

			if (callback_lock_pdomain) {
				unlock_pdomain(presentity->pdomain);
			}
		}
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
	if ((!_uri->s) || (puri.user.len < 1)) {
		_uri->s = puri.host.s;
		_uri->len = puri.host.len;
		return -1;
	}
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
	LOG(L_ERR, "get_pres_uri: _puri=%.*s\n", _puri->len, _puri->s);

	if (extract_plain_uri(_puri) < 0) {
		_puri->s = get_to(_m)->uri.s;
		_puri->len = get_to(_m)->uri.len;
		LOG(L_ERR, "get_pres_uri(2): _puri=%.*s\n", _puri->len, _puri->s);
	
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
	if ( ((rc = parse_headers(_m, HDR_FROM_F | HDR_EVENT_F | HDR_EXPIRES_F | HDR_ACCEPT_F, 0)) == -1) 
	     || (_m->from==0) || (_m->event==0) ) {
		paerrno = PA_PARSE_ERR;
		LOG(L_ERR, "parse_hfs(): Error while parsing headers: rc=%d\n", rc);
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

	/* now look for Accept header */
	if (_m->accept) {
		LOG(L_ERR, "parsing accept header\n");
		if (parse_accept_hdr(_m) < 0) {
			paerrno = PA_ACCEPT_PARSE;
			LOG(L_ERR, "parse_hfs(): Error while parsing Accept header field\n");
			return -10;
		}
	} else if (accept_header_required) {
		LOG(L_ERR, "no accept header\n");
		return -11;
	}

	return 0;
}


/*
 * Check if a message received has been constructed properly
 */
int check_message(struct sip_msg* _m)
{
	LOG(L_ERR, "check_message -0- _m=%p\n", _m);
	if (_m->event) {
		event_t *parsed_event;
		int *accepts_mimes = NULL;

		LOG(L_ERR, "check_message -1-");

		if (_m->accept) {
			accepts_mimes = get_accept(_m);
			if (accepts_mimes) {
				char buf[100];
				int offset = 0;
				int *a = accepts_mimes;
				buf[0] = '0';
				while (*a) {
					offset += sprintf(buf+offset, ":%#06x", *a);
					a++;
				}
				LOG(L_ERR, "pa check_message: accept=%.*s parsed=%s\n",
				    _m->accept->body.len, _m->accept->body.s, buf);
			}
		}
		LOG(L_ERR, "check_message -2- accepts_mimes=%p\n", accepts_mimes);

		if (!_m->event->parsed)
			parse_event(_m->event);
		LOG(L_ERR, "check_message -3-\n");
		parsed_event = (event_t*)(_m->event->parsed);

		LOG(L_ERR, "check_message -4- parsed_event=%p\n", parsed_event);
		if (parsed_event && accepts_mimes) {
			int i = 0;
			int eventtype = parsed_event->parsed;
			LOG(L_ERR, "check_message -4- eventtype=%#06x\n", eventtype);
			while (event_package_mimetypes[i].event_type != -1) {
				LOG(L_ERR, "check_message -4a- eventtype=%#x epm[i].event_type=%#x", 
				    eventtype, event_package_mimetypes[i].event_type);
				if (eventtype == event_package_mimetypes[i].event_type) {
					int j = 0;
					int mimetype;
					while ((mimetype = event_package_mimetypes[i].mimes[j]) != 0) {
						int k = 0;
						while (accepts_mimes[k]) {
							LOG(L_ERR, "check_message -4c- eventtype=%#x mimetype=%#x accepts_mimes[k]=%#x\n", eventtype, mimetype, accepts_mimes[k]);

							if (accepts_mimes[k] == mimetype) {
								int am0 = accepts_mimes[0];
								/* we have a match */
								LOG(L_ERR, "check_message -4b- eventtype=%#x accepts_mime=%#x\n", eventtype, mimetype);
								/* move it to front for later */
								accepts_mimes[0] = mimetype;
								accepts_mimes[k] = am0;
								return 0;
							}
							k++;
						}
						j++;
					}
				}
				i++;
			}
			/* else, none of the mimetypes accepted are generated for this event package */
			{
				char *accept_s = NULL;
				int accept_len = 0;
				if (_m->accept && _m->accept->body.len) {
					accept_s = _m->accept->body.s;
					accept_len = _m->accept->body.len;
				}
				LOG(L_ERR, "check_message(): Accepts %.*s not valid for event package et=%.*s\n",
				    _m->accept->body.len, _m->accept->body.s, _m->event->body.len, _m->event->body.s);
				return -1;
			}
		}
		LOG(L_ERR, "check_message -5-\n");
	}
	return 0;
}


int get_preferred_event_mimetype(struct sip_msg *_m, int et)
{
	int acc = 0;
	if (_m->accept) {
		//LOG(L_ERR, "%s: has accept header\n", __FUNCTION__);
		int *accepts_mimes = get_accept(_m);
		acc = accepts_mimes[0];
	} else {
		int i = 0;
		//LOG(L_ERR, "%s: no accept header\n", __FUNCTION__);
		while (event_package_mimetypes[i].event_type != -1) {
			if (event_package_mimetypes[i].event_type == et) {
				acc = event_package_mimetypes[i].mimes[0];
				LOG(L_ERR, "%s: defaulting to mimetype %x for event_type=%d\n", __FUNCTION__, acc, et);
				break;
			}
			i++;
		}
	}
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
	dst->s = (char *)shm_malloc(dst->len + 1);
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

/*
 * Create a new presentity and corresponding watcher list
 */
int create_presentity(struct sip_msg* _m, struct pdomain* _d, str* _puri, 
			     struct presentity** _p, struct watcher** _w)
{
	time_t e;
	dlg_t* dialog;
	str watch_uri;
	str watch_dn;
	str server_contact = {0, 0};
	event_t *event = NULL;
	int et = 0;
	int acc = 0;
	if (_m->event) {
		event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		et = EVENT_PRESENCE;
	}
	acc = get_preferred_event_mimetype(_m, et);

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

	if (get_watch_uri(_m, &watch_uri, &watch_dn) < 0) {
		LOG(L_ERR, "create_presentity(): Error while extracting watcher URI\n");
		return -1;
	}

	if (new_presentity(_d, _puri, _p) < 0) {
		LOG(L_ERR, "create_presentity(): Error while creating presentity\n");
		return -2;
	}

	if (tmb.new_dlg_uas(_m, 200, &dialog) < 0) {
		paerrno = PA_DIALOG_ERR;
		LOG(L_ERR, "create_presentity(): Error while creating dialog state\n");
		free_presentity(*_p);
		return -3;
	}

	if (extract_server_contact(_m, &server_contact) != 0) {
		paerrno = PA_DIALOG_ERR;
		LOG(L_ERR, "create_presentity(): Error while extracting server contact\n");
		free_presentity(*_p);
		return -3;
	}
	
	if (et != EVENT_PRESENCE_WINFO) {
		if (add_watcher(*_p, &watch_uri, e, et, acc, dialog, &watch_dn, &server_contact, _w) < 0) {
			LOG(L_ERR, "create_presentity(): Error while adding a watcher\n");
			if (server_contact.s) shm_free(server_contact.s);
			tmb.free_dlg(dialog);
			free_presentity(*_p);
			return -4;
		}
	} else if (et == EVENT_PRESENCE_WINFO) {
		if (add_winfo_watcher(*_p, &watch_uri, e, et, acc, dialog, &watch_dn, &server_contact, _w) < 0) {
			LOG(L_ERR, "create_presentity(): Error while adding a winfo watcher\n");
			if (server_contact.s) shm_free(server_contact.s);
			tmb.free_dlg(dialog);
			free_presentity(*_p);
			return -5;
		}
	}
	if (server_contact.s) shm_free(server_contact.s);
	add_presentity(_d, *_p);

	/* FIXME: experimental */
/*	_d->reg(&watch_uri, _puri, (void*)callback, *_p);
	LOG(L_ERR, "registering callback to %.*s (%p),  %.*s, %p (%p)\n",
			watch_uri.len, watch_uri.s, &watch_uri,
			_puri->len, _puri->s, *_p, _p); */
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
	str watch_dn;
	str server_contact = {0, 0};
	event_t *event = NULL;
	int et = 0;
	int acc = 0;
	dlg_id_t dlg_id;
	
	if (_m->event) {
		event = (event_t*)(_m->event->parsed);
		et = event->parsed;
	} else {
		LOG(L_ERR, "update_presentity defaulting to EVENT_PRESENCE\n");
		et = EVENT_PRESENCE;
	}
	acc = get_preferred_event_mimetype(_m, et);

	if (_m->expires) {
		e = ((exp_body_t*)_m->expires->parsed)->val;
	} else {
		e = default_expires;
	}

	if (get_watch_uri(_m, &watch_uri, &watch_dn) < 0) {
		LOG(L_ERR, "update_presentity(): Error while extracting watcher URI\n");
		return -1;
	}

	get_dlg_id(_m, &dlg_id);
	
	if (find_watcher_dlg(_p, &dlg_id, et, _w) == 0) { /* FIXME: experimental */
	/* if (find_watcher(_p, &watch_uri, et, _w) == 0) { */
		LOG(L_ERR, "update_presentity() found watcher\n");
		if (e == 0) {
			/* FIXME: experimental */
/*			
			if (et != EVENT_PRESENCE_WINFO) {
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
*/			
			(*_w)->expires = 0;   /* The watcher will be freed after NOTIFY is sent */
			/* FIXME: experimental */
/*			if (!_p->watchers && !_p->winfo_watchers) {
				remove_presentity(_d, _p); 
			}*/
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
			
			if (extract_server_contact(_m, &server_contact) != 0) {
				paerrno = PA_DIALOG_ERR;
				LOG(L_ERR, "create_presentity(): Error while extracting server contact\n");
				return -3;
			}

			if (et != EVENT_PRESENCE_WINFO) {
				if (add_watcher(_p, &watch_uri, e, et, acc, dialog, &watch_dn, &server_contact, _w) < 0) {
					LOG(L_ERR, "update_presentity(): Error while creating presentity\n");
					tmb.free_dlg(dialog);
					if (server_contact.s) shm_free(server_contact.s);
					return -5;
				}
			} else {
				if (add_winfo_watcher(_p, &watch_uri, e, et, acc, dialog, &watch_dn, &server_contact, _w) < 0) {
					LOG(L_ERR, "update_presentity(): Error while creating winfo watcher\n");
					tmb.free_dlg(dialog);
					if (server_contact.s) shm_free(server_contact.s);
					return -5;
				}			
			}
			
			if (server_contact.s) shm_free(server_contact.s);
			
		} else {
			DBG("update_presentity(): expires = 0 but no watcher found\n");
			*_w = 0;
		}
	}

	return 0;
}


/*
 * Handle a registration request -- make sure aor exists in presentity table
 */
/*
 * Extract Address of Record
 */
#define MAX_AOR_LEN 256

int pa_extract_aor(str* _uri, str* _a)
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

     LOG(L_ERR, "pa_handle_registration about to return 1");
     unlock_pdomain(d);
     return 1;
	
 error2:
     LOG(L_ERR, "pa_handle_registration about to return -1\n");
     unlock_pdomain(d);
     return -1;
 error:
     LOG(L_ERR, "pa_handle_registration about to return -2\n");
     return -1;
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
	char tmp[64];
	int i;

	LOG(L_ERR, "handle_subscription() entered\n");
	get_act_time();
	paerrno = PA_OK;

	if (parse_hfs(_m, 0) < 0) {
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
	
	LOG(L_ERR, "handle_subscription(): -1-\n");
	if (find_presentity(d, &p_uri, &p) > 0) {
		LOG(L_ERR, "handle_subscription(): -2-\n");
		if (create_presentity(_m, d, &p_uri, &p, &w) < 0) {
			LOG(L_ERR, "handle_subscription(): Error while creating new presentity\n");
			goto error2;
		}
	} else {
		LOG(L_ERR, "handle_subscription(): -3-\n");
		if (update_presentity(_m, d, p, &w) < 0) {
			LOG(L_ERR, "handle_subscription(): Error while updating presentity\n");
			goto error2;
		}
	}

	/* add expires header field into response */
	if (w) i = w->expires - act_time;
	else i = 0;
	if (i < 0) i = 0;
	sprintf(tmp, "Expires: %d\r\n", i);
	if (!add_lump_rpl(_m, tmp, strlen(tmp), LUMP_RPL_HDR)) {
		LOG(L_ERR, "handle_subscription(): Can't add Expires header to the response\n");
		/* return -1; */
	}
	
	if (send_reply(_m) < 0) {
	  LOG(L_ERR, "handle_subscription(): Error while sending reply\n");
	  goto error2;
	}

	if (p) {
		p->flags |= PFLAG_WATCHERINFO_CHANGED;
	}
	if (w) {
		w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
	}

	LOG(L_ERR, "handle_subscription about to return 1: w->event_package=%d w->accept=%d p->flags=%x w->flags=%x w=%p\n",
	    (w ? w->event_package : -1), (w ? w->preferred_mimetype : -1), (p ? p->flags : -1), (w ? w->flags : -1), w);
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
		if (find_watcher_dlg(p, &w_dlg_id, et, &w) == 0) { /* FIXME: experimental */
		/* if (find_watcher(p, &w_uri, et, &w) == 0) { */
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


/*
 * Returns 1 if possibly a user agent can handle SUBSCRIBE
 * itself, 0 if it cannot for sure
 */
int pua_exists(struct sip_msg* _m, char* _domain, char* _s2)
{

	return 0;
}




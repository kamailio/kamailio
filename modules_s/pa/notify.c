/*
 * Presence Agent, notifications
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
 * --------
 * 2003-02-28 protocolization of t_uac_dlg completed (jiri)
 */

#include "../../str.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../parser/parse_event.h"
#include "pa_mod.h"
#include "lpidf.h"
#include "xpidf.h"
#include "presentity.h"
#include "pidf.h"
#include "common.h"
#include "paerrno.h"
#include "notify.h"
#include "watcher.h"
#include "location.h"


#define CONTENT_TYPE "Content-Type: "
#define CONTENT_TYPE_L  (sizeof(CONTENT_TYPE) - 1)

#define EVENT "Event: "
#define EVENT_L (sizeof(EVENT) - 1)

#define PRESENCE_TEXT "presence"
#define PRESENCE_TEXT_L (sizeof(PRESENCE_TEXT) - 1)

#define WINFO_TEXT "presence.winfo"
#define WINFO_TEXT_L (sizeof(WINFO_TEXT) - 1)

#define XCAP_CHANGE_TEXT "xcap-change"
#define XCAP_CHANGE_TEXT_L (sizeof(XCAP_CHANGE_TEXT) - 1)

#define CONT_TYPE_XPIDF "application/xpidf+xml"
#define CONT_TYPE_XPIDF_L  (sizeof(CONT_TYPE_XPIDF) - 1)

#define CONT_TYPE_LPIDF "text/lpidf"
#define CONT_TYPE_LPIDF_L (sizeof(CONT_TYPE_LPIDF) - 1)

#define CONT_TYPE_PIDF "application/cpim-pidf+xml"
#define CONT_TYPE_PIDF_L (sizeof(CONT_TYPE_PIDF) - 1)

#define CONT_TYPE_WINFO "application/watcherinfo+xml"
#define CONT_TYPE_WINFO_L (sizeof(CONT_TYPE_WINFO) - 1)

#define CONT_TYPE_XCAP_CHANGE "application/xcap-change+xml"
#define CONT_TYPE_XCAP_CHANGE_L (sizeof(CONT_TYPE_XCAP_CHANGE) - 1)

#define SUBSCRIPTION_STATE "Subscription-State: "
#define SUBSCRIPTION_STATE_L (sizeof(SUBSCRIPTION_STATE) - 1)

#define SS_EXPIRES ";expires="
#define SS_EXPIRES_L (sizeof(SS_EXPIRES) - 1)

#define SS_REASON ";reason="
#define SS_REASON_L (sizeof(SS_REASON) - 1)

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)

#define ST_ACTIVE "active"
#define ST_ACTIVE_L (sizeof(ST_ACTIVE) - 1)

#define ST_TERMINATED "terminated"
#define ST_TERMINATED_L (sizeof(ST_TERMINATED) - 1)

#define ST_PENDING "pending"
#define ST_PENDING_L (sizeof(ST_PENDING) - 1)

#define REASON_DEACTIVATED "deactivated"
#define REASON_DEACTIVATED_L (sizeof(REASON_DEACTIVATED) - 1)

#define REASON_NORESOURCE "noresource"
#define REASON_NORESOURCE_L (sizeof(REASON_NORESOURCE) - 1)

#define REASON_PROBATION "probation"
#define REASON_PROBATION_L (sizeof(REASON_PROBATION) - 1)

#define REASON_REJECTED "rejected"
#define REASON_REJECTED_L (sizeof(REASON_REJECTED) - 1)

#define REASON_TIMEOUT "timeout"
#define REASON_TIMEOUT_L (sizeof(REASON_TIMEOUT) - 1)

#define REASON_GIVEUP "giveup"
#define REASON_GIVEUP_L (sizeof(REASON_GIVEUP) - 1)

#define METHOD_NOTIFY "NOTIFY"
#define METHOD_NOTIFY_L (sizeof(METHOD_NOTIFY) - 1)


/*
 * Subscription-State values
 */
static str subs_states[] = {
	{ST_ACTIVE,     ST_ACTIVE_L    },
	{ST_TERMINATED, ST_TERMINATED_L},
	{ST_PENDING,    ST_PENDING_L   }
};


/* 
 * Subscription-State reason parameter values
 */
static str reason[] = {
	{REASON_DEACTIVATED, REASON_DEACTIVATED_L},
	{REASON_NORESOURCE,  REASON_NORESOURCE_L },
	{REASON_PROBATION,   REASON_PROBATION_L  },
	{REASON_REJECTED,    REASON_REJECTED_L   },
	{REASON_TIMEOUT,     REASON_TIMEOUT_L    },
	{REASON_GIVEUP,      REASON_GIVEUP_L     }
};


static str method = {METHOD_NOTIFY, METHOD_NOTIFY_L};


#define BUF_LEN 4096

static char headers_buf[BUF_LEN];
static char buffer[BUF_LEN];

static str headers = {headers_buf, 0};
static str body = {buffer, 0};


static inline int add_event_hf(str* _h, int _l, int accept)
{
	int event_l;
	char *event;
	if (accept == DOC_WINFO) {
		event = WINFO_TEXT;
		event_l = WINFO_TEXT_L;
	} else if (accept == DOC_XCAP_CHANGE) {
		event = XCAP_CHANGE_TEXT;
		event_l = XCAP_CHANGE_TEXT_L;
	} else {
		event = PRESENCE_TEXT;
		event_l = PRESENCE_TEXT_L;
	}

	if (_l < EVENT_L + event_l + CRLF_L) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "add_event_hf(): Buffer too small\n");
		return -1;
	}

	str_append(_h, EVENT, EVENT_L);
	str_append(_h, event, event_l);
	str_append(_h, CRLF, CRLF_L);
	return 0;
}


static inline int add_cont_type_hf(str* _h, int _l, doctype_t _d)
{
	switch(_d) {
	case DOC_XPIDF:
		if (_l < CONTENT_TYPE_L + CONT_TYPE_XPIDF_L + CRLF_L) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -1;
		}
		
		str_append(_h, CONTENT_TYPE CONT_TYPE_XPIDF CRLF,
			   CONTENT_TYPE_L + CONT_TYPE_XPIDF_L + CRLF_L);
		return 0;
		
	case DOC_LPIDF:
		if (_l < CONTENT_TYPE_L + CONT_TYPE_LPIDF_L + CRLF_L) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -2;
		}
		str_append(_h, CONTENT_TYPE CONT_TYPE_LPIDF CRLF,
			   CONTENT_TYPE_L + CONT_TYPE_LPIDF_L + CRLF_L);
		return 0;

	case DOC_PIDF:
		if (_l < CONTENT_TYPE_L + CONT_TYPE_PIDF_L + CRLF_L) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -2;
		}
		str_append(_h, CONTENT_TYPE CONT_TYPE_PIDF CRLF,
			   CONTENT_TYPE_L + CONT_TYPE_PIDF_L + CRLF_L);
		return 0;

	case DOC_WINFO:
		if (_l < CONTENT_TYPE_L + CONT_TYPE_WINFO_L + CRLF_L) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -2;
		}
		str_append(_h, CONTENT_TYPE CONT_TYPE_WINFO CRLF,
			   CONTENT_TYPE_L + CONT_TYPE_WINFO_L + CRLF_L);
		return 0;

	case DOC_XCAP_CHANGE:
		if (_l < CONTENT_TYPE_L + CONT_TYPE_XCAP_CHANGE_L + CRLF_L) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -2;
		}
		str_append(_h, CONTENT_TYPE CONT_TYPE_XCAP_CHANGE CRLF,
			   CONTENT_TYPE_L + CONT_TYPE_XCAP_CHANGE_L + CRLF_L);
		return 0;

	default:
		paerrno = PA_UNSUPP_DOC;
		LOG(L_ERR, "add_cont_type_hf(): Unsupported document type\n");
		return -3;
	}
}


static inline int add_subs_state_hf(str* _h, int _l, subs_state_t _s, ss_reason_t _r, time_t _e)
{
	char* num;
	int len;

	if (_l < SUBSCRIPTION_STATE_L + subs_states[_s].len + SS_EXPIRES_L + 
	    SS_REASON_L + reason[_r].len + CRLF_L) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "add_subs_state_hf(): Buffer too small\n");
		return -1;
	}

	str_append(_h, SUBSCRIPTION_STATE, SUBSCRIPTION_STATE_L);
	str_append(_h, subs_states[_s].s, subs_states[_s].len);
	
	switch(_s) {
	case SS_ACTIVE:
		str_append(_h, SS_EXPIRES, SS_EXPIRES_L);
		num = int2str((unsigned int)_e, &len);
		str_append(_h, num, len);
		break;

	case SS_TERMINATED:
		str_append(_h, SS_REASON, SS_REASON_L);
		str_append(_h, reason[_r].s, reason[_r].len);
		break;

	case SS_PENDING:
		break;
	}

	str_append(_h, CRLF, CRLF_L);
	return 0;
}


static inline int create_headers(struct watcher* _w)
{
	time_t t;
	subs_state_t s;

	headers.len = 0;
	
	if (add_event_hf(&headers, BUF_LEN, _w->accept) < 0) {
		LOG(L_ERR, "create_headers(): Error while adding Event header field\n");
		return -1;
	}

	if (add_cont_type_hf(&headers, BUF_LEN - headers.len, _w->accept)  < 0) {
		LOG(L_ERR, "create_headers(): Error while adding Content-Type header field\n");
		return -2;
	}

	if (_w && _w->expires) t = _w->expires - time(0);
	else t = 0;

	if (t == 0) {
		s = SS_TERMINATED;
	} else {
		s = SS_ACTIVE;
	}

	if (add_subs_state_hf(&headers, BUF_LEN - headers.len, s, SR_TIMEOUT, t) < 0) {
		LOG(L_ERR, "create_headers(): Error while adding Subscription-State\n");
		return -3;
	}

	return 0;
}


static int send_xpidf_notify(struct presentity* _p, struct watcher* _w)
{
	xpidf_status_t st;
	presence_tuple_t *tuple = _p->tuples;

	/* Send a notify, saved Contact will be put in
	 * Request-URI, To will be put in from and new tag
	 * will be generated, callid will be callid,
	 * from will be put in to including tag
	 */

	if (start_xpidf_doc(&body, BUF_LEN) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): start_xpidf_doc failed\n");
		return -1;
	}

	if (xpidf_add_presentity(&body, BUF_LEN - body.len, &_p->uri) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): xpidf_add_presentity failed\n");
		return -3;
	}
	while (tuple) {

		switch(tuple->state) {
		case PS_ONLINE: st = XPIDF_ST_OPEN; break;
		default: st = XPIDF_ST_CLOSED; break;
		}

		if (xpidf_add_address(&body, BUF_LEN - body.len, &_p->uri, st) < 0) {
			LOG(L_ERR, "send_xpidf_notify(): xpidf_add_address failed\n");
			return -3;
		}

		tuple = tuple->next;
	}
	if (end_xpidf_doc(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): end_xpidf_doc failed\n");
		return -5;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): Error while adding headers\n");
		return -6;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	return 0;

}


static int send_lpidf_notify(struct presentity* _p, struct watcher* _w)
{
	lpidf_status_t st;
	presence_tuple_t *tuple = _p->tuples;

	if (lpidf_add_presentity(&body, BUF_LEN - body.len, &_p->uri) < 0) {
		LOG(L_ERR, "send_lpidf_notify(): Error in lpidf_add_presentity\n");
		return -2;
	}

	while (tuple) {
		switch(tuple->state) {
		case PS_OFFLINE: st = LPIDF_ST_CLOSED; break;
		default: st = LPIDF_ST_OPEN; break;
		}

		if (lpidf_add_address(&body, BUF_LEN - body.len, &_p->uri, st) < 0) {
			LOG(L_ERR, "send_lpidf_notify(): lpidf_add_address failed\n");
			return -3;
		}

		tuple = tuple->next;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_lpidf_notify(): Error while adding headers\n");
		return -4;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	return 0;
}

static int send_pidf_notify(struct presentity* _p, struct watcher* _w)
{
	xpidf_status_t st;
	presence_tuple_t *tuple = _p->tuples;

	/* Send a notify, saved Contact will be put in
	 * Request-URI, To will be put in from and new tag
	 * will be generated, callid will be callid,
	 * from will be put in to including tag
	 */

	LOG(L_ERR, "  send_pidf_notify\n");

	if (start_pidf_doc(&body, BUF_LEN) < 0) {
		LOG(L_ERR, "send_pidf_notify(): start_pidf_doc failed\n");
		return -1;
	}

	if (pidf_add_presentity(&body, BUF_LEN - body.len, &_p->uri) < 0) {
		LOG(L_ERR, "send_pidf_notify(): pidf_add_presentity failed\n");
		return -3;
	}

	while (tuple) {
		if (start_pidf_tuple(&body, &tuple->id, BUF_LEN - body.len) < 0) {
			LOG(L_ERR, "send_pidf_notify(): start_pidf_tuple failed\n");
			return -4;
		}

		switch(tuple->state) {
		case PS_ONLINE: st = XPIDF_ST_OPEN; break;
		default: st = XPIDF_ST_CLOSED; break;
		}

		if (pidf_add_contact(&body, BUF_LEN - body.len, &tuple->contact, st, tuple->priority) < 0) {
			LOG(L_ERR, "send_pidf_notify(): pidf_add_contact failed\n");
			return -3;
		}

		if (pidf_add_location(&body, BUF_LEN - body.len,
				      &tuple->location.loc,
				      &tuple->location.site, &tuple->location.floor, &tuple->location.room,
				      tuple->location.x, tuple->location.y, tuple->location.radius, 
				      tuple->prescaps) < 0) {
			LOG(L_ERR, "send_pidf_notify(): pidf_add_location failed\n");
			return -4;
		}

		if (end_pidf_tuple(&body, BUF_LEN - body.len) < 0) {
			LOG(L_ERR, "send_pidf_notify(): end_pidf_tuple failed\n");
			return -5;
		}
		tuple = tuple->next;
	}

	if (end_pidf_doc(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_pidf_notify(): end_xpidf_doc failed\n");
		return -6;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_pidf_notify(): Error while adding headers\n");
		return -7;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	return 0;
}

static int send_winfo_notify(struct presentity* _p, struct watcher* _w)
{
	watcher_t *watcher = _p->watchers;

	LOG(L_INFO, "send_winfo_notify: watcher=%p winfo_watcher=%p\n", watcher, _w);
	if (start_winfo_doc(&body, BUF_LEN) < 0) {
		LOG(L_ERR, "send_winfo_notify(): start_winfo_doc failed\n");
		return -1;
	}

	if (winfo_start_resource(&body, BUF_LEN - body.len, &_p->uri, _w) < 0) {
		LOG(L_ERR, "send_winfo_notify(): winfo_add_resource failed\n");
		return -3;
	}

	while (watcher) {
		if (winfo_add_watcher(&body, BUF_LEN - body.len, watcher) < 0) {
			LOG(L_ERR, "send_winfo_notify(): winfo_add_watcher failed\n");
			return -3;
		}

		watcher = watcher->next;
	}

	if (winfo_end_resource(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_winfo_notify(): winfo_add_resource failed\n");
		return -5;
	}

	if (end_winfo_doc(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_winfo_notify(): end_xwinfo_doc failed\n");
		return -6;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_winfo_notify(): Error while adding headers\n");
		return -7;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	return 0;
}

static int send_xcap_change_notify(struct presentity* _p, struct watcher* _w)
{
	int len = 0;
	int presence_list_changed = _p->flags & PFLAG_PRESENCE_LISTS_CHANGED;
	int watcherinfo_changed = _p->flags & PFLAG_WATCHERINFO_CHANGED;

	
	LOG(L_ERR, "  send_xcap_change flags=%x\n", _p->flags);

	len += sprintf(body.s + len, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	len += sprintf(body.s + len, "<documents xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\r\n");
	if (presence_list_changed) { 
		len += sprintf(body.s + len, "  <document uri=\"http://%.*s/presence-lists/users/%.*s/presence.xml\">\r\n",
			       pa_domain.len, pa_domain.s, _p->uri.len, _p->uri.s);
		len += sprintf(body.s + len, "    <change method=\"PUT\">someone@example.com</change>\r\n");
		len += sprintf(body.s + len, "  </document>\r\n");
	}
	if (watcherinfo_changed) {
		len += sprintf(body.s + len, "  <document uri=\"http://%.*s/watcherinfo/users/%.*s/watcherinfo.xml\">\r\n",
			       pa_domain.len, pa_domain.s, _p->uri.len, _p->uri.s);
		len += sprintf(body.s + len, "    <change method=\"PUT\">someone@example.com</change>\r\n");
		len += sprintf(body.s + len, "  </document>\r\n");
	}
	len += sprintf(body.s + len, "</documents>\r\n");
	body.len = len;

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_location_notify(): Error while adding headers\n");
		return -7;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	return 0;
}

int send_location_notify(struct presentity* _p, struct watcher* _w)
{
	resource_list_t *user = _p->location_package.users;

	LOG(L_ERR, "send_location_notify to watcher %.*s\n", _w->uri.len, _w->uri.s);

	if (location_doc_start(&body, BUF_LEN) < 0) {
		LOG(L_ERR, "send_location_notify(): start_location_doc failed\n");
		return -1;
	}

	if (location_doc_start_userlist(&body, BUF_LEN - body.len, &_p->uri) < 0) {
		LOG(L_ERR, "send_location_notify(): location_add_uri failed\n");
		return -3;
	}

	while (user) {
		if (location_doc_add_user(&body, BUF_LEN - body.len, &user->uri) < 0) {
			LOG(L_ERR, "send_location_notify(): location_add_watcher failed\n");
			return -3;
		}

		user = user->next;
	}

	if (location_doc_end_resource(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_location_notify(): location_add_resource failed\n");
		return -5;
	}

	if (location_doc_end(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_location_notify(): end_xlocation_doc failed\n");
		return -6;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_location_notify(): Error while adding headers\n");
		return -7;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	return 0;
}

int send_notify(struct presentity* _p, struct watcher* _w)
{
	int rc = 0;
	body.len = 0;

	if (_w->uri.s == NULL) {
		LOG(L_ERR, "watcher uri.s is NULL\n");
		return -1;
	}
	if (strlen(_w->uri.s) == 0) {
		LOG(L_ERR, "watcher uri.s is zero length\n");
		return -2;
	}

	LOG(L_ERR, "notifying %.*s _p->flags=%x _w->event_package=%d _w->accept=%d _w->status=%d\n", 
	    _w->uri.len, _w->uri.s, _p->flags, _w->event_package, _w->accept, _w->status);
	if ((_p->flags & (PFLAG_PRESENCE_CHANGED|PFLAG_WATCHERINFO_CHANGED)) 
	    && (_w->event_package == EVENT_PRESENCE)
	    && (_w->status = WS_ACTIVE)) {
		switch(_w->accept) {
		case DOC_XPIDF:
			rc = send_xpidf_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_xpidf_notify returned %d\n", rc);
			break;

		case DOC_LPIDF:
			rc = send_lpidf_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_lpidf_notify returned %d\n", rc);
			break;

		case DOC_PIDF:
		default:
			rc = send_pidf_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_pidf_notify returned %d\n", rc);
		}
	}
	if ((_p->flags & PFLAG_WATCHERINFO_CHANGED) 
	    && (_w->event_package == EVENT_PRESENCE_WINFO)) {
		switch(_w->accept) {
		case DOC_WINFO:
			rc = send_winfo_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_winfo_notify returned %d\n", rc);
			return rc;
		default:
			/* inapplicable */
		  ;
		}
	}
	if ((_p->flags & PFLAG_XCAP_CHANGED) 
	    && (_w->event_package == EVENT_XCAP_CHANGE)) {
		switch(_w->accept) {
		case DOC_XCAP_CHANGE:
		default:
			rc = send_xcap_change_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_xcap_change_notify returned %d\n", rc);
		}
	}
	if ((_p->flags & PFLAG_LOCATION_CHANGED) 
	    && (_w->event_package == EVENT_LOCATION)) {
		switch(_w->accept) {
		case DOC_LOCATION:
			rc = send_location_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_location_notify returned %d\n", rc);
			return rc;
		default:
		  rc = -1;
		  ;
		}
	}

	return rc;
}

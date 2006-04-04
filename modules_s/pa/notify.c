/*
 * Presence Agent, notifications
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
 * --------
 * 2003-02-28 protocolization of t_uac_dlg completed (jiri)
 */

#include "../../str.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../parser/parse_event.h"
#include "pa_mod.h"
#include "presentity.h"
#include "common.h"
#include "paerrno.h"
#include "notify.h"
#include "watcher.h"
#include "location.h"
#include "qsa_interface.h"

#include <presence/pidf.h>
#include <presence/xpidf.h>
#include <presence/lpidf.h>
#include "winfo_doc.h"

#define CONTACT "Contact: "
#define CONTACT_L  (sizeof(CONTACT) - 1)

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

#define CONT_TYPE_PIDF "application/pidf+xml"
#define CONT_TYPE_PIDF_L (sizeof(CONT_TYPE_PIDF) - 1)

#define CONT_TYPE_CPIM_PIDF "application/cpim-pidf+xml"
#define CONT_TYPE_CPIM_PIDF_L (sizeof(CONT_TYPE_CPIM_PIDF) - 1)

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

#define REASON_NORESOURCE "noresource"

#define REASON_PROBATION "probation"

#define REASON_REJECTED "rejected"

#define REASON_TIMEOUT "timeout"

#define REASON_GIVEUP "giveup"

#define METHOD_NOTIFY "NOTIFY"


/* 
 * Subscription-State reason parameter values
 */
static str reason[] = {
	STR_STATIC_INIT(REASON_DEACTIVATED),
	STR_STATIC_INIT(REASON_NORESOURCE),
	STR_STATIC_INIT(REASON_PROBATION),
	STR_STATIC_INIT(REASON_REJECTED),
	STR_STATIC_INIT(REASON_TIMEOUT),
	STR_STATIC_INIT(REASON_GIVEUP),
};


static str method = STR_STATIC_INIT(METHOD_NOTIFY);

static inline int add_event_hf(dstring_t *buf, int event_package)
{
	dstr_append_zt(buf, "Event: ");
	dstr_append_zt(buf, event_package2str(event_package));
	dstr_append_zt(buf, "\r\n");
	
	return 0;
}


static inline int add_cont_type_hf(dstring_t *buf, str *content_type)
{
	/* content types can have dynamical parameters (multipart/related)
	 * => don't generate them "staticaly"; use values created in the
	 * time of document creation */
	
	if (is_str_empty(content_type)) return 0; /* documents without body doesn't need it */
	
	dstr_append_zt(buf, "Content-Type: ");
	
	/* FIXME: remove */
/*	switch(doc_type) {
		case DOC_XPIDF:
			dstr_append_zt(buf, "application/xpidf+xml");
			break;
		case DOC_LPIDF:
			dstr_append_zt(buf, "text/lpidf");
			break;

/ *		case DOC_MSRTC_PIDF: * /
		case DOC_PIDF:
			dstr_append_zt(buf, "application/pidf+xml");
			break;
			
		case DOC_CPIM_PIDF:
			dstr_append_zt(buf, "application/cpim-pidf+xml");
			break;
			
		case DOC_WINFO:
			dstr_append_zt(buf, "application/watcherinfo+xml");
			break;

		/ * case DOC_XCAP_CHANGE:
			dstr_append_zt(buf, "application/xcap-change+xml");
			break; * /

		default:
			paerrno = PA_UNSUPP_DOC;
			LOG(L_ERR, "add_cont_type_hf(): Unsupported document type\n");
			return -3;
	}*/
	
	dstr_append_str(buf, content_type);
	dstr_append_zt(buf, "\r\n");
	
	return 0;
}


static inline int add_subs_state_hf(dstring_t *buf, watcher_status_t _s, time_t _e)
{
	char* num;
	int len;
	str s = STR_NULL;
	ss_reason_t _r;
	
	if (_e <= 0) _r = SR_TIMEOUT;
	else _r = SR_REJECTED;

	switch(_s) {
		case WS_ACTIVE: ;
			s = watcher_status_names[WS_ACTIVE];
			break;
		case WS_REJECTED:
		case WS_PENDING_TERMINATED:
		case WS_TERMINATED:
			s = watcher_status_names[WS_TERMINATED];
			break;
		case WS_PENDING: 
			s = watcher_status_names[WS_PENDING];
			break;
	}
	
	dstr_append_zt(buf, "Subscription-State: ");
	dstr_append_str(buf, &s);
	
	switch(_s) {
		case WS_PENDING:;
		case WS_ACTIVE:
			dstr_append_zt(buf, ";expires=");
			num = int2str((unsigned int)_e, &len);
			dstr_append(buf, num, len);
			break;

		case WS_REJECTED:
		case WS_PENDING_TERMINATED:
		case WS_TERMINATED:
			dstr_append_zt(buf, ";reason=");
			dstr_append_str(buf, &reason[_r]);
			break;

	}

	dstr_append_zt(buf, "\r\n");
	return 0;
}

static inline int create_headers(struct watcher* _w, str *dst, str *content_type)
{
	dstring_t buf;
	time_t t;
	int err = 0;
	
	dstr_init(&buf, 256);
	str_clear(dst);

	/* Event header */

	dstr_append_zt(&buf, "Event: ");
	dstr_append_zt(&buf, event_package2str(_w->event_package));
	dstr_append_zt(&buf, "\r\n");
	
	/* Content-Type header */
	
	/* content types can have dynamical parameters (multipart/related)
	 * => don't generate them "staticaly"; use values created in the
	 * time of document creation */
	if (!is_str_empty(content_type)) { /* documents without body doesn't need it */
		dstr_append_zt(&buf, "Content-Type: ");
		dstr_append_str(&buf, content_type);
		dstr_append_zt(&buf, "\r\n");
	}
	
	/* Contact header */
	
	if (is_str_empty(&_w->server_contact)) {
		LOG(L_WARN, "add_contact_hf(): Can't add empty contact to NOTIFY.\n");
	}
	else {
		dstr_append_zt(&buf, "Contact: ");
		dstr_append_str(&buf, &_w->server_contact);
		dstr_append_zt(&buf, "\r\n");
	}

	/* Subscription-State header */
	
	if (_w->expires) t = _w->expires - time(0);
	else t = 0;

	if (add_subs_state_hf(&buf, _w->status, t) < 0) {
		LOG(L_ERR, "create_headers(): Error while adding Subscription-State\n");
		dstr_destroy(&buf);
		return -3;
	}

	err = dstr_get_str(&buf, dst);
	dstr_destroy(&buf);

	return err;
}

static int send_presence_notify(struct presentity* _p, struct watcher* _w)
{
	/* Send a notify, saved Contact will be put in
	 * Request-URI, To will be put in from and new tag
	 * will be generated, callid will be callid,
	 * from will be put in to including tag
	 */
	str doc = STR_NULL;
	str content_type = STR_NULL;
	str headers = STR_NULL;
	str body = STR_STATIC_INIT("");
	presentity_info_t *pinfo = NULL;
	int res = 0;
	
	pinfo = presentity2presentity_info(_p);
	if (!pinfo) {
		LOG(L_ERR, "can't create presence document (0)\n");
		return -1;
	}

	switch(_w->preferred_mimetype) {
		case DOC_XPIDF:
			res = create_xpidf_document(pinfo, &doc, &content_type);
			break;

		case DOC_LPIDF:
			res = create_lpidf_document(pinfo, &doc, &content_type);
			break;
		
		case DOC_CPIM_PIDF:
			res = create_cpim_pidf_document(pinfo, &doc, &content_type);
			break;

		case DOC_MSRTC_PIDF:
		case DOC_PIDF:
		default:
			res = create_pidf_document(pinfo, &doc, &content_type);
	}
	free_presentity_info(pinfo);
	
	if (res != 0) {
		LOG(L_ERR, "can't create presence document (%d)\n", _w->preferred_mimetype);
		return -2;
	}
	
	if (create_headers(_w, &headers, &content_type) < 0) {
		LOG(L_ERR, "send_presence_notify(): Error while adding headers\n");
		str_free_content(&doc);
		str_free_content(&content_type);
		
		return -7;
	}

	if (!is_str_empty(&doc)) body = doc;
	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	str_free_content(&doc);
	str_free_content(&headers);
	str_free_content(&content_type);
		
	if (use_db) db_update_watcher(_p, _w); /* dialog has changed */
	
	return 0;
}

static int send_winfo_notify(struct presentity* _p, struct watcher* _w)
{
	str doc = STR_NULL;
	str content_type = STR_NULL;
	str headers = STR_NULL;
	int res = 0;
	str body = STR_STATIC_INIT("");

	DEBUG("sending winfo notify\n");
	
	switch (_w->preferred_mimetype) {
		case DOC_WINFO:
			create_winfo_document(_p, _w, &doc, &content_type);
			DEBUG("winfo document created\n");
			break;
		/* other formats ? */
		default:
			ERR("unknow doctype\n");
			return -1;
	}

	DEBUG("creating headers\n");
	if (create_headers(_w, &headers, &content_type) < 0) {
		ERR("Error while adding headers\n");
		str_free_content(&doc);
		str_free_content(&content_type);
		return -7;
	}
	DEBUG("headers created\n");

	if (!is_str_empty(&doc)) body = doc;
	res = tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);
	DEBUG("request sent with result %d\n", res);
	if (res < 0) {
		ERR("Can't send watcherinfo notification (%d)\n", res);
	}

	str_free_content(&doc);
	str_free_content(&headers);
	str_free_content(&content_type);

	_w->document_index++; /* increment index for next document */
	
	if (use_db) db_update_watcher(_p, _w); /* dialog and index have changed */

	return res;
}

int send_winfo_notify_offline(struct presentity* _p, 
		struct watcher* _w, 
		offline_winfo_t *info, 
		transaction_cb completion_cb, void* cbp)
{
	str doc = STR_NULL;
	str content_type = STR_NULL;
	str headers = STR_NULL;
	str body = STR_STATIC_INIT("");
	
	switch (_w->preferred_mimetype) {
		case DOC_WINFO:
			create_winfo_document_offline(_p, _w, info, &doc, &content_type);
			break;
		/* other formats ? */
		default:
			ERR("send_winfo_notify: unknow doctype\n");
			return -1;
	}

	if (create_headers(_w, &headers, &content_type) < 0) {
		ERR("send_winfo_notify(): Error while adding headers\n");
		str_free_content(&doc);
		str_free_content(&content_type);
		return -7;
	}

	if (!is_str_empty(&doc)) body = doc;
	tmb.t_request_within(&method, &headers, &body, _w->dialog, completion_cb, cbp);

	str_free_content(&doc);
	str_free_content(&headers);
	str_free_content(&content_type);

	_w->document_index++; /* increment index for next document */
	
	if (use_db) db_update_watcher(_p, _w); /* dialog and index have changed */

	return 0;
}

/* FIXME: will be removed */
#if 0 
#ifdef HAVE_XCAP_CHANGE_NOTIFY
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
#endif /* HAVE_XCAP_CHANGE_NOTIFY */

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
#endif
		
int notify_unauthorized_watcher(struct presentity* _p, struct watcher* _w)
{
	str headers = STR_NULL;
	str body = STR_STATIC_INIT("");

	/* send notifications to unauthorized (pending) watchers */
	if (create_headers(_w, &headers, NULL) < 0) {
		LOG(L_ERR, "notify_unauthorized_watcher(): Error while adding headers\n");
		return -7;
	}

	tmb.t_request_within(&method, &headers, &body, _w->dialog, 0, 0);

	str_free_content(&headers);
		
	if (use_db) db_update_watcher(_p, _w); /* dialog has changed */
	
	return 0;
}

int send_notify(struct presentity* _p, struct watcher* _w)
{
	int rc = 0;

	LOG(L_DBG, "notifying %.*s _p->flags=%x _w->event_package=%d _w->preferred_mimetype=%d _w->status=%d\n", 
	    _w->uri.len, _w->uri.s, _p->flags, _w->event_package, _w->preferred_mimetype, _w->status);

	if ((_w->status == WS_PENDING) || 
			(_w->status == WS_PENDING_TERMINATED) ||
			(_w->status == WS_REJECTED)) {
		notify_unauthorized_watcher(_p, _w);
		return 0;
	}

	switch (_w->event_package) {
		case EVENT_PRESENCE:
			rc = send_presence_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_presence_notify returned %d\n", rc);
			break;
		case EVENT_PRESENCE_WINFO:
			rc = send_winfo_notify(_p, _w);
			if (rc < 0) LOG(L_ERR, "send_winfo_notify returned %d\n", rc);
			break;
		default: LOG(L_ERR, "sending notify for unknow package\n");
	}

/* FIXME: will be removed */
#if 0 
#ifdef HAVE_XCAP_CHANGE_NOTIFY
	if ((_p->flags & PFLAG_XCAP_CHANGED) 
	    && (_w->event_package == EVENT_XCAP_CHANGE)) {
		switch(_w->preferred_mimetype) {
#ifdef DOC_XCAP_CHANGE
		case DOC_XCAP_CHANGE:
#endif
		default:
			rc = send_xcap_change_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_xcap_change_notify returned %d\n", rc);
		}
	}
#endif /* HAVE_XCAP_CHANGE_NOTIFY */
#ifdef PFLAG_LOCATION_CHANGED
	if ((_p->flags & PFLAG_LOCATION_CHANGED) 
	    && (_w->event_package == EVENT_LOCATION)) {
		switch(_w->preferred_mimetype) {
		case DOC_LOCATION:
			rc = send_location_notify(_p, _w);
			if (rc) LOG(L_ERR, "send_location_notify returned %d\n", rc);
			break;
		default:
		  rc = -1;
		  ;
		}
	}
#endif /* PFLAG_LOCATION_CHANGED */
#endif
	
	return rc;
}

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "paerrno.h"
#include "notify.h"
#include "watcher.h"
#include "qsa_interface.h"

#include <presence/pidf.h>
#include <presence/xpidf.h>
#include <presence/lpidf.h>
#include "winfo_doc.h"
#include "dlist.h"


static str notify = STR_STATIC_INIT("NOTIFY");

/* TM callback processing */

typedef struct {
	dlg_id_t id;
	str uid;
	struct pdomain *domain; /* replace with domain name for safe stop */
	char buf[1];
} pa_notify_cb_param_t;

static pa_notify_cb_param_t *create_notify_cb_param(presentity_t *p, watcher_t *w)
{
	pa_notify_cb_param_t *cbd;
	int size;

	if ((!p) || (!w)) return NULL;
	if (!w->dialog) return NULL;

	size = sizeof(*cbd) + p->uuid.len +
			w->dialog->id.call_id.len +
			w->dialog->id.rem_tag.len +
			w->dialog->id.loc_tag.len;
	
	cbd = (pa_notify_cb_param_t*)mem_alloc(size);
	if (!cbd) {
		ERR("can't allocate memory (%d bytes)\n", size);
		return NULL;
	}

	cbd->domain = p->pdomain;

	cbd->uid.s = cbd->buf;
	cbd->uid.len = p->uuid.len;
	
	cbd->id.call_id.s = cbd->uid.s + cbd->uid.len;
	cbd->id.call_id.len = w->dialog->id.call_id.len;
	
	cbd->id.rem_tag.s = cbd->id.call_id.s + cbd->id.call_id.len;
	cbd->id.rem_tag.len = w->dialog->id.rem_tag.len;
	
	cbd->id.loc_tag.s = cbd->id.rem_tag.s + cbd->id.rem_tag.len;
	cbd->id.loc_tag.len = w->dialog->id.loc_tag.len;
	
	/* copy data */
	if (p->uuid.s) memcpy(cbd->uid.s, p->uuid.s, p->uuid.len); 
	if (w->dialog->id.call_id.s) memcpy(cbd->id.call_id.s, 
			w->dialog->id.call_id.s, w->dialog->id.call_id.len); 
	if (w->dialog->id.rem_tag.s) memcpy(cbd->id.rem_tag.s, 
			w->dialog->id.rem_tag.s, w->dialog->id.rem_tag.len); 
	if (w->dialog->id.loc_tag.s) memcpy(cbd->id.loc_tag.s, 
			w->dialog->id.loc_tag.s, w->dialog->id.loc_tag.len); 
	
	return cbd;
}

static int get_watcher(pa_notify_cb_param_t *cbd, 
		watcher_t **w, presentity_t **p)
{
	int et = EVENT_PRESENCE;
	
	if (find_presentity_uid(cbd->domain, &cbd->uid, p) != 0) {
		return -1;
	}
	
	if (find_watcher_dlg(*p, &cbd->id, et, w) != 0) {
		/* presence watcher NOT found */
		
		et = EVENT_PRESENCE_WINFO;
		if (find_watcher_dlg(*p, &cbd->id, et, w) != 0) {
			/* presence.winfo watcher NOT found */
			return -1;
		}
	}
	return 0;
}

static void destroy_subscription(pa_notify_cb_param_t *cbd)
{
	presentity_t *p = NULL;
	watcher_t *w = NULL;
	
/*	if (find_pdomain(cbd->domain, &domain) != 0) {
		ERR("can't find PA domain\n");
		return;
	} */
	lock_pdomain(cbd->domain);
	
	if (get_watcher(cbd, &w, &p) != 0) {
		unlock_pdomain(cbd->domain);
		return;
	}
	
	remove_watcher(p, w);
	free_watcher(w);

	unlock_pdomain(cbd->domain);
	
}

static void refresh_dialog(pa_notify_cb_param_t *cbd, struct sip_msg *m)
{
	watcher_t *w;
	presentity_t *p;
	
	lock_pdomain(cbd->domain);
	if (get_watcher(cbd, &w, &p) >= 0)
		tmb.dlg_response_uac(w->dialog, m, notify_is_refresh != 0 ? IS_TARGET_REFRESH : IS_NOT_TARGET_REFRESH);
	unlock_pdomain(cbd->domain);
}

static void pa_notify_cb(struct cell* t, int type, struct tmcb_params* params)
{
	pa_notify_cb_param_t *cbd = NULL;
	
	if (!params) return;

	/* Possible problems - see subscribe_cb in presence_b2b/euac_funcs.c */

	if (params->param) cbd = (pa_notify_cb_param_t *)*(params->param);
	if (!cbd) {
		ERR("BUG empty cbd parameter given to callback function\n");
		return;
	}

	if ((params->code >= 200) && (params->code < 300)) {
		if (params->rpl && (params->rpl != FAKED_REPLY)) 
			refresh_dialog(cbd, params->rpl);
	}
	if ((params->code >= 300)) {
		int ignore = 0;
		
		switch (params->code) {
			case 408: 
				if (ignore_408_on_notify) ignore = 1;
				/* due to eyeBeam's problems with processing more NOTIFY
				 * requests sent consequently without big delay */
				break;
		}
	
		if (!ignore) {
			WARN("destroying subscription from callback due to %d response on NOTIFY\n", params->code);
			destroy_subscription(cbd);
			TRACE("subscription destroyed!!!\n");
		}
	}
	
	shm_free(cbd);
}

/* helper functions */

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
	
	dstr_append_str(buf, content_type);
	dstr_append_zt(buf, "\r\n");
	
	return 0;
}


static inline int add_subs_state_hf(dstring_t *buf, watcher_status_t _s, time_t _e)
{
	char* num;
	int len;
	str s = STR_NULL;
	static str timeout = STR_STATIC_INIT("timeout");
	static str rejected = STR_STATIC_INIT("rejected");
	
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
			if (_e <= 0) dstr_append_str(buf, &timeout);
			else dstr_append_str(buf, &rejected);
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

	/* required by RFC 3261 */
	dstr_append_zt(&buf, "Max-Forwards: 70\r\n"); 
	
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

/* NOTIFY creation functions for specific events/state */

static int prepare_presence_notify(struct retr_buf **dst, 
		struct presentity* _p, struct watcher* _w,
		pa_notify_cb_param_t *cbd)
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
	int res = 0;
	uac_req_t	uac_r;
	
	switch(_w->preferred_mimetype) {
		case DOC_XPIDF:
			res = create_xpidf_document(&_p->data, &doc, &content_type);
			break;

		case DOC_LPIDF:
			res = create_lpidf_document(&_p->data, &doc, &content_type);
			break;
		
		case DOC_CPIM_PIDF:
			res = create_cpim_pidf_document(&_p->data, &doc, &content_type);
			break;

		case DOC_MSRTC_PIDF:
		case DOC_PIDF:
		default:
			res = create_pidf_document(&_p->data, &doc, &content_type);
	}
	
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
/*	res = tmb.t_request_within(&notify, &headers, &body, 
			_w->dialog, pa_notify_cb, cbd);*/
	set_uac_req(&uac_r,
			&notify,
			&headers,
			&body, 
			_w->dialog,
			TMCB_LOCAL_COMPLETED,
			pa_notify_cb,
			cbd
		);
	res = tmb.prepare_request_within(&uac_r, dst);
	if (res < 0) {
		ERR("Can't send NOTIFY (%d) in dlg %.*s, %.*s, %.*s\n", res, 
			FMT_STR(_w->dialog->id.call_id), 
			FMT_STR(_w->dialog->id.rem_tag), 
			FMT_STR(_w->dialog->id.loc_tag));
	}

	str_free_content(&doc);
	str_free_content(&headers);
	str_free_content(&content_type);	
	
	return res;
}

static int prepare_winfo_notify(struct retr_buf **dst,
		struct presentity* _p, struct watcher* _w,
		pa_notify_cb_param_t *cbd)
{
	str doc = STR_NULL;
	str content_type = STR_NULL;
	str headers = STR_NULL;
	int res = 0;
	str body = STR_STATIC_INIT("");
	uac_req_t	uac_r;
	
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

	if (create_headers(_w, &headers, &content_type) < 0) {
		ERR("Error while adding headers\n");
		str_free_content(&doc);
		str_free_content(&content_type);
		return -7;
	}

	if (!is_str_empty(&doc)) body = doc;
	/* res = tmb.t_request_within(&notify, &headers, &body, _w->dialog, 0, 0); */
	set_uac_req(&uac_r,
			&notify,
			&headers,
			&body, 
			_w->dialog,
			TMCB_LOCAL_COMPLETED,
			pa_notify_cb,
			cbd
		);
	res = tmb.prepare_request_within(&uac_r, dst);
	if (res < 0) {
		ERR("Can't send watcherinfo notification (%d)\n", res);
	}
	else {
		_w->document_index++; /* increment index for next document */
	}

	str_free_content(&doc);
	str_free_content(&headers);
	str_free_content(&content_type);

	return res;
}

int prepare_unauthorized_notify(struct retr_buf **dst, 
		struct presentity* _p, struct watcher* _w,
		pa_notify_cb_param_t *cbd)
{
	str headers = STR_NULL;
	str body = STR_STATIC_INIT("");
	int res;
	unc_req_t	uac_r;

	/* send notifications to unauthorized (pending) watchers */
	if (create_headers(_w, &headers, NULL) < 0) {
		LOG(L_ERR, "notify_unauthorized_watcher(): Error while adding headers\n");
		return -7;
	}

	set_uac_req(&uac_r,
			&notify,
			&headers,
			&body, 
			_w->dialog,
			TMCB_LOCAL_COMPLETED,
			pa_notify_cb,
			cbd
		);
	res = tmb.prepare_request_within(&uac_r, dst);
	if (res < 0) {
		ERR("Can't send NOTIFY (%d) in dlg %.*s, %.*s, %.*s\n", res, 
			FMT_STR(_w->dialog->id.call_id), 
			FMT_STR(_w->dialog->id.rem_tag), 
			FMT_STR(_w->dialog->id.loc_tag));
	}

	str_free_content(&headers);
		
	
	return res;
}

int prepare_notify(struct retr_buf **dst, 
		struct presentity* _p, struct watcher* _w)
{
	int rc = 0;
	pa_notify_cb_param_t *cbd = NULL;

	/* alloc data for callback */
	cbd = create_notify_cb_param(_p, _w);
	if (!cbd) {
		ERR("can't allocate data for callback\n");
		/* FIXME: destroy subscription? */
		return -1;
	}	
	
	LOG(L_DBG, "notifying %.*s _p->flags=%x _w->event_package=%d _w->preferred_mimetype=%d _w->status=%d\n", 
	    _w->uri.len, _w->uri.s, _p->flags, _w->event_package, _w->preferred_mimetype, _w->status);

	if ((_w->status == WS_PENDING) || 
			(_w->status == WS_PENDING_TERMINATED) ||
			(_w->status == WS_REJECTED)) {
		rc = prepare_unauthorized_notify(dst, _p, _w, cbd);
	}
	else {
		switch (_w->event_package) {
			case EVENT_PRESENCE:
				rc = prepare_presence_notify(dst, _p, _w, cbd);
				break;
			case EVENT_PRESENCE_WINFO:
				rc = prepare_winfo_notify(dst, _p, _w, cbd);
				break;
			default: 
				LOG(L_ERR, "sending notify for unknow package\n");
				rc = -1;
		}
	}
	if ((rc < 0) && cbd) shm_free(cbd); /* ??? or not ??? */
	else {
		 /* At least dialog has changed (sometimes more - for example
		  * version counter for winfo)! 
		  * Ignore errors there because the watcher need NOT to be in DB
		  * (polling). */
		if (use_db) db_update_watcher(_p, _w);
		/* if (use_db && (!is_watcher_terminated(_w)))
				db_update_watcher(_p, _w);	 */
	}

	return rc;
}

int send_notify(struct presentity* _p, struct watcher* _w)
{
	struct retr_buf *request;
	int res = prepare_notify(&request, _p, _w);
	if (res < 0) return res;
	tmb.send_prepared_request(request);
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
	uac_req_t	uac_r;
	
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
	set_uac_req(&uac_r,
			&notify,
			&headers,
			&body, 
			_w->dialog,
			TMCB_LOCAL_COMPLETED,
			completion_cb,
			cbp
		);
	tmb.t_request_within(&uac_r);

	str_free_content(&doc);
	str_free_content(&headers);
	str_free_content(&content_type);

	_w->document_index++; /* increment index for next document */
	
	if (use_db) db_update_watcher(_p, _w); /* dialog and index have changed */

	return 0;
}


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


#include "notify.h"
#include "../../str.h"
#include "xpidf.h"
#include "common.h"
#include "../../dprint.h"
#include "pa_mod.h"
#include "../../trim.h"
#include "lpidf.h"
#include "paerrno.h"


#define CONTENT_TYPE "Content-Type: "
#define CONTENT_TYPE_LEN  (sizeof(CONTENT_TYPE) - 1)

#define CONT_TYPE_XPIDF "application/xpidf+xml"
#define CONT_TYPE_XPIDF_LEN  (sizeof(CONT_TYPE_XPIDF) - 1)

#define CONT_TYPE_LPIDF "text/lpidf"
#define CONT_TYPE_LPIDF_LEN (sizeof(CONT_TYPE_LPIDF) - 1)

#define SUBSCRIPTION_STATE "Subscription-State: "
#define SUBSCRIPTION_STATE_LEN (sizeof(SUBSCRIPTION_STATE) - 1)

#define SS_EXPIRES ";expires="
#define SS_EXPIRES_LEN (sizeof(SS_EXPIRES) - 1)

#define SS_REASON ";reason="
#define SS_REASON_LEN (sizeof(SS_REASON) - 1)

#define CRLF "\r\n"
#define CRLF_LEN (sizeof(CRLF) - 1)

#define ST_ACTIVE "active"
#define ST_ACTIVE_LEN (sizeof(ST_ACTIVE) - 1)

#define ST_TERMINATED "terminated"
#define ST_TERMINATED_LEN (sizeof(ST_TERMINATED) - 1)

#define ST_PENDING "pending"
#define ST_PENDING_LEN (sizeof(ST_PENDING) - 1)

#define REASON_DEACTIVATED "deactivated"
#define REASON_DEACTIVATED_LEN (sizeof(REASON_DEACTIVATED) - 1)

#define REASON_NORESOURCE "noresource"
#define REASON_NORESOURCE_LEN (sizeof(REASON_NORESOURCE) - 1)

#define REASON_PROBATION "probation"
#define REASON_PROBATION_LEN (sizeof(REASON_PROBATION) - 1)

#define REASON_REJECTED "rejected"
#define REASON_REJECTED_LEN (sizeof(REASON_REJECTED) - 1)

#define REASON_TIMEOUT "timeout"
#define REASON_TIMEOUT_LEN (sizeof(REASON_TIMEOUT) - 1)

#define REASON_GIVEUP "giveup"
#define REASON_GIVEUP_LEN (sizeof(REASON_GIVEUP) - 1)

#define METHOD_NOTIFY "NOTIFY"
#define METHOD_NOTIFY_LEN (sizeof(METHOD_NOTIFY) - 1)


/*
 * Subscription-State values
 */
static str subs_states[] = {
	{ST_ACTIVE, S   T_ACTIVE_LEN     },
	{ST_TERMINATED, ST_TERMINATED_LEN},
	{ST_PENDING,    ST_PENDING_LEN   }
};


/* 
 * Subscription-State reason parameter values
 */
static str reason[] = {
	{REASON_DEACTIVATED, REASON_DEACTIVATED_LEN},
	{REASON_NORESOURCE,  REASON_NORESOURCE_LEN },
	{REASON_PROBATION,   REASON_PROBATION_LEN  },
	{REASON_REJECTED,    REASON_REJECTED_LEN   },
	{REASON_TIMEOUT,     REASON_TIMEOUT_LEN    },
	{REASON_GIVEUP,      REASON_GIVEUP_LEN     }
};


static str method = {METHOD_NOTIFY, METHOD_NOTIFY_LEN};


#define BUF_LEN 1024

static char headers_buf[BUF_LEN];
static char buffer[BUF_LEN];

static str headers = { headers_buf, 0 };
static str body = { buffer, 0 };


static inline int add_event_hf(str* _h, int _l)
{
	if (_l < 17) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "add_event_hf(): Buffer too small\n");
		return -1;
	}

	memcpy(_h->s + _h->len, "Event: presence\r\n", 17);
	_h->len += 17;
	return 0;
}


static inline int add_cont_type_hf(str* _h, int _l, doctype_t _d)
{
	switch(_d) {
	case DOC_XPIDF:
		if (_l < CONTENT_TYPE_LEN + CONT_TYPE_XPIDF_LEN + CRLF_LEN) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -1;
		}
		memcpy(_h->s + _h->len, CONTENT_TYPE CONT_TYPE_XPIDF CRLF,
		       CONTENT_TYPE_LEN + CONT_TYPE_XPIDF_LEN + CRLF_LEN);
		_h->len += CONTENT_TYPE_LEN + CONT_TYPE_XPIDF_LEN + CRLF_LEN;
		return 0;
		
	case DOC_LPIDF:
		if (_l < CONTENT_TYPE_LEN + CONT_TYPE_LPIDF_LEN + CRLF_LEN) {
			paerrno = PA_SMALL_BUFFER;
			LOG(L_ERR, "add_cont_type_hf(): Buffer too small\n");
			return -2;
		}
		memcpy(_h->s + _h->len, CONTENT_TYPE CONT_TYPE_LPIDF CRLF,
		       CONTENT_TYPE_LEN + CONT_TYPE_LPIDF_LEN + CRLF_LEN);
		_h->len += CONTENT_TYPE_LEN + CONT_TYPE_LPIDF_LEN + CRLF_LEN;
		return 0;

	default:
		paerrno = PA_UNSUPP_DOC;
		LOG(L_ERR, "add_cont_type_hf(): Unsupported document type\n");
		return -3;
	}
}


/*
 * Convert unsigned int to str representation
 */
static inline void itos(str* _s, unsigned int _i)
{
	unsigned int r = 1000000000, j, f = 0;

	while (r) {
		j = _i / r;
		_i %= r;
		r /= 10;
		if (j) f = 1;
		if (f) _s->s[_s->len++] = j + '0';
	}
	
	if (!f) _s->s[_s->len++] = '0';
}



static inline int add_subs_state_hf(str* _h, int _l, subs_state_t _s, ss_reason_t _r, time_t _e)
{
	if (_l < SUBSCRIPTION_STATE_LEN + subs_states[_s].len + SS_EXPIRES_LEN + 10 + 
	    SS_REASON_LEN + reason[_r].len + CRLF_LEN) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "add_subs_state_hf(): Buffer too small\n");
		return -1;
	}

	memcpy(_h->s + _h->len, SUBSCRIPTION_STATE, SUBSCRIPTION_STATE_LEN);
        _h->len += SUBSCRIPTION_STATE_LEN;
	
	memcpy(_h->s + _h->len, subs_states[_s].s, subs_states[_s].len);
	_h->len += subs_states[_s].len;
	
	switch(_s) {
	case SS_ACTIVE:
		memcpy(_h->s + _h->len, SS_EXPIRES, SS_EXPIRES_LEN);
		_h->len += SS_EXPIRES_LEN;
		itos(_h, (unsigned int)_e);
		break;

	case SS_TERMINATED:
		memcpy(_h->s + _h->len, SS_REASON, SS_REASON_LEN);
		_h->len += SS_REASON_LEN;
		memcpy(_h->s + _h->len, reason[_r].s, reason[_r].len);
		_h->len += reason[_r].len;
		break;

	case SS_PENDING:
		break;
	}

	memcpy(_h->s + _h->len, CRLF, CRLF_LEN);
	_h->len += CRLF_LEN;
	
	return 0;
}


static inline int create_headers(struct watcher* _w)
{
	time_t t;
	subs_state_t s;

	headers.len = 0;
	
	if (add_event_hf(&headers, BUF_LEN) < 0) {
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


static inline int send_xpidf_notify(struct presentity* _p, struct watcher* _w)
{
	
	str to;
	xpidf_status_t st;

	     /* Send a notify, saved Contact will be put in
	      * Request-URI, To will be put in from and new tag
	      * will be generated, callid will be callid,
	      * from will be put in to including tag
	      */

	if (start_xpidf_doc(&body, BUF_LEN) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): start_xpidf_doc failed\n");
		return -1;
	}
	
	to.s = _w->dialog.to.s;
	to.len = _w->dialog.to.len;

	trim(&to);
	get_raw_uri(&to);

	if (xpidf_add_presentity(&body, BUF_LEN - body.len, &to) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): xpidf_add_presentity failed\n");
		return -2;
	}

	switch(_p->state) {
	case PS_OFFLINE: st = XPIDF_ST_CLOSED; break;
	default: st = XPIDF_ST_OPEN; break;
	}

	if (xpidf_add_address(&body, BUF_LEN - body.len, &to, st) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): xpidf_add_address failed\n");
		return -3;
	}

	if (end_xpidf_doc(&body, BUF_LEN - body.len) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): end_xpidf_doc failed\n");
		return -5;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_xpidf_notify(): Error while adding headers\n");
		return -6;
	}

	tmb.t_uac_dlg(&method,              /* NOTIFY */ 
		      0,                    /* dst */
			PROTO_UDP,
		      &_w->contact,         /* R-URI */
		      &_w->from,            /* From -> To */
		      &_w->dialog.to,       /* To -> From */
		      &_w->dialog.from_tag, /* From tag -> To tag */
		      0,                    /* From tag automatically generated */
		      &_w->dialog.cseq,     /* CSeq */
		      &_w->dialog.callid,   /* Call-ID */
		      &headers,             /* Headers */
		      &body,                /* Body */
                      0,                    /* completition_cb */
		      0                     /* cbp */
		      );
	_w->dialog.cseq++;
	return 0;

}


static inline int send_lpidf_notify(struct presentity* _p, struct watcher* _w)
{
	str to;
	lpidf_status_t st;

	to.s = _w->dialog.to.s;
	to.len = _w->dialog.to.len;

	trim(&to);
	get_raw_uri(&to);

	if (lpidf_add_presentity(&body, BUF_LEN - body.len, &to) < 0) {
		LOG(L_ERR, "send_lpidf_notify(): Error in lpidf_add_presentity\n");
		return -1;
	}

	switch(_p->state) {
	case PS_OFFLINE: st = LPIDF_ST_CLOSED; break;
	default: st = LPIDF_ST_OPEN; break;
	}

	if (lpidf_add_address(&body, BUF_LEN - body.len, &to, st) < 0) {
		LOG(L_ERR, "send_lpidf_notify(): lpidf_add_address failed\n");
		return -2;
	}

	if (create_headers(_w) < 0) {
		LOG(L_ERR, "send_lpidf_notify(): Error while adding headers\n");
		return -6;
	}

	tmb.t_uac_dlg(&method,              /* NOTIFY */ 
		      0,                    /* dst */
			PROTO_UDP,
		      &_w->contact,         /* R-URI */
		      &_w->from,            /* From -> To */
		      &_w->dialog.to,       /* To -> From */
		      &_w->dialog.from_tag, /* From tag -> To tag */
		      0,                    /* From tag automatically generated */
		      &_w->dialog.cseq,    /* CSeq */
		      &_w->dialog.callid,   /* Call-ID */
		      &headers,             /* Headers */
		      &body,                /* Body */
                      0,                    /* completition_cb */
		      0                     /* cbp */
		      );

	_w->dialog.cseq++;
	return 0;
}


int send_notify(struct presentity* _p, struct watcher* _w)
{
	body.len = 0;

	switch(_w->accept) {
	case DOC_XPIDF:
		return send_xpidf_notify(_p, _w);
		break;

	case DOC_LPIDF:
		return send_lpidf_notify(_p, _w);
		break;
	}

	return -1;
}

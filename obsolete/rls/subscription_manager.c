#include "subscription_manager.h"

#include "../../parser/parse_expires.h"
#include "../../modules/tm/tm_load.h"
#include "../../parser/hf.h"
#include "../../parser/parse_from.h"
#include "../../data_lump_rpl.h"

#include <cds/dstring.h>
#include <cds/logger.h>
#include "result_codes.h"
#include <presence/utils.h>

static struct tm_binds tmb;

/****** Functions for global initialization ******/

int subscription_management_init(void)
{
    load_tm_f load_tm;

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "subscription_management_init(): Can't import tm!\n");
		return -1;
	}

	/* let the auto-loading function load all TM stuff */
	if (load_tm(&tmb)==-1) {
		LOG(L_ERR, "subscription_management_init(): load_tm() failed\n");
		return -1;
	}

	return 0;
}

/****** Functions for standalone subscription manager manipulation ******/

int sm_init(subscription_manager_t *sm,
		send_notify_func notify, 
		terminate_func terminate, 
		subscription_authorize_func authorize,
		gen_lock_t *mutex,
		int min_exp, 
		int max_exp, 
		int default_exp,
		int expiration_timer_period)
{
	if (!sm) return -1;
	
	sm->first = NULL;
	sm->last = NULL;
	sm->notify = notify;
	sm->terminate = terminate;
	sm->authorize = authorize;
	sm->mutex = mutex;
	sm->default_expiration = default_exp;
	sm->min_expiration = min_exp;
	sm->max_expiration = max_exp;
	return tem_init(&sm->timer, 
			expiration_timer_period, /* atomic time = 1 s */
			4093, /* time slot count */
			1, /* enable delay <= terminate AFTER the timeout */
			mutex);
}

subscription_manager_t *sm_create(send_notify_func notify, 
		terminate_func terminate, 
		subscription_authorize_func authorize,
		gen_lock_t *mutex,
		int min_exp, 
		int max_exp, 
		int default_exp,
		int expiration_timer_period)
{
	subscription_manager_t *sm;
	
	sm = (subscription_manager_t*)mem_alloc(sizeof(subscription_manager_t));
	if (!sm) {
		LOG(L_ERR, "can't allocate subscription manager\n");
		return sm;
	}

	if (sm_init(sm, notify, terminate, authorize, mutex, min_exp, 
				max_exp, default_exp, expiration_timer_period) != 0) {
		mem_free(sm);
		return NULL;
	}

	return sm;
}

void sm_add_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *s)
{
	/* adds the subscription into the list of subscriptions */

	if (!s) return;
	
	s->next = NULL;
	s->prev = mng->last;
	if (mng->last) mng->last->next = s;
	else mng->first = s;
	mng->last = s;
}

void sm_remove_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *s)
{
	/* removes the subscription from the list of subscriptions */
	if (s->prev) s->prev->next = s->next;
	else mng->first = s->next;
	if (s->next) s->next->prev = s->prev;
	else mng->last = s->prev;
	s->next = NULL;
	s->prev = NULL;
}

/****** Helper functions for subscription initialization ******/

static int create_subscription_dialog(subscription_data_t *dst, 
		struct sip_msg *m)
{
	/* create SIP dialog for subscription */
	if (tmb.new_dlg_uas(m, 200, &dst->dialog) < 0) {
		LOG(L_ERR, "create_subscription_dialog(): Error while creating dialog.\n");
		return -1;
	}
	else {
		DEBUG_LOG("create_subscription_dialog(): new dialog created (%.*s, %.*s, %.*s)\n", 
				dst->dialog->id.call_id.len, dst->dialog->id.call_id.s,
				dst->dialog->id.rem_tag.len, dst->dialog->id.rem_tag.s,
				dst->dialog->id.loc_tag.len, dst->dialog->id.loc_tag.s);
	}
	return 0;
}

static int get_subscription_expiration(subscription_manager_t *mng,
		struct sip_msg *m)
{
	int e = 0;
	
	/* parse Expires header field */
	if (parse_headers(m, HDR_EXPIRES_T, 0) == -1) {
		LOG(L_ERR, "set_subscription_expiration(): Error while parsing headers\n");
		return RES_PARSE_HEADERS_ERR;
	}
	if (m->expires) {
		if (parse_expires(m->expires) < 0) {
			LOG(L_ERR, "set_subscription_expiration(): Error parsing Expires header\n");
			return RES_PARSE_HEADERS_ERR;
		}
	}

	e = mng->default_expiration;
	if (m->expires) {
		exp_body_t *expires = (exp_body_t *)m->expires->parsed;
		if (expires) if (expires->valid) e = expires->val;
	}
	if (e < 0) e = 0;
	
	if ((e != 0) && (e < mng->min_expiration)) {
		/* e = 0;  */
		/*e = mng->min_expiration;*/
		
		/* Interval too short - must not be longer (RFC 3265) */
		LOG(L_ERR, "set_subscription_expiration(): interval too short (%d s)\n", e);
		return RES_EXPIRATION_INTERVAL_TOO_SHORT;
	}
	if (e > mng->max_expiration) e = mng->max_expiration;

	return e;
}

static void free_subscription_dialog(subscription_data_t *dst)
{
	if (dst->dialog) tmb.free_dlg(dst->dialog);
	dst->dialog = NULL;
}

static int cmp_subscription(str_t *from_tag, str_t *to_tag, str_t *call_id,
			subscription_data_t *s)
{
/*	LOG(L_TRACE, "comparing element dlg: %.*s, %.*s, %.*s\n", 
			s->dialog->id.call_id.len, s->dialog->id.call_id.s,
			s->dialog->id.rem_tag.len, s->dialog->id.rem_tag.s,
			s->dialog->id.loc_tag.len, s->dialog->id.loc_tag.s);
	LOG(L_TRACE, "searching for: %.*s, %.*s, %.*s\n", 
			call_id->len, call_id->s,
			from_tag->len, from_tag->s,
			to_tag->len, to_tag->s);
*/			
	if (str_case_equals(call_id, &s->dialog->id.call_id) != 0) return 1;
	if (str_nocase_equals(from_tag, &s->dialog->id.rem_tag) != 0) return 1;
	if (str_nocase_equals(to_tag, &s->dialog->id.loc_tag) != 0) return 1;
	/* are the tags case sensitive? */
	return 0;
}

/* Get resource-list URI from SUBSCRIBE request */
static int get_dst_uri(struct sip_msg* _m, str* dst_uri)
{
	/* FIXME: get raw request URI? or from TO? 
	 * FIXME: skip uri parameters and everything else, leave only
	 * sip:xxx@yyy ???!!! */
	str uri;

	if (_m->new_uri.s) {
		uri.s = _m->new_uri.s;
		uri.len = _m->new_uri.len;
	} else {
		uri.s = _m->first_line.u.request.uri.s;
		uri.len = _m->first_line.u.request.uri.len;
	}
	
	if (dst_uri) *dst_uri = uri;
	return RES_OK;
}

static inline int get_from_uri(struct sip_msg* _m, str* _u)
{
	if (parse_from_header(_m) < 0) {
		LOG(L_ERR, "get_from_uri(): Error while parsing From body\n");
		return -1;
	}
	
	_u->s = ((struct to_body*)_m->from->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->from->parsed)->uri.len;

	return 0;
}

/* Get subscriber's URI from SUBSCRIBE request */
static int get_subscribers_uri(struct sip_msg* _m, str* dst_uri)
{
	/* FIXME: skip uri parameters !!! */
	/* str uri; */
	str u;
	struct sip_uri s;

	if (!dst_uri) return RES_INTERNAL_ERR;
	
	if (parse_from_header(_m) < 0) {
		LOG(L_ERR, "get_subscribers_uri(): Error while parsing From header\n");
		return RES_PARSE_HEADERS_ERR;
	}

	u = ((struct to_body*)_m->from->parsed)->uri;
	
	if (parse_uri(u.s, u.len, &s) < 0) {
		LOG(L_ERR, "get_subscribers_uri(): Error while parsing From content\n");
		return RES_PARSE_HEADERS_ERR;
	}
	
	dst_uri->s = u.s;
	dst_uri->len = s.host.s + s.host.len - dst_uri->s;
/*
	if (s.user.len > 0) uri.s = s.user.s;
	else uri.s = u.s;
	if (s.host.len <= 0) uri = u;
	else uri.len = s.host.s - uri.s + s.host.len;
	
	if (dst_uri) *dst_uri = uri;*/
	return RES_OK;
}

/* Get Event package from SUBSCRIBE request */
static int get_package(struct sip_msg* m, str* dst)
{
	dst->len = 0;
	dst->s = NULL;
	if ( (parse_headers(m, HDR_EVENT_T, 0) == -1) || (!m->event) ) {
		LOG(L_ERR, "get_package(): Error while parsing Event header\n");
		return RES_PARSE_HEADERS_ERR;
	}
	
	dst->s = m->event->body.s;
	dst->len = m->event->body.len;
	return RES_OK;
}

static int set_subscription_info(struct sip_msg *m, subscription_data_t *s)
{
	str uri, subscriber_uri;
	str package;
	int r;

	/* get requested resource list URI */
	r = get_dst_uri(m, &uri);
	if (r != RES_OK) {
		LOG(L_ERR, "set_rls_info(): Can't decode resource list URI\n");
		return r; 
	}
	
	/* get subscriber's URI */
	r = get_subscribers_uri(m, &subscriber_uri);
	if (r != RES_OK) {
		LOG(L_ERR, "set_rls_info(): Can't decode subscriber's URI\n");
		return r; 
	}
	
	/* get event package */
	r = get_package(m, &package);
	if (r != RES_OK) {
		return r;
	}
	
	extract_server_contact(m, &s->contact, 0);
	
	DEBUG_LOG("set_subscription_info(): uri=\'%.*s\'\n", FMT_STR(uri));
	DEBUG_LOG("set_subscription_info(): package=\'%.*s\'\n", FMT_STR(package));
	DEBUG_LOG("set_subscription_info(): subscriber_uri=\'%.*s\'\n", FMT_STR(subscriber_uri));
	DEBUG_LOG("set_subscription_info(): contact=\'%.*s\'\n", FMT_STR(s->contact));

	r = str_dup(&s->record_id, &uri);
	if (r == 0) r = str_dup(&s->subscriber, &subscriber_uri);
	else str_clear(&s->subscriber);
	if (r == 0) r = str_dup(&s->package, &package);
	else str_clear(&s->package);
	
	return r;
}

static void free_subscription(subscription_data_t *s)
{
	DEBUG_LOG("subscription manager: freeing subscription\n");
	str_free_content(&s->record_id);
	str_free_content(&s->package);
	str_free_content(&s->subscriber);
	str_free_content(&s->contact);
	free_subscription_dialog(s);
}

/****** Functions for standalone subscription manipulation ******/

void subscription_expiration_cb(struct _time_event_data_t *ted)
{
	/* the time event manager uses the same mutex and it is locked now ! */

	time_t t = time(NULL);
	subscription_manager_t *mng;
	subscription_data_t *s;
	
	mng = ted->cb_param1;
	s = ted->cb_param;
	DBG("subscription %p(%p) expired at: %s\n", s, mng, ctime(&t));

	if (mng && s) {
		if (s->status == subscription_pending)
			s->status = subscription_terminated_pending_to;
		else
			s->status = subscription_terminated_to;
		if (mng->notify) mng->notify(s);
		if (mng->terminate) mng->terminate(s);
	}
}

int sm_init_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *dst, 
		struct sip_msg *m)
{
	int e, res;
	authorization_result_t ares = auth_granted;
	
	if (!dst) return RES_INTERNAL_ERR;
	
	/* dst->usr_data = NULL; */ /* !!! do not initialize this - its user's and may be already initialized !!! */
	dst->prev = NULL;
	dst->next = NULL;
	dst->dialog = NULL;
	dst->contact.s = NULL;
	dst->contact.len = 0;
	dst->status = subscription_uninitialized;
	str_clear(&dst->record_id);
	str_clear(&dst->subscriber);
	str_clear(&dst->package);
	
	/* fill time event structure */
	dst->expiration.cb = subscription_expiration_cb;
	dst->expiration.cb_param = dst;	
	dst->expiration.cb_param1 = mng;	

	res = set_subscription_info(m, dst);
	if (res != RES_OK) {
		free_subscription(dst);
		return res;
	}
	create_subscription_dialog(dst, m);
	
	if (mng->authorize) ares = mng->authorize(dst);
	switch (ares) {
		case auth_granted:
			dst->status = subscription_active;
			break;
		case auth_polite_block:
			LOG(L_WARN, "polite blocking not implemented - marking subscription as rejected!\n");
			/* other possibility is to give it to pending state, but this eats resources */
			dst->status = subscription_terminated;
			return RES_SUBSCRIPTION_REJECTED;
		case auth_rejected:
			dst->status = subscription_terminated;
			return RES_SUBSCRIPTION_REJECTED;
		case auth_unresolved:
			dst->status = subscription_pending;
			break;
	}
	
	/* set expiration timeout from min, max, default and Expires header field */
	e = get_subscription_expiration(mng, m);
	if (e < 0) {
		free_subscription(dst);
		return e; /* it contains the error number */
	}
	
	/* add this subscription to the list of subscriptions */
	sm_add_subscription_nolock(mng, dst); 
	/* FIXME - bug? - add if e == 0 too? */
		
	if (e > 0) {
		/* start timeout timer for this subscription */
		tem_add_event_nolock(&mng->timer, e, &dst->expiration);
		
		DEBUG_LOG("subscription will expire in %d s\n", e);
	}
	else {	/* polling */
		if (dst->status == subscription_pending)
			dst->status = subscription_terminated_pending; 
		else
			dst->status = subscription_terminated; 
	}

	return RES_OK;
}

int sm_init_subscription_nolock_ex(subscription_manager_t *mng,
		subscription_data_t *dst, 
		dlg_t *dialog,
		subscription_status_t status,
		const str_t *contact,
		const str_t *record_id,
		const str_t *package,
		const str_t *subscriber,
		int expires_after,
		void *subscription_data)
{
	int r = 0;
	
	if (!dst) return RES_INTERNAL_ERR;
	
	dst->usr_data = subscription_data;
	dst->prev = NULL;
	dst->next = NULL;
	dst->dialog = dialog;
	r = str_dup(&dst->contact, contact);
	dst->status = status;
	if (r == 0) r = str_dup(&dst->record_id, record_id);
	else str_clear(&dst->record_id);
	if (r == 0) r = str_dup(&dst->subscriber, subscriber);
	else str_clear(&dst->subscriber);
	if (r == 0) r = str_dup(&dst->package, package);
	else str_clear(&dst->package);
	
	/* fill time event structure */
	dst->expiration.cb = subscription_expiration_cb;
	dst->expiration.cb_param = dst;	
	dst->expiration.cb_param1 = mng;	
	
	DEBUG_LOG("uri=\'%.*s\'\n", FMT_STR(dst->record_id));
	DEBUG_LOG("package=\'%.*s\'\n", FMT_STR(dst->package));
	DEBUG_LOG("subscriber_uri=\'%.*s\'\n", FMT_STR(dst->subscriber));
	DEBUG_LOG("contact=\'%.*s\'\n", FMT_STR(dst->contact));

	/* set expiration timeout from min, max, default and Expires header field */
	if (expires_after < 0) expires_after = 0;
	if (expires_after > 0) {
		/* start timeout timer for this subscription */
		tem_add_event_nolock(&mng->timer, expires_after, &dst->expiration);
		
		DEBUG_LOG("subscription will expire in %d s\n", expires_after);
	}
	else {	/* polling */
		if (dst->status == subscription_pending)
			dst->status = subscription_terminated_pending; 
		else
			dst->status = subscription_terminated; 
	}

	/* add this subscription to the list of subscriptions */
	sm_add_subscription_nolock(mng, dst);
	/* FIXME - bug? - add if e == 0 too? */

	return r;
}

int sm_refresh_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *s, 
		struct sip_msg *m)
{
	int e;
	
	if (!s) return RES_INTERNAL_ERR;
	
	/* refresh SIP dialog */	
	if (s->dialog) tmb.dlg_request_uas(s->dialog, m, IS_TARGET_REFRESH);
		
	if (sm_subscription_terminated(s) != 0) { /* not terminated */
		tem_remove_event_nolock(&mng->timer, &s->expiration);
	}
	else return RES_SUBSCRIPTION_TERMINATED;
	
	/* fill time event structure */
	s->expiration.cb = subscription_expiration_cb;
	s->expiration.cb_param = s;	
	s->expiration.cb_param1 = mng;	

	/* set expiration timeout from min, max, default and Expires header field */
	e = get_subscription_expiration(mng, m);
	if (e < 0) 	return e; /* it contains the error number */
	if (e == 0) {	/* unsubscribe */
		if (s->status == subscription_pending)
			s->status = subscription_terminated_pending; 
		else
			s->status = subscription_terminated; 
	}
	else {
		/* start timeout timer for this subscription */
		tem_add_event_nolock(&mng->timer, e, &s->expiration);
		
		DEBUG_LOG("subscription refreshed,  will expire in %d s\n", e);
	}
	
	return RES_OK;
}

void sm_release_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *dst)
{
	if (!dst) return;
	if (dst->status == subscription_uninitialized) return;
	
	if (sm_subscription_terminated(dst) != 0) { /* NOT terminated */
		/* remove timeout timer */
		tem_remove_event_nolock(&mng->timer, &dst->expiration);
	}
	/* remove this subscription from the list */
	sm_remove_subscription_nolock(mng, dst);

	free_subscription(dst);
}

int sm_prepare_subscription_response(subscription_manager_t *mng,
		subscription_data_t *s, 
		struct sip_msg *m)
{
	char tmp[64];
	int t = 0;
	
	if (s->contact.len > 0) {
		if (!add_lump_rpl(m, s->contact.s, s->contact.len, LUMP_RPL_HDR)) {
			LOG(L_ERR, "sm_prepare_subscription_response(): Can't add Contact header to the response\n");
			return -1;
		}
	}
	t = sm_subscription_expires_in(mng, s);
	sprintf(tmp, "Expires: %d\r\n", t);
	if (!add_lump_rpl(m, tmp, strlen(tmp), LUMP_RPL_HDR)) {
		LOG(L_ERR, "sm_prepare_subscription_response(): Can't add Expires header to the response\n");
		return -1;
	}
	return 0;
}

int sm_subscription_expires_in(subscription_manager_t *mng,
		subscription_data_t *s)
{
	int t = 0;
	if (sm_subscription_terminated(s) != 0) /* NOT terminated */
		t = (s->expiration.tick_time - mng->timer.tick_counter) * mng->timer.atomic_time;
	return t;
}

int sm_find_subscription(subscription_manager_t *mng,
		str_t *from_tag, str_t *to_tag, str_t *call_id, 
		subscription_data_t **dst)
{
	subscription_data_t *e;
	
	/* FIXME: use hash table or something like that ! */
	*dst = NULL;
	e = mng->first;
	while (e) {
		if (cmp_subscription(from_tag, to_tag, call_id, e) == 0) {
			*dst = e;
			return RES_OK;
		}
		e = e->next;
	}
	return RES_NOT_FOUND;
}

int sm_subscription_terminated(subscription_data_t *s)
{
	if (!s) return 0;
	if (s->status == subscription_terminated) return 0;
	if (s->status == subscription_terminated_to) return 0;
	if (s->status == subscription_terminated_pending) return 0;
	if (s->status == subscription_terminated_pending_to) return 0;
	return 1; /* 1 means NOT terminated ! */
}

int sm_subscription_pending(subscription_data_t *s)
{
	if (!s) return 0;
	if (s->status == subscription_pending) return 0;
	if (s->status == subscription_terminated_pending) return 0;
	if (s->status == subscription_terminated_pending_to) return 0;
	return 1; /* 1 means NOT pending ! */
}

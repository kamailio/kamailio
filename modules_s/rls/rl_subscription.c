#include "rl_subscription.h"
#include "rls_mod.h"
#include <cds/dstring.h>
#include <cds/list.h>
#include <cds/logger.h>
#include "result_codes.h"
#include "rlmi_doc.h"
#include <xcap/resource_list.h>
#include <presence/pres_doc.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lock_alloc.h"
#include "../../ut.h"
#include "../../parser/hf.h"
#include "../../parser/parse_from.h"
#include "../../data_lump_rpl.h"
#include "qsa_rls.h"

/* shared data - access due to tracing */
typedef struct {
	rl_subscription_t *first;
	rl_subscription_t *last;
	/* hash, ... */
} rls_data_t;

static rls_data_t *rls = NULL;
static gen_lock_t *rls_mutex = NULL;
subscription_manager_t *rls_manager = NULL;

#define METHOD_NOTIFY "NOTIFY"

static int get_user_from_uri(str_t *uri, str_t *user)
{
	char *a;
	char *d;
	char *s;

	/* we can't use SER's parser - the uri may have not the protocol prefix! */
	
	str_clear(user);
	if (uri->len > 0) {
		d = strchr(uri->s, ':');
		if (d) s = d + 1;
		else s = uri->s;
		a = strchr(s, '@');
		if (a) {
			user->s = s;
			user->len = a - s;
			return 0;
		}
	}
	return -1;
}

/************* subscription callback functions ************/

static int send_notify_cb(struct _subscription_data_t *s)
{
	if (s) 
		rls_generate_notify((rl_subscription_t *)s->usr_data, 1);
	return 0;
}

static int terminate_subscription_cb(struct _subscription_data_t *s)
{
	if (s) rls_remove((rl_subscription_t*)s->usr_data);
	return 0;
}

static authorization_result_t authorize_implicit(struct _subscription_data_t *s)
{
	str_t user, list;
	str_t list_user, list_rest;
	str_t appendix = { s: "-list", len: 5 };
	
	if (get_user_from_uri(&s->subscriber, &user) != 0) 
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */
	if (get_user_from_uri(&s->record_id, &list) != 0)
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */
	
	if (list.len <= appendix.len)
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */
	
	list_rest.len = appendix.len;
	list_rest.s = list.s + list.len - appendix.len;
	if (str_case_equals(&list_rest, &appendix) != 0) 
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */

	/* now we know, that it ends with implicit uri ending */
	
	list_user.s = list.s;
	list_user.len = list.len - appendix.len;
	if (str_case_equals(&user, &list_user) != 0) return auth_rejected;
	else return auth_granted;
}

static authorization_result_t authorize_subscription_cb(struct _subscription_data_t *s)
{
	switch (rls_auth_params.type) {
		case rls_auth_none:
			return auth_granted; /* ! no auth done ! */
		case rls_auth_implicit:
			return authorize_implicit(s);
		case rls_auth_xcap:
			LOG(L_ERR, "XCAP auth for resource lists not done yet!\n");
			return auth_unresolved;
	}
	return auth_unresolved;
}

/************* global functions ************/

void rls_lock()
{
	/* FIXME: solve locking more efficiently - locking whole RLS in 
	 * all cases of manipulating internal structures is not good
	 * solution */
	lock_get(rls_mutex);
}

void rls_unlock()
{
	lock_release(rls_mutex);
}

int rls_init()
{
	rls = (rls_data_t*)shm_malloc(sizeof(rls_data_t));
	if (!rls) {
		LOG(L_ERR, "rls_init(): memory allocation error\n");
		return -1;
	}
	rls->first = NULL;
	rls->last = NULL;
	
	rls_mutex = lock_alloc();
	if (!rls_mutex) {
		LOG(L_ERR, "rls_init(): Can't initialize mutex\n");
		return -1;
	}
	lock_init(rls_mutex);

	rls_manager = sm_create(send_notify_cb, 
			terminate_subscription_cb, 
			authorize_subscription_cb,
			rls_mutex,
			rls_min_expiration,	/* min expiration time in seconds */
			rls_max_expiration, /* max expiration time in seconds */
			rls_default_expiration /* default expiration time in seconds */
			);
	
	return 0;
}

int rls_destroy()
{
	TRACE_LOG("rls_destroy() called\n");
	/* FIXME: destroy the whole rl_subscription list */
	/* sm_destroy(rls_manager); */

	if (rls_mutex) {
		lock_destroy(rls_mutex);
		lock_dealloc(rls_mutex);
	}
	if (rls) {
		shm_free(rls);
		rls = NULL;
	}
	return 0;
}

/************* Helper functions for RL subscription manipulation ************/

static int get_user_from_list_uri(str_t *uri, str_t *user)
{
	str_t list, list_rest;
	str_t appendix = { s: "-list", len: 5 };

	if (!uri) return -1;
	if (get_user_from_uri(uri, &list) != 0) return -1;

	if (user) *user = list;
	
	/* remove suffix -list */
	if (list.len <= appendix.len) 
		return 1; /* it is not something like xxx-list@... */

	list_rest.len = appendix.len;
	list_rest.s = list.s + list.len - appendix.len;
	if (str_case_equals(&list_rest, &appendix) != 0) 
		return 1; /* it is not something like xxx-list@... */

	if (user) {
		user->s = list.s;
		user->len = list.len - appendix.len;
	}
	return 0;
}

str_t * rls_get_uri(rl_subscription_t *s)
{
	if (!s) return NULL;
	
	if (s->type == rls_external_subscription) {
		return &((s)->external.record_id);
	}
	else {
		if (s->internal.s) return &s->internal.s->record_id;
	}
	return NULL;
}

str_t * rls_get_package(rl_subscription_t *s)
{
	static str presence = STR_STATIC_INIT("presence");
	
	if (!s) return NULL;
	
	if (s->type == rls_external_subscription)
		return &((s)->external.package);
	else return &presence;

	return NULL;
}

str_t * rls_get_subscriber(rl_subscription_t *subscription)
{
	if (!subscription) return NULL;
	
	switch (subscription->type) {
		case rls_external_subscription:
			return &subscription->external.subscriber;
		case rls_internal_subscription:
			if (subscription->internal.s)
				return &subscription->internal.s->subscriber_id;
			break;
	}

	return NULL;
}

int create_virtual_subscriptions(rl_subscription_t *ss, const char *xcap_root)
{
	flat_list_t *e, *flat = NULL;
	xcap_query_t xcap;
	virtual_subscription_t *vs;
	int res = 0;
	str s;
	str_t user;
	str_t *ss_uri = NULL;
	str_t *ss_package = NULL;
	
	/* TODO: create virtual subscriptions using Accept headers ... (for remote subscriptions) */

	ss_uri = rls_get_uri(ss);
	ss_package = rls_get_package(ss);

	/* XCAP query */
	memset(&xcap, 0, sizeof(xcap));
	DEBUG_LOG("rli_create_content(): doing XCAP query\n");
	switch (rls_mode) {
		case rls_mode_full:
			if (reduce_xcap_needs) {
				res = get_rls_from_full_doc(xcap_root, ss_uri, &xcap, 
					ss_package, &flat);
			}
			else {
				res = get_rls(xcap_root, ss_uri, &xcap, 
					ss_package, &flat);
			}
			break;
		case rls_mode_simple:
			if (get_user_from_list_uri(ss_uri, &user) == 0) {
				/* it is uri in the form user-list@domain */
				res = get_resource_list_as_rls(xcap_root, &user, &xcap, &flat);
			}
			else {
				/* it is NOT uri in the form xxx-list@domain -> try to use
				 * standard RLS subscription processing */
				if (reduce_xcap_needs) {
					res = get_rls_from_full_doc(xcap_root, ss_uri, &xcap, 
						ss_package, &flat);
				}
				else {
					res = get_rls(xcap_root, ss_uri, &xcap, 
							ss_package, &flat);
				}
			}
			break;
	}
			
	if (res != RES_OK) return res;

	/* go through flat list and find/create virtual subscriptions */
	e = flat;
	while (e) {
		s.s = e->uri;
		if (s.s) s.len = strlen(s.s);
		else s.len = 0;

		res = vs_create(&s, rls_get_package(ss), &vs, e->names, ss);
		
		if (res != RES_OK) return res;
		ptr_vector_add(&ss->vs, vs);

		e = e->next;
	}

	DEBUG_LOG("rli_create_content(): freeing flat list\n");
	free_flat_list(flat);
	return RES_OK;
}
/************* RL subscription manipulation function  ************/

rl_subscription_t *rls_alloc_subscription(rls_subscription_type_t type)
{
	rl_subscription_t *s;
	
	s = (rl_subscription_t*)shm_malloc(sizeof(rl_subscription_t));
	if (!s) {
		LOG(L_ERR, "rls_alloc_subscription(): can't allocate memory\n");
		return NULL;
	}
	s->external.status = subscription_uninitialized;
	s->external.usr_data = s;
	s->internal.s = NULL;
	s->internal.next = NULL;
	s->internal.prev = NULL;
	s->internal.rls = s;
	s->doc_version = 0;
	s->changed = 0;
	s->type = type;
	s->dbid[0] = 0;

	/* s->first_vs = NULL;
	s->last_vs = NULL; */
	ptr_vector_init(&s->vs, 4);
	
	return s;
}

int rls_create_subscription(struct sip_msg *m, rl_subscription_t **dst, const char *xcap_root)
{
	rl_subscription_t *s;
	int res;

	if (!dst) return RES_INTERNAL_ERR;
	*dst = NULL;

	/* FIXME: test if there is a "Require: eventlist" header - if not,
	 * it is probbably not a message for RLS! */
	/* FIXME: test if Accept = multipart/related, application/rlmi+xml! */
	
	s = rls_alloc_subscription(rls_external_subscription);
	if (!s) {
		LOG(L_ERR, "rls_create_new(): can't allocate memory\n");
		return RES_MEMORY_ERR;
	}
	generate_db_id(&s->dbid, s);
			
	res = sm_init_subscription_nolock(rls_manager, &s->external, m);
	if (res != RES_OK) {
		rls_free(s);
		return res;
	}

	/* store pointer to this RL subscription as user data of (low level) subscription */
	s->external.usr_data = s; 
			
/*	res = set_rls_info(m, s, xcap_root);
	if (res != 0) {
		rls_free(s);
		return res;
	}*/
	
	res = create_virtual_subscriptions(s, xcap_root);
	if (res != 0) {
		rls_free(s);
		return res;
	}

	if (use_db) {
		if (rls_db_add(s) != 0) {
			rls_free(s);
			return RES_INTERNAL_ERR; /* FIXME RES_DB_ERR */
		}
	}
	
	*dst = s;
	return RES_OK;
}

int rls_find_subscription(str *from_tag, str *to_tag, str *call_id, rl_subscription_t **dst)
{
	subscription_data_t *s;
	int res;

	/* FIXME: hashing ! */
	*dst = NULL;
	res = sm_find_subscription(rls_manager, from_tag, to_tag, call_id, &s);
	if ((res == RES_OK) && (s)) {
		if (!s->usr_data) {
			LOG(L_ERR, "found subscription without filled usr_data\n");
			return RES_INTERNAL_ERR;
		}
		else {
			*dst = (rl_subscription_t*)s->usr_data;
			return RES_OK;
		}
	}
	return RES_NOT_FOUND;
}

int rls_refresh_subscription(struct sip_msg *m, rl_subscription_t *s)
{
	int res;

	if (!s) return RES_INTERNAL_ERR;
	if (s->type != rls_external_subscription) return RES_INTERNAL_ERR;

	res = sm_refresh_subscription_nolock(rls_manager, &s->external, m);

	if (use_db) rls_db_update(s);

	return res;
}

void rls_remove(rl_subscription_t *s)
{
	if (!s) return;
	
	if (use_db) rls_db_remove(s);

	rls_free(s);
}

void rls_free(rl_subscription_t *s)
{
	int i, cnt;
	virtual_subscription_t *vs;
	
	if (!s) return;

	switch (s->type) {
		case rls_external_subscription:
			DEBUG_LOG("rls_free(): %.*s\n", FMT_STR(s->external.record_id));
			break;
		case rls_internal_subscription:
			if (s->internal.s)
				DEBUG_LOG("rls_free(): internal s. %.*s\n", FMT_STR(s->internal.s->record_id));
			else DEBUG_LOG("rls_free(): internal s. ???\n");
			break;
	}

	if (use_db) rls_db_remove(s);

	if (s->type == rls_external_subscription)
		sm_release_subscription_nolock(rls_manager, &s->external);
	else {
		release_internal_subscription(s);
	}

	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;
		vs_free(vs);
	}
	ptr_vector_destroy(&s->vs);
	shm_free(s);
}

/* void rls_notify_cb(struct cell* t, struct sip_msg* msg, int code, void *param) */
void rls_notify_cb(struct cell* t, int type, struct tmcb_params* params)
{
	rl_subscription_t *s = NULL;

	if (!params) return;

	if (params->param) s = (rl_subscription_t *)*(params->param);

	if (params->code >= 300) { /* what else can we do with 3xx ? */
		LOG(L_ERR, "rls_notify_cb(): %d response on NOTIFY - removing subscription %p\n", params->code, s);

		rls_lock();
		if (s) rls_remove(s);
		rls_unlock();
	}
}

static int rls_generate_notify_ext(rl_subscription_t *s, int full_info)
{
	/* !!! the main mutex must be locked here !!! */
	int res;
	str doc;
	dstring_t dstr;
	str headers, content_type;
	static str method = STR_STATIC_INIT(METHOD_NOTIFY);
	dlg_t *dlg;
	
	dlg = s->external.dialog;
	if (!dlg) return -1;

	str_clear(&doc);
	str_clear(&content_type);
	if (sm_subscription_pending(&s->external) != 0) {
		/* create the document only for non-pending subscriptions */
		if (create_rlmi_document(&doc, &content_type, s, full_info) != 0) {
			return -1;
		}
	}
	
	dstr_init(&dstr, 256);
	dstr_append_zt(&dstr, "Subscription-State: ");
	switch (s->external.status) {
		case subscription_active: 
				dstr_append_zt(&dstr, "active\r\n");
				break;
		case subscription_pending: 
				dstr_append_zt(&dstr, "pending\r\n");
				break;
		case subscription_terminated_pending: 
		case subscription_terminated: 
				dstr_append_zt(&dstr, "terminated\r\n");
				break;
		case subscription_terminated_pending_to: 
		case subscription_terminated_to: 
				dstr_append_zt(&dstr, 
					"terminated;reason=timeout\r\n");
				break;
		case subscription_uninitialized: 
				dstr_append_zt(&dstr, "pending\r\n");
				/* this is an error ! */
				LOG(L_ERR, "sending NOTIFY for an unitialized subscription!\n");
				break;
	}
	dstr_append_str(&dstr, &s->external.contact);
	
	dstr_append_zt(&dstr, "Event: ");
	dstr_append_str(&dstr, rls_get_package(s));
	dstr_append_zt(&dstr, "\r\n");
	dstr_append_zt(&dstr, "Require: eventlist\r\n");
	dstr_append_str(&dstr, &content_type);

	headers.len = dstr_get_data_length(&dstr);
	headers.s = cds_malloc(headers.len);
	if (!headers.s) headers.len = 0;
	else dstr_get_data(&dstr, headers.s);
	dstr_destroy(&dstr);

	/* DEBUG_LOG("sending NOTIFY message to %.*s (subscription %p)\n",  
			dlg->rem_uri.len, 
			ZSW(dlg->rem_uri.s), s); */
	
	if (sm_subscription_terminated(&s->external) == 0) {
		/* doesn't matter if delivered or not, it will be freed otherwise !!! */
		res = tmb.t_request_within(&method, &headers, &doc, dlg, 0, 0);
	}
	else {
		/* the subscritpion will be destroyed if NOTIFY delivery problems */
		rls_unlock(); /* the callback locks this mutex ! */
		
		/* !!!! FIXME: callbacks can't be safely used (may be called or not,
		 * may free memory automaticaly or not) !!! */
		res = tmb.t_request_within(&method, &headers, &doc, dlg, 0, 0);
		
/*		res = tmb.t_request_within(&method, &headers, &doc, dlg, rls_notify_cb, s); */
		rls_lock(); /* the callback locks this mutex ! */
		if (res < 0) {
			/* does this mean, that the callback was not called ??? */
			LOG(L_ERR, "rls_generate_notify(): t_request_within FAILED: %d! Freeing RL subscription.\n", res);
			rls_remove(s); /* ?????? */
		}
	}
	
	if (doc.s) cds_free(doc.s);
	if (content_type.s) cds_free(content_type.s);
	if (headers.s) cds_free(headers.s);
	
	if (use_db) rls_db_update(s);
	
	return res;
}

static list_presence_info_t* rls2list_presence_info(rl_subscription_t *s)
{
	list_presence_info_t *info;
	info = create_list_presence_info(&s->internal.s->record_id);
	if (!info) return info;

	str_clear(&info->pres_doc);
	str_clear(&info->content_type);
	TRACE_LOG(" ... create RLMI document\n");
	create_rlmi_document(&info->pres_doc, &info->content_type, s, 1);

	return info;
}

static int rls_generate_notify_int(rl_subscription_t *s)
{
	/* generate internal notification */
	client_notify_info_t *info;
	mq_message_t *msg;
	static str_t notifier_name = { s: "rls", len: 3 };
	
	TRACE_LOG("generating internal list notify\n");

	if (!s->internal.s) return 1;

	TRACE_LOG(" ... creating message\n");
	msg = create_message_ex(sizeof(client_notify_info_t));
	if (!msg) {
		ERROR_LOG("can't create internal notify message!\n");
		return -1; 
	}
	set_data_destroy_function(msg, (destroy_function_f)free_client_notify_info_content);
	info = (client_notify_info_t*)msg->data;

	TRACE_LOG(" ... setting info\n");
	str_dup(&info->record_id, &s->internal.s->record_id);
	str_dup(&info->package, &s->internal.s->package->name);
	str_dup(&info->notifier, &notifier_name);
	TRACE_LOG(" ... setting list presence info\n");
	info->data = rls2list_presence_info(s);
	info->data_len = sizeof(list_presence_info_t);
	info->destroy_func = (destroy_function_f)free_list_presence_info;

	TRACE_LOG(" ... before notify_subscriber\n");
	notify_subscriber(s->internal.s, msg);
	TRACE_LOG(" ... after notify_subscriber\n");
	return 0;
}

int rls_generate_notify(rl_subscription_t *s, int full_info)
{
	/* !!! the main mutex must be locked here !!! */
	if (!s) {
		LOG(L_ERR, "rls_generate_notify(): called with <null> subscription\n");
		return -1;
	}
	
	switch (s->type) {
		case rls_external_subscription:
			return rls_generate_notify_ext(s, full_info);
		case rls_internal_subscription:
			return rls_generate_notify_int(s);
	}
	
	return -1;
}

int rls_prepare_subscription_response(rl_subscription_t *s, struct sip_msg *m) {
	/* char *hdr = "Supported: eventlist\r\n"; */
	char *hdr = "Require: eventlist\r\n";

	if (s->type != rls_external_subscription) return -1;
	
	if (!add_lump_rpl(m, hdr, strlen(hdr), LUMP_RPL_HDR)) return -1;
	
	return sm_prepare_subscription_response(rls_manager, &s->external, m);
}

/** returns the count of seconds remaining to subscription expiration */
int rls_subscription_expires_in(rl_subscription_t *s) 
{
	if (s->type == rls_external_subscription) 
		return sm_subscription_expires_in(rls_manager, &s->external);
	else return -1;
}

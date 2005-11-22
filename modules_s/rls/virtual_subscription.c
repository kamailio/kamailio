#include "rl_subscription.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include <xcap/resource_list.h>
#include "rls_mod.h"
#include "result_codes.h"
#include <cds/dstring.h>
#include <cds/logger.h>
#include <presence/qsa.h>
#include <presence/pres_doc.h>
#include <presence/pidf.h>

/* shared structure holding the data */
typedef struct {
	virtual_subscription_t *first;
	virtual_subscription_t *last;
	/* hash, ... */
	notifier_domain_t *domain;
} vs_data_t;

static vs_data_t *vsd = NULL;

static void vs_timer_cb(unsigned int ticks, void *param);

/******** global functions (initialization) ********/

int vs_init()
{
	qsa_initialize();

	vsd = (vs_data_t*)shm_malloc(sizeof(vs_data_t));
	if (!vsd) {
		LOG(L_ERR, "vs_init(): memory allocation error\n");
		return -1;
	}
	vsd->first = NULL;
	vsd->last = NULL;
	vsd->domain = qsa_get_default_domain();
	if (!vsd->domain) {
		LOG(L_ERR, "vs_init(): can't register notifier domain\n");
		return -1;	
	}
	DEBUG_LOG("QSA (vs) domain: %p\n", vsd->domain);
	
	/* register a SER timer */
	if (register_timer(vs_timer_cb, NULL, 10) < 0) {
		LOG(L_ERR, "vs_init(): can't register timer\n");
		return -1;
	}
	return 0;
}

int vs_destroy()
{
	/* FIXME: destroy the whole vs list */
	qsa_release_domain(vsd->domain);

	/* qsa_destroy(); */
	return 0;
}

/******** Helper functions ********/
static void process_notify_info(virtual_subscription_t *vs, client_notify_info_t *info) 
{
	presentity_info_t *pinfo;

	if ((!vs) || (!info)) return;
	pinfo = (presentity_info_t*)info->data;	/* TODO: only for "presence" package, not for "presence-list" */
	if (!pinfo) return;
	
	str_free_content(&vs->state_document); /* release old document if set */
	create_pidf_document(pinfo, &vs->state_document, &vs->content_type);
	vs->status = subscription_active;
	
	DEBUG_LOG("created pidf document:\n %.*s\n", FMT_STR(vs->state_document));

	free_presentity_info(pinfo); /* destroy !*/
	info->data = NULL;
}

/* returns positive value if status of this VS changed */
static int process_vs_messages(virtual_subscription_t *vs)
{
	int cnt = 0;
	client_notify_info_t *info;
	mq_message_t *msg;
	
	if (!vs->local_subscription) return 0;

	while (!is_msg_queue_empty(&vs->mq)) {
		msg = pop_message(&vs->mq);
		if (!msg) continue;
		info = (client_notify_info_t *)msg->data;
		if (info) {
			DEBUG_LOG("received NOTIFY MESSAGE for %.*s\n", FMT_STR(info->record_id));
			process_notify_info(vs, info);
			free_client_notify_info_content(info);
			cnt++;
		}
		free_message(msg);
	}
	return cnt;
}

static void mark_as_modified(virtual_subscription_t *vs)
{
	if (sm_subscription_pending(&vs->subscription->subscription) != 0) {
		/* NOTIFY should be send only for nonpending subscriptions (or active?)*/
		vs->subscription->changed++;
		DEBUG_LOG("RL subscription status changed (%p, %d)\n", 
			vs->subscription,
			vs->subscription->changed);
	}
}

static void notify_all_modified()
{
	virtual_subscription_t *vs;

	vs = vsd->first;
	while (vs) {
		if (vs->subscription->changed > 0) {
			DEBUG_LOG("RL subscription generate notify\n");
			rls_generate_notify(vs->subscription, 1);
			vs->subscription->changed = 0;
		}
		vs = vs->next;
	}
}

static void vs_timer_cb(unsigned int ticks, void *param)
{
	virtual_subscription_t *vs;
	int changed = 0;

	rls_lock();

	/* process all messages for virtual subscriptions */
	vs = vsd->first;
	while (vs) {
		if (process_vs_messages(vs) > 0) {
			DEBUG_LOG("VS status changed\n");
			mark_as_modified(vs);
			changed = 1;
		}
		vs = vs->next;
	}
	
	if (changed) {
		/* this could be called from some rli_timer ? */
		notify_all_modified(); 	
	}
	
	rls_unlock();
}

static int add_to_vs_list(virtual_subscription_t *vs)
{
	if (!vs) return RES_INTERNAL_ERR;
	if (!vsd) {
		LOG(L_ERR, "vs_add(): vsd not set!\n");
		return RES_INTERNAL_ERR;
	}
	if (vsd->last) vsd->last->next = vs;
	else vsd->first = vs;
	vsd->last = vs;

	return RES_OK;
}

static int remove_from_vs_list(virtual_subscription_t *vs)
{
	if (!vs) return RES_INTERNAL_ERR;
	if (!vsd) {
		LOG(L_ERR, "vs_remove(): vsd not set!\n");
		return RES_INTERNAL_ERR;
	}

	if (vs->next) vs->next->prev = vs->prev;
	else vsd->last = vs->prev;
	if (vs->prev) vs->prev->next = vs->next;
	else vsd->first = vs->next;
	
	return RES_OK;
}

static int is_local_uri(str *uri)
{
	/* TODO */
	return 0;	/* 0 means it is local uri ! */
}

static int get_local_uri(const str_t *src_uri, str_t* dst_uri)
{
	str uri;
	struct sip_uri parsed;
	int res = -1;

	if (dst_uri && src_uri) {
		*dst_uri = *src_uri;
		return 0;
	}
	else return -1;
	
	uri.s = src_uri->s;
	uri.len = src_uri->len;
	
	if (parse_uri(src_uri->s, src_uri->len, &parsed) == 0) {
		if (parsed.user.s) {
			uri.s = parsed.user.s;
			uri.len = src_uri->len - (parsed.user.s - src_uri->s);
			res = 0;
		} 
	}
	
	if (dst_uri) *dst_uri = uri;
	return res;
}

static int create_local_subscription(virtual_subscription_t *vs)
{
	/* create concrete local subscription */
	str uri;
	
	/* remove sip:, ... so pa/usrloc will be satisfied */
	get_local_uri(&vs->uri, &uri); 	

	DEBUG_LOG("creating local subscription to %.*s [%.*s]\n",
			FMT_STR(vs->uri), FMT_STR(*rls_get_package(vs->subscription)));
	if (msg_queue_init(&vs->mq) != 0) {
		LOG(L_ERR, "can't initialize message queue!\n");
		return -1;
	}
	vs->local_subscription = subscribe(vsd->domain, 
			rls_get_package(vs->subscription), 
			&uri, &vs->subscription->subscription.subscriber, &vs->mq, vs);
	if (!vs->local_subscription) {
		LOG(L_ERR, "can't create local subscription!\n");
		return -1;
	}
	return 0;
}

static int create_remote_subscription(virtual_subscription_t *vs)
{
	/* TODO: create concrete Back-End subscription */
	LOG(L_ERR, "remote subscription handling not implemented\n");
	return 0;
}

/******** VS manipulation ********/

int vs_create(str *uri, str *package, virtual_subscription_t **dst, display_name_t *dnames, rl_subscription_t *subscription)
{
	int res;
	display_name_t *d;

	if (!dst) return RES_INTERNAL_ERR;
	*dst = NULL;
	if (!uri) {
		LOG(L_ERR, "vs_create(): no uri given\n");
		return RES_INTERNAL_ERR;
	}
	if ((!uri->s) || (uri->len < 1)) {
		LOG(L_ERR, "vs_create(): no uri given\n");
		return RES_INTERNAL_ERR;
	}
	
	*dst = (virtual_subscription_t*)shm_malloc(sizeof(virtual_subscription_t) + uri->len + 1);
	if (!(*dst)) {
		LOG(L_ERR, "vs_create(): can't allocate memory\n");
		return RES_MEMORY_ERR;
	}

	(*dst)->next = NULL;
	(*dst)->prev = NULL;
	vector_init(&(*dst)->display_names, sizeof(vs_display_name_t), 4);
	memcpy((*dst)->uri_str, uri->s, uri->len);
	(*dst)->uri.s = (*dst)->uri_str;
	(*dst)->uri.len = uri->len;
/*	str_dup(&(*dst)->package, package); */
	(*dst)->state_document.len = 0;
	(*dst)->state_document.s = NULL;
	(*dst)->content_type.len = 0;
	(*dst)->content_type.s = NULL;
	(*dst)->status = subscription_pending;
	(*dst)->local_subscription = NULL;
	(*dst)->subscription = subscription;
	generate_db_id(&(*dst)->dbid, *dst);

	add_to_vs_list(*dst);

	/*LOG(L_TRACE, "created Virtual Subscription to %.*s\n", uri->len, uri->s);*/
	
	if (is_local_uri(uri) == 0) {	/* it IS local uri */
		res = create_local_subscription(*dst);
	}
	else {	/* nonlocal uri - points to other servers */
		res = create_remote_subscription(*dst);
	}
	if (res != 0) {
		vs_free(*dst);
		return res;
	}
	
	/* TODO: remember the list of Accept headers from client subscribe
	 * it will be used for Back-End subscriptions */
			
	/* add names */
	if (dnames) {
		d = SEQUENCE_FIRST(dnames);
		while (d) {
			vs_add_display_name((*dst), d->name, d->lang);
			d = SEQUENCE_NEXT(d);
		}
	}

	return RES_OK;
}

int vs_add_display_name(virtual_subscription_t *vs, const char *name, const char *lang)
{
	vs_display_name_t dn;

	if (name) {
		dn.name.len = strlen(name);
		if (dn.name.len > 0) {
			dn.name.s = (char *)shm_malloc(dn.name.len);
			if (!dn.name.s) dn.name.len = 0;
			else memcpy(dn.name.s, name, dn.name.len);
		}
	}
	else {
		dn.name.len = 0;
		dn.name.s = NULL;
	}
	
	if (lang) {
		dn.lang.len = strlen(lang);
		if (dn.lang.len > 0) {
			dn.lang.s = (char *)shm_malloc(dn.lang.len);
			if (!dn.lang.s) dn.lang.len = 0;
			else memcpy(dn.lang.s, lang, dn.lang.len);
		}
	}
	else {
		dn.lang.len = 0;
		dn.lang.s = NULL;
	}
/*	TRACE_LOG("adding display name: %s\n", name);*/
	return vector_add(&vs->display_names, &dn);
}

void vs_free(virtual_subscription_t *vs)
{
	int i, cnt;
	vs_display_name_t dn;
	
	if (vs) {
		DEBUG_LOG("freeing Virtual Subscription %p to %.*s\n", vs, FMT_STR(vs->uri));
		remove_from_vs_list(vs);
		
/*		if ( (vs->package.len > 0) && (vs->package.s) ) 
			shm_free(vs->package.s);*/
		
		str_free_content(&vs->state_document);
		str_free_content(&vs->content_type);
		
		cnt = vector_size(&vs->display_names);
		for (i = 0; i < cnt; i++) {
			if (vector_get(&vs->display_names, i, &dn) != 0) continue;
			if (dn.name.s && (dn.name.len > 0)) shm_free(dn.name.s);
			if (dn.lang.s && (dn.lang.len > 0)) shm_free(dn.lang.s);
		}
		vector_destroy(&vs->display_names);

		if (vs->local_subscription) {
			unsubscribe(vsd->domain, vs->local_subscription);
			msg_queue_destroy(&vs->mq);
		}

		shm_free(vs);
/*		LOG(L_TRACE, "Virtual Subscription freed\n");*/
	}
}


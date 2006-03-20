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
#include <cds/list.h>
#include "rls_data.h"
#include "rls_auth.h"

/* shared structure holding the data */
typedef struct {
	virtual_subscription_t *first;
	virtual_subscription_t *last;
	/* hash, ... */
	notifier_domain_t *domain;
} vs_data_t;

static vs_data_t *vsd = NULL;

/******** global functions (initialization) ********/

int vs_init()
{
	vsd = (vs_data_t*)mem_alloc(sizeof(vs_data_t));
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
	
	return 0;
}

int vs_destroy()
{
	/* virtual subscriptions are freed in rls_free */
	if (vsd) {
		qsa_release_domain(vsd->domain);
		vsd->domain = NULL;
		mem_free(vsd);
		vsd = NULL;
	}
	return 0;
}

/******** Helper functions ********/
void process_rls_notification(virtual_subscription_t *vs, client_notify_info_t *info) 
{
	presentity_info_t *pinfo;
	raw_presence_info_t *raw;

	if ((!vs) || (!info)) return;
	
	TRACE("Processing notification for VS %p\n", vs);

	/* release old document if set */
	/* TRACE(" ... freeing old documents\n"); */
	str_free_content(&vs->state_document); 		
	str_free_content(&vs->content_type);
			
	switch (info->data_type) {
		case PRESENTITY_RAW_INFO:
			/* TRACE("Processing raw notification\n"); */
			
			raw = (raw_presence_info_t*)info->data;
			if (!raw) return;
			
			/* create_presence_rlmi_document(linfo, &vs->state_document, &vs->content_type); */
			/* TRACE(" ... duplicating documents\n"); */
			str_dup(&vs->state_document, &raw->pres_doc);
			str_dup(&vs->content_type, &raw->content_type);
			
			vs->status = subscription_active;
			break;
		case PRESENTITY_INFO_TYPE:
			/* TRACE("Processing structured notification\n"); */
			
			pinfo = (presentity_info_t*)info->data;
			if (!pinfo) return;
			
			create_pidf_document(pinfo, &vs->state_document, &vs->content_type);
			vs->status = subscription_active;
			
			/* DEBUG_LOG("created pidf document:\n %.*s\n", FMT_STR(vs->state_document)); */
			break;
		default:
			ERR("received unacceptable notification (%d)\n", info->data_type);
			return;
	}

	rls_generate_notify(vs->subscription, 1);
}

void process_internal_notify(virtual_subscription_t *vs, 
		str_t *new_state_document,
		str_t *new_content_type) 
{
	if (!vs) return;
	
	TRACE("Processing internal notification for VS %p\n", vs);
		
	/* free old_documents */
	str_free_content(&vs->state_document); 		
	str_free_content(&vs->content_type);
	
	/* don't copy documents - use them directly - 
	 * see rls_generate_notify_int */
	if (new_state_document) vs->state_document = *new_state_document;
	else str_clear(&vs->state_document);
	if (new_content_type) vs->content_type = *new_content_type;
	else str_clear(&vs->content_type);
	
	/* propagate notification */
	rls_generate_notify(vs->subscription, 1);
	vs->subscription->changed = 0;
}

#if 0

static void mark_as_modified(virtual_subscription_t *vs)
{
	rl_subscription_t *rls = vs->subscription;

	switch (rls->type) {
		case rls_external_subscription:
			if (sm_subscription_pending(&rls->u.external) == 0) {
				/* pending subscription will not be notified */
				return; 
			}
			break;
		case rls_internal_subscription:
			/* FIXME: something like above? */
			break;
	}
				
	/* NOTIFY should be send only for nonpending subscriptions (or active?)*/

	vs->subscription->changed++;
	DEBUG_LOG("RL subscription status changed (%p, %d)\n", 
		rls, rls->changed);
}

static void vs_timer_cb(unsigned int ticks, void *param)
{
	virtual_subscription_t *vs;
	int changed = 0;
	int cntr = 0;
	time_t start, stop;

	start = time(NULL);
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
		cntr++; /* debugging purposes */
	}

	/* TRACE_LOG("processed messages for %d virtual subscription(s)\n", cntr); */
	
	if (changed) {
		/* this could be called from some rli_timer ? */
		notify_all_modified(); 	
	}
	
	rls_unlock();
	stop = time(NULL);

	if (stop - start > 1) WARN("vs_timer_cb took %d secs\n", (int) (stop - start));
}

#endif

void notify_all_modified_vs()
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

static int add_to_vs_list(virtual_subscription_t *vs)
{
	if (!vs) return RES_INTERNAL_ERR;
	if (!vsd) {
		LOG(L_ERR, "vs_add(): vsd not set!\n");
		return RES_INTERNAL_ERR;
	}
	DOUBLE_LINKED_LIST_ADD(vsd->first, vsd->last, vs);

	return RES_OK;
}

static int remove_from_vs_list(virtual_subscription_t *vs)
{
	if (!vs) return RES_INTERNAL_ERR;
	if (!vsd) {
		LOG(L_ERR, "vs_remove(): vsd not set!\n");
		return RES_INTERNAL_ERR;
	}

	DOUBLE_LINKED_LIST_REMOVE(vsd->first, vsd->last, vs);
	
	return RES_OK;
}

int xcap_query_rls_services(const str_t *xcap_root, 
		xcap_query_params_t *xcap_params,
		const str *uri, const str *package, 
		flat_list_t **dst)
{
	if (dst) *dst = NULL;
	
	if (reduce_xcap_needs)
		return get_rls_from_full_doc(xcap_root, uri, xcap_params, package, dst);
	else
		return get_rls(xcap_root, uri, xcap_params, package, dst);
}

static int create_subscriptions(virtual_subscription_t *vs)
{
	/* create concrete local subscription */
	str *package = NULL;
	str *subscriber = NULL;
	flat_list_t *flat = NULL;

	package = rls_get_package(vs->subscription);

	DEBUG_LOG("creating local subscription to %.*s\n", FMT_STR(vs->uri));

	if (xcap_query_rls_services(&vs->subscription->xcap_root,
				&vs->subscription->xcap_params,
				&vs->uri, package, &flat) == 0) {
		/* it is resource list -> do internal subscription to RLS */
		if (rls_create_internal_subscription(vs,
					&vs->local_subscription_list, flat) != 0) {
			ERR("can't create internal subscription\n");
			free_flat_list(flat);
			return -1;
		}
		free_flat_list(flat);
		
		vs->status = subscription_active;
		/* FIXME: rls_authorize_subscription(vs->local_subscription_list); */
	}
	else {
		subscriber = rls_get_subscriber(vs->subscription);
		/* not RLS record -> do QSA subscription to given package */
		vs->local_subscription_pres = subscribe(vsd->domain, 
				package, 
				&vs->uri, subscriber, 
				&rls->notify_mq, vs);
		if (!vs->local_subscription_pres) {
			LOG(L_ERR, "can't create local subscription (pres)!\n");
			return -1;
		}
	}
	
	return 0;
}

/******** VS manipulation ********/

int vs_create(str *uri, 
		virtual_subscription_t **dst, 
		display_name_t *dnames, 
		rl_subscription_t *subscription)
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
	
	*dst = (virtual_subscription_t*)mem_alloc(sizeof(virtual_subscription_t) + uri->len + 1);
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
	(*dst)->state_document.len = 0;
	(*dst)->state_document.s = NULL;
	(*dst)->content_type.len = 0;
	(*dst)->content_type.s = NULL;
	(*dst)->status = subscription_pending;
	(*dst)->local_subscription_pres = NULL;
	(*dst)->local_subscription_list = NULL;
	(*dst)->subscription = subscription;
	generate_db_id(&(*dst)->dbid, *dst);

	add_to_vs_list(*dst);

	TRACE("created VS %p to %.*s\n", *dst, uri->len, uri->s);
	
	res = create_subscriptions(*dst);
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
			dn.name.s = (char *)mem_alloc(dn.name.len);
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
			dn.lang.s = (char *)mem_alloc(dn.lang.len);
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
		if (vs->local_subscription_pres)
			unsubscribe(vsd->domain, vs->local_subscription_pres);
		if (vs->local_subscription_list) 
			rls_remove(vs->local_subscription_list);
		
		destroy_vs_notifications(vs);
		
		remove_from_vs_list(vs);
		
		str_free_content(&vs->state_document);
		str_free_content(&vs->content_type);

		/* if ( (vs->package.len > 0) && (vs->package.s) ) 
		mem_free(vs->package.s); */
		
		cnt = vector_size(&vs->display_names);
		for (i = 0; i < cnt; i++) {
			if (vector_get(&vs->display_names, i, &dn) != 0) continue;
			if (dn.name.s && (dn.name.len > 0)) mem_free(dn.name.s);
			if (dn.lang.s && (dn.lang.len > 0)) mem_free(dn.lang.s);
		}
		vector_destroy(&vs->display_names);

		mem_free(vs);
/*		LOG(L_TRACE, "Virtual Subscription freed\n");*/
	}
}


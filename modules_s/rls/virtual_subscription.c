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
	qsa_content_type_t *ct_presence_info;
	qsa_content_type_t *ct_raw;
} vs_data_t;

static vs_data_t *vsd = NULL;

/******** global functions (initialization) ********/

int vs_init()
{
	static str presence_info = STR_STATIC_INIT(CT_PRESENCE_INFO);
	static str raw = STR_STATIC_INIT(CT_RAW);
	
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
	
	vsd->ct_presence_info = register_content_type(vsd->domain, 
			&presence_info, (destroy_function_f)free_presentity_info);
	if (!vsd->ct_presence_info) {
		ERR("can't register QSA content type\n");
		return -1;
	}
	else TRACE("RLS_PRESENCE_INFO: %p\n", vsd->ct_presence_info);
	
	vsd->ct_raw = register_content_type(vsd->domain, 
			&raw, (destroy_function_f)free_raw_presence_info);
	if (!vsd->ct_raw) {
		ERR("can't register QSA content type\n");
		return -1;
	}
	else TRACE("RLS_RAW: %p\n", vsd->ct_raw);
	
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

/* sets new documents (frees them if equal) */
static void set_vs_document(virtual_subscription_t *vs,
		str_t *new_doc, 
		str_t *new_content_type)
{
	if (str_case_equals(&vs->state_document, new_doc) == 0) {
		/* DEBUG("new document is equal to the older one\n"); */
		str_free_content(new_doc);
	}
	else {
		str_free_content(&vs->state_document);
		if (new_doc) vs->state_document = *new_doc;
		else str_clear(&vs->state_document);
		vs->changed = 1;
	}
	
	if (str_case_equals(&vs->content_type, new_content_type) == 0) {
		/* DEBUG("new content-type is equal to the older one\n"); */
		str_free_content(new_content_type);
	}
	else {
		str_free_content(&vs->content_type);
		if (new_content_type) vs->content_type = *new_content_type;
		else str_clear(&vs->content_type);
		vs->changed = 1;
	}
}

/* duplicates documents if changed */
static int set_vs_document_dup(virtual_subscription_t *vs,
		str_t *new_doc, 
		str_t *new_content_type)
{
	if (str_case_equals(&vs->state_document, new_doc) == 0) {
		/* DEBUG("new document is equal to the older one\n"); */
	}
	else {
		str_free_content(&vs->state_document);
		str_dup(&vs->state_document, new_doc);
		vs->changed = 1;
	}
	
	if (str_case_equals(&vs->content_type, new_content_type) == 0) {
		/* DEBUG("new content-type is equal to the older one\n"); */
	}
	else {
		str_free_content(&vs->content_type);
		str_dup(&vs->content_type, new_content_type);
		vs->changed = 1;
	}
	return 0;
}

static void propagate_change(virtual_subscription_t *vs)
{
	if (vs->subscription->type == rls_internal_subscription) {
		/* propagate change to higher level */
		rls_generate_notify(vs->subscription, 1);
	}
	else {
		/* external subscriptions will send NOTIFY only sometimes 
		 * => we mark it as changed now */
		vs->subscription->changed++;
		/* FIXME: put this subscription in some queue? (needs remove from it
		 * when freeing!) */
		if (rls) rls->changed_subscriptions++; /* change indicator */
	}
}

void process_rls_notification(virtual_subscription_t *vs, client_notify_info_t *info) 
{
	presentity_info_t *pinfo;
	raw_presence_info_t *raw;
	str_t new_doc = STR_NULL;
	str_t new_type = STR_NULL;
	subscription_status_t old_status;
	
	if ((!vs) || (!info)) return;
	
	DBG("Processing notification for VS %p\n", vs);
	/* FIXME: put information from more sources together ? */

	old_status = vs->status;
	switch (info->status) {
		case qsa_subscription_active:
			vs->status = subscription_active;
			break;
		case qsa_subscription_pending:
			vs->status = subscription_pending;
			break;
		case qsa_subscription_rejected:
			vs->status = subscription_terminated;
			break;
		case qsa_subscription_terminated:
			vs->status = subscription_terminated;
			break;
	}
	if (old_status != vs->status) vs->changed = 1;

	if (info->content_type == vsd->ct_raw) {
		DEBUG("Processing raw notification\n");
			
		raw = (raw_presence_info_t*)info->data;
		if (!raw) return;
	
		/* document MUST be duplicated !!! */
		if (set_vs_document_dup(vs, &raw->pres_doc, &raw->content_type) < 0) {
			ERR("can't set new status document for VS %p\n", vs);
			return;
		}
	}
	else {	
		if (info->content_type == vsd->ct_presence_info) {
			DEBUG("Processing structured notification\n");
			
			pinfo = (presentity_info_t*)info->data;
			if (!pinfo) {
				str_clear(&new_doc);
				str_clear(&new_type);
			}
			else {
				if (create_pidf_document(pinfo, &new_doc, &new_type) < 0) {
					ERR("can't create PIDF document\n");
					str_free_content(&vs->state_document); 		
					str_free_content(&vs->content_type);
					return;
				}
				set_vs_document(vs, &new_doc, &new_type);
			}
		}
		else {
			if (info->content_type)
				ERR("received unacceptable notification (%.*s)\n", 
					FMT_STR(info->content_type->name));
			else ERR("received unacceptable notification without content type\n");
			str_free_content(&vs->state_document); 		
			str_free_content(&vs->content_type);
			return;
		}
	}
	
	if (vs->changed) propagate_change(vs);
}

void process_internal_notify(virtual_subscription_t *vs, 
		str_t *new_state_document,
		str_t *new_content_type) 
{
	if (!vs) return;
	
	DBG("Processing internal notification for VS %p\n", vs);
	
	/* don't copy document - use it directly */
	set_vs_document(vs, new_state_document, new_content_type);
	if (vs->changed) propagate_change(vs);
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

int xcap_query_rls_services(xcap_query_params_t *xcap_params,
		const str *uri, const str *package, 
		flat_list_t **dst)
{
	if (dst) *dst = NULL;
	
	if (reduce_xcap_needs)
		return get_rls_from_full_doc(uri, xcap_params, package, dst);
	else
		return get_rls(uri, xcap_params, package, dst);
}

static int create_subscriptions(virtual_subscription_t *vs, int nesting_level)
{
	/* create concrete local subscription */
	str *package = NULL;
	str *subscriber = NULL;
	flat_list_t *flat = NULL;

	package = rls_get_package(vs->subscription);

	DEBUG_LOG("creating local subscription to %.*s\n", FMT_STR(vs->uri));

	if ((nesting_level != 0) &&
			(xcap_query_rls_services(&vs->subscription->xcap_params,
				&vs->uri, package, &flat) == 0)) {
		if (nesting_level > 0) nesting_level--;
		/* it is resource list -> do internal subscription to RLS */
		if (rls_create_internal_subscription(vs,
					&vs->local_subscription_list, flat,
					nesting_level) != 0) {
			ERR("can't create internal subscription\n");
			free_flat_list(flat);
			return -1;
		}
		free_flat_list(flat);
		
		vs->status = subscription_active;
		/* FIXME: rls_authorize_subscription(vs->local_subscription_list); */
	}
	else {
		/* fill QSA subscription data */
		clear_subscription_data(&vs->local_subscription_pres_data);
		vs->local_subscription_pres_data.dst = &rls->notify_mq;
		vs->local_subscription_pres_data.record_id = vs->uri;
		subscriber = rls_get_subscriber(vs->subscription);
		vs->local_subscription_pres_data.subscriber_data = vs;
		if (subscriber) 
			vs->local_subscription_pres_data.subscriber_id = *subscriber;

		/* not RLS record -> do QSA subscription to given package */
		vs->local_subscription_pres = subscribe(vsd->domain, 
				package, &vs->local_subscription_pres_data);
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
		rl_subscription_t *subscription,
		int nesting_level)
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
	(*dst)->changed = 0;
	generate_db_id(&(*dst)->dbid, *dst);

	add_to_vs_list(*dst);

	DBG("created VS %p to %.*s\n", *dst, uri->len, uri->s);
	
	res = create_subscriptions(*dst, nesting_level);
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
	
		/* remove notification messages for given subscription */
		destroy_notifications(vs->local_subscription_pres);
		
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


#include "qsa_interface.h"
#include "pdomain.h"
#include "pa_mod.h"
#include "dlist.h"
#include "auth.h"

#include <presence/qsa.h>
#include <presence/notifier.h>
#include <cds/logger.h>
#include <cds/memory.h>
#include <cds/list.h>
#include <presence/pres_doc.h>

static notifier_domain_t *domain = NULL;
static notifier_t *notifier = NULL;

static qsa_content_type_t *ct_presence_info = NULL;
/*static qsa_content_type_t *ct_pidf_xml = NULL;*/

/* static str_t notifier_name = { s: "pa", len: 2 }; */

static int pa_subscribe(notifier_t *n, qsa_subscription_t *subscription);
static void pa_unsubscribe(notifier_t *n, qsa_subscription_t *subscription);

extern dlist_t* root; /* ugly !!!!! */

int accept_internal_subscriptions = 0;

/* QSA interface initialization */
int pa_qsa_interface_init()
{
	static str presence_info = STR_STATIC_INIT(CT_PRESENCE_INFO);
	static str_t presence_package = { s: "presence", len: 8 };
	
	domain = qsa_get_default_domain();
	if (!domain) {
		ERR("can't register notifier domain\n");
		return -1;	
	}
	/* DBG("QSA (pa) domain: %p\n", domain); */

	notifier = register_notifier(domain, &presence_package, 
			pa_subscribe, pa_unsubscribe, NULL);
	if (!notifier) {
		ERR("can't register notifier\n");
		return -1;	
	}
	
	ct_presence_info = register_content_type(domain, 
			&presence_info, (destroy_function_f)free_presentity_info);
	if (!ct_presence_info) {
		ERR("can't register QSA content type\n");
		return -1;
	}
	else TRACE("PA_CONTENT_TYPE: %p\n", ct_presence_info);
	
	/* DBG("pa_qsa_interface_init(): created notifier %.*s\n", FMT_STR(notifier_name)); */
	return 0;
}

void pa_qsa_interface_destroy()
{
	if (domain && notifier) unregister_notifier(domain, notifier);
	notifier = NULL;
	/* no new qsa subscriptions will be created now - now can be
	 * released all existing ones */
	
	if (domain) qsa_release_domain(domain);
	/* no QSA operations should be done there (don't send 
	 * notification messages, ...) only subscriptions may
	 * be released */
	domain = NULL;
}

/* notifier functions */

static int add_internal_subscription(presentity_t *p, internal_pa_subscription_t *is)
{
	if (is->status != WS_ACTIVE) 
		is->status = authorize_internal_watcher(p, is);
	if (is->status == WS_REJECTED) return -1;
	DOUBLE_LINKED_LIST_ADD(p->first_qsa_subscription, 
			p->last_qsa_subscription, is);

	return 0;
}

internal_pa_subscription_t *create_internal_subscription(qsa_subscription_t *s)
{
	internal_pa_subscription_t *ss = cds_malloc(sizeof(internal_pa_subscription_t));
	if (!ss) return ss;
	ss->subscription = s;
	ss->status = WS_PENDING;
	ss->prev = NULL;
	ss->next = NULL;
	return ss;
}

void free_internal_subscription(internal_pa_subscription_t *is)
{
	if (is) cds_free(is);
}
		
static int pa_subscribe(notifier_t *n, qsa_subscription_t *subscription)
{
	dlist_t *dl;
	presentity_t *p = NULL;
	internal_pa_subscription_t *ss;
	str uid = STR_NULL;
	str *record_id = NULL;
	xcap_query_params_t xcap_params;
	
	if (!accept_internal_subscriptions) return 0; /* do not accept subscriptions */
	
	record_id = get_record_id(subscription);
	if (!record_id) {
		ERR("BUG: subscription to empty record\n");
		return -1;
	}
	
	DBG("SUBSCRIBE to PA for %.*s [%.*s]\n", 
			FMT_STR(*record_id),
			FMT_STR(subscription->package->name));

	if (pres_uri2uid(&uid, record_id) != 0) {
		/* can't convert uri to uid */
		INFO("can't convert URI to UID for internal PA subscription\n");
		return -1;
	}

	/* DBG("SUBSCRIBE to uid: %.*s\n", FMT_STR(uid)); */
	
	dl = root;	/* FIXME: ugly and possibly unsafe (locking needed?) */
	while (dl) {
		/* create new server subscription */
		ss = create_internal_subscription(subscription);
		if (!ss) {
			ERROR_LOG("can't allocate memory for internal pa subscription\n");
			break;
		}
		
		lock_pdomain(dl->d);	
		if (find_presentity_uid(dl->d, &uid, &p) != 0) p = NULL;
		if (!p) {
			memset(&xcap_params, 0, sizeof(xcap_params));
			if (fill_xcap_params) fill_xcap_params(NULL, &xcap_params);
			if (new_presentity(dl->d, record_id, &uid, &xcap_params, &p) < 0) {
				ERR("can't create presentity\n");
			}
		}
		if (p) {
			/* add server subscription to p */
			if (add_internal_subscription(p, ss) == 0) {
				p->flags |= PFLAG_WATCHERINFO_CHANGED;
				notify_internal_watcher(p, ss);
			}
			else {
				/* error adding subscription to p (auth failed, ...) */
				free_internal_subscription(ss);
			}
		}
		unlock_pdomain(dl->d);
		dl = dl->next;
	}

	str_free_content(&uid);
	DBG("finished SUBSCRIBE to PA for %.*s [%.*s]\n", 
			FMT_STR(*record_id),
			FMT_STR(subscription->package->name));
	
	return 0;
}

static void remove_internal_subscription(presentity_t *p, qsa_subscription_t *s)
{	
	internal_pa_subscription_t *ss;
	ss = p->first_qsa_subscription;
	while (ss) {
		if (s == ss->subscription) {
			DOUBLE_LINKED_LIST_REMOVE(p->first_qsa_subscription, p->last_qsa_subscription, ss);
			free_internal_subscription(ss);
			break; /* may be only once */
		}
		ss = ss->next;
	}
}

static void pa_unsubscribe(notifier_t *n, qsa_subscription_t *subscription)
{
	dlist_t *dl;
	presentity_t *p = NULL;
	str uid = STR_NULL;
	str *record_id = NULL;
	
	if (!accept_internal_subscriptions) return; /* do not accept subscriptions */
	
	record_id = get_record_id(subscription);
	if (!record_id) {
		ERR("BUG: unsubscription to empty record\n");
		return;
	}
	
	if (pres_uri2uid(&uid, record_id) != 0) {
		/* can't convert uri to uid */
		ERR("can't convert URI to UID for internal PA unsubscription\n");
		return;
	}
	
	/* DBG("UNBSCRIBE from PA for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name)); */

	dl = root;	/* FIXME: ugly and possibly unsafe (locking needed?) */
	while (dl) {
		lock_pdomain(dl->d);	
		if (find_presentity_uid(dl->d, &uid, &p) != 0) p = NULL;
		if (!p) continue;
			
		remove_internal_subscription(p, subscription);
		p->flags |= PFLAG_WATCHERINFO_CHANGED;
		
		unlock_pdomain(dl->d);
		dl = dl->next;
	}
	str_free_content(&uid);
}

int notify_internal_watcher(presentity_t *p, internal_pa_subscription_t *ss)
{
	presentity_info_t *pinfo;

	/* notify only accepted watchers */
	switch (ss->status) {
		case WS_PENDING:
				return notify_subscriber(ss->subscription, notifier, 
							ct_presence_info, NULL, qsa_subscription_pending);
		case WS_REJECTED:
				return notify_subscriber(ss->subscription, notifier, 
							ct_presence_info, NULL, qsa_subscription_rejected);

		case WS_PENDING_TERMINATED:
		case WS_TERMINATED:
				return notify_subscriber(ss->subscription, notifier,
							ct_presence_info, NULL, qsa_subscription_terminated);
		case WS_ACTIVE:
				pinfo = dup_presentity_info(&p->data);
				if (!pinfo) {
					ERROR_LOG("can't create presentity info from presentity!\n");
					return -1; 
				}
				
				return notify_subscriber(ss->subscription, notifier, 
						ct_presence_info, pinfo, qsa_subscription_active);
	}

	return 0;
}
	
int notify_qsa_watchers(presentity_t *p)
{
	internal_pa_subscription_t *ss;
	int res = 0;
	
	/* DBG("notify_qsa_watchers for %.*s\n", FMT_STR(p->uri)); */
	ss = p->first_qsa_subscription;
	while (ss) {
		if (notify_internal_watcher(p, ss) < 0) res = -1;
		ss = ss->next;
	}
	return res;
}

int subscribe_to_user(presentity_t *_p)
{
	static str package = STR_STATIC_INIT("presence");
	
	clear_subscription_data(&_p->presence_subscription_data);
	/* ??? FIXME msg queue */ _p->presence_subscription_data.dst = &_p->mq;
	_p->presence_subscription_data.record_id = _p->data.uri;
	_p->presence_subscription_data.subscriber_id = pa_subscription_uri;
	_p->presence_subscription_data.subscriber_data = _p;
	_p->presence_subscription = subscribe(domain, 
			&package, &_p->presence_subscription_data);

	if (_p->presence_subscription) return 0;
	else return -1;
}

int unsubscribe_to_user(presentity_t *_p)
{
	unsubscribe(domain, _p->presence_subscription);
	/* TODO ? clean messages ? they will be freed automaticaly ... */
	return 0;
}


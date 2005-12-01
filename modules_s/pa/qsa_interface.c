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

static str_t notifier_name = { s: "pa", len: 2 };
static str_t presence_package = { s: "presence", len: 8 };

static int pa_subscribe(notifier_t *n, subscription_t *subscription);
static void pa_unsubscribe(notifier_t *n, subscription_t *subscription);

extern dlist_t* root; /* ugly !!!!! */


/* QSA interface initialization */
int pa_qsa_interface_init()
{
	domain = qsa_get_default_domain();
	if (!domain) {
		LOG(L_ERR, "pa_qsa_interface_init(): can't register notifier domain\n");
		return -1;	
	}
	/* DEBUG_LOG("QSA (pa) domain: %p\n", domain); */

	notifier = register_notifier(domain, &presence_package, 
			pa_subscribe, pa_unsubscribe, NULL);
	if (!notifier) {
		LOG(L_ERR, "pa_qsa_interface_init(): can't register notifier\n");
		return -1;	
	}
	/* DEBUG_LOG("pa_qsa_interface_init(): created notifier %.*s\n", FMT_STR(notifier_name)); */
	return 0;
}

void pa_qsa_interface_destroy()
{
	if (domain && notifier) unregister_notifier(domain, notifier);
	notifier = NULL;
	/* no new qsa subscriptions will be created now - now can be
	 * released all existing ones */
	
	qsa_release_domain(domain);
	/* no QSA operations should be done there (don't send 
	 * notification messages, ...) only subscriptions may
	 * be released */
	domain = NULL;
}

/* notifier functions */
static int create_presentity_ex(pdomain_t *_d, str_t *_puri, presentity_t **_p)
{
	if (new_presentity(_d, _puri, _p) < 0) {
		LOG(L_ERR, "create_presentity_only(): Error while creating presentity\n");
		return -2;
	}

	/*(*_p)->flags |= PFLAG_PRESENCE_CHANGED; */

	return 0;
}

static int add_internal_subscription(presentity_t *p, internal_pa_subscription_t *is)
{
	if (is->status != WS_ACTIVE) 
		is->status = authorize_internal_watcher(p, is);
	if (is->status == WS_REJECTED) return -1;
	DOUBLE_LINKED_LIST_ADD(p->first_qsa_subscription, 
			p->last_qsa_subscription, is);

	return 0;
}

internal_pa_subscription_t *create_internal_subscription(subscription_t *s)
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
		
static int pa_subscribe(notifier_t *n, subscription_t *subscription)
{
	dlist_t *dl;
	presentity_t *p = NULL;
	internal_pa_subscription_t *ss;
	
	DEBUG_LOG("SUBSCRIBE to PA for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name));

	dl = root;	/* FIXME: ugly and possibly unsafe (locking needed?) */
	while (dl) {
		/* create new server subscription */
		ss = create_internal_subscription(subscription);
		if (!ss) {
			ERROR_LOG("can't allocate memory for internal pa subscription\n");
			break;
		}
		
		lock_pdomain(dl->d);	
		if (find_presentity(dl->d, &subscription->record_id, &p) != 0) p = NULL;
		if (!p) {
			DEBUG_LOG("creating presentity\n");
			if (create_presentity_ex(dl->d, &subscription->record_id, &p) < 0) {
				ERROR_LOG("can't create presentity\n");
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
	return 0;
}

static void remove_internal_subscription(presentity_t *p, subscription_t *s)
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

static void pa_unsubscribe(notifier_t *n, subscription_t *subscription)
{
	dlist_t *dl;
	presentity_t *p = NULL;
	
	/* DEBUG_LOG("UNBSCRIBE from PA for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name)); */

	dl = root;	/* FIXME: ugly and possibly unsafe (locking needed?) */
	while (dl) {
		lock_pdomain(dl->d);	
		if (find_presentity(dl->d, &subscription->record_id, &p) != 0) p = NULL;
		if (!p) continue;
			
		remove_internal_subscription(p, subscription);
		
		unlock_pdomain(dl->d);
		dl = dl->next;
	}
}

presentity_info_t *presentity2presentity_info(presentity_t *p)
{
	presentity_info_t *pinfo;
	presence_tuple_info_t *tinfo;
	presence_tuple_status_t s;
	presence_tuple_t *t;

	/* DEBUG_LOG("p2p_info()\n"); */
	if (!p) return NULL;
/*	pinfo = (presentity_info_t*)cds_malloc(sizeof(*pinfo)); */
	pinfo = create_presentity_info(&p->uri);
	if (!pinfo) {
		ERROR_LOG("can't allocate memory\n");
		return NULL;
	}
	/* DEBUG_LOG("p2p_info(): created presentity info\n"); */

	t = p->tuples;
	while (t) {
		s = presence_tuple_open;
		if (t->state == PS_OFFLINE) s = presence_tuple_closed;
		tinfo = create_tuple_info(&t->contact, s);
		if (!tinfo) {
			ERROR_LOG("can't create tuple info\n");
			break;
		}
		add_tuple_info(pinfo, tinfo);
		t = t->next;
	}
	/* DEBUG_LOG("p2p_info() finished\n"); */
	return pinfo;
}

int notify_internal_watcher(presentity_t *p, internal_pa_subscription_t *ss)
{
	client_notify_info_t *info;
	mq_message_t *msg;
	presentity_info_t *pinfo;

	/* notify only accepted watchers */
	if ((ss->status == WS_PENDING) || (ss->status == WS_REJECTED)) 
		return 1;
	
	pinfo = presentity2presentity_info(p);
	if (!pinfo) {
		ERROR_LOG("can't create presentity info from presentity!\n");
		return -1; 
	}
	
	msg = create_message_ex(sizeof(client_notify_info_t));
	if (!msg) {
		ERROR_LOG("can't create notify message!\n");
		free_presentity_info(pinfo);
		return -1; 
	}
	set_data_destroy_function(msg, (destroy_function_f)free_client_notify_info_content);
	info = (client_notify_info_t*)msg->data;

	str_dup(&info->package, &ss->subscription->package->name); /* ? */
	str_dup(&info->record_id, &p->uri);
	str_dup(&info->notifier, &notifier_name);
	info->data = pinfo;
	info->data_len = sizeof(*pinfo);
	info->destroy_func = (destroy_function_f)free_presentity_info;

	/* push_message(ss->subscription->dst, msg); */
	notify_subscriber(ss->subscription, msg);
	
	return 0;
}
	
int notify_qsa_watchers(presentity_t *p)
{
	internal_pa_subscription_t *ss;
	int res = 0;
	
	/* DEBUG_LOG("notify_qsa_watchers for %.*s\n", FMT_STR(p->uri)); */
	ss = p->first_qsa_subscription;
	while (ss) {
		if (notify_internal_watcher(p, ss) < 0) res = -1;
		ss = ss->next;
	}
	return res;
}


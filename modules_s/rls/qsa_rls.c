/* code for handling internal list subscriptions */

#include "qsa_rls.h"
#include "rls_mod.h"
#include "../../timer.h"

#include <presence/qsa.h>
#include <presence/notifier.h>
#include <cds/logger.h>
#include <cds/memory.h>
#include <cds/list.h>
#include <presence/pres_doc.h>

static notifier_domain_t *domain = NULL;
static notifier_t *notifier = NULL;

rls_internal_data_t *rls_internal_data = NULL;

/* static str_t notifier_name = { s: "rls", len: 3 }; */
static str_t pres_list_package = { s: "presence.list", len: 13 };

static int rls_subscribe(notifier_t *n, subscription_t *subscription);
static void rls_unsubscribe(notifier_t *n, subscription_t *subscription);

static void process_subscription(subscription_t *subscription);
static void process_unsubscription(subscription_t *subscription);

int is_presence_list_package(const str_t *package)
{
	return (str_case_equals(package, &pres_list_package) == 0);
}

static void rls_timer_cb(unsigned int ticks, void *param)
{
	mq_message_t *m;
	internal_subscription_msg_t *im;
	
	if (!rls_internal_data) return;
	
	/* process messages in internal_subscriptions queue */
	while ((m = pop_message(&rls_internal_data->s_msgs)) != NULL) {
		im = (internal_subscription_msg_t*)get_message_data(m);
		if (im) {
			switch (im->action) {
				case internal_subscribe:
					process_subscription(im->s);
					break;
				case internal_unsubscribe:
					process_unsubscription(im->s);
					break;
			}
		}
		free_message(m);
	}
}

/* QSA interface initialization */
int rls_qsa_interface_init()
{
	domain = qsa_get_default_domain();
	if (!domain) {
		LOG(L_ERR, "rls_qsa_interface_init(): can't register notifier domain\n");
		return -1;	
	}

	notifier = register_notifier(domain, &pres_list_package, 
			rls_subscribe, rls_unsubscribe, NULL);
	if (!notifier) {
		LOG(L_ERR, "rls_qsa_interface_init(): can't register notifier\n");
		return -1;	
	}

	rls_internal_data = (rls_internal_data_t*)shm_malloc(sizeof(rls_internal_data_t));
	if (!rls_internal_data) {
		LOG(L_ERR, "rls_qsa_interface_init(): can't allocate memory for internal rls_internal_data\n");
		return -1;
	}
	
	if (msg_queue_init(&rls_internal_data->s_msgs) != 0) {
		LOG(L_ERR, "rls_qsa_interface_init(): can't initialize internal queue\n");
		return -1;
	}
	rls_internal_data->first = NULL;
	rls_internal_data->last = NULL;

	/* initialize timer for processing internal subscriptions from message queue */
	if (register_timer(rls_timer_cb, NULL, 1) < 0) {
		LOG(L_ERR, "rls_qsa_interface_init(): can't register timer\n");
		return -1;
	}
	
	return 0;
}

void rls_qsa_interface_destroy()
{
	rls_internal_data_t *d;
	
	if (domain && notifier) unregister_notifier(domain, notifier);
	notifier = NULL;
	/* no new qsa subscriptions will be created now - now can be
	 * released all existing ones */
	
	qsa_release_domain(domain);
	/* no QSA operations should be done there (don't send 
	 * notification messages, ...) only subscriptions may
	 * be released */
	domain = NULL;

	d = rls_internal_data;
	rls_internal_data = NULL;
	/* FIXME: may be problems with timer? */
	msg_queue_destroy(&d->s_msgs);
	shm_free(d);
}

static int rls_subscribe(notifier_t *n, subscription_t *subscription)
{
	mq_message_t *msg;
	internal_subscription_msg_t *im;
	
	if ((!subscription) || (!rls_internal_data)) return -1;
	
/*	DEBUG_LOG("internal subscribe to RLS for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name));
	*/
	/* subscriptions MUST be processed asynchronously due to
	 * locking the same mutex in subscriber and NOTIFIER 
	 * (RLS subscribes to itself) !!! */
	
	accept_subscription(subscription); /* must be released if not needed ! */
	
	msg = create_message_ex(sizeof(internal_subscription_msg_t));
	if (msg) {
		im = (internal_subscription_msg_t*)get_message_data(msg);
		im->s = subscription;
		im->action = internal_subscribe;
		push_message(&rls_internal_data->s_msgs, msg);
	}
	
	return 0;
}

static void rls_unsubscribe(notifier_t *n, subscription_t *subscription)
{
	mq_message_t *msg;
	internal_subscription_msg_t *im;
	
	if ((!subscription) || (!rls_internal_data)) return;
	
/*	DEBUG_LOG("internal unsubscribe to RLS for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name));*/
	
	/* subscriptions MUST be processed asynchronously due to
	 * locking the same mutex in subscriber and NOTIFIER 
	 * (RLS subscribes to itself) !!!*/
	
	msg = create_message_ex(sizeof(internal_subscription_msg_t));
	if (msg) {
		im = (internal_subscription_msg_t*)get_message_data(msg);
		im->s = subscription;
		im->action = internal_unsubscribe;
		push_message(&rls_internal_data->s_msgs, msg);
	}
}

/* ----------------- Internal implementation -------------- */

static void process_subscription(subscription_t *subscription)
{
	rl_subscription_t *rls;
	char *xcap_root = "http://localhost/simulated-xcap"; /* FIXME: testing only!!! */
	
	/* try to make subscription and release it if internal subscription 
	 * not created */
	
	/* DEBUG_LOG("*** processing INTERNAL RLS subscription ***\n"); */
	
	/* FIXME: test if no such subscription exists yet (possibility of cyclus) */

	rls_lock();

	rls = rls_alloc_subscription(rls_internal_subscription);
	if (!rls) {
		DEBUG_LOG("processing INTERNAL RLS subscription - memory allocation error\n");
		release_subscription(subscription);
		rls_unlock();
		return;
	}

	rls->internal.s = subscription;
	DOUBLE_LINKED_LIST_ADD(rls_internal_data->first, 
			rls_internal_data->last, &rls->internal);

	if (create_virtual_subscriptions(rls, xcap_root) != 0) {
		/* DEBUG_LOG("*** INTERNAL RLS subscription not for list - removing it ***\n"); */
		rls_free(rls);
		rls_unlock();
		return;
	}
	
	DEBUG_LOG("*** added INTERNAL RLS subscription to %.*s (%p)***\n", 
			FMT_STR(subscription->record_id), subscription);
	
	rls_unlock();
}

static void process_unsubscription(subscription_t *subscription)
{
	rl_subscription_t *rls = NULL;
	internal_rl_subscription_t *is = NULL;
	
	/* verify if subscription is processed and release it if yes */
	rls_lock();
	DEBUG_LOG("*** processing INTERNAL RLS unsubscription (%p) ***\n", 
			subscription);

	/* find the subscription - FIXME: use hashing? */
	is = rls_internal_data->first;
	while (is) {
		if (is->s == subscription) {
			rls = is->rls;
			break;
		}
		is = is->next;
	}

	if (rls) rls_free(rls);
		/* release_internal_subscription(rls); */
	
	rls_unlock();
}

void release_internal_subscription(rl_subscription_t *s)
{
	if (!s) return;
	if (s->type != rls_internal_subscription) return;
	if (!s->internal.s) return;

	DOUBLE_LINKED_LIST_REMOVE(rls_internal_data->first, rls_internal_data->last, &s->internal);

	release_subscription(s->internal.s);
	s->internal.s = NULL;
	s->internal.prev = NULL;
	s->internal.next = NULL;
}


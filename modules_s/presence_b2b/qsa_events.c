#include "qsa_events.h"

#include <presence/qsa.h>
#include <presence/subscriber.h>
#include <presence/notifier.h>
#include <cds/vector.h>
#include <cds/ptr_vector.h>
#include "euac_internals.h"
#include <cds/hash_table.h>
#include <cds/list.h>
#include <presence/pres_doc.h>
#include <presence/pidf.h>

/* typedef struct {
	subscription_t *s;
	enum { internal_subscribe, internal_unsubscribe } action;
} internal_subscription_msg_t;

typedef struct {
	msg_queue_t s_msgs;
	internal_rl_subscription_t *first, *last;
} rls_internal_data_t; */

static notifier_domain_t *domain = NULL;
static notifier_t *presence_notifier = NULL;

/* package for SIP subscriptions */
static str presence_events_package = { s: "presence", len: 8 };
/* package for internal subscriptions */
static str presence_qsa_package = { s: "presence", len: 8 };
/* b2b UA notifier name*/
static str notifier_name = STR_STATIC_INIT("presence_b2b");

/* default route for presence UAC */
str presence_route = {s: "", len: 0 };

typedef struct _events_subscription_t {
	events_uac_t *uac; /* SIP subscription */
	subscription_t *internal_subscription;
	struct _events_subscription_t *next,  *prev;
} events_subscription_t;

typedef struct {
	events_subscription_t *presence_subscriptions_first, *presence_subscriptions_last;
	hash_table_t presence_ht;
	cds_mutex_t mutex;
} event_qsa_internals_t;

static event_qsa_internals_t *internals = NULL;

void lock_events_qsa()
{
	if (internals) cds_mutex_lock(&internals->mutex);
}

void unlock_events_qsa()
{
	if (internals) cds_mutex_unlock(&internals->mutex);
}

/* ************** presence operations *************** */

static events_subscription_t *find_presence_subscription(subscription_t *subscription)
{
	events_subscription_t *es;
	
/*	ERR("searching ES %.*s, %.*s\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->subscriber_id));*/
	
	es = (events_subscription_t *)ht_find(&internals->presence_ht, subscription);

	return es;
}

static void presence_notification_cb(struct sip_msg *m, void *param)
{
	subscription_t *subscription = (subscription_t*)param;
	char *body;
	int len = 0;
	presentity_info_t *p = NULL;
	mq_message_t *msg;
	client_notify_info_t *info;
	
	if (!subscription) return;
	DBG("received notification for %.*s\n", 
			FMT_STR(subscription->record_id));
	
	/* get body */
	body = get_body(m);
	len = strlen(body); /* FIXME: use content-lenght instead */
		
	/* parse as PIDF if given */
	if (len > 0) {
		if (parse_pidf_document(&p, body, len) != 0) {
			ERR("can't parse PIDF document\n");
			return;
		}
	}

	/* create message */
	msg = create_message_ex(sizeof(client_notify_info_t));
	if (!msg) {
		ERR("can't create notify message!\n");
		free_presentity_info(p);
		return; 
	}

	set_data_destroy_function(msg, (destroy_function_f)free_client_notify_info_content);
	info = (client_notify_info_t*)msg->data;

	str_dup(&info->package, &subscription->package->name);
	str_dup(&info->record_id, &subscription->record_id);
	str_dup(&info->notifier, &notifier_name);
	info->data = p;
	info->data_len = sizeof(*p);
	info->destroy_func = (destroy_function_f)free_presentity_info;
	
	/* send the message to internal subscriber */
	notify_subscriber(subscription, msg);
}

static events_subscription_t *create_presence_subscription(subscription_t *subscription)
{
	events_subscription_t *es;
	
	es = (events_subscription_t*) shm_malloc(sizeof(*es));
	if (!es) {
		ERR("can't allocate memory\n");
		return es;
	}
	
	memset(es, 0, sizeof(*es));

	es->internal_subscription = subscription;
	es->uac = create_events_uac(&subscription->record_id, 
			&subscription->subscriber_id, 
			&presence_events_package, 
			presence_notification_cb, /* callback function */
			subscription, /* parameter for callback */
			NULL, /* additional headers */
			&presence_route);
	
/*	ERR("created new ES (%p, uac = %p) %.*s, %.*s\n", es, 
			es->uac,
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->subscriber_id));
*/	
	return es;
}

static void add_presence_subscription(events_subscription_t *es)
{
	ht_add(&internals->presence_ht, es->internal_subscription, es);
	DOUBLE_LINKED_LIST_ADD(internals->presence_subscriptions_first,
			internals->presence_subscriptions_last, es);
}

static void destroy_presence_subscription(events_subscription_t *es)
{
	ht_remove(&internals->presence_ht, es->internal_subscription);
	DOUBLE_LINKED_LIST_REMOVE(internals->presence_subscriptions_first,
			internals->presence_subscriptions_last, es);
	shm_free(es);
}

static int presence_subscribe(notifier_t *n, subscription_t *subscription)
{
	events_subscription_t *es;

/*	ERR("internal subscribe to presence_b2b for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name));
*/
	lock_events_qsa();
	
	es = create_presence_subscription(subscription);
	if (!es) {
		ERR("can't create subscription\n");
		unlock_events_qsa();
		return -1;
	}
	add_presence_subscription(es);

	unlock_events_qsa();
	
	return 0;
}

static void presence_unsubscribe(notifier_t *n, subscription_t *subscription)
{
	events_subscription_t *es;
	
/*	ERR("internal unsubscribe to presence_b2b for %.*s [%.*s]\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->package->name));
*/
	lock_events_qsa();
	
	/* try to find internal structure with this record, subscriber */
	es = find_presence_subscription(subscription);
	if (!es) {
		/* subscription doesn't exist */
		INFO("unsubscribe to nonexisting ES %.*s, %.*s\n", 
			FMT_STR(subscription->record_id),
			FMT_STR(subscription->subscriber_id));
		unlock_events_qsa();
		return;
	}
	
	/* destroy SIP subscription */
	destroy_events_uac(es->uac);
		
	/* destroy this events subscription */
	destroy_presence_subscription(es);
	
	unlock_events_qsa();
}

/************************************************************/
/* initialization / destruction + helper functions for that */

static unsigned int hash_events_subscription(subscription_t *s)
{
	if (s) return rshash((char *)&s, sizeof(s));
	else return 0;
}

static int cmp_events_subscription(subscription_t *a, subscription_t *b)
{
	if (a == b) return 0;
	else return 1;
}

int events_qsa_interface_init()
{
	domain = qsa_get_default_domain();
	if (!domain) {
		ERR("can't register notifier domain\n");
		return -1;	
	}

	presence_notifier = register_notifier(domain, 
			&presence_qsa_package, 
			presence_subscribe, presence_unsubscribe, 
			NULL);
	if (!presence_notifier) {
		ERR("can't register notifier for presence\n");
		return -1;	
	}

	internals = (event_qsa_internals_t*)shm_malloc(sizeof(*internals));
	if (!internals) {
		ERR("can't allocate memory\n");
		return -1;
	}

	memset(internals, 0, sizeof(*internals));
	cds_mutex_init(&internals->mutex);
	ht_init(&internals->presence_ht, (hash_func_t)hash_events_subscription, 
			(key_cmp_func_t)cmp_events_subscription, 16603);
	
	return 0;
}

void events_qsa_interface_destroy()
{
	if (domain) {
		if (presence_notifier) unregister_notifier(domain, presence_notifier);
	}
	presence_notifier = NULL;
	/* no new qsa subscriptions will be created now - now can be
	 * released all existing ones */
	
	if (domain) qsa_release_domain(domain);
	/* no QSA operations should be done there (don't send 
	 * notification messages, ...) only subscriptions may
	 * be released */
	domain = NULL;

	if (internals) {
		/* TODO: destroy all remaining subscriptions */
		cds_mutex_destroy(&internals->mutex);
		ht_destroy(&internals->presence_ht);
		shm_free(internals);
		internals = NULL;
	}
}


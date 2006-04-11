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
static int create_presentity_ex(pdomain_t *_d, str_t *_puri, str_t *uid, presentity_t **_p)
{
	if (new_presentity(_d, _puri, uid, _p) < 0) {
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
			if (create_presentity_ex(dl->d, record_id, &uid, &p) < 0) {
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

int copy_tuple_notes(presence_tuple_info_t *dst_info, const presence_tuple_t *src)
{
	presence_note_t *n, *nn;

	n = src->notes;
	while (n) {
		nn = create_presence_note(&n->value, &n->lang);
		if (!nn) {
			ERR("can't create presence note\n");
			return -1;
		}
		DOUBLE_LINKED_LIST_ADD(dst_info->first_note, dst_info->last_note, nn);
		n = n->next;
	}
	return 0;
}

presentity_info_t *presentity2presentity_info(presentity_t *p)
{
	presentity_info_t *pinfo;
	presence_tuple_info_t *tinfo;
	presence_tuple_status_t s;
	presence_tuple_t *t;
	pa_presence_note_t *pan;
	presence_note_t *n;
	pa_person_element_t *paps;
	person_t *ps, *last_ps;
	int err = 0;

	/* DBG("p2p_info()\n"); */
	if (!p) return NULL;
/*	pinfo = (presentity_info_t*)cds_malloc(sizeof(*pinfo)); */
	pinfo = create_presentity_info(&p->uri);
	if (!pinfo) {
		ERROR_LOG("can't allocate memory\n");
		return NULL;
	}
	/* DBG("p2p_info(): created presentity info\n"); */

	t = p->tuples;
	while (t) {
		s = presence_tuple_open;
		if (t->state == PS_OFFLINE) s = presence_tuple_closed;
		tinfo = create_tuple_info(&t->contact, &t->id, s);
		if (!tinfo) {
			ERROR_LOG("can't create tuple info\n");
			err = 1;
			break;
		}
		tinfo->priority = t->priority;
		tinfo->expires = t->expires;
		add_tuple_info(pinfo, tinfo);
		if (copy_tuple_notes(tinfo, t) < 0) {
			ERROR_LOG("can't copy tuple notes\n");
			err = 1;
			break;
		}
		t = t->next;
	}

	/* notes */
	if (!err) {
		pan = p->notes;
		while (pan) {
			n = create_presence_note(&pan->note, &pan->lang);
			if (n) DOUBLE_LINKED_LIST_ADD(pinfo->first_note, pinfo->last_note, n);
			else {
				ERROR_LOG("can't copy presence notes\n");
				err = 1;
				break;
			}
			pan = pan->next;
		}
	}
	
	/* person elements */
	if (!err) {
		last_ps = NULL;
		paps = p->person_elements;
		while (paps) {
			ps = create_person(&paps->person, &paps->id);
			if (ps) LINKED_LIST_ADD(pinfo->first_person, last_ps, ps);
			else {
				ERROR_LOG("can't copy person elements\n");
				err = 1;
				break;
			}
			paps = paps->next;
		}
	}
	
	if (err) {
		free_presentity_info(pinfo);
		return NULL;
	}
	
	/* DBG("p2p_info() finished\n"); */
	return pinfo;
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
				pinfo = presentity2presentity_info(p);
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


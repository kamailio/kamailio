/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/sync.h>
#include <stdio.h>
#include <presence/notifier_domain.h>
#include <presence/notifier.h>
#include <presence/subscriber.h>
#include <cds/list.h>

static void free_notifier(notifier_t *info);
static void free_subscription(subscription_t *s);

/* -------- package functions -------- */

static notifier_package_t *find_package(notifier_domain_t *d, const str_t *name)
{
	notifier_package_t *p;

	if (!d) return NULL;
	p = d->first_package;
	while (p) {
		if (str_case_equals(name, &p->name) == 0) return p;
		p = p->next;
	}
	return NULL;
}

static void add_package(notifier_domain_t *d, notifier_package_t *p)
{
	p->domain = d;
	DOUBLE_LINKED_LIST_ADD(d->first_package, d->last_package, p);
}

static notifier_package_t *create_package(const str_t *name)
{
	notifier_package_t *p = (notifier_package_t*)cds_malloc(sizeof(notifier_package_t));
	if (p) {
		p->first_subscription = NULL;
		p->last_subscription = NULL;
		p->first_notifier = NULL;
		p->last_notifier = NULL;
		p->next = NULL;
		p->prev = NULL;
		p->domain = NULL;
		str_dup(&p->name, name);
	}
	return p;
}

/** finds existing package or adds new if not exists */
static notifier_package_t *get_package(notifier_domain_t *d, const str_t *name)
{
	notifier_package_t *p = NULL;
	
	if (is_str_empty(name)) return NULL;
	
	p = find_package(d, name);
	if (!p) {
		p = create_package(name);
		if (p) add_package(d, p);
	}
	return p;
}
	
static void destroy_package(notifier_package_t *p) 
{
	notifier_t *e, *n;
	subscription_t *s, *ns;
	
	/* release all subscriptions  */
	s = p->first_subscription;
	while (s) {
		ns = s->next;
		/* unsubscribe(p->domain, s) */
		/* release_subscription(s); */
		free_subscription(s);
		s = ns;
	}
	
	/* release all registered notifiers */
	e = p->first_notifier;
	while (e) {
		n = e->next;
		free_notifier(e);
		/* maybe: call some notifier callback ? */
		e = n;
	}
	p->first_notifier = NULL;
	p->last_notifier = NULL;
	str_free_content(&p->name);

	cds_free(p);
}

/* -------- Helper functions -------- */

static void free_notifier(notifier_t *info)
{
	cds_free(info);
}

static void free_subscription(subscription_t *s)
{
	str_free_content(&s->record_id);
	str_free_content(&s->subscriber_id);
	cds_free(s);
}

/*static void add_server_subscription(notifier_t *n, subscription_t *s)
{
	server_subscription_t server_s;
	
	server_s.notifier_data = NULL;
	if (n->subscribe(n, &s->record_id, s, &server_s.notifier_data) == 0) {
		server_s.notifier = n;
		vector_add(&s->server_subscriptions, &server_s);
	}
	else ERROR_LOG("subscription not accepted by notifier %p\n", n);
}
			
static void remove_notifier_from_subscription(subscription_t *s, notifier_t *n)
{
	int cnt,i;

	cnt = vector_size(&s->server_subscriptions);
	for (i = 0; i < cnt; i++) {
		ss = vector_get_ptr(&s->server_subscriptions, i);
		if (!ss) continue;
		/ * FIXME: call n->unsubsribe ??? 
		 * NO this is called from unregister which is initiated
		 * by the notifier (may be synchronized there!) * /
		if (ss->notifier == n) ss->notifier = NULL; / * "zombie" * /
	}
}
*/

/* -------- Domain initialization/destruction functions -------- */

/** Creates a new domain using cds memory functions. */
notifier_domain_t *create_notifier_domain(const str_t *name)
{
	notifier_domain_t *d = (notifier_domain_t*)cds_malloc(sizeof(notifier_domain_t));
	if (d) {
		d->first_package = NULL;
		d->last_package = NULL;
		str_dup(&d->name, name);
		cds_mutex_init(&d->mutex);
	}
	return d;
}

/** Destroys domain and whole information stored in internal
 * structures. If there are any subscribers, they are unsubscribed,
 * if there are any notifiers, they are unregistered. */
void destroy_notifier_domain(notifier_domain_t *domain)
{
	notifier_package_t *p, *n;
	
	lock_notifier_domain(domain);
	
	/* remove packages */
	p = domain->first_package;
	while (p) {
		n = p->next;
		destroy_package(p);
		p = n;
	}
	domain->first_package = NULL;
	domain->last_package = NULL;
	
	unlock_notifier_domain(domain);
	
	str_free_content(&domain->name);
	cds_mutex_destroy(&domain->mutex);
	cds_free(domain);
}

/* -------- Notifier functions -------- */

/* Returns the id of created notifier. Indicates error if less than 0 */
notifier_t *register_notifier(
	notifier_domain_t *domain,
	const str_t *package,
	server_subscribe_func subscribe,
	server_unsubscribe_func unsubscribe,
	void *user_data)
{
	notifier_t *info;
	notifier_package_t *p;
	subscription_t *s;

	lock_notifier_domain(domain);
	p = get_package(domain, package);
	if (!p) {
		unlock_notifier_domain(domain);
		return NULL;
	}
		
	info = cds_malloc(sizeof(notifier_t));
	if (!info) return info;

	info->subscribe = subscribe;
	info->unsubscribe = unsubscribe;
	info->user_data = user_data;
	info->package = p;
	DEBUG_LOG("registered notifier for %.*s\n", FMT_STR(*package));

	DOUBLE_LINKED_LIST_ADD(p->first_notifier, p->last_notifier, info);
	
	/* go through all subscriptions for package and 
	 * add them to this notifier */
	s = p->first_subscription;
	while (s) {
		info->subscribe(info, s);
		s = s->next;
	}
	
	unlock_notifier_domain(domain);
	
	return info;
}

void unregister_notifier(notifier_domain_t *domain, notifier_t *info)
{
	notifier_package_t *p;

	/* maybe: test if the NOTIFIER is registered before unregistration */

	lock_notifier_domain(domain);
	
	p = info->package;
	if (p) {
		/* subscription_t *s;
		s = p->first_subscription;
		while (s) {
			info->unsubscribe(info, s);
			s = s->next;
		}*/

		DOUBLE_LINKED_LIST_REMOVE(p->first_notifier, p->last_notifier, info);
		/* DEBUG_LOG("UNregistered notifier for %.*s\n", FMT_STR(p->name)); */
		free_notifier(info);
	}
	unlock_notifier_domain(domain);
}

/* -------- Subscriber functions -------- */

/* If a notifier publishing watched state registeres after subscibe
 * call, it receives the subscription automaticaly too! */
subscription_t *subscribe(notifier_domain_t *domain, 
		str_t *package,
		str_t *record_id,
		str_t *subscriber_id,
		msg_queue_t *dst,
		void *subscriber_data)
{
	subscription_t *s;
	notifier_t *e;
	notifier_package_t *p;
	int cnt = 0;

	lock_notifier_domain(domain);
	p = get_package(domain, package);
	if (!p) {
		ERROR_LOG("can't find package for subscription\n");
		unlock_notifier_domain(domain);
		return NULL;
	}
	
	s = cds_malloc(sizeof(subscription_t));
	if (!s) {
		ERROR_LOG("can't allocate memory\n");
		return s;
	}

	s->package = p;
	s->dst = dst;
	s->subscriber_data = subscriber_data;
	str_dup(&s->record_id, record_id);
	str_dup(&s->subscriber_id, subscriber_id);

	DOUBLE_LINKED_LIST_ADD(p->first_subscription, p->last_subscription, s);

	/* browse all notifiers in given package and subscribe to them
	 * and add them to notifiers list */
	cnt = 0;
	e = p->first_notifier;
	while (e) {
		cnt++;
		e->subscribe(e, s);
		e = e->next;
	}
	unlock_notifier_domain(domain);
	DEBUG_LOG("subscribed to %d notifier(s)\n", cnt);
	
	return s;
}

/** Destroys an existing subscription */
void unsubscribe(notifier_domain_t *domain, subscription_t *s)
{
	notifier_package_t *p;
	notifier_t *e;

	lock_notifier_domain(domain);
	
	/* maybe: test if the SUBSCRIBER is subscribed before unsubsc. */
	p = s->package;
	if (!p) {
		unlock_notifier_domain(domain);
		return;
	}

	DOUBLE_LINKED_LIST_REMOVE(p->first_subscription, p->last_subscription, s);
	
	e = p->first_notifier;
	while (e) {
		e->unsubscribe(e, s);
		e = e->next;
	}
	
	unlock_notifier_domain(domain);
	
	free_subscription(s);
}


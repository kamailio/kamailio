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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/sync.h>
#include <stdio.h>
#include <presence/notifier_domain.h>
#include <presence/notifier.h>
#include <presence/subscriber.h>
#include <cds/list.h>
#include <cds/cds.h>
#include <string.h>

/*#define lock_subscription_data(s) if (s->mutex) cds_mutex_lock(s->mutex);
#define unlock_subscription_data(s) if (s->mutex) cds_mutex_unlock(s->mutex);*/

static void lock_subscription_data(qsa_subscription_t *s)
{
	/* is function due to debugging */
	if (s->mutex) cds_mutex_lock(s->mutex);
}

static void unlock_subscription_data(qsa_subscription_t *s) {
	/* is function due to debugging */
	if (s->mutex) cds_mutex_unlock(s->mutex);
}

static void free_notifier(notifier_t *info);
static void free_subscription(qsa_subscription_t *s);

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
		if (str_dup(&p->name, name) < 0) {
			cds_free(p);
			ERROR_LOG("can't allocate memory\n");
			return NULL;
		}
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
	/* notifier_t *e, *n; */
	qsa_subscription_t *s, *ns;
	
	/* release all subscriptions ???  */
	s = p->first_subscription;
	while (s) {
		ns = s->next;
		/* CAN NOT be called !!!! : unsubscribe(p->domain, s) */
		release_subscription(s);
		s = ns;
	}
	
	/* !!! don't release notifiers - its their job !!! */
	/* it may lead to errors there */
	/* e = p->first_notifier;
	while (e) {
		n = e->next;
		free_notifier(e);
		e = n;
	} */
	
	p->first_notifier = NULL;
	p->last_notifier = NULL;
	str_free_content(&p->name);

	cds_free(p);
}

/* -------- content type functions -------- */

static qsa_content_type_t *find_content_type(notifier_domain_t *d, const str_t *name)
{
	qsa_content_type_t *p;

	if (!d) return NULL;
	p = d->first_content_type;
	while (p) {
		if (str_case_equals(name, &p->name) == 0) return p;
		p = p->next;
	}
	return NULL;
}

static void add_content_type(notifier_domain_t *d, qsa_content_type_t *p)
{
	DOUBLE_LINKED_LIST_ADD(d->first_content_type, d->last_content_type, p);
}

static qsa_content_type_t *create_content_type(const str_t *name, 
		destroy_function_f destroy_func)
{
	qsa_content_type_t *p = (qsa_content_type_t*)cds_malloc(sizeof(qsa_content_type_t) + str_len(name));
	if (p) {
		p->next = NULL;
		p->prev = NULL;
		p->name.s = p->buf;
		if (str_len(name) > 0) {
			memcpy(p->name.s, name->s, name->len);
			p->name.len = name->len;
		}
		else p->name.len = 0;
		p->destroy_func = destroy_func;
	}
	return p;
}

/** finds existing package or adds new if not exists */
qsa_content_type_t *register_content_type(notifier_domain_t *d, 
		const str_t *name,
		destroy_function_f destroy_func)
{
	qsa_content_type_t *p = NULL;
	
	if (is_str_empty(name)) return NULL;
	
	p = find_content_type(d, name);
	if (!p) {
		p = create_content_type(name, destroy_func);
		if (p) add_content_type(d, p);
	}
	return p;
}
	
static void destroy_content_type(qsa_content_type_t *p) 
{
	cds_free(p);
}

/* -------- Helper functions -------- */

static void free_notifier(notifier_t *info)
{
	cds_free(info);
}

static void free_subscription(qsa_subscription_t *s)
{
	DEBUG_LOG("freeing subscription to %p\n", s);
	cds_free(s);
}

/*static void add_server_subscription(notifier_t *n, qsa_subscription_t *s)
{
	server_subscription_t server_s;
	
	server_s.notifier_data = NULL;
	if (n->subscribe(n, &s->record_id, s, &server_s.notifier_data) == 0) {
		server_s.notifier = n;
		vector_add(&s->server_subscriptions, &server_s);
	}
	else ERROR_LOG("subscription not accepted by notifier %p\n", n);
}
			
static void remove_notifier_from_subscription(qsa_subscription_t *s, notifier_t *n)
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
notifier_domain_t *create_notifier_domain(reference_counter_group_t *g, const str_t *name)
{
	notifier_domain_t *d = (notifier_domain_t*)cds_malloc(sizeof(notifier_domain_t));
	if (d) {
		d->first_package = NULL;
		d->last_package = NULL;
		d->first_content_type = NULL;
		d->last_content_type = NULL;
		d->rc_grp = g;
		if (str_dup(&d->name, name) < 0) {
			cds_free(d);
			ERROR_LOG("can't allocate memory\n");
			return NULL;
		}
		cds_mutex_init(&d->mutex);
		cds_mutex_init(&d->data_mutex);
		init_reference_counter(g, &d->ref);
	}
	return d;
}

/** Destroys domain and whole information stored in internal
 * structures. If there are any subscribers, they are unsubscribed,
 * if there are any notifiers, they are unregistered. */
void destroy_notifier_domain(notifier_domain_t *domain)
{
	notifier_package_t *p, *n;
	qsa_content_type_t *c, *tmp;

	/* this function is always called only if no only one reference
	 * to domain exists (see domain maintainer), this should mean, that 
	 * all subscribers freed their subscriptions */
	
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
	
	c = domain->first_content_type;
	while (c) {
		tmp = c;
		c = c->next;
		destroy_content_type(tmp);
	}
	domain->first_content_type = NULL;
	domain->last_content_type = NULL;
	
	unlock_notifier_domain(domain);
	
	str_free_content(&domain->name);
	cds_mutex_destroy(&domain->mutex);
	cds_mutex_init(&domain->data_mutex);
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
	qsa_subscription_t *s;

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

	if ((!info) || (!domain)) return;

	/* maybe: test if the NOTIFIER is registered before unregistration */

	lock_notifier_domain(domain);
	
	p = info->package;
	if (p) {
		/* accepted subscriptions MUST be removed by the notifier 
		 * how to solve this ? */
		
		/* qsa_subscription_t *s;
		s = p->first_subscription;
		while (s) {
			CAN NOT be called !!!!! info->unsubscribe(info, s);
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
qsa_subscription_t *subscribe(notifier_domain_t *domain, 
		str_t *package,
		qsa_subscription_data_t *data)
{
	qsa_subscription_t *s;
	notifier_t *e;
	notifier_package_t *p;
	int cnt = 0;

	lock_notifier_domain(domain);
	p = get_package(domain, package);
	if (!p) {
		ERROR_LOG("can't find/add package for subscription\n");
		unlock_notifier_domain(domain);
		return NULL;
	}
	
	s = cds_malloc(sizeof(qsa_subscription_t));
	if (!s) {
		ERROR_LOG("can't allocate memory\n");
		unlock_notifier_domain(domain);
		return s;
	}

	s->package = p;
	s->mutex = &domain->data_mutex;
	s->data = data;
	s->allow_notifications = 1;
	init_reference_counter(domain->rc_grp, &s->ref);

	DOUBLE_LINKED_LIST_ADD(p->first_subscription, p->last_subscription, s);

	/* add a reference for calling subscriber */
	add_reference(&s->ref);
	
	/* browse all notifiers in given package and subscribe to them
	 * and add them to notifiers list */
	cnt = 0;
	e = p->first_notifier;
	while (e) {
		cnt++;
		/* each notifier MUST add its own reference if
		 * it wants to accept the subscription !!! */
		e->subscribe(e, s);
		e = e->next;
	}
	unlock_notifier_domain(domain);
	DEBUG_LOG("subscribed to %d notifier(s)\n", cnt);
	
	return s;
}
	
void release_subscription(qsa_subscription_t *s)
{
	if (!s) return;
	if (remove_reference(&s->ref)) free_subscription(s);
}

void accept_subscription(qsa_subscription_t *s)
{
	if (!s) return;
	add_reference(&s->ref);
}

/** Destroys an existing subscription - can be called ONLY by client !!! */
void unsubscribe(notifier_domain_t *domain, qsa_subscription_t *s)
{
	notifier_package_t *p;
	notifier_t *e;

	/* mark subscription as un-notifyable */
	lock_subscription_data(s);
	s->allow_notifications = 0;
	unlock_subscription_data(s);

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
	
	/* mark subscription data as invalid */
	lock_subscription_data(s);
	s->data = NULL;
	unlock_subscription_data(s);
	
	/* remove clients reference (dont give references to client?) */
	remove_reference(&s->ref);
	
	release_subscription(s); 
}

/* void notify_subscriber(qsa_subscription_t *s, mq_message_t *msg) */
int notify_subscriber(qsa_subscription_t *s, 
		notifier_t *n,
		qsa_content_type_t *content_type, 
		void *data, 
		qsa_subscription_status_t status)
{
	int ok = 1;
	int sent = 0;
	mq_message_t *msg = NULL;
	client_notify_info_t* info = NULL;

	if (!s) {
		ERROR_LOG("BUG: sending notify for <null> subscription\n");
		ok = 0;
	}
	
	if (!content_type) {
		ERROR_LOG("BUG: content type not given! Possible memory leaks!\n");
		return -1;
	}
	
	if (ok) {
		msg = create_message_ex(sizeof(client_notify_info_t));
		if (!msg) {
			ERROR_LOG("can't create notify message!\n");
			ok = 0; 
		}
	}
	
	if (ok) {
		set_data_destroy_function(msg, (destroy_function_f)free_client_notify_info_content);
		info = (client_notify_info_t*)msg->data;
		
		info->subscription = s;
		info->content_type = content_type;
		info->data = data;
		info->status = status;
		
		lock_subscription_data(s);
		if ((s->allow_notifications) && (s->data)) {
			if (s->data->dst) {
				if (push_message(s->data->dst, msg) < 0) ok = 0;
				else sent = 1;
			}
		}
		unlock_subscription_data(s);
	}
	
	if (!sent) {
		/* free unsent messages */
		if (msg) free_message(msg);
		else if (data) content_type->destroy_func(data);
	}

	if (ok) return 0;
	else return 1; /* !!! Warning: data are destroyed !!! */
}

void free_client_notify_info_content(client_notify_info_t *info)
{
	DEBUG_LOG(" ... freeing notify info content\n");
/*	str_free_content(&info->package);
	str_free_content(&info->record_id);
	str_free_content(&info->notifier); */
	DEBUG_LOG(" ... calling destroy func on data\n");
	if (info->content_type) {
		if (info->data) info->content_type->destroy_func(info->data);
	}
	else ERROR_LOG("BUG: content-type not given! Possible memory leaks!\n");
}

/* this can be called in notifier and the returned value is valid
 * before finishes "unsubscribe" processing */
str_t *get_subscriber_id(qsa_subscription_t *s)
{
	if (!s) return NULL;
	if (!s->data) return NULL;
	return &s->data->subscriber_id;
}

/* this can be called in notifier and the returned value is valid
 * before finishes "unsubscribe" processing */
str_t *get_record_id(qsa_subscription_t *s)
{
	if (!s) return NULL;
	if (!s->data) return NULL;
	return &s->data->record_id;
}

/* this can be called in notifier and the returned value is valid
 * before finishes "unsubscribe" processing */
void *get_subscriber_data(qsa_subscription_t *s)
{
	if (!s) return NULL;
	if (!s->data) return NULL;
	return s->data->subscriber_data;
}

void clear_subscription_data(qsa_subscription_data_t *data)
{
	if (data) memset(data, 0, sizeof(*data));
}


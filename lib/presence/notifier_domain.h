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

#ifndef __NOTIFIER_DOMAIN_H
#define __NOTIFIER_DOMAIN_H

#include <cds/sstr.h>
#include <cds/ptr_vector.h>
#include <cds/sync.h>
#include <cds/msg_queue.h>

#include <presence/client_notify_info.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _subscription_t;
typedef struct _subscription_t subscription_t;
struct _notifier_package_t;
typedef struct _notifier_package_t notifier_package_t;
struct _notifier_t;
typedef struct _notifier_t notifier_t;
struct _notifier_domain_t;
typedef struct _notifier_domain_t notifier_domain_t;

/* typedef void (*client_notify_func)(client_notify_info_t *info); */

/** Internal structure holding informations about
 * created client subscriptions.
 */
struct _subscription_t {
	/* client_notify_func notify; */
	msg_queue_t *dst;
	str_t record_id;
	str_t subscriber_id;
	notifier_package_t *package;
	void *subscriber_data;
	struct _subscription_t *prev, *next;
};

typedef int (*server_subscribe_func)(notifier_t *n, subscription_t *subscription);

typedef void (*server_unsubscribe_func)(notifier_t *n, subscription_t *subscription);

/** Internal structure storing registered notifiers. */
struct _notifier_t {
	server_subscribe_func subscribe;
	server_unsubscribe_func unsubscribe;
	void *user_data; /* private data for this notifier */
	notifier_package_t *package;
	struct _notifier_t *prev, *next; 
};

struct _notifier_package_t {
	str_t name;
	/* maybe: serialize and deserialize methods */
	notifier_domain_t *domain;
	notifier_t *first_notifier, *last_notifier; /* notifiers are linked in theirs package! */
	subscription_t *first_subscription, *last_subscription;
	notifier_package_t *next, *prev;
};

struct _notifier_domain_t {
	cds_mutex_t mutex;
	str_t name;
	notifier_package_t *first_package, *last_package;
};

/* -------- Domain initialization/destruction functions -------- */

/** Creates a new domain using cds memory functions. */
notifier_domain_t *create_notifier_domain(const str_t *name);

/** Destroys domain and whole information stored in internal
 * structures. If there are any subscribers, they are unsubscribed,
 * if there are any notifiers, they are unregistered. */
void destroy_notifier_domain(notifier_domain_t *domain);

#define lock_notifier_domain(d) cds_mutex_lock(&(d->mutex))
#define unlock_notifier_domain(d) cds_mutex_unlock(&(d->mutex))

#ifdef __cplusplus
}
#endif


#endif

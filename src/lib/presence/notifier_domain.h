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

#ifndef __NOTIFIER_DOMAIN_H
#define __NOTIFIER_DOMAIN_H

#include <cds/sstr.h>
#include <cds/ptr_vector.h>
#include <cds/sync.h>

#include <presence/qsa_params.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _qsa_subscription_t;
typedef struct _qsa_subscription_t qsa_subscription_t;
struct _notifier_package_t;
typedef struct _notifier_package_t notifier_package_t;
struct _notifier_t;
typedef struct _notifier_t notifier_t;
struct _notifier_domain_t;
typedef struct _notifier_domain_t notifier_domain_t;

/* data hold by subscriber for the time of subscription duration 
 * (from subscribe to unsubscribe; after calling unsubscribe can
 * be destroyed contents of them) */
typedef struct _qsa_subscription_data_t {
	msg_queue_t *dst;
	str_t record_id;
	str_t subscriber_id;
	qsa_subscription_params_t *first_param;
	void *subscriber_data;
} qsa_subscription_data_t;

/** Internal structure holding informations about
 * created client subscriptions.
 */
struct _qsa_subscription_t {
	/* client_notify_func notify; */
	cds_mutex_t *mutex;
	notifier_package_t *package;
	int allow_notifications;
	qsa_subscription_data_t *data;
	struct _qsa_subscription_t *prev, *next;
	reference_counter_data_t ref;
};

/* typedef void (*client_notify_func)(client_notify_info_t *info); */

typedef int (*server_subscribe_func)(notifier_t *n, qsa_subscription_t *subscription);

typedef void (*server_unsubscribe_func)(notifier_t *n, qsa_subscription_t *subscription);

typedef struct _qsa_content_type_t {
	struct _qsa_content_type_t *next, *prev;
	str_t name;
	destroy_function_f destroy_func;
	char buf[1]; /* buffer for name allocation together with the structure */
} qsa_content_type_t;

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
	qsa_subscription_t *first_subscription, *last_subscription;
	notifier_package_t *next, *prev;
};

struct _notifier_domain_t {
	cds_mutex_t mutex;
	cds_mutex_t data_mutex; /* mutex for locking standalone subscription data, may be changed to mutex pool */
	str_t name;
	notifier_package_t *first_package, *last_package;
	qsa_content_type_t *first_content_type, *last_content_type;
	reference_counter_data_t ref;
	reference_counter_group_t *rc_grp;
};

/* -------- Domain initialization/destruction functions -------- */

/** Creates a new domain using cds memory functions. */
notifier_domain_t *create_notifier_domain(reference_counter_group_t *g, const str_t *name);

/** Destroys domain and whole information stored in internal
 * structures. If there are any subscribers, they are unsubscribed,
 * if there are any notifiers, they are unregistered. */
void destroy_notifier_domain(notifier_domain_t *domain);

qsa_content_type_t *register_content_type(notifier_domain_t *d, 
		const str_t *name,
		destroy_function_f destroy_func);

#define lock_notifier_domain(d) cds_mutex_lock(&(d->mutex))
#define unlock_notifier_domain(d) cds_mutex_unlock(&(d->mutex))

#ifdef __cplusplus
}
#endif


#endif

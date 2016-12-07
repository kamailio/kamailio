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

#ifndef __NOTIFIER_H
#define __NOTIFIER_H

/* Notifier functions for notifier_domain */

#include <presence/notifier_domain.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the created notifier. Indicates error if NULL. */
notifier_t *register_notifier(
	notifier_domain_t *domain,
	const str_t *package,
	server_subscribe_func subscribe,
	server_unsubscribe_func unsubscribe,
	void *user_data);

void unregister_notifier(notifier_domain_t *domain, notifier_t *info);

/** accepts subscription (internaly adds reference to it), thus it can 
 * be handled by notifier which called this function 
 * MUST be called in notifier's subscribe function, otherwise the 
 * subscription can NOT be accepted 
 *
 * Note: only for asynchonously processed subscriptions (synchronous 
 * don't need it) */
void accept_subscription(qsa_subscription_t *s);

/** releases accepted subscription - MUST be called on all accepted 
 * subscriptions (only on them!) to be freed from memory !
 * Note: only for asynchonously processed subscriptions (synchronous 
 * don't need it) */
void release_subscription(qsa_subscription_t *s);

/** This structure is sent via message queue
 * to client. It must contain all information
 * for processing the status info. */

typedef enum {
	qsa_subscription_active,
	qsa_subscription_pending,
	qsa_subscription_terminated,
	qsa_subscription_rejected
} qsa_subscription_status_t;

typedef struct {
	/* replacement for record_id, package, ... it is much more efficient */
	qsa_subscription_t *subscription; 
	qsa_content_type_t *content_type;
	void *data;
	int data_len;
	qsa_subscription_status_t status;
} client_notify_info_t;


void free_client_notify_info_content(client_notify_info_t *info);

/* notifications SHOULD be sent through this method */
int notify_subscriber(qsa_subscription_t *s, 
		notifier_t *n,
		qsa_content_type_t *content_type, 
		void *data, 
		qsa_subscription_status_t status);

/* this can be called in notifier and the returned value is valid
 * before finishes "unsubscribe" processing */
str_t *get_subscriber_id(qsa_subscription_t *s);

/* this can be called in notifier and the returned value is valid
 * before finishes "unsubscribe" processing */
str_t *get_record_id(qsa_subscription_t *s);

/* this can be called in notifier and the returned value is valid
 * before finishes "unsubscribe" processing */
void *get_subscriber_data(qsa_subscription_t *s);

#ifdef __cplusplus
}
#endif

#endif

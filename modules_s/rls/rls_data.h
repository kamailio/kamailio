#ifndef __RLS_DATA_H
#define __RLS_DATA_H

#include "rl_subscription.h"

typedef struct {
	/* optimization - when a subscription is flagged as changed,
	 * this number is increased (means something like priority of
	 * call to "change all modified RLS") */
	int changed_subscriptions;
	/* hash, ... */
	msg_queue_t notify_mq;
} rls_data_t;

extern rls_data_t *rls;
extern subscription_manager_t *rls_manager;

void destroy_vs_notifications(virtual_subscription_t *vs);

int rls_init();
int rls_destroy();
void rls_lock();
void rls_unlock();

#endif

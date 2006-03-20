#ifndef __RLS_DATA_H
#define __RLS_DATA_H

#include "rl_subscription.h"

typedef struct {
	rl_subscription_t *first;
	rl_subscription_t *last;
	/* hash, ... */
	msg_queue_t notify_mq;
} rls_data_t;

extern rls_data_t *rls;
extern subscription_manager_t *rls_manager;

void destroy_vs_notifications(virtual_subscription_t *vs);

#endif

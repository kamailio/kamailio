#ifndef __QSA_RLS_H
#define __QSA_RLS_H

#include "rl_subscription.h"

int rls_qsa_interface_init();
void rls_qsa_interface_destroy();
void release_internal_subscription(rl_subscription_t *s);

/* helper functions */
int is_presence_list_package(const str_t *package);

/* folowing structures and data members exported due to tracing */

typedef struct {
	subscription_t *s;
	enum { internal_subscribe, internal_unsubscribe } action;
} internal_subscription_msg_t;

typedef struct {
	msg_queue_t s_msgs;
	internal_rl_subscription_t *first, *last;
} rls_internal_data_t;

extern rls_internal_data_t *rls_internal_data;

#endif

#ifndef __PARSE_SUBSCRIPTION_STATE_H
#define __PARSE_SUBSCRIPTION_STATE_H

#include "hf.h"

typedef enum {
	ss_active,
	ss_pending,
	ss_terminated,
	ss_extension
} substate_value_t;

typedef struct _subscription_state_t {
	substate_value_t value;
	unsigned int expires;
	int expires_set; /* expires is valid if nonzero here */
} subscription_state_t;

int parse_subscription_state(struct hdr_field *h);

void free_subscription_state(subscription_state_t **ss);

#endif

#ifndef __RLS_AUTHORIZE
#define __RLS_AUTHORIZE

#include "rl_subscription.h"
#include "subscription_manager.h"
#include "rls_mod.h"

authorization_result_t rls_authorize_subscription(struct _subscription_data_t *s);

#endif

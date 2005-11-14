#ifndef __RLS_MOD_H
#define __RLS_MOD_H

#include "../tm/tm_load.h"
#include "rl_subscription.h"

extern struct tm_binds tmb;

/** min interval for subscription expiration */
extern int rls_min_expiration;

/** max interval for subscription expiration */
extern int rls_max_expiration;

/** default expiration timeout */
extern int rls_default_expiration;

/** authorization parameters */
extern rls_auth_params_t rls_auth_params;

#endif

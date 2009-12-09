#ifndef __RLS_MOD_H
#define __RLS_MOD_H

#include "../../modules/tm/tm_load.h"
#include "../../lib/srdb2/db.h"
#include "rl_subscription.h"
#include "../dialog/dlg_mod.h"
#include "rls_data.h"
#include <xcap/xcap_client.h>
#include "../xcap/xcap_mod.h"

extern struct tm_binds tmb;

/** min interval for subscription expiration */
extern int rls_min_expiration;

/** max interval for subscription expiration */
extern int rls_max_expiration;

/* how often test subscriptions for expiration */
extern int rls_expiration_timer_period;

/** default expiration timeout */
extern int rls_default_expiration;

/** authorization parameters */
extern rls_auth_params_t rls_auth_params;

extern int use_db;
extern db_con_t* rls_db; /* database connection handle */
extern db_func_t rls_dbf;	/* database functions */
extern dlg_func_t dlg_func;
extern char *db_url;
extern int reduce_xcap_needs; /* allows XCAP simulation with web server if possible */
extern int rls_timer_interval;

extern fill_xcap_params_func fill_xcap_params;

/* parameters for optimizations */
extern int max_notifications_at_once;

extern int max_list_nesting_level;

extern int rls_ignore_408_on_notify;

#endif

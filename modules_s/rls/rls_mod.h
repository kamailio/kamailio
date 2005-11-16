#ifndef __RLS_MOD_H
#define __RLS_MOD_H

#include "../tm/tm_load.h"
#include "../../db/db.h"
#include "rl_subscription.h"
#include "../dialog/dlg_mod.h"

extern struct tm_binds tmb;

/** min interval for subscription expiration */
extern int rls_min_expiration;

/** max interval for subscription expiration */
extern int rls_max_expiration;

/** default expiration timeout */
extern int rls_default_expiration;

/** authorization parameters */
extern rls_auth_params_t rls_auth_params;

extern int use_db;
extern db_con_t* rls_db; /* database connection handle */
extern db_func_t rls_dbf;	/* database functions */
extern dlg_func_t dlg_func;
extern char *db_url;

#endif

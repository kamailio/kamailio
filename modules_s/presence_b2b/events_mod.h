#ifndef __EVENTS_MOD_H
#define __EVENTS_MOD_H

#include "../tm/tm_load.h"
#include "../../db/db.h"
#include "../dialog/dlg_mod.h"

extern struct tm_binds tmb;
extern dlg_func_t dlg_func;

/** default expiration timeout */
extern int default_expiration;

#endif

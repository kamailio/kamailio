#ifndef __ASYNC_AUTH_H
#define __ASYNC_AUTH_H

#include "presentity.h"
#include <xcap/pres_rules.h>

int ask_auth_rules(presentity_t *p);
int async_auth_timer_init();

extern int max_auth_requests_per_tick;
extern int async_auth_queries;

#endif

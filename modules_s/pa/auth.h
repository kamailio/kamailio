#ifndef __AUTHORIZATION_H
#define __AUTHORIZATION_H


#include "watcher.h"
#include "presentity.h"
#include "qsa_interface.h"

watcher_status_t authorize_internal_watcher(presentity_t *p, internal_pa_subscription_t *is);
watcher_status_t authorize_watcher(presentity_t *p, watcher_t *w);

#endif

#ifndef __AUTHORIZATION_H
#define __AUTHORIZATION_H

#include <xcap/pres_rules.h>

typedef enum {
	auth_none, 
	auth_implicit, /* implicit authorization rules (may differ for packages, ...) */
	auth_xcap
} authorization_type_t;

typedef struct {
	authorization_type_t type;
	char *xcap_root; /* zero terminated (using libcurl, why not?) */
} auth_params_t;

#include "watcher.h"
#include "presentity.h"
#include "qsa_interface.h"

watcher_status_t authorize_internal_watcher(presentity_t *p, internal_pa_subscription_t *is);
watcher_status_t authorize_watcher(presentity_t *p, watcher_t *w);

#endif

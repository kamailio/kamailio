#include "auth.h"
#include "../../parser/parse_event.h"
#include "pa_mod.h"

/* Authorization */

typedef enum { 
	auth_type_xcap
	/* add other authorization methods ? */
} auth_type_t;

/* authorization method */
/*auth_type_t auth_type;
auth_xcap_root*/

watcher_status_t authorize_watcher(presentity_t *p, watcher_t *w)
{
	if (!authorize_watchers) return WS_ACTIVE;
	
	if (w->event_package == EVENT_PRESENCE_WINFO) {
		/* TODO: watcherinfo enable only for the URIs */
		return WS_ACTIVE; /* enable all winfo watchers for tests */
	}
	else {
		/* TODO: XCAP query ? */
		/*return WS_REJECTED;*/
		return WS_PENDING;
		/*return WS_ACTIVE;*/
	}
}

watcher_status_t authorize_internal_watcher(presentity_t *p, internal_pa_subscription_t *is)
{
	if (!authorize_watchers) return WS_ACTIVE;

	/* TODO: XCAP query ? */
/*	return WS_ACTIVE; */
	return WS_PENDING;
}


#include "auth.h"
#include "../../parser/parse_event.h"
#include "pa_mod.h"
#include <string.h>
#include <xcap/pres_rules.h>
#include <cds/logger.h>

/* Authorization */
static watcher_status_t xcap_authorize(presentity_t *p, str *w_uri)
{
	sub_handling_t sh;
	
	if (!p->authorization_info) {
		/* DBG("got empty set of authorization rules for %.*s\n", 
				p->uri.len, ZSW(p->uri.s)); */
		return WS_PENDING;
	}

	/* process rules for given watcher's uri (w_uri) */

	sh = sub_handling_confirm;
	get_pres_rules_action(p->authorization_info, w_uri, &sh);

	switch (sh) {
		case sub_handling_block: 
			DBG("XCAP AUTH: block\n");
			return WS_REJECTED;
		case sub_handling_confirm: 
			DBG("XCAP AUTH: confirm\n");
			return WS_PENDING;
		case sub_handling_polite_block: 
			DBG("XCAP AUTH: polite block\n");
			return WS_REJECTED;
		case sub_handling_allow: 
			DBG("XCAP AUTH: allow\n");
			return WS_ACTIVE;
	}

	return WS_PENDING;
}

static watcher_status_t winfo_implicit_auth(presentity_t *p, watcher_t *w)
{
	/* implicit authorization rules for watcher info */
	/*str_t p_user, w_user;
	
	if (get_user_from_uri(&p->uri, p_user) != 0) return WS_REJECTED;
	if (get_user_from_uri(&w->uri, w_user) != 0) return WS_REJECTED;*/

	if (str_case_equals(&p->data.uri, &w->uri) == 0) {
		DBG("winfo_implicit_auth(%.*s): enabled for %.*s\n", 
				FMT_STR(p->data.uri), FMT_STR(w->uri));
		return WS_ACTIVE;
	}
	else {
		DBG("winfo_implicit_auth(%.*s): disabled for %.*s\n", 
				FMT_STR(p->data.uri), FMT_STR(w->uri));
		return WS_REJECTED;
	}
}

watcher_status_t authorize_watcher(presentity_t *p, watcher_t *w)
{
	if (w->event_package == EVENT_PRESENCE_WINFO) {
		switch (winfo_auth_params.type) {
			case auth_none: return WS_ACTIVE;
			case auth_implicit: return winfo_implicit_auth(p, w);
			case auth_xcap: 
						ERROR_LOG("XCAP authorization for winfo is not implemented! "
								"Using \'implicit\' auth.\n");
						return winfo_implicit_auth(p, w);
		}
	}
	else {
		switch (pa_auth_params.type) {
			case auth_none: return WS_ACTIVE;
			case auth_implicit: return WS_PENDING;
			case auth_xcap: return xcap_authorize(p, &w->uri);
		}
	}
	return WS_PENDING;
}

watcher_status_t authorize_internal_watcher(presentity_t *p, internal_pa_subscription_t *is)
{
	switch (pa_auth_params.type) {
		case auth_none: return WS_ACTIVE;
		case auth_implicit: return WS_PENDING;
		case auth_xcap: return xcap_authorize(p, 
								get_subscriber_id(is->subscription));
	}
	return WS_PENDING;
}


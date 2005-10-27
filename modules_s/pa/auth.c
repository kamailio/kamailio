#include "auth.h"
#include "../../parser/parse_event.h"
#include "pa_mod.h"
#include <string.h>
#include <xcap/pres_rules.h>
#include <cds/logger.h>

int get_user_from_uri(str *uri, str *user)
{
	char *a;
	char *d;
	char *s;
	
	str_clear(user);
	if (uri->len > 0) {
		d = strchr(uri->s, ':');
		if (d) s = d + 1;
		else s = uri->s;
		a = strchr(s, '@');
		if (a) {
			user->s = s;
			user->len = a - s;
			return 0;
		}
	}
	return -1;
}

/* Authorization */
static int xcap_get_pres_rules(str *uri, 
		cp_ruleset_t **dst, auth_params_t *params)
{
	xcap_query_t xcap;
	int res;
	str u;
	
	/* get only presentity name, not whole uri
	 * can't use parse_uri because of absence 
	 * of protocol specification ! */
	if (get_user_from_uri(uri, &u) != 0) u = *uri;
	
	memset(&xcap, 0, sizeof(xcap));
	/* TODO: 
	xcap.auth_user = "???";
	xcap.auth_pass = "???"; */
	xcap.enable_unverified_ssl_peer = 1;
	res = get_pres_rules(params->xcap_root, &u, &xcap, dst);
	return res;
}
		
static watcher_status_t xcap_authorize(presentity_t *p, 
		str *w_uri, auth_params_t *params)
{
	sub_handling_t sh;
	int res = 0;

	/* hack - this is due to unimplemented "subscriptions to XCAP change"
	 * -> reads authorization info on each authorization request - this
	 * is VERY inefficient -> FIXME */
	if (p->authorization_info) {
		free_pres_rules(p->authorization_info);
		p->authorization_info = NULL;
	}
	
	if (!p->authorization_info) {
		res = xcap_get_pres_rules(&p->uri, &p->authorization_info, params);
		if (res != 0) {
			LOG(L_ERR, "can't get authorization rules for %.*s\n", 
					p->uri.len, ZSW(p->uri.s));
			return WS_PENDING;
		}
		if (!p->authorization_info) {
			LOG(L_WARN, "got empty set of authorization rules for %.*s\n", 
					p->uri.len, ZSW(p->uri.s));
			return WS_PENDING;
		}
	}

	/* process rules for given watcher's uri (w_uri) */

	sh = sub_handling_confirm;
	get_pres_rules_action(p->authorization_info, w_uri, &sh);

	switch (sh) {
		case sub_handling_block: 
			DEBUG_LOG("XCAP AUTH: block\n");
			return WS_REJECTED;
		case sub_handling_confirm: 
			DEBUG_LOG("XCAP AUTH: confirm\n");
			return WS_PENDING;
		case sub_handling_polite_block: 
			DEBUG_LOG("XCAP AUTH: polite block\n");
			return WS_REJECTED;
		case sub_handling_allow: 
			DEBUG_LOG("XCAP AUTH: allow\n");
			return WS_ACTIVE;
	}

	return WS_PENDING;
}

watcher_status_t authorize_watcher(presentity_t *p, watcher_t *w)
{
	if (w->event_package == EVENT_PRESENCE_WINFO) {
		/* TODO: watcherinfo enable only for the URIs */
		return WS_ACTIVE; /* enable all winfo watchers for tests */
	}
	else {
		switch (pa_auth_params.type) {
			case auth_none: return WS_ACTIVE;
			case auth_xcap: return xcap_authorize(p, &w->uri, &pa_auth_params);
		}
	}
	return WS_PENDING;
}

watcher_status_t authorize_internal_watcher(presentity_t *p, internal_pa_subscription_t *is)
{
	switch (pa_auth_params.type) {
		case auth_none: return WS_ACTIVE;
		case auth_xcap: return xcap_authorize(p, 
								&is->subscription->subscriber_id,
								&pa_auth_params);
	}
	return WS_PENDING;
}


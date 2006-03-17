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
static int xcap_get_pres_rules(str *uid, 
		cp_ruleset_t **dst, auth_params_t *params)
{
	xcap_query_params_t xcap;
	int res;
	/* str u; */
	
	/* get only presentity name, not whole uri
	 * can't use parse_uri because of absence 
	 * of protocol specification ! */
	/* if (get_user_from_uri(uri, &u) != 0) u = *uri; */
	
	memset(&xcap, 0, sizeof(xcap));
	/* TODO: 
	xcap.auth_user = "???";
	xcap.auth_pass = "???"; */
	xcap.enable_unverified_ssl_peer = 1;
	res = get_pres_rules(params->xcap_root, uid, &xcap, dst);
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
		res = xcap_get_pres_rules(&p->uuid, &p->authorization_info, params);
		if (res != 0) {
			DBG("can't get authorization rules for %.*s\n", 
					p->uri.len, ZSW(p->uri.s));
			return WS_PENDING;
		}
		if (!p->authorization_info) {
			/* DBG("got empty set of authorization rules for %.*s\n", 
					p->uri.len, ZSW(p->uri.s)); */
			return WS_PENDING;
		}
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

	if (str_case_equals(&p->uri, &w->uri) == 0) {
		DBG("winfo_implicit_auth(%.*s): enabled for %.*s\n", 
				FMT_STR(p->uri), FMT_STR(w->uri));
		return WS_ACTIVE;
	}
	else {
		DBG("winfo_implicit_auth(%.*s): disabled for %.*s\n", 
				FMT_STR(p->uri), FMT_STR(w->uri));
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
			case auth_xcap: return xcap_authorize(p, &w->uri, &pa_auth_params);
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
								&is->subscription->subscriber_id,
								&pa_auth_params);
	}
	return WS_PENDING;
}


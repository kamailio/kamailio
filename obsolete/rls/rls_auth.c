#include "rls_auth.h"

#if 0

static int get_user_from_uri(str_t *uri, str_t *user)
{
	char *a;
	char *d;
	char *s;

	/* we can't use SER's parser - the uri may have not the protocol prefix! */
	
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

static authorization_result_t authorize_implicit(struct _subscription_data_t *s)
{
	str_t user, list;
	str_t list_user, list_rest;
	str_t appendix = { s: "-list", len: 5 };
	
	if (get_user_from_uri(&s->subscriber, &user) != 0) 
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */
	if (get_user_from_uri(&s->record_id, &list) != 0)
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */
	
	if (list.len <= appendix.len)
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */
	
	list_rest.len = appendix.len;
	list_rest.s = list.s + list.len - appendix.len;
	if (str_case_equals(&list_rest, &appendix) != 0) 
		return auth_unresolved; /* we can't decide - it is not "implicit" uri */

	/* now we know, that it ends with implicit uri ending */
	
	list_user.s = list.s;
	list_user.len = list.len - appendix.len;
	if (str_case_equals(&user, &list_user) != 0) return auth_rejected;
	else return auth_granted;
}

#endif

authorization_result_t rls_authorize_subscription(struct _subscription_data_t *s)
{
	switch (rls_auth_params.type) {
		case rls_auth_none:
			return auth_granted; /* ! no auth done ! */
		case rls_auth_implicit:
			return auth_granted; /* ! no auth done ! */
			/* return authorize_implicit(s); */
		case rls_auth_xcap:
			LOG(L_ERR, "XCAP auth for resource lists not done yet!\n");
			return auth_unresolved;
	}
	return auth_unresolved;
}


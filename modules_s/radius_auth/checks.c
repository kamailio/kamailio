/*
 * $Id$
 *
 * Checks if To and From header fields contain the same
 * username as digest credentials
 */

#include "checks.h"
#include "../../str.h"
#include "../../dprint.h"
#include <string.h>
#include "defs.h"
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "common.h"
#include "../../ut.h"


/*
 * Check if To header field contains the same username
 * as digest credentials
 */
static inline int check_username(struct sip_msg* _m, struct hdr_field* _h)
{
	struct hdr_field* h;
	auth_body_t* c;

#ifdef USER_DOMAIN_HACK
	char* ptr;
#endif

	str user;
	int len;

	if (!_h) {
		LOG(L_ERR, "check_username(): To HF not found\n");
		return -1;
	}

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);

	user.s = _h->body.s;
	user.len = _h->body.len;

	if (auth_get_username(&user) < 0) {
		LOG(L_ERR, "is_user(): Can't extract username\n");
		return -1;
	}

	if (!user.len) return -1;

	len = c->digest.username.len;

#ifdef USER_DOMAIN_HACK
	ptr = q_memchr(c->digest.username.s, '@', len);
	if (ptr) {
		len = ptr - c->digest.username.s;
	}
#endif

	if (user.len == len) {
		if (strncasecmp(user.s, c->digest.username.s, len) == 0) {
			DBG("check_username(): auth id and To username are equal\n");
			return 1;
		}
	}
	
	DBG("check_username(): auth id and To username differ\n");
	return -1;
}


/*
 * Check username part in To header field
 */
int check_to(struct sip_msg* _msg, char* _s1, char* _s2)
{
	return check_username(_msg, _msg->to);
}


/*
 * Check username part in From header field
 */
int check_from(struct sip_msg* _msg, char* _s1, char* _s2)
{
	return check_username(_msg, _msg->from);
}

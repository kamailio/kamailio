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


/*
 * Finds specified character, that is not quoted 
 * If there is no such character, returns NULL
 *
 * PARAMS : char* _b : input buffer
 *        : char _c  : character to find
 * RETURNS: char*    : points to character found, NULL if not found
 */
static inline char* find_not_quoted(str* _b, char _c)
{
	int quoted = 0, i;
	
	if (_b->s == 0) return NULL;

	for(i = 0; i < _b->len; i++) {
		if (!quoted) {
			if (_b->s[i] == '\"') quoted = 1;
			else if (_b->s[i] == _c) return _b->s + i;
		} else {
			if ((_b->s[i] == '\"') && (_b->s[i - 1] != '\\')) quoted = 0;
		}
	}
	return NULL;
}


/*
 * Cut username part of a URL
 */
static inline void get_username(str* _s)
{
	char* at, *dcolon, *dc;

	     /* Find double colon, double colon
	      * separates schema and the rest of
	      * URL
	      */
	dcolon = find_not_quoted(_s, ':');

	     /* No double colon found means error */
	if (!dcolon) {
		_s->len = 0;
		return;
	}

	     /* Skip the double colon */
	_s->len -= dcolon + 1 - _s->s;
	_s->s = dcolon + 1;

	     /* Try to find @ or another doublecolon
	      * if the URL contains also pasword, username
	      * and password will be delimited by double
	      * colon, if there is no password, @ delimites
	      * username from the rest of the URL, if there
	      * is no @, there is no username in the URL
	      */
	at = memchr(_s->s, '@', _s->len); /* FIXME: one pass */
	dc = memchr(_s->s, ':', _s->len);
	if (at) {
		     /* The double colon must be before
		      * @ to delimit username, otherwise
		      * it delimits hostname from port number
		      */
		if ((dc) && (dc < at)) {
			_s->len = dc - dcolon - 1;
			return;
		}
		
		_s->len = at - dcolon - 1;
	} else {
		_s->len = 0;
	} 
}


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

	get_username(&user);

	if (!user.len) return -1;

	len = c->digest.username.len;

#ifdef USER_DOMAIN_HACK
	ptr = memchr(c->digest.username.s, '@', len);
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

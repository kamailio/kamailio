/*
 * $Id$
 *
 * Common stuff
 */

#include "common.h"
#include <string.h> 
#include "../../dprint.h"
#include "../../ut.h"      /* q_memchr */


/*
 * Find a character occurence that is not quoted
 */
char* find_not_quoted(str* _s, char _c)
{
	int quoted = 0, i;
	
	for(i = 0; i < _s->len; i++) {
		if (!quoted) {
			if (_s->s[i] == '\"') quoted = 1;
			else if (_s->s[i] == _c) return _s->s + i;
		} else {
			if ((_s->s[i] == '\"') && (_s->s[i - 1] != '\\')) quoted = 0;
		}
	}
	return 0;
}


/*
 * Extract username part from URI
 */
int get_user(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = find_not_quoted(_s, ':');

	LOG(L_ERR, "get_user(): %.*s\n", _s->len, _s->s);
	if (dcolon == 0) {
		LOG(L_ERR, "No : found\n");
		abort();
		return -1;
	}

	_s->s = dcolon + 1;
	_s->len -= dcolon - _s->s + 1;
	
	LOG(L_ERR, "get_user2(): %.*s\n", _s->len, _s->s);

	at = q_memchr(_s->s, '@', _s->len);
	dc = q_memchr(_s->s, ':', _s->len);
	if (at) {
		if ((dc) && (dc < at)) {
			_s->len = dc - _s->s;
			return 0;
		}
		
		_s->len = at - _s->s;
		return 0;
	} else return -2;
}

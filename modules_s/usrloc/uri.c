/*
 * $Id$
 *
 * URI related functions
 */

#include "uri.h"
#include "common.h"


/*
 * This function skips name part
 * uri parsed by parse_contact must be used
 * (the uri must not contain any leading or
 *  trailing part and if angle bracket were
 *  used, right angle bracket must be the
 *  last character in the string)
 *
 * _s will be modified so it should be a tmp
 * copy
 */
void get_raw_uri(str* _s)
{
	char* aq;
	
	if (_s->s[_s->len - 1] == '>') {
		aq = find_not_quoted(_s, '<');
		_s->len -= aq - _s->s + 2;
		_s->s = aq + 1;
	}
}


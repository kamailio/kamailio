/*
 * $Id$
 */

#include "utils.h"
#include "../../dprint.h"
#include <string.h>


/*
 * To field parser for usrloc module
 */


void get_username(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = find_not_quoted(_s->s, ':');

	if (!dcolon) {
		_s->len = 0;
		return;
	}
	_s->s = dcolon + 1;

	at = strchr(_s->s, '@');
	dc = strchr(_s->s, ':');
	if (at) {
		if ((dc) && (dc < at)) {
			_s->len = dc - dcolon - 1;
			return;
		}
		
		_s->len = at - dcolon - 1;
		/*	_s->s[_s->len] = '\0'; */
	} else {
		_s->len = 0;
	} 
	return;
}




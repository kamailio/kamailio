/*
 * Presence Agent, common functions
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "common.h"
#include "../../ut.h"


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
 * Extract username part from a URI
 */
int extract_username(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = find_not_quoted(_s, ':');

	if (dcolon == 0) return -1;

	_s->s = dcolon + 1;
	_s->len -= dcolon - _s->s + 1;
	
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

	if (_s->len < 1) return;
	
	if (_s->s[_s->len - 1] == '>') {
		aq = find_not_quoted(_s, '<');
		_s->len -= aq - _s->s + 2;
		_s->s = aq + 1;
	}
}

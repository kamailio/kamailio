/* $Id: utils.c
 *
 * Set of utils to extract the user-name from the FROM field
 * borrowed from the auth module.
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
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

#include "utils.h"
#include "../../ut.h"            /* q_memchr* */

/*
 * Finds specified character, that is not quoted 
 * If there is no such character, returns NULL
 *
 * PARAMS : char* _b : input buffer
 *        : char _c  : character to find
 * RETURNS: char*    : points to character found, NULL if not found
 */
static inline char* auth_fnq(str* _b, char _c)
{
	int quoted = 0, i;
	
	if (_b->s == 0) return 0;

	for(i = 0; i < _b->len; i++) {
		if (!quoted) {
			if (_b->s[i] == '\"') quoted = 1;
			else if (_b->s[i] == _c) return _b->s + i;
		} else {
			if ((_b->s[i] == '\"') && (_b->s[i - 1] != '\\')) quoted = 0;
		}
	}
	return 0;
}

/*
 * Cut username part of a URL
 */
int auth_get_username(str* _s)
{
	char* at, *dcolon, *dc;

	     /* Find double colon, double colon
	      * separates schema and the rest of
	      * URL
	      */
	dcolon = auth_fnq(_s, ':');

	     /* No double colon found means error */
	if (!dcolon) {
		_s->len = 0;
		return -1;
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
	at = q_memchr(_s->s, '@', _s->len); /* FIXME: one pass */
	dc = q_memchr(_s->s, ':', _s->len);
	if (at) {
		     /* The double colon must be before
		      * @ to delimit username, otherwise
		      * it delimits hostname from port number
		      */
		if ((dc) && (dc < at)) {
			_s->len = dc - dcolon - 1;
			return 0;
		}
		
		_s->len = at - dcolon - 1;
		return 0;
	}

	_s->len = 0;
	return -2;
}

/*
 * This method simply cleans off the trailing character of the string body.
 * params: str body
 * returns: the new char* or NULL on failure
 */
char * cleanbody(str body) 
{	
	char* tmp;
	/*
	 * This works because when the structure is created it is memset to 0
	 */
	if (body.s == NULL)
		return NULL;
		
	tmp = &body.s[0];
	tmp[body.len] = '\0';

	return tmp;
}


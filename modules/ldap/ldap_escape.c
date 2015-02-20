/*
 * Kamailio LDAP Module
 *
 * Copyright (C) 2007 University of North Carolina
 *
 * Original author: Christian Schlatter, cs@unc.edu
 *
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stdio.h>
#include <ctype.h>
#include "ldap_escape.h"

static const char hex[] = "0123456789ABCDEF";

/*
* escape string following RFC 4515 (LDAP filter syntax) escaping rules:
*
*   * --> \2a
*   ( --> \28
*   ) --> \29
*   \ --> \5c
*
* and percent encode '?' according RFC 4516 (LDAP URL), Section 2.1
* if url_encode equals TRUE
*
*   ? --> %3F
*
*/
int ldap_rfc4515_escape(str *sin, str *sout, int url_encode)
{
	char *src, *dst;
	
	if (sin == NULL || sout == NULL || sin->s == NULL || sout->s == NULL 
			|| sin->len <= 0 || sout->len < 3*sin->len+1)
	{
		return -1;
	}

	src = sin->s;
	dst = sout->s;

	while (src < (sin->s + sin->len))
	{
		switch (*src)
		{
		case '*':
			*dst++ = '\\';
			*dst++ = '2';
			*dst = 'a';
			break;
		case '(':
			*dst++ = '\\';
			*dst++ = '2';
			*dst = '8';
			break;
		case ')':
			*dst++ = '\\';
			*dst++ = '2';
			*dst = '9';
			break;
		case '\\':
			*dst++ = '\\';
			*dst++ = '5';
			*dst = 'c';
			break;
		case '?':
			if (url_encode)
			{
				*dst++ = '%';
				*dst++ = '3';
				*dst = 'F';
			} else
			{
				*dst = *src;
			}
			break;
		default:
			*dst = *src;
		}

		src++;
		dst++;
	}

	*dst = 0;
	sout->len = dst - sout->s;
	return 0;
}

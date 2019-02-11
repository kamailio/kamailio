/*
 *
 * Copyright (C) 2015 kamailio.org
 * Copyright (C) 2014 Victor Seva <vseva@sipwise.com>
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*!
 * \file
 * \brief Kamailio core :: Append to str data
 * \ingroup core
 * Module: \ref core
 */

#include <string.h>
#include "str.h"
#include "mem/mem.h"

int str_append(str *orig, str *suffix, str *dest)
{
	if(orig == NULL || suffix == NULL || suffix->len == 0 || dest == NULL)
	{
		LM_ERR("wrong parameters\n");
		return -1;
	}
	dest->len = orig->len + suffix->len;
	dest->s = pkg_malloc(sizeof(char)*dest->len);
	if(dest->s==NULL)
	{
		PKG_MEM_ERROR;
		return -1;
	}
	if(orig->len>0)
	{
		memcpy(dest->s, orig->s, orig->len);
	}
	memcpy(dest->s+orig->len, suffix->s, suffix->len);
	return 0;
}

/*
* Find the first occurrence of find in s, where the search is limited to the
* first slen characters of s.
*/
char * _strnstr(const char* s, const char* find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

/*
 * Find the first case insensitive occurrence of find in s, where the
 * search is limited to the first slen characters of s.
 * Based on FreeBSD strnstr.
 */
char* _strnistr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0' || slen-- < 1)
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}


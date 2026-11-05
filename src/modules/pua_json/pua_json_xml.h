/*
 * XML escaping helpers for the pua_json module
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/* Include after Kamailio's str and pkg_malloc declarations. */

#ifndef _PUA_JSON_XML_H_
#define _PUA_JSON_XML_H_

#include <limits.h>
#include <stddef.h>
#include <string.h>

/* Escape XML text and attribute values. The caller owns *storage. */
static const char *pua_json_xml_entity(unsigned char ch, size_t *len)
{
	switch(ch) {
		case '&':
			*len = 5;
			return "&amp;";
		case '<':
			*len = 4;
			return "&lt;";
		case '>':
			*len = 4;
			return "&gt;";
		case '"':
			*len = 6;
			return "&quot;";
		case '\'':
			*len = 6;
			return "&apos;";
		default:
			*len = 1;
			return NULL;
	}
}

static int pua_json_xml_escape_fields(
		const str *src, str *dst, int count, char **storage)
{
	size_t total = 0;
	size_t step;
	const char *entity;
	char *buf;
	char *p;
	unsigned char ch;
	int i;
	int j;

	if(src == NULL || dst == NULL || storage == NULL || count < 0)
		return -1;
	*storage = NULL;

	for(i = 0; i < count; i++) {
		if(src[i].len < 0 || (src[i].len > 0 && src[i].s == NULL))
			return -1;
		for(j = 0; j < src[i].len; j++) {
			ch = (unsigned char)src[i].s[j];
			/* These control characters are forbidden in XML 1.0. */
			if(ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r')
				return -1;
			entity = pua_json_xml_entity(ch, &step);
			(void)entity;
			if(total > (size_t)INT_MAX - step - 1)
				return -1;
			total += step;
		}
		if(total == (size_t)INT_MAX)
			return -1;
		total++; /* NUL terminator per field */
	}

	buf = pkg_malloc(total ? total : 1);
	if(buf == NULL)
		return -1;
	p = buf;
	for(i = 0; i < count; i++) {
		dst[i].s = p;
		for(j = 0; j < src[i].len; j++) {
			ch = (unsigned char)src[i].s[j];
			entity = pua_json_xml_entity(ch, &step);
			if(entity != NULL) {
				memcpy(p, entity, step);
				p += step;
			} else {
				*p++ = (char)ch;
			}
		}
		dst[i].len = (int)(p - dst[i].s);
		*p++ = '\0';
	}
	*storage = buf;
	return 0;
}

#endif /* _PUA_JSON_XML_H_ */

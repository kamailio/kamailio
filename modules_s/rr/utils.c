/* 
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


#include <ctype.h>
#include "utils.h"
#include <string.h>
#include "../../dprint.h"


/*
 * Miscelaneous utilities
 */


/*
 * Finds specified character, that is not quoted 
 * If there is no such character, returns NULL
 *
 * PARAMS : char* _b : input buffer
 *        : char _c  : character to find
 * RETURNS: char*    : points to character found, NULL if not found
 */
char* find_not_quoted(char* _b, char _c)
{
	int quoted = 0;
	
	if (!_b) return NULL;

	while (*_b) {
		if (!quoted) {
			if (*_b == '\"') quoted = 1;
			else if (*_b == _c) return _b;
		} else {
			if ((*_b == '\"') && (*(_b-1) != '\\')) quoted = 0;
		}
		_b++;
	}
	return NULL;
}


/*
 * Remove any leading spaces and tabs
 */
inline char* trim_leading(char* _s)
{
#ifdef PARANOID
	if (!_s) return NULL;
#endif
	     /* Remove spaces and tabs from the begining of string */
	while ((*_s == ' ') || (*_s == '\t')) _s++;
	return _s;
}


/*
 * Remove any trailing spaces and tabs
 */
inline char* trim_trailing(char* _s)
{
	int len;
	char* end;

#ifdef PARANOID
	if (!_s) return NULL;
#endif
	len = strlen(_s);

        end = _s + len - 1;

	     /* Remove trailing spaces and tabs */
	while ((*end == ' ') || (*end == '\t')) end--;
	if (end != (_s + len - 1)) {
		*(end+1) = '\0';
	}
	return _s;
}


/*
 * Remove any tabs and spaces from the begining and the end of
 * a string
 */
char* trim(char* _s)
{
	_s = trim_leading(_s);
	return trim_trailing(_s);
}


/* 
 * Eat linear white space 
 *
 * PARAMS : char* _b : input buffer
 * RETURNS: char*    : points after skipped WS
 */
char* eat_lws(char* _b)
{
	while ((*_b == ' ') || (*_b == '\t')) _b++;
	while ((*_b == '\r') || (*_b == '\n')) _b++;
	while ((*_b == ' ') || (*_b == '\t')) _b++;
	return _b;
}


/* Substitute CR LF characters in field body with spaces */
struct hdr_field* remove_crlf(struct hdr_field* _hf)
{
	char* tmp;
	for (tmp = _hf->body.s; (*tmp); tmp++)
		if ((*tmp == '\r') || (*tmp == '\n'))
			*tmp = ' ';
	return _hf;
}


char* strlower(char* _s, int _len)
{
	int i;

	for(i = 0; i < _len; i++) {
		_s[i] = tolower(_s[i]);
	}
	return _s;
}


char* strupper(char* _s, int _len)
{
	int i;

	for(i = 0; i < _len; i++) {
		_s[i] = toupper(_s[i]);
	}

	return _s;
}




/*
 * This function returns pointer to the beginning of URI
 *
 * PARAMS : char* _b : input buffer
 * RETURNS: char*    : points immediately after name part
 *
 */
char* eat_name(char* _b)
{
	int quoted = 0;
	char* b = _b;
	char* last_ws = _b;

	if (!_b) return NULL;

	     /* < means start of URI, : is URI scheme
	      * separator, these two characters cannot
	      * occur in non-quoted name string
	      */
	while(*b) {
		if (!quoted) {
			if ((*b == ' ') || (*b == '\t')) {
				last_ws = b;
			} else {
				if (*b == '<') return b;  /* We will end here if there is a name */
				if (*b == ':') return last_ws; /* There is no name in this case */
				if (*b == '\"') quoted = 1;
			}
		} else {
			if ((*b == '\"') && (*(b-1) != '\\')) quoted = 0;
		}
		b++;
	}

	return _b;  /* Some error */
}
		


void bin2hex(unsigned char* _hex, unsigned char* _bin, int _blen)
{
	unsigned short i;
	unsigned char j;
    
	for (i = 0; i < _blen; i++) {
		j = (_bin[i] >> 4) & 0xf;
		if (j <= 9)
			_hex[i * 2] = (j + '0');
		else
			_hex[i * 2] = (j + 'a' - 10);
		j = _bin[i] & 0xf;
		if (j <= 9)
			_hex[i * 2 + 1] = (j + '0');
		else
			_hex[i * 2 + 1] = (j + 'a' - 10);
	};
	_hex[_blen * 2] = '\0';
}
